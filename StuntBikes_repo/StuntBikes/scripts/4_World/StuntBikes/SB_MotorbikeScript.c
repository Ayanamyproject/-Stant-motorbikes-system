modded class MotorbikeScript
{
	protected ref SB_Control m_SB_Control;

	// server: last stunt state reported by the driver's client
	protected int m_SB_RemoteMode;
	protected float m_SB_RemotePitch;
	protected float m_SB_RemoteRate;
	protected float m_SB_RemoteTime;
	// client: send throttling
	protected float m_SB_SendTimer;
	protected int m_SB_SentMode;

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

	// client -> server, ~20 times a second while a stunt is in progress
	protected void SB_SendState(float dt, SB_Config cfg)
	{
		if (!GetGame().IsMultiplayer() || GetGame().IsServer())
			return;

		int mode = SB_GetControl().GetMode();
		m_SB_SendTimer -= dt;
		if (mode == SB_PitchMode.NONE && m_SB_SentMode == SB_PitchMode.NONE)
			return;
		if (mode == m_SB_SentMode && m_SB_SendTimer > 0)
			return;

		m_SB_SendTimer = cfg.StateSendInterval;
		m_SB_SentMode = mode;

		ScriptRPC rpc = new ScriptRPC();
		rpc.Write(mode);
		rpc.Write(SB_Control.FullPitch(this));
		rpc.Write(SB_GetControl().GetPitchRate());
		rpc.Send(this, SB_RPC_STATE, false, null);
	}

	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		super.OnRPC(sender, rpc_type, ctx);
		if (rpc_type != SB_RPC_STATE || !GetGame().IsServer())
			return;

		int mode;
		float pitch;
		float rate;
		if (!ctx.Read(mode) || !ctx.Read(pitch) || !ctx.Read(rate))
			return;

		// only the driver may control the bike
		Human driver = CrewMember(0);
		if (!driver || !sender || !driver.GetIdentity() || driver.GetIdentity().GetId() != sender.GetId())
			return;

		m_SB_RemoteMode = mode;
		m_SB_RemotePitch = SB_Control.WrapDeg(pitch);
		m_SB_RemoteRate = Math.Clamp(rate, -10, 10);
		m_SB_RemoteTime = GetGame().GetTickTime();
	}

	protected void SB_StepControl(float dt)
	{
		SB_Config cfg = SB_Settings.Get();
		if (!cfg.Enabled)
			return;

		if (!CrewMember(0))
		{
			m_SB_RemoteMode = SB_PitchMode.NONE;
			return;
		}

		// the rider's own machine simulates the stunt (offline: the only machine)
		if (SB_IsLocallyControlled())
		{
			SB_GetControl().Step(this, SB_Control.ReadLocalLean(), dt, cfg);
			SB_SendState(dt, cfg);
			return;
		}

		// dedicated server: mirror the driver's stunt pitch
		if (GetGame().IsServer() && GetGame().IsMultiplayer())
		{
			float age = GetGame().GetTickTime() - m_SB_RemoteTime;
			if (m_SB_RemoteMode != SB_PitchMode.NONE && age < cfg.StateTimeout)
				SB_GetControl().Follow(this, m_SB_RemotePitch, m_SB_RemoteRate, age, dt, cfg);
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
