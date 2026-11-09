modded class PlayerBase
{
	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		super.OnRPC(sender, rpc_type, ctx);

		if (rpc_type != SB_RPC_CONFIG || GetGame().IsServer())
			return;

		// config arrives in chunks: long strings get corrupted in a single RPC write
		int count;
		if (!ctx.Read(count) || count <= 0 || count > 200)
			return;
		string json = "";
		for (int i = 0; i < count; i++)
		{
			string part;
			if (!ctx.Read(part))
				return;
			json += part;
		}
		SB_Settings.FromJson(json);
	}

	void SB_SendConfig()
	{
		if (!GetGame().IsServer() || !GetGame().IsMultiplayer() || !GetIdentity())
			return;

		string json = SB_Settings.ToJson();
		array<string> parts = new array<string>;
		int len = json.Length();
		for (int pos = 0; pos < len; pos += 200)
		{
			int n = Math.Min(200, len - pos);
			parts.Insert(json.Substring(pos, n));
		}

		ScriptRPC rpc = new ScriptRPC();
		rpc.Write(parts.Count());
		foreach (string part : parts)
			rpc.Write(part);
		rpc.Send(this, SB_RPC_CONFIG, true, GetIdentity());
	}
}

class SB_Park
{
	protected static bool s_Spawned;

	static void SpawnOnce()
	{
		if (s_Spawned || !GetGame().IsServer())
			return;
		s_Spawned = true;

		SB_Config cfg = SB_Settings.Get();
		if (!cfg.Enabled || !cfg.ParkObjects)
			return;

		int count = 0;
		foreach (SB_ParkObject po : cfg.ParkObjects)
		{
			if (!po || po.Type == "")
				continue;

			Object obj = GetGame().CreateObjectEx(po.Type, po.Position, ECE_SETUP | ECE_UPDATEPATHGRAPH | ECE_CREATEPHYSICS | ECE_NOLIFETIME | ECE_KEEPHEIGHT);
			if (!obj)
			{
				Print("[StuntBikes] park: cannot create " + po.Type);
				continue;
			}
			obj.SetPosition(po.Position);
			obj.SetOrientation(po.Orientation);
			obj.Update();
			count++;
		}
		if (count > 0)
			Print("[StuntBikes] park: spawned " + count + " objects");
	}
}
