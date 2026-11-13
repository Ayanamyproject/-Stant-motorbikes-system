// Stunt physics ported from MX Bikes (PiBoSo), default data = MSM 250XF.
//
// The stock DayZ motorbike stabilizer kills pitch rotation, so while a stunt is in progress
// this class runs its own rigid-body pitch dynamics and writes the result into the body's
// angular velocity. Nothing here is a "wheelie button": the bike lifts when the physics say so.
//
// Engine:   MX Bikes torque + engine-brake curves, crank inertia, rev limiter.
// Clutch:   lever with dead zone + autoclutch, max clutch torque. Pull it, rev, drop it.
// Gearbox:  MX Bikes ratios, primary and final drive (virtual autoshift by default).
// Wheelie:  pivot on the rear contact patch.  torque = drive force * COM height
//                                                    - weight * COM distance ahead of the contact
//           -> balance point where the COM is right above the rear wheel (~42 deg seated).
// Stoppie:  same on the front contact patch with the front brake force.
// Rider:    body is a mass that slides fore/aft (lean keys) and up/down (pump key). Moving it
//           shifts the COM and the push/pull on the bars gives the "pop".
// Air:      free rotation about the COM. Gas spins the rear wheel up -> nose up, rear brake
//           stops it -> nose down (angular momentum of the wheel). Body english, whips, scrubs.
//
// Runs on the driver's machine (offline: the only one). The dedicated server follows it.

enum SB_PitchMode
{
	NONE,
	WHEELIE,
	STOPPIE,
	AIR
}

class SB_Control
{
	static const float SB_G = 9.81;

	// smoothed inputs
	protected float m_Lean;          // -1..1 (+ = back)
	protected float m_Thr;           // 0..1
	protected float m_RearBrake;
	protected float m_FrontBrake;
	protected float m_Clutch;        // 0 = engaged, 1 = lever pulled
	protected float m_Pump;          // 0..1 crouch

	// rider body
	protected float m_RiderX;        // m, + = back
	protected float m_RiderXV;
	protected float m_RiderXA;
	protected float m_RiderY;        // m, + = up
	protected float m_RiderYV;
	protected float m_RiderYA;

	// drivetrain
	protected float m_EngineW;       // rad/s crank
	protected float m_RearWheelW;    // rad/s, + = rolling forward
	protected int m_VGear = 1;
	protected int m_Gear;            // gear used this step, 0 = no drive
	protected float m_WheelTorque;   // Nm at the rear wheel
	protected float m_DriveForce;    // N at the rear contact (after traction limit)
	protected bool m_ClutchLocked;

	// rotation
	protected float m_W;             // pitch rate rad/s (+ = nose up)
	protected float m_Alpha;         // smoothed pitch acceleration
	protected float m_RollW;
	protected float m_YawW;
	protected int m_Mode;
	protected float m_DebugTimer;
	protected float m_FallTimer;
	protected bool m_LoopedOut;

	protected float m_AssistTime;    // wobble clock
	protected float m_LastPitch;     // for the HUD
	protected bool m_Pop;            // rider level: front popping up after the key press
	protected float m_CatchTimer;    // rider level: system has taken over after the red zone
	protected float m_Drift;
	protected int m_Level;           // stunt level used in the last step

	ref SB_TestInputs m_Test;        // automated test only

	// ------------------------------------------------------------------ inputs
	static float ReadLocalInput(string name)
	{
		UAInput inp = GetUApi().GetInputByName(name);
		if (!inp)
			return 0;
		return Math.Clamp(inp.LocalValue(), 0, 1);
	}

	// + = nose up. The UP arrow action is still called UAStuntLeanForward (v1 name) so that
	// key bindings players already saved keep working: UP = nose up / wheelie.
	static float ReadLocalLean()
	{
		return Math.Clamp(ReadLocalInput("UAStuntLeanForward") - ReadLocalInput("UAStuntLeanBack"), -1, 1);
	}

	protected static float Approach(float value, float target, float rate, float dt)
	{
		float k = 1.0 - Math.Pow(2.718281828, -rate * dt);
		return value + (target - value) * k;
	}

