modded class MissionGameplay
{
	override void OnInit()
	{
		super.OnInit();
		if (GetGame().IsServer())
			SB_Park.SpawnOnce(); // offline / listen game
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
