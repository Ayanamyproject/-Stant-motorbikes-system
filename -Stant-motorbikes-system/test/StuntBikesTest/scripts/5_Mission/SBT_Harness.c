// Automated physics test (server only). Starts 25 s after mission start, spawns a bike south
// of the test spot and drives it with scripted inputs. Results: "[SBT-H]" lines in script_*.log.

class SBT_Harness
{
	protected static ref SBT_Harness s_Instance;
	protected MotorbikeScript m_Bike;
	protected ref SB_TestInputs m_In;
	protected float m_T;
	protected float m_MaxPitch;
	protected float m_MinPitch;
	protected string m_Phase;
	protected vector m_StartPos;

	static void Start(vector spot)
	{
		if (s_Instance)
			return;
		s_Instance = new SBT_Harness();
		s_Instance.Init(spot);
	}

	void Init(vector spot)
	{
		vector pos = spot + Vector(12, 0, -10); // clear lane north of the test spot, beside the ramps
		pos[1] = GetGame().SurfaceY(pos[0], pos[2]) + 0.4;
		m_Bike = MotorbikeScript.Cast(GetGame().CreateObjectEx("Motorbike_02_Red", pos, ECE_PLACE_ON_SURFACE | ECE_CREATEPHYSICS | ECE_SETUP));
		if (!m_Bike)
		{
			Print("[SBT-H] cannot create bike");
			return;
		}
		m_Bike.SetOrientation(Vector(0, 0, 0));
		m_StartPos = pos;
		m_Bike.GetInventory().CreateAttachment("Motorbike_02_Wheel_1");
		m_Bike.GetInventory().CreateAttachment("Motorbike_02_Wheel_2");
		m_Bike.GetInventory().CreateAttachment("SparkPlug");
		m_Bike.Fill(MotorbikeFluid.FUEL, m_Bike.GetFluidCapacity(MotorbikeFluid.FUEL));
		m_Bike.SetKickstand(0);
		m_In = new SB_TestInputs();
		m_In.Speed = 0;
		m_T = 0;
		Print("[SBT-H] harness bike at " + pos.ToString());
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(StartDrive, 3000, false);
	}

	void StartDrive()
	{
		dBodyActive(m_Bike, ActiveState.ALWAYS_ACTIVE);
		SetVelocity(m_Bike, m_Bike.GetDirection() * 2.0);
		m_Bike.SB_SetTest(m_In);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Tick, 100, true);
	}

	protected void SetPhase(string name)
	{
		if (m_Phase == name)
			return;
		if (m_Phase != "")
			Print("[SBT-H] RESULT " + m_Phase + " maxPitch=" + m_MaxPitch + " minPitch=" + m_MinPitch + " fallen=" + m_Bike.IsFallen());
		// start each test group upright on the flat start spot
		if (m_Bike.IsFallen() || Math.AbsFloat(Roll()) > 30 || name.IndexOf("reset") >= 0)
		{
			m_Bike.SetPosition(m_StartPos);
			SetVelocity(m_Bike, "0 0 0");
			vector o = m_Bike.GetOrientation();
			m_Bike.SetOrientation(Vector(o[0], 0, 0));
			m_Bike.PickUp();
			dBodySetAngularVelocity(m_Bike, "0 0 0");
			Print("[SBT-H] bike reset upright, fallen=" + m_Bike.IsFallen());
		}
		m_Phase = name;
		m_MaxPitch = -999;
		m_MinPitch = 999;
		Print("[SBT-H] PHASE " + name + " t=" + m_T);
	}

	float Roll()
	{
		vector right = (m_Bike.GetDirection() * m_Bike.GetDirectionUp()).Normalized();
		return Math.Asin(Math.Clamp(right[1], -1, 1)) * Math.RAD2DEG;
	}

	void Tick()
	{
		if (!m_Bike)
		{
			Print("[SBT-H] bike is gone, stopping");
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(Tick);
			return;
		}
		m_T += 0.1;
		float t = m_T;
		// a parked bike without rider sleeps and gets no physics steps
		dBodyActive(m_Bike, ActiveState.ALWAYS_ACTIVE);

		m_In.Clutch = 0;
		m_In.RearBrake = 0;
		m_In.FrontBrake = 0;
		m_In.Lean = 0;
		m_In.Speed = 8;

		float cur = SB_Control.FullPitch(m_Bike);
		m_In.Throttle = 0.4;
		if (t < 2)
		{
			SetPhase("N0_novice_full_gas_no_key");
			m_In.Level = 0;
			m_In.Throttle = 1;
		}
		else if (t < 5)
		{
			SetPhase("N1_novice_hold_UP");
			m_In.Lean = 1;
		}
		else if (t < 7)
		{
			SetPhase("N2_novice_release");
		}
		else if (t < 9)
		{
			SetPhase("R0_reset_rider_cruise");
			m_In.Level = 1;
		}
		else if (t < 12)
		{
			SetPhase("R1_rider_tap_UP_then_nothing");
			if (t < 9.3)
				m_In.Lean = 1;
		}
		else if (t < 16)
		{
			SetPhase("R2_rider_holds_green");
			// rider taps the keys to stay between 30 and 40 deg
			if (cur < 30)
				m_In.Lean = 1;
			else if (cur > 40)
				m_In.Lean = -1;
		}
		else if (t < 19)
		{
			SetPhase("R3_rider_holds_UP_into_red");
			m_In.Lean = 1;
		}
		else if (t < 21)
		{
			SetPhase("R4_rider_release");
		}
		else if (t < 23)
		{
			SetPhase("S0_reset_stoppie_cruise");
			m_In.Speed = 9;
		}
		else if (t < 25)
		{
			SetPhase("S1_rider_hold_DOWN_stoppie");
			m_In.Speed = 9;
			m_In.Lean = -1;
		}
		else if (t < 27)
		{
			SetPhase("S2_release");
			m_In.Speed = 9;
		}
		else if (t < 29)
		{
			SetPhase("P0_reset_pro_full_gas_no_key");
			m_In.Level = 2;
			m_In.Throttle = 1;
		}
		else if (t < 30.5)
		{
			SetPhase("P1_pro_full_gas_UP");
			m_In.Throttle = 1;
			m_In.Lean = 1;
		}
		else if (t < 33)
		{
			SetPhase("P2_pro_rear_brake");
			m_In.Throttle = 0;
			m_In.RearBrake = 1;
		}
		else
		{
			SetPhase("end");
			m_Bike.SB_SetTest(null);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(Tick);
			Print("[SBT-H] DONE fallen=" + m_Bike.IsFallen());
			return;
		}

		float pitch = SB_Control.FullPitch(m_Bike);
		if (pitch > m_MaxPitch)
			m_MaxPitch = pitch;
		if (pitch < m_MinPitch)
			m_MinPitch = pitch;
		SB_Control c = m_Bike.SB_GetControlPublic();
		Print("[SBT-H] t=" + t + " " + m_Phase + " level=" + c.GetLevel() + " mode=" + c.GetMode() + " catch=" + c.IsCatching() + " pitch=" + pitch + " roll=" + Roll() + " rpm=" + c.GetEngineRPM() + " gear=" + c.GetGearUsed() + " v=" + GetVelocity(m_Bike).Length() + " thr=" + m_In.Throttle + " rb=" + m_In.RearBrake + " front=" + m_Bike.WheelHasContact(0) + " rear=" + m_Bike.WheelHasContact(1) + " fallen=" + m_Bike.IsFallen());
	}
}
