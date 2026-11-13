modded class MissionGameplay
{
	protected ref SB_Hud m_SB_Hud;

	override void OnInit()
	{
		super.OnInit();
		if (GetGame().IsServer())
			SB_Park.SpawnOnce(); // offline / listen game
		m_SB_Hud = new SB_Hud();
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);
		if (m_SB_Hud)
			m_SB_Hud.Update();
		SB_CheckLevelKey();
	}

	// End key: cycle Novice -> Rider -> Pro
	protected void SB_CheckLevelKey()
	{
		UAInput inp = GetUApi().GetInputByName("UAStuntLevel");
		if (!inp || !inp.LocalPress())
			return;
		SB_Config cfg = SB_Settings.Get();
		if (!cfg.Enabled)
			return;
		if (!cfg.AllowPlayerModeSwitch)
		{
			NotificationSystem.AddNotificationExtended(3, "Stunt Bikes", "Сервер не разрешает менять уровень: " + SB_Settings.ModeName(cfg.StuntMode));
			return;
		}
		int next = SB_Settings.GetMode(cfg) + 1;
		if (next > 2)
			next = 0;
		SB_Settings.SetPlayerMode(next);
		string hint = "Держи ↑ — вилли, ↓ — стоппи";
		if (next == 1)
			hint = "↑ подбросить перёд, дальше держи угол в зелёной зоне";
		else if (next == 2)
			hint = "Физика MX Bikes: газ, сцепление, вес. Можно перевернуться";
		NotificationSystem.AddNotificationExtended(4, "Stunt Bikes: " + SB_Settings.ModeName(next), hint);
	}
}

modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();
		SB_Settings.Get();
		SB_Park.SpawnOnce();
	}

	override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
	{
		super.InvokeOnConnect(player, identity);
		if (player)
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(player.SB_SendConfig, 2000, false);
	}
}
