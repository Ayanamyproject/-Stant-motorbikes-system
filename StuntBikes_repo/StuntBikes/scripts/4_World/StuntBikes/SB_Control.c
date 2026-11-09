// Physics-style stunt control (Trials / real riding feel).
// The stock motorbike stabilizer kills pitch rotation, so while a stunt is in progress this
// class runs its own pitch dynamics and writes the result into the body's angular velocity.
//
// Wheelie:  must be STARTED with lean back (pull on the bars) + gas. Once up, gas keeps it up,
//           gravity pulls it down, past the balance point it goes over (loop out). Rear brake
//           drops the nose. Lean only helps a little - feathering the gas is the skill.
// Stoppie:  must be started with lean forward + front brake at speed.
// Air:      lean rotates, gas = nose up (rear wheel spin), rear brake = nose down.
//
// All inputs are smoothed (keyboard keys are on/off), forces are smoothed, so motion is fluid.
// Runs on the server AND on the driver's client with the same inputs.

enum SB_PitchMode
{
	NONE,
	WHEELIE,
	STOPPIE,
	AIR
}

class SB_Control
{
	protected float m_Lean;          // smoothed rider lean -1..1 (+ = back)
	protected float m_Thr;           // smoothed throttle 0..1
	protected float m_RearBrake;     // smoothed
	protected float m_FrontBrake;    // smoothed
	protected float m_W;             // our pitch rate (rad/s, + = nose up)
	protected float m_Alpha;         // smoothed pitch acceleration
	protected int m_Mode;
	protected float m_DebugTimer;
	protected float m_FallTimer;
	protected bool m_LoopedOut;

	static float ReadLocalLean()
	{
		float lean = 0;
		UAInput inBack = GetUApi().GetInputByName("UAStuntLeanBack");
		UAInput inFwd = GetUApi().GetInputByName("UAStuntLeanForward");
		if (inBack)
			lean += inBack.LocalValue();
		if (inFwd)
			lean -= inFwd.LocalValue();
		return Math.Clamp(lean, -1, 1);
	}

	protected static float Approach(float value, float target, float rate, float dt)
	{
		float k = 1.0 - Math.Pow(2.718281828, -rate * dt);
		return value + (target - value) * k;
	}

	int GetMode()
	{
		return m_Mode;
	}

	float GetPitchRate()
	{
		return m_W;
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

	// Server side in multiplayer: follow the pitch the driver's client reports, so the server
	// copy of the bike matches what the rider sees and reconciliation has nothing to correct.
	void Follow(MotorbikeScript bike, float remotePitch, float remoteRate, float age, float dt, SB_Config cfg)
	{
		if (dt <= 0 || dt > 0.25)
			return;
		if (m_DebugTimer > 0)
			m_DebugTimer -= dt;

		vector fwd = bike.GetDirection();
		vector up = bike.GetDirectionUp();
		vector axis = (fwd * up).Normalized();
		vector angVel = dBodyGetAngularVelocity(bike);
		float rate = vector.Dot(angVel, axis);

		float predicted = remotePitch + remoteRate * Math.RAD2DEG * Math.Min(age, 0.2);
		float mine = FullPitch(bike);
		float err = WrapDeg(predicted - mine);
		float wanted = remoteRate + err * Math.DEG2RAD * cfg.FollowGain;
		wanted = Math.Clamp(wanted, -cfg.MaxPitchRate * 1.5, cfg.MaxPitchRate * 1.5);
		m_W = wanted;

		dBodySetAngularVelocity(bike, angVel + axis * (wanted - rate));

		// looped out on the ground: rider falls
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
			Print("[StuntBikes] server follow client=" + remotePitch + " pred=" + predicted + " mine=" + mine + " err=" + err + " rate=" + rate + " -> " + wanted + " age=" + age);
		}
	}

