modded class MotorbikeScript
{
	protected ref SB_Control m_SB_Control;

	// server: last stunt state reported by the driver's client
	protected int m_SB_RemoteMode;
	protected vector m_SB_RemoteFwd;
	protected vector m_SB_RemoteUp;
	protected vector m_SB_RemoteAngVel;
	protected float m_SB_RemotePump;
	protected float m_SB_RemoteTime;
	// client: send throttling
	protected float m_SB_SendTimer;
	protected int m_SB_SentMode;
	protected bool m_SB_SentPump;

	bool SB_IsLocallyControlled()
	{
		Human driver = CrewMember(0);
		if (!driver)
			return false;
		return driver == GetGame().GetPlayer();
	}

	protected SB_Control SB_GetControl()
	{
		if (!m_SB_Control)
			m_SB_Control = new SB_Control();
		return m_SB_Control;
	}

	// client -> server, ~20 times a second while a stunt (or pumping) is in progress
	protected void SB_SendState(float dt, SB_Config cfg)
	{
		if (!GetGame().IsMultiplayer() || GetGame().IsServer())
			return;

		SB_Control ctl = SB_GetControl();
		int mode = ctl.GetMode();
		float pump = ctl.GetPump();
		bool pumping = pump > 0.02;
		m_SB_SendTimer -= dt;
		if (mode == SB_PitchMode.NONE && m_SB_SentMode == SB_PitchMode.NONE && !pumping && !m_SB_SentPump)
			return;
		if (mode == m_SB_SentMode && pumping == m_SB_SentPump && m_SB_SendTimer > 0)
			return;

		m_SB_SendTimer = cfg.StateSendInterval;
		m_SB_SentMode = mode;
		m_SB_SentPump = pumping;

		ScriptRPC rpc = new ScriptRPC();
		rpc.Write(mode);
		rpc.Write(GetDirection());
		rpc.Write(GetDirectionUp());
		rpc.Write(dBodyGetAngularVelocity(this));
		rpc.Write(pump);
		rpc.Send(this, SB_RPC_STATE, false, null);
	}

	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		super.OnRPC(sender, rpc_type, ctx);
		if (rpc_type != SB_RPC_STATE || !GetGame().IsServer())
			return;

		int mode;
		vector fwd;
		vector up;
		vector angVel;
		float pump;
		if (!ctx.Read(mode) || !ctx.Read(fwd) || !ctx.Read(up) || !ctx.Read(angVel) || !ctx.Read(pump))
			return;

		// only the driver may control the bike
		Human driver = CrewMember(0);
		if (!driver || !sender || !driver.GetIdentity() || driver.GetIdentity().GetId() != sender.GetId())
			return;

		// sanity: unit vectors, bounded spin
		if (fwd.Length() < 0.5 || up.Length() < 0.5)
			return;
		if (angVel.Length() > 20)
			angVel = angVel.Normalized() * 20;

		m_SB_RemoteMode = mode;
		m_SB_RemoteFwd = fwd.Normalized();
		m_SB_RemoteUp = up.Normalized();
		m_SB_RemoteAngVel = angVel;
		m_SB_RemotePump = Math.Clamp(pump, 0, 1);
		m_SB_RemoteTime = GetGame().GetTickTime();
	}

	// automated test only: drive this bike with scripted inputs (server, no rider needed)
	void SB_SetTest(SB_TestInputs t)
	{
		SB_GetControl().m_Test = t;
	}

	SB_Control SB_GetControlPublic()
	{
		return SB_GetControl();
	}

	protected void SB_StepTest(float dt, SB_Config cfg)
	{
		SB_TestInputs t = SB_GetControl().m_Test;
		vector fwd = GetDirection();
		if (t.Speed >= 0)
		{
			vector flat = Vector(fwd[0], 0, fwd[2]).Normalized();
			vector vel = GetVelocity(this);
			SetVelocity(this, Vector(flat[0] * t.Speed, vel[1], flat[2] * t.Speed));
		}
		if (t.HoldUpright)
		{
			// a bike without rider has nobody to balance it: keep roll near zero
			vector av = dBodyGetAngularVelocity(this);
			vector right = (fwd * GetDirectionUp()).Normalized();
			float roll = vector.Dot(right, "0 1 0");
			float rollRate = vector.Dot(av, fwd);
			dBodySetAngularVelocity(this, av + fwd * (roll * 6.0 - rollRate));
		}
		SB_GetControl().Step(this, t.Lean, t.Clutch, 0, dt, cfg);
	}

	protected void SB_StepControl(float dt)
	{
		SB_Config cfg = SB_Settings.Get();
		if (!cfg.Enabled)
			return;

		if (m_SB_Control && m_SB_Control.m_Test)
		{
			SB_StepTest(dt, cfg);
			return;
		}

		if (!CrewMember(0))
		{
			m_SB_RemoteMode = SB_PitchMode.NONE;
			return;
		}

		// the rider's own machine simulates the stunt (offline: the only machine)
		if (SB_IsLocallyControlled())
		{
			float clutch = SB_Control.ReadLocalInput("UAStuntClutch");
			float pump = SB_Control.ReadLocalInput("UAStuntPump");
			SB_GetControl().Step(this, SB_Control.ReadLocalLean(), clutch, pump, dt, cfg);
			SB_SendState(dt, cfg);
			return;
		}

		// dedicated server: mirror the driver's stunt orientation and pumping
		if (GetGame().IsServer() && GetGame().IsMultiplayer())
		{
			float age = GetGame().GetTickTime() - m_SB_RemoteTime;
			bool fresh = age < cfg.StateTimeout;
			float remotePump = 0;
			if (fresh)
				remotePump = m_SB_RemotePump;
			SB_GetControl().StepPumpRemote(this, remotePump, dt, cfg);
			if (m_SB_RemoteMode != SB_PitchMode.NONE && fresh)
				SB_GetControl().Follow(this, m_SB_RemoteMode, m_SB_RemoteFwd, m_SB_RemoteUp, m_SB_RemoteAngVel, age, dt, cfg);
		}
	}

	void MotorbikeScript()
	{
		// run our control inside the physics step (also replayed by network reconciliation)
		SetEventMask(EntityEvent.SIMULATE);
	}

	override void EOnSimulate(IEntity other, float dt)
	{
		super.EOnSimulate(other, dt);
		SB_StepControl(dt);
	}

	override void OnDriverExit(Human player)
	{
		super.OnDriverExit(player);
		m_SB_RemoteMode = SB_PitchMode.NONE;
		m_SB_RemotePump = 0;
	}

	// driver's own client: with the clutch pulled the engine sound follows the free-revving
	// MX Bikes engine, so you hear the revs before dropping the clutch
	override float OnSound(MotorbikeSoundCtrl ctrl, float oldValue)
	{
		float value = super.OnSound(ctrl, oldValue);
		if (ctrl != MotorbikeSoundCtrl.RPM || !m_SB_Control || !EngineIsOn())
			return value;
		if (m_SB_Control.GetClutch() < 0.3 || !SB_IsLocallyControlled())
			return value;
		SB_Config cfg = SB_Settings.Get();
		if (!cfg.Enabled || cfg.LimiterRPM <= 0)
			return value;
		float revs = m_SB_Control.GetEngineRPM() / cfg.LimiterRPM * EngineGetRPMRedline();
		return Math.Max(value, revs);
	}

	// Server: a hard landing on the wheels should not throw the rider off.
	// Vanilla turns contact damage >= 25 into FallOver(), >= 40 also hurts the crew.
	override protected void CheckContactCache()
	{
		SB_Config cfg = SB_Settings.Get();
		if (cfg.Enabled && cfg.SafeLandings && m_ContactCache && m_ContactCache.Count() > 0 && SB_IsSafeLanding(cfg))
		{
			for (int i = m_ContactCache.Count() - 1; i >= 0; i--)
			{
				array<ref VehicleContactData> data = m_ContactCache.GetElement(i);
				if (!data || data.Count() == 0)
					continue;

				float dmg = Math.AbsFloat(data[0].m_Impulse * m_VehicleContactDamageCoef);
				if (dmg < cfg.SafeLandMaxDamage)
				{
					if (cfg.DebugLog)
						Print("[StuntBikes] safe landing, ignored contact zone=" + m_ContactCache.GetKey(i) + " dmg=" + dmg);
					m_ContactCache.RemoveElement(i);
				}
			}
		}

		super.CheckContactCache();
	}

	protected bool SB_IsSafeLanding(SB_Config cfg)
	{
		vector up = GetDirectionUp();
		float tilt = Math.Acos(Math.Clamp(up[1], -1, 1)) * Math.RAD2DEG;
		if (tilt > cfg.SafeLandUprightAngle)
			return false;

		// the impact must be mostly vertical (landing), not a wall hit
		vector dv = GetVelocity(this) - m_VelocityPrevTick;
		float horizontal = Math.Sqrt(dv[0] * dv[0] + dv[2] * dv[2]);
		if (dv[1] <= 0)
			return false;
		return dv[1] >= horizontal * 0.8;
	}
}