	protected static float Interp(array<float> xs, array<float> ys, float x)
	{
		int n = xs.Count();
		if (x <= xs[0])
			return ys[0];
		if (x >= xs[n - 1])
			return ys[n - 1];
		for (int i = 1; i < n; i++)
		{
			if (x <= xs[i])
			{
				float t = (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
				return ys[i - 1] + (ys[i] - ys[i - 1]) * t;
			}
		}
		return ys[n - 1];
	}

	// ------------------------------------------------------------------ getters
	int GetMode()
	{
		return m_Mode;
	}

	float GetPitchRate()
	{
		return m_W;
	}

	float GetClutch()
	{
		return m_Clutch;
	}

	float GetPump()
	{
		return m_Pump;
	}

	int GetGearUsed()
	{
		return m_Gear;
	}

	float GetLastPitch()
	{
		return m_LastPitch;
	}

	bool IsCatching()
	{
		return m_CatchTimer > 0;
	}

	int GetLevel()
	{
		return m_Level;
	}

	float GetEngineRPM()
	{
		return m_EngineW * 30.0 / Math.PI;
	}

	// full 360 degree pitch in the bike's vertical plane (+ = nose up), independent of heading
	static float FullPitch(MotorbikeScript bike)
	{
		vector fwd = bike.GetDirection();
		vector up = bike.GetDirectionUp();
		return Math.Atan2(fwd[1], up[1]) * Math.RAD2DEG;
	}

	static float WrapDeg(float a)
	{
		while (a > 180)
			a -= 360;
		while (a < -180)
			a += 360;
		return a;
	}

	// The engine rotates a body about its centre of mass. A wheelie / stoppie rotates about the
	// tyre contact patch, so add the matching linear velocity: v += dOmega x (com - pivot).
	// Without this the rear end is pushed into the ground and the bike digs in and stops.
	static void PivotCorrect(MotorbikeScript bike, vector dOmega, int wheelIdx)
	{
		if (!bike.WheelHasContact(wheelIdx))
			return;
		vector pivot = bike.WheelGetContactPosition(wheelIdx);
		if (vector.Distance(pivot, bike.GetPosition()) > 4)
			pivot = bike.ModelToWorld(pivot); // returned in model space
		vector com = bike.ModelToWorld(dBodyGetCenterOfMass(bike));
		vector r = com - pivot;
		if (r.Length() > 3)
			return; // nonsense contact, do nothing
		SetVelocity(bike, GetVelocity(bike) + dOmega * r);
	}

	// ------------------------------------------------------------------ rider body
	protected void StepRider(float dt, SB_Config cfg)
	{
		float w = cfg.RiderBodyFreq;
		float targetX = m_Lean * cfg.RiderShift;
		m_RiderXA = w * w * (targetX - m_RiderX) - 2.0 * w * m_RiderXV;
		m_RiderXV += m_RiderXA * dt;
		m_RiderX += m_RiderXV * dt;

		float targetY = -m_Pump * cfg.RiderCrouch;
		m_RiderYA = w * w * (targetY - m_RiderY) - 2.0 * w * m_RiderYV;
		m_RiderYV += m_RiderYA * dt;
		m_RiderY += m_RiderYV * dt;
	}

	// legs pushing the bike into its suspension (pumping a lip). Ground only.
	protected void ApplyPump(MotorbikeScript bike, float dt, SB_Config cfg)
	{
		if (cfg.PumpForceScale <= 0 || cfg.RiderCrouch <= 0)
			return;
		if (!bike.WheelHasContact(0) && !bike.WheelHasContact(1))
			return;
		// rider accelerating up pushes the bike down and the other way round
		float force = -cfg.RiderMass * m_RiderYA * cfg.PumpForceScale;
		force = Math.Clamp(force, -cfg.RiderMass * SB_G * 1.5, cfg.RiderMass * SB_G * 1.5);
		vector impulse = bike.GetDirectionUp() * (force * dt);
		dBodyApplyImpulse(bike, impulse);
	}

	// server: run only the rider's up/down body from the input the client reported
	void StepPumpRemote(MotorbikeScript bike, float pump, float dt, SB_Config cfg)
	{
		if (dt <= 0 || dt > 0.25)
			return;
		m_Pump = Approach(m_Pump, Math.Clamp(pump, 0, 1), cfg.ThrottleResponse * 2, dt);
		StepRider(dt, cfg);
		ApplyPump(bike, dt, cfg);
	}

	// ------------------------------------------------------------------ engine / clutch / gearbox
	protected float EngineTorque(float rpm, float thr, SB_Config cfg)
	{
		// idle governor (MX Bikes IdleThrottle)
		float idleThr = Math.Clamp((cfg.IdleRPM - rpm) / 500.0, 0, 1);
		if (thr < idleThr)
			thr = idleThr;

		float drive = 0;
		if (rpm < cfg.LimiterRPM)
			drive = Interp(cfg.TorqueRPM, cfg.TorqueNm, rpm);
		float braking = Interp(cfg.EngineBrakeRPM, cfg.EngineBrakeNm, rpm);
		return thr * drive + (1.0 - thr) * braking;
	}

	protected float GearRatio(int gear, SB_Config cfg)
	{
		if (gear <= 0)
			return 0;
		int idx = gear;
		if (idx > cfg.GearRatios.Count())
			idx = cfg.GearRatios.Count();
		idx = idx - 1;
		return cfg.PrimaryRatio * cfg.GearRatios[idx] * cfg.FinalRatio;
	}

	protected void UpdateGear(MotorbikeScript bike, bool airborne, SB_Config cfg)
	{
		int gameGear = bike.GetGear() - bike.GetNeutralGear();
		if (gameGear < 0)
		{
			m_Gear = 0; // reverse: no stunt drive
			return;
		}
		if (cfg.UseGameGear)
		{
			m_Gear = gameGear;
			return;
		}

		// virtual MX Bikes gearbox: shift on the locked engine speed
		if (!airborne && m_Clutch < 0.5)
		{
			float lockedRpm = m_RearWheelW * GearRatio(m_VGear, cfg) * 30.0 / Math.PI;
			if (lockedRpm > cfg.UpshiftRPM && m_VGear < cfg.GearRatios.Count())
				m_VGear++;
			else if (lockedRpm < cfg.DownshiftRPM && m_VGear > 1)
				m_VGear--;
		}
		m_Gear = m_VGear;
	}

	protected void StepDrivetrain(bool engineOn, bool airborne, float dt, SB_Config cfg)
	{
		float ratio = GearRatio(m_Gear, cfg);
		float maxW = cfg.LimiterRPM * Math.PI / 30.0 * 1.05;
		float idleW = cfg.IdleRPM * Math.PI / 30.0;
		float brakeT = m_RearBrake * cfg.RearBrakeMaxForce * cfg.RearWheelRadius;

		if (!engineOn)
		{
			m_EngineW = Approach(m_EngineW, 0, 3.0, dt);
			m_WheelTorque = 0;
			m_ClutchLocked = false;
			if (airborne)
				StepAirWheel(0, brakeT, dt, cfg);
			return;
		}

		float rpm = m_EngineW * 30.0 / Math.PI;
		float te = EngineTorque(rpm, m_Thr, cfg);

		// clutch capacity: lever (with dead zone) and autoclutch (2500-3000 rpm)
		float lever = Math.Clamp((1.0 - m_Clutch) / (1.0 - cfg.ClutchDeadZone), 0, 1);
		float autoEngage = Math.Clamp((rpm - 2500.0) / 500.0, 0, 1);
		float cap = cfg.ClutchMaxTorque * Math.Min(lever, autoEngage);
		if (ratio <= 0)
			cap = 0;

		float wLock = m_RearWheelW * ratio;   // crank speed if the clutch were locked
		float teShaft = te * cfg.PrimaryRatio; // engine torque at the clutch shaft
		float slip = m_EngineW - wLock;
		float tcl = 0;                         // torque through the clutch (clutch shaft)
		float wheelGear = 0;
		if (ratio > 0)
			wheelGear = ratio / cfg.PrimaryRatio * cfg.DriveEfficiency;

		m_ClutchLocked = cap > 0 && Math.AbsFloat(slip) < 20 && Math.AbsFloat(teShaft) <= cap && wLock > idleW * 0.8;

		if (m_ClutchLocked && airborne)
		{
			// engine and rear wheel spin together: the crank inertia is felt through the gearing
			float inertia = cfg.RearWheelInertia + cfg.EngineInertia * ratio * ratio;
			float wheelSign = 1;
			if (m_RearWheelW < 0)
				wheelSign = -1;
			float tw = te * ratio * cfg.DriveEfficiency - brakeT * wheelSign;
			m_RearWheelW = m_RearWheelW + tw / Math.Max(inertia, 0.01) * dt;
			m_EngineW = Math.Clamp(m_RearWheelW * ratio, 0, maxW);
			m_RearWheelW = m_EngineW / ratio;
			m_WheelTorque = te * ratio * cfg.DriveEfficiency;
			return;
		}

		if (m_ClutchLocked)
		{
			m_EngineW = Math.Clamp(wLock, 0, maxW);
			tcl = teShaft;
		}
		else
		{
			if (cap > 0)
				tcl = cap * Math.Clamp(slip / 20.0, -1, 1);
			float newW = m_EngineW + (te - tcl / cfg.PrimaryRatio) / cfg.EngineInertia * dt;
			if (cap > 0 && (m_EngineW - wLock) * (newW - wLock) < 0)
				newW = wLock; // slip crossed zero: clutch bites
			m_EngineW = Math.Clamp(newW, idleW * 0.5, maxW);
		}

		m_WheelTorque = tcl * wheelGear;

		if (airborne)
			StepAirWheel(m_WheelTorque, brakeT, dt, cfg);
	}

	// free rear wheel in the air. Returns nothing: the wheel's change of angular momentum is
	// applied to the chassis in Step (gas = nose up, rear brake = nose down).
	protected void StepAirWheel(float driveT, float brakeT, float dt, SB_Config cfg)
	{
		if (cfg.RearWheelInertia <= 0)
			return;
		float before = m_RearWheelW;
		float after = before + driveT / cfg.RearWheelInertia * dt;
		// brake only slows the wheel, never spins it backwards
		float brakeStep = brakeT / cfg.RearWheelInertia * dt;
		if (after > 0)
			after = Math.Max(0, after - brakeStep);
		else
			after = Math.Min(0, after + brakeStep);
		m_RearWheelW = after;
	}

	// ------------------------------------------------------------------ server follow
	// Dedicated server: follow the full orientation and spin the driver's client reports, so
	// the server copy matches what the rider sees and reconciliation has nothing to correct.
	void Follow(MotorbikeScript bike, int remoteMode, vector rFwd, vector rUp, vector rAngVel, float age, float dt, SB_Config cfg)
	{
		if (dt <= 0 || dt > 0.25)
			return;
		if (m_DebugTimer > 0)
			m_DebugTimer -= dt;

		vector fwd = bike.GetDirection();
		vector up = bike.GetDirectionUp();
		vector right = fwd * up;
		vector rRight = rFwd * rUp;

		// rotation vector from our orientation to the reported one (|v| = sin(angle))
		vector err = (fwd * rFwd + up * rUp + right * rRight) * 0.5;
		// predict where the client is now
		err = err + rAngVel * Math.Min(age, 0.2);

		vector wanted = rAngVel + err * cfg.FollowGain;
		float maxRate = cfg.MaxPitchRate * 1.5;
		if (wanted.Length() > maxRate)
			wanted = wanted.Normalized() * maxRate;
		vector before = dBodyGetAngularVelocity(bike);
		dBodySetAngularVelocity(bike, wanted);
		if (remoteMode == SB_PitchMode.WHEELIE)
			PivotCorrect(bike, wanted - before, 1);
		else if (remoteMode == SB_PitchMode.STOPPIE)
			PivotCorrect(bike, wanted - before, 0);

		vector axis = right.Normalized();
		m_W = vector.Dot(wanted, axis);

		// looped out on the ground: rider falls
		float mine = FullPitch(bike);
		bool onGround = bike.WheelHasContact(0) || bike.WheelHasContact(1);
		if (onGround && (mine > cfg.LoopOutAngle || mine < -cfg.EndoAngle))
		{
			m_FallTimer += dt;
			if (m_FallTimer > 0.3 && !bike.IsFallen())
			{
				m_FallTimer = 0;
				bike.FallOver();
			}
		}
		else
		{
			m_FallTimer = 0;
		}

		if (cfg.DebugLog && m_DebugTimer <= 0)
		{
			m_DebugTimer = 0.2;
			Print("[StuntBikes] server follow pitch=" + mine + " err=" + err.Length() + " wanted=" + wanted + " age=" + age);
		}
	}

	// ------------------------------------------------------------------ main step (driver)
	void Step(MotorbikeScript bike, float rawLean, float rawClutch, float rawPump, float dt, SB_Config cfg)
	{
		if (dt <= 0 || dt > 0.25)
			return;
		if (m_DebugTimer > 0)
			m_DebugTimer -= dt;

		// ---- smoothed inputs --------------------------------------------------
		float rawThr = Math.Clamp(bike.GetThrottle(), 0, 1);
		float rawRear = Math.Clamp(bike.GetBrake(), 0, 1);
		float rawFront = Math.Clamp(bike.GetHandbrake(), 0, 1);
		bool engineOn = bike.EngineIsOn();
		if (m_Test)
		{
			rawThr = m_Test.Throttle;
			rawRear = m_Test.RearBrake;
			rawFront = m_Test.FrontBrake;
			if (m_Test.ForceEngine)
				engineOn = true;
		}
		if (!engineOn)
			rawThr = 0;
		m_Thr = Approach(m_Thr, rawThr, cfg.ThrottleResponse, dt);
		m_RearBrake = Approach(m_RearBrake, rawRear, cfg.ThrottleResponse, dt);
		m_FrontBrake = Approach(m_FrontBrake, rawFront, cfg.ThrottleResponse, dt);
		m_Clutch = Approach(m_Clutch, Math.Clamp(rawClutch, 0, 1), cfg.ClutchResponse, dt);
		m_Pump = Approach(m_Pump, Math.Clamp(rawPump, 0, 1), cfg.ThrottleResponse * 2, dt);
		m_Lean = Math.Clamp(rawLean, -1, 1); // the body spring below does the smoothing

		StepRider(dt, cfg);

		// ---- state ------------------------------------------------------------
		bool front = bike.WheelHasContact(0);
		bool rear = bike.WheelHasContact(1);
		bool airborne = !front && !rear;
		vector fwd = bike.GetDirection();
		// speed from the physics body (the speedometer reads 0 in some states)
		float speed = Math.AbsFloat(vector.Dot(GetVelocity(bike), fwd)) * 3.6;

		vector up = bike.GetDirectionUp();
		vector axis = (fwd * up).Normalized(); // + rotation = nose up
		vector angVel = dBodyGetAngularVelocity(bike);
		float rate = vector.Dot(angVel, axis);
		float rollRate = vector.Dot(angVel, fwd);
		float yawRate = vector.Dot(angVel, up);

		vector n = "0 1 0";
		if (rear)
			n = bike.WheelGetContactNormal(1);
		else if (front)
			n = bike.WheelGetContactNormal(0);
		if (n.Length() < 0.1)
			n = "0 1 0";
		n.Normalize();
		float pitch = Math.Asin(Math.Clamp(vector.Dot(fwd, n), -1, 1)) * Math.RAD2DEG;

		m_LastPitch = pitch;
		m_Level = SB_Settings.GetMode(cfg);
		if (m_Test && m_Test.Level >= 0)
			m_Level = m_Test.Level;
		if (m_Level == 0)
		{
			StepAssist(bike, front, rear, airborne, speed, pitch, axis, angVel, rate, dt, cfg);
			return;
		}
		if (m_Level == 1)
		{
			StepRiderLevel(bike, front, rear, airborne, speed, pitch, axis, angVel, rate, dt, cfg);
			return;
		}

		// ---- drivetrain ---------------------------------------------------------
		float wheelBefore = m_RearWheelW;
		if (!airborne)
		{
			float vFwd = vector.Dot(GetVelocity(bike), fwd);
			m_RearWheelW = vFwd / cfg.RearWheelRadius;
		}
		UpdateGear(bike, airborne, cfg);
		StepDrivetrain(engineOn, airborne, dt, cfg);

		ApplyPump(bike, dt, cfg);

		// ---- mass properties with the rider where he is now ----------------------
		float mass = cfg.BikeMass + cfg.RiderMass;
		float k = cfg.RiderMass / mass;
		float lr = Math.Max(0.05, cfg.RearToCom - m_RiderX * k);
		float lf = Math.Max(0.05, cfg.FrontToCom + m_RiderX * k);
		float h = Math.Max(0.2, cfg.ComHeight + m_RiderY * k);
		float weight = mass * SB_G;
		float tauPull = cfg.RiderMass * m_RiderXA * cfg.BarPullArm; // + = nose up

		// ---- contact forces -------------------------------------------------------
		float drive = m_WheelTorque / cfg.RearWheelRadius;
		// rear load with weight transfer under acceleration
		float nRear = weight;
		if (front)
			nRear = Math.Min(weight, (weight * lf + Math.Max(drive, 0) * h) / (lf + lr));
		float rearBrakeF = 0;
		if (speed > 1)
			rearBrakeF = m_RearBrake * cfg.RearBrakeMaxForce;
		float fRear = Math.Clamp(drive - rearBrakeF, -cfg.TyreFriction * nRear, cfg.TyreFriction * nRear);
		m_DriveForce = fRear;

		// the brake can only push as long as the bike is still rolling: fade out below 12 km/h
		float frontBrakeF = m_FrontBrake * cfg.FrontBrakeMaxForce * Math.Clamp(speed / 12.0, 0, 1);
		frontBrakeF = Math.Min(frontBrakeF, cfg.TyreFriction * weight);

		// pivot torques at the current angle
		float th = pitch * Math.DEG2RAD;
		float wx = lr * Math.Cos(th) - h * Math.Sin(th);
		float wy = lr * Math.Sin(th) + h * Math.Cos(th);
		float tauWheelie = fRear * wy - weight * wx + tauPull;

		float ph = -th;
		float sx = lf * Math.Cos(ph) - h * Math.Sin(ph);
		float sy = lf * Math.Sin(ph) + h * Math.Cos(ph);
		float tauStoppie = frontBrakeF * sy - weight * sx - tauPull; // + = rear goes up

		// ---- which stunt (if any) owns the pitch -----------------------------
		int mode = SB_PitchMode.NONE;
		if (airborne)
		{
			mode = SB_PitchMode.AIR;
		}
		else if (m_LoopedOut)
		{
			mode = SB_PitchMode.NONE; // looped out: let the bike fall naturally
			if (pitch < cfg.WheelieStartAngle && pitch > -cfg.WheelieStartAngle)
				m_LoopedOut = false;
		}
		else if (m_Mode == SB_PitchMode.WHEELIE && rear && pitch > cfg.WheelieStartAngle && (!front || tauWheelie > 0))
		{
			mode = SB_PitchMode.WHEELIE; // keep riding it until the front wheel is back down
		}
		else if (m_Mode == SB_PitchMode.STOPPIE && front && pitch < -cfg.WheelieStartAngle && (!rear || tauStoppie > 0))
		{
			mode = SB_PitchMode.STOPPIE;
		}
		else if (rear && m_Lean > 0.3 && speed >= cfg.MinStuntSpeed && tauWheelie > 0 && pitch > -cfg.WheelieStartAngle)
		{
			mode = SB_PitchMode.WHEELIE; // front wheel unloads
		}
		else if (front && m_Lean < -0.3 && speed >= cfg.MinStuntSpeed && tauStoppie > 0 && pitch < cfg.WheelieStartAngle)
		{
			mode = SB_PitchMode.STOPPIE; // rear wheel unloads
		}
		else if (rear && !front && pitch > cfg.TakeOverAngle)
		{
			mode = SB_PitchMode.WHEELIE; // e.g. landed on the rear wheel from a ramp
		}

		if (mode == SB_PitchMode.NONE)
		{
			m_Mode = mode;
			m_W = rate;
			m_RollW = rollRate;
			m_YawW = yawRate;
			m_Alpha = 0;
			m_FallTimer = 0;
			SB_DebugPrint(bike, mode, pitch, speed, rate, dt, cfg);
			return;
		}

		if (m_Mode != mode)
		{
			// take over smoothly from the current motion
			if (m_Mode == SB_PitchMode.NONE)
				m_W = rate;
			if (mode == SB_PitchMode.AIR)
			{
				m_RollW = rollRate;
				m_YawW = yawRate;
			}
			m_Mode = mode;
		}

		// ---- pitch acceleration -----------------------------------------------
		float alpha = 0;
		if (mode == SB_PitchMode.AIR)
		{
			alpha = m_Lean * cfg.AirLeanAccel - m_W * cfg.AirDamping;
		}
		else if (mode == SB_PitchMode.WHEELIE)
		{
			float iRear = cfg.PitchInertia + mass * (lr * lr + h * h);
			alpha = tauWheelie / iRear;
		}
		else if (mode == SB_PitchMode.STOPPIE)
		{
			float iFront = cfg.PitchInertia + mass * (lf * lf + h * h);
			alpha = -tauStoppie / iFront;
		}

		// smooth the force itself so nothing snaps
		m_Alpha = Approach(m_Alpha, alpha, cfg.ForceResponse, dt);

		// a real hit (landing, obstacle) changed the spin a lot: follow the physics, no fight
		if (Math.AbsFloat(rate - m_W) > cfg.ResyncRate)
			m_W = Approach(m_W, rate, 20, dt);

		m_W = m_W + m_Alpha * dt;

		// air: the rear wheel's change of spin goes into the chassis (gas = nose up)
		if (mode == SB_PitchMode.AIR)
			m_W = m_W + cfg.RearWheelInertia * (m_RearWheelW - wheelBefore) / cfg.PitchInertia;

		float maxRate = cfg.GroundMaxPitchRate;
		if (mode == SB_PitchMode.AIR)
			maxRate = cfg.MaxPitchRate;
		m_W = Math.Clamp(m_W, -maxRate, maxRate);

		// wheel coming back down: let it settle softly instead of slamming into the ground
		if (mode == SB_PitchMode.WHEELIE && pitch < 6 && m_W < 0)
			m_W = Math.Max(m_W, -cfg.SettleRate * Math.Clamp(pitch / 6.0, 0, 1));
		if (mode == SB_PitchMode.STOPPIE && pitch > -6 && m_W > 0)
			m_W = Math.Min(m_W, cfg.SettleRate * Math.Clamp(-pitch / 6.0, 0, 1));

		// looped out / over the bars: release control so the bike falls naturally
		if (mode == SB_PitchMode.WHEELIE && pitch > cfg.LoopOutAngle)
		{
			m_LoopedOut = true;
			m_Mode = SB_PitchMode.NONE;
		}
		if (mode == SB_PitchMode.STOPPIE && pitch < -cfg.EndoAngle)
		{
			m_LoopedOut = true;
			m_Mode = SB_PitchMode.NONE;
		}

		if (!m_LoopedOut)
		{
			vector newAng = angVel + axis * (m_W - rate);
			if (mode == SB_PitchMode.AIR && cfg.AirTricks)
			{
				float steer = Math.Clamp(bike.GetSteering(), -1, 1);
				m_RollW = m_RollW + (steer * cfg.AirRollTorque / cfg.RollInertia - m_RollW * cfg.AirTrickDamping) * dt;
				m_YawW = m_YawW + (steer * cfg.AirYawTorque / cfg.YawInertia - m_YawW * cfg.AirTrickDamping) * dt;
				m_RollW = Math.Clamp(m_RollW, -cfg.MaxPitchRate, cfg.MaxPitchRate);
				m_YawW = Math.Clamp(m_YawW, -cfg.MaxPitchRate, cfg.MaxPitchRate);
				newAng = newAng + fwd * (m_RollW - rollRate) + up * (m_YawW - yawRate);
			}
			dBodySetAngularVelocity(bike, newAng);
			if (mode == SB_PitchMode.WHEELIE)
				PivotCorrect(bike, axis * (m_W - rate), 1);
			else if (mode == SB_PitchMode.STOPPIE)
				PivotCorrect(bike, axis * (m_W - rate), 0);
		}

		// rider falls if the bike stays past the limit (server decides)
		if (GetGame().IsServer() && !airborne && (pitch > cfg.LoopOutAngle || pitch < -cfg.EndoAngle))
		{
			m_FallTimer += dt;
			if (m_FallTimer > 0.3 && !bike.IsFallen())
			{
				m_FallTimer = 0;
				if (cfg.DebugLog)
					Print("[StuntBikes] loop-out / endo at pitch " + pitch + " -> FallOver");
				bike.FallOver();
			}
		}
		else
		{
			m_FallTimer = 0;
		}

		if (cfg.DebugLog && m_DebugTimer <= 0)
			Print("[StuntBikes] tauW=" + tauWheelie + " tauS=" + tauStoppie + " a=" + m_Alpha + " w=" + m_W);
		SB_DebugPrint(bike, mode, pitch, speed, rate, dt, cfg);
	}

	// air for the novice and rider levels: UP/DOWN rotate, no key = level out for landing
	protected void StepEasyAir(bool key, float pitch, float dt, SB_Config cfg)
	{
		float airAlpha = 0;
		if (key)
			airAlpha = m_Lean * cfg.AssistAirRotate - m_W * 0.8;
		else if (cfg.AssistAirLevel)
			airAlpha = -pitch * Math.DEG2RAD * 3.0 - m_W * 2.5;
		m_W = Math.Clamp(m_W + airAlpha * dt, -2.5, 2.5);
	}

	// ------------------------------------------------------------------ RIDER level (variant 2)
	// UP pops the front to RiderPopAngle. Then the player holds it: on a wheelie UP raises and
	// DOWN lowers (on a stoppie DOWN raises the rear, UP lowers it), with no key the wheel sinks
	// and a small random drift has to be corrected. Above the green zone the bike wants to go
	// over; at the red angle the system catches it and brings it back into the green zone.
	protected void StepRiderLevel(MotorbikeScript bike, bool front, bool rear, bool airborne, float speed, float pitch, vector axis, vector angVel, float rate, float dt, SB_Config cfg)
	{
		bool upKey = m_Lean > 0.5;
		bool downKey = m_Lean < -0.5;
		int mode = SB_PitchMode.NONE;

		if (airborne)
		{
			mode = SB_PitchMode.AIR;
		}
		else if (m_Mode == SB_PitchMode.WHEELIE && rear && pitch > cfg.WheelieStartAngle && speed >= cfg.AssistMinSpeed * 0.5 && (!front || m_Pop))
		{
			mode = SB_PitchMode.WHEELIE;
		}
		else if (m_Mode == SB_PitchMode.STOPPIE && front && pitch < -cfg.WheelieStartAngle && speed >= cfg.AssistMinSpeed * 0.5 && (!rear || m_Pop))
		{
			mode = SB_PitchMode.STOPPIE;
		}
		else if (rear && upKey && speed >= cfg.AssistMinSpeed && pitch > -cfg.WheelieStartAngle)
		{
			mode = SB_PitchMode.WHEELIE;
			m_Pop = true;
		}
		else if (front && downKey && speed >= cfg.AssistMinSpeed && pitch < cfg.WheelieStartAngle)
		{
			mode = SB_PitchMode.STOPPIE;
			m_Pop = true;
		}
		else if (rear && !front && pitch > cfg.TakeOverAngle)
		{
			mode = SB_PitchMode.WHEELIE; // landed on the rear wheel
		}
		else if ((m_Mode == SB_PitchMode.WHEELIE && !front && pitch > cfg.WheelieStartAngle) || (m_Mode == SB_PitchMode.STOPPIE && !rear && pitch < -cfg.WheelieStartAngle))
		{
			mode = m_Mode; // too slow: still bring the wheel down softly
		}

		if (mode == SB_PitchMode.NONE)
		{
			m_Mode = mode;
			m_W = rate;
			m_Pop = false;
			m_CatchTimer = 0;
			m_Drift = 0;
			SB_DebugPrint(bike, mode, pitch, speed, rate, dt, cfg);
			return;
		}
		if (m_Mode == SB_PitchMode.NONE)
			m_W = rate;
		m_Mode = mode;

		if (mode == SB_PitchMode.AIR)
		{
			m_Pop = false;
			m_CatchTimer = 0;
			StepEasyAir(upKey || downKey, pitch, dt, cfg);
		}
		else
		{
			bool wheelie = mode == SB_PitchMode.WHEELIE;
			float lift = pitch;     // degrees the lifted wheel is up
			float greenMin = cfg.RiderGreenMin;
			float greenMax = cfg.RiderGreenMax;
			float red = cfg.RiderRedAngle;
			bool raise = upKey;
			bool lower = downKey;
			if (!wheelie)
			{
				lift = -pitch;
				greenMin = cfg.RiderStoppieGreenMin;
				greenMax = cfg.RiderStoppieGreenMax;
				red = cfg.RiderStoppieRedAngle;
				raise = downKey;
				lower = upKey;
			}

			bool slow = speed < cfg.AssistMinSpeed * 0.5;
			float liftRate = 0; // rad/s, + = wheel goes higher
			if (slow)
			{
				m_Pop = false;
				liftRate = -cfg.AssistDropRate;
			}
			else if (m_CatchTimer > 0)
			{
				m_CatchTimer -= dt;
				float mid = (greenMin + greenMax) * 0.5;
				liftRate = Math.Clamp((mid - lift) * Math.DEG2RAD * 3.0, -cfg.AssistDropRate, cfg.AssistLiftRate);
			}
			else if (m_Pop)
			{
				liftRate = cfg.AssistLiftRate;
				if (lift >= cfg.RiderPopAngle || lower)
					m_Pop = false;
			}
			else
			{
				if (raise)
					liftRate = cfg.RiderRaiseRate;
				else if (lower)
					liftRate = -cfg.RiderLowerRate;
				else
					liftRate = -cfg.RiderSinkRate;

				// drift the rider has to correct
				m_Drift = Math.Clamp(m_Drift + Math.RandomFloat(-1, 1) * cfg.RiderDrift * 4.0 * dt, -cfg.RiderDrift, cfg.RiderDrift);
				liftRate += m_Drift;

				// past the green zone the bike wants to keep going over
				if (lift > greenMax)
					liftRate += (lift - greenMax) * Math.DEG2RAD * 2.0;

				if (lift >= red)
				{
					m_CatchTimer = cfg.RiderCatchTime;
					if (cfg.DebugLog)
						Print("[StuntBikes] rider: red zone at " + lift + " deg, catching");
				}
			}

			float wantedRate = liftRate;
			if (!wheelie)
				wantedRate = -liftRate;
			float maxStepR = cfg.AssistAccel * dt;
			m_W = m_W + Math.Clamp(wantedRate - m_W, -maxStepR, maxStepR);

			if (wheelie && pitch < 6 && m_W < 0)
				m_W = Math.Max(m_W, -cfg.SettleRate * Math.Clamp(pitch / 6.0, 0.15, 1));
			if (!wheelie && pitch > -6 && m_W > 0)
				m_W = Math.Min(m_W, cfg.SettleRate * Math.Clamp(-pitch / 6.0, 0.15, 1));
		}

		dBodySetAngularVelocity(bike, angVel + axis * (m_W - rate));
		if (mode == SB_PitchMode.WHEELIE)
			PivotCorrect(bike, axis * (m_W - rate), 1);
		else if (mode == SB_PitchMode.STOPPIE)
			PivotCorrect(bike, axis * (m_W - rate), 0);

		SB_DebugPrint(bike, mode, pitch, speed, rate, dt, cfg);
	}

	// ------------------------------------------------------------------ ASSIST mode (variant 1)
	// Hold UP: the front rises to AssistWheelieAngle and stays there. Release: it comes down
	// softly. Hold DOWN: the same on the front wheel (stoppie). Gas alone never lifts the bike,
	// nothing can loop out. In the air UP/DOWN rotate, no key = the bike levels itself.
	protected void StepAssist(MotorbikeScript bike, bool front, bool rear, bool airborne, float speed, float pitch, vector axis, vector angVel, float rate, float dt, SB_Config cfg)
	{
		bool upKey = m_Lean > 0.5;
		bool downKey = m_Lean < -0.5;
		int mode = SB_PitchMode.NONE;
		float target = 0;

		if (airborne)
		{
			mode = SB_PitchMode.AIR;
		}
		else if (rear && upKey && speed >= cfg.AssistMinSpeed && pitch > -cfg.WheelieStartAngle)
		{
			mode = SB_PitchMode.WHEELIE;
			target = cfg.AssistWheelieAngle;
		}
		else if (front && downKey && speed >= cfg.AssistMinSpeed && pitch < cfg.WheelieStartAngle)
		{
			mode = SB_PitchMode.STOPPIE;
			target = -cfg.AssistStoppieAngle;
		}
		else if (m_Mode == SB_PitchMode.WHEELIE && rear && !front && pitch > cfg.WheelieStartAngle)
		{
			mode = SB_PitchMode.WHEELIE; // key released or too slow: bring it down
		}
		else if (m_Mode == SB_PitchMode.STOPPIE && front && !rear && pitch < -cfg.WheelieStartAngle)
		{
			mode = SB_PitchMode.STOPPIE;
		}
		else if (rear && !front && pitch > cfg.TakeOverAngle)
		{
			mode = SB_PitchMode.WHEELIE; // landed on the rear wheel: put the front down gently
		}

		if (mode == SB_PitchMode.NONE)
		{
			m_Mode = mode;
			m_W = rate;
			m_AssistTime = 0;
			SB_DebugPrint(bike, mode, pitch, speed, rate, dt, cfg);
			return;
		}
		if (m_Mode == SB_PitchMode.NONE)
			m_W = rate; // take over from the current motion
		m_Mode = mode;

		float wanted = 0;
		if (mode == SB_PitchMode.AIR)
		{
			StepEasyAir(upKey || downKey, pitch, dt, cfg);
		}
		else
		{
			if (target != 0)
			{
				m_AssistTime += dt;
				// a little life in it, like a rider feathering the gas
				target += Math.Sin(m_AssistTime * 2.1) * cfg.AssistWobble + Math.Sin(m_AssistTime * 3.7) * cfg.AssistWobble * 0.4;
			}
			wanted = Math.Clamp((target - pitch) * Math.DEG2RAD * cfg.AssistGain, -cfg.AssistDropRate, cfg.AssistLiftRate);
			if (mode == SB_PitchMode.STOPPIE)
				wanted = Math.Clamp((target - pitch) * Math.DEG2RAD * cfg.AssistGain, -cfg.AssistLiftRate, cfg.AssistDropRate);
			float maxStep = cfg.AssistAccel * dt;
			m_W = m_W + Math.Clamp(wanted - m_W, -maxStep, maxStep);

			// last few degrees: touch down softly
			if (mode == SB_PitchMode.WHEELIE && pitch < 6 && m_W < 0)
				m_W = Math.Max(m_W, -cfg.SettleRate * Math.Clamp(pitch / 6.0, 0.15, 1));
			if (mode == SB_PitchMode.STOPPIE && pitch > -6 && m_W > 0)
				m_W = Math.Min(m_W, cfg.SettleRate * Math.Clamp(-pitch / 6.0, 0.15, 1));
		}

		dBodySetAngularVelocity(bike, angVel + axis * (m_W - rate));
		if (mode == SB_PitchMode.WHEELIE)
			PivotCorrect(bike, axis * (m_W - rate), 1);
		else if (mode == SB_PitchMode.STOPPIE)
			PivotCorrect(bike, axis * (m_W - rate), 0);

		if (cfg.DebugLog && m_DebugTimer <= 0)
			Print("[StuntBikes] assist target=" + target + " wanted=" + wanted + " w=" + m_W);
		SB_DebugPrint(bike, mode, pitch, speed, rate, dt, cfg);
	}

	protected void SB_DebugPrint(MotorbikeScript bike, int mode, float pitch, float speed, float rate, float dt, SB_Config cfg)
	{
		if (!cfg.DebugLog || m_DebugTimer > 0)
			return;
		m_DebugTimer = 0.2;
		string side = "client";
		if (GetGame().IsServer())
			side = "server";
		Print("[StuntBikes] " + side + " mode=" + mode + " pitch=" + pitch + " v=" + speed + " gear=" + m_Gear + " rpm=" + GetEngineRPM() + " clutch=" + m_Clutch + " locked=" + m_ClutchLocked + " Tw=" + m_WheelTorque + " F=" + m_DriveForce + " thr=" + m_Thr + " rb=" + m_RearBrake + " fb=" + m_FrontBrake + " riderX=" + m_RiderX + " riderY=" + m_RiderY + " wheelW=" + m_RearWheelW + " rate=" + rate + " dt=" + dt);
	}
}