	void Step(MotorbikeScript bike, float rawLean, float dt, SB_Config cfg)
	{
		if (dt <= 0 || dt > 0.25)
			return;
		if (m_DebugTimer > 0)
			m_DebugTimer -= dt;

		// ---- smoothed inputs ------------------------------------------------
		float rawThr = Math.Clamp(bike.GetThrottle(), 0, 1);
		if (!bike.EngineIsOn())
			rawThr = 0;
		m_Lean = Approach(m_Lean, rawLean, cfg.LeanResponse, dt);
		m_Thr = Approach(m_Thr, rawThr, cfg.ThrottleResponse, dt);
		m_RearBrake = Approach(m_RearBrake, Math.Clamp(bike.GetBrake(), 0, 1), cfg.ThrottleResponse, dt);
		m_FrontBrake = Approach(m_FrontBrake, Math.Clamp(bike.GetHandbrake(), 0, 1), cfg.ThrottleResponse, dt);

		// ---- state ------------------------------------------------------------
		bool front = bike.WheelHasContact(0);
		bool rear = bike.WheelHasContact(1);
		bool airborne = !front && !rear;
		float speed = bike.GetSpeedometerAbsolute();

		vector fwd = bike.GetDirection();
		vector up = bike.GetDirectionUp();
		vector axis = (fwd * up).Normalized(); // + rotation = nose up
		vector angVel = dBodyGetAngularVelocity(bike);
		float rate = vector.Dot(angVel, axis);

		vector n = "0 1 0";
		if (rear)
			n = bike.WheelGetContactNormal(1);
		else if (front)
			n = bike.WheelGetContactNormal(0);
		if (n.Length() < 0.1)
			n = "0 1 0";
		n.Normalize();
		float pitch = Math.Asin(Math.Clamp(vector.Dot(fwd, n), -1, 1)) * Math.RAD2DEG;

		// ---- which stunt (if any) owns the pitch -----------------------------
		int mode = SB_PitchMode.NONE;
		if (airborne)
		{
			mode = SB_PitchMode.AIR;
		}
		else if (m_LoopedOut)
		{
			mode = SB_PitchMode.NONE; // looped out: let the bike fall naturally
			if (pitch < cfg.WheelieStartAngle)
				m_LoopedOut = false;
		}
		else if (m_Mode == SB_PitchMode.WHEELIE && rear && pitch > cfg.WheelieStartAngle)
		{
			mode = SB_PitchMode.WHEELIE; // keep riding the wheelie
		}
		else if (m_Mode == SB_PitchMode.STOPPIE && front && pitch < -cfg.WheelieStartAngle)
		{
			mode = SB_PitchMode.STOPPIE;
		}
		else if (rear && m_Lean > cfg.EngageLean && speed >= cfg.MinStuntSpeed * 0.5)
		{
			mode = SB_PitchMode.WHEELIE; // pull on the bars to pop the front
		}
		else if (front && m_Lean < -cfg.EngageLean && m_FrontBrake > 0.3 && speed >= cfg.MinStuntSpeed)
		{
			mode = SB_PitchMode.STOPPIE;
		}
		else if (rear && !front && pitch > cfg.TakeOverAngle)
		{
			mode = SB_PitchMode.WHEELIE; // e.g. landed on the rear wheel from a ramp
		}

		if (mode == SB_PitchMode.NONE)
		{
			m_Mode = mode;
			m_W = rate;
			m_Alpha = 0;
			m_FallTimer = 0;
			return;
		}

		if (m_Mode != mode)
		{
			// take over smoothly from the current motion
			if (m_Mode == SB_PitchMode.NONE)
				m_W = rate;
			m_Mode = mode;
		}

		// ---- pitch acceleration -----------------------------------------------
		float alpha = 0;
		if (mode == SB_PitchMode.AIR)
		{
			alpha = m_Lean * cfg.AirLeanAccel + m_Thr * cfg.AirThrottleAccel - m_RearBrake * cfg.AirBrakeAccel - m_W * cfg.AirDamping;
		}
		else if (mode == SB_PitchMode.WHEELIE)
		{
			float gearFactor = Math.Clamp(1.0 - speed / cfg.ThrottleLiftFadeSpeed, cfg.ThrottleLiftMinFactor, 1.0);
			// gravity: pulls down below the balance point, pulls over above it
			float gravity = Math.Sin((pitch - cfg.WheelieBalanceAngle) * Math.DEG2RAD) * cfg.GravityPitchAccel;
			float lift = m_Thr * cfg.ThrottleLiftAccel * gearFactor;
			alpha = lift + gravity + m_Lean * cfg.GroundLeanAccel - m_RearBrake * cfg.RearBrakeDropAccel - m_W * cfg.GroundDamping;
		}
		else if (mode == SB_PitchMode.STOPPIE)
		{
			float speedFactor = Math.Clamp(speed / cfg.StoppieFullSpeed, 0, 1);
			float gravityS = Math.Sin((-pitch - cfg.StoppieBalanceAngle) * Math.DEG2RAD) * cfg.GravityPitchAccel;
			alpha = -m_FrontBrake * cfg.FrontBrakeLiftAccel * speedFactor - gravityS + m_Lean * cfg.GroundLeanAccel - m_W * cfg.GroundDamping;
		}

		// smooth the force itself so nothing snaps
		m_Alpha = Approach(m_Alpha, alpha, cfg.ForceResponse, dt);

		// a real hit (landing, obstacle) changed the spin a lot: follow the physics, no fight
		if (Math.AbsFloat(rate - m_W) > cfg.ResyncRate)
			m_W = Approach(m_W, rate, 20, dt);

		m_W = m_W + m_Alpha * dt;

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
			dBodySetAngularVelocity(bike, angVel + axis * (m_W - rate));

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
		{
			m_DebugTimer = 0.2;
			string side = "client";
			if (GetGame().IsServer())
				side = "server";
			Print("[StuntBikes] " + side + " mode=" + mode + " pitch=" + pitch + " w=" + m_W + " a=" + m_Alpha + " thr=" + m_Thr + " rb=" + m_RearBrake + " fb=" + m_FrontBrake + " lean=" + m_Lean + " v=" + speed + " rate=" + rate + " dt=" + dt);
		}
	}
}
