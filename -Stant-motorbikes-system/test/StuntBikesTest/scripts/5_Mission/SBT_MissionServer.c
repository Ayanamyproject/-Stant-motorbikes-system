// Test helper (server-only mod): puts every connecting player on a flat open test spot
// with two ready-to-ride motorbikes and a few ramps. Not for production servers.

modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(SBT_StartHarness, 100000, false);
	}

	void SBT_StartHarness()
	{
		SBT_Harness.Start(SBT_FindSpot());
	}

	protected static bool s_SBT_SpotFound;
	protected static vector s_SBT_Spot;

	override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
	{
		super.InvokeOnConnect(player, identity);
		if (!player)
			return;
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(SBT_Setup, 1500, false, player);
	}

	protected bool SBT_IsGoodSpot(vector p)
	{
		if (GetGame().SurfaceIsSea(p[0], p[2]) || GetGame().SurfaceIsPond(p[0], p[2]))
			return false;

		float h0 = GetGame().SurfaceY(p[0], p[2]);
		for (int a = 0; a < 8; a++)
		{
			float ang = a * Math.PI * 0.25;
			for (int r = 20; r <= 120; r += 20)
			{
				float x = p[0] + Math.Cos(ang) * r;
				float z = p[2] + Math.Sin(ang) * r;
				if (GetGame().SurfaceIsSea(x, z) || GetGame().SurfaceIsPond(x, z))
					return false;
				if (Math.AbsFloat(GetGame().SurfaceY(x, z) - h0) > r * 0.03)
					return false;
			}
		}

		// no objects (buildings, trees, bushes) in a corridor 120 m to the north
		array<Object> objs = new array<Object>;
		for (int s = 0; s <= 120; s += 30)
		{
			objs.Clear();
			vector c = Vector(p[0], h0, p[2] + s);
			GetGame().GetObjectsAtPosition3D(c, 22, objs, null);
			foreach (Object o : objs)
			{
				if (o && !o.IsInherited(Man))
					return false;
			}
		}
		return true;
	}

	protected vector SBT_FindSpot()
	{
		if (s_SBT_SpotFound)
			return s_SBT_Spot;

		vector start = Vector(4076.15, 0, 2665.38);
		for (int ring = 0; ring < 60; ring++)
		{
			int steps = 1 + ring * 6;
			for (int i = 0; i < steps; i++)
			{
				float ang = i * Math.PI * 2 / steps;
				vector p = Vector(start[0] + Math.Cos(ang) * ring * 60, 0, start[2] + Math.Sin(ang) * ring * 60);
				if (SBT_IsGoodSpot(p))
				{
					p[1] = GetGame().SurfaceY(p[0], p[2]);
					s_SBT_Spot = p;
					s_SBT_SpotFound = true;
					Print("[SBT] test spot " + p.ToString());
					SBT_SpawnRamps(p);
					return p;
				}
			}
		}
		Print("[SBT] no flat spot found, using start");
		start[1] = GetGame().SurfaceY(start[0], start[2]);
		s_SBT_Spot = start;
		s_SBT_SpotFound = true;
		SBT_SpawnRamps(start);
		return start;
	}

	protected void SBT_SpawnRamps(vector spot)
	{
		SBT_Ramp("StaticObj_Misc_Obstacle_Ramp", spot + Vector(-15, 0, 25), 0);
		SBT_Ramp("StaticObj_Platform1_Ramp", spot + Vector(0, 0, 45), 0);
		SBT_Ramp("StaticObj_Rail_ConcreteRamp", spot + Vector(40, 0, 45), 0);
	}

	protected void SBT_Ramp(string type, vector pos, float yaw)
	{
		pos[1] = GetGame().SurfaceY(pos[0], pos[2]);
		Object obj = GetGame().CreateObjectEx(type, pos, ECE_SETUP | ECE_CREATEPHYSICS | ECE_NOLIFETIME | ECE_KEEPHEIGHT);
		if (!obj)
		{
			Print("[SBT] cannot create " + type);
			return;
		}
		obj.SetOrientation(Vector(yaw, 0, 0));
		obj.SetPosition(pos);
		obj.Update();
		vector mm[2];
		obj.GetCollisionBox(mm);
		Print("[SBT] ramp " + type + " at " + pos.ToString() + " box " + mm[0].ToString() + " .. " + mm[1].ToString());
	}

	void SBT_Setup(PlayerBase player)
	{
		if (!player || !player.IsAlive())
			return;

		vector spot = SBT_FindSpot();
		player.SetPosition(spot);
		player.SetOrientation(Vector(0, 0, 0));
		Print("[SBT] player moved to " + spot.ToString());

		SBT_SpawnBike("Motorbike_01_Red", "Motorbike_01_Wheel_1", "Motorbike_01_Wheel_2", spot + Vector(2.5, 0, 3));
		SBT_SpawnBike("Motorbike_02_Red", "Motorbike_02_Wheel_1", "Motorbike_02_Wheel_2", spot + Vector(-2.5, 0, 3));
	}

	void SBT_SpawnBike(string type, string w1, string w2, vector pos)
	{
		pos[1] = GetGame().SurfaceY(pos[0], pos[2]) + 0.3;
		MotorbikeScript bike = MotorbikeScript.Cast(GetGame().CreateObjectEx(type, pos, ECE_PLACE_ON_SURFACE | ECE_CREATEPHYSICS | ECE_SETUP));
		if (!bike)
		{
			Print("[SBT] cannot create " + type);
			return;
		}
		bike.SetOrientation(Vector(0, 0, 0));
		bike.GetInventory().CreateAttachment(w1);
		bike.GetInventory().CreateAttachment(w2);
		bike.GetInventory().CreateAttachment("SparkPlug");
		bike.GetInventory().CreateAttachment("HeadlightH7");
		bike.Fill(MotorbikeFluid.FUEL, bike.GetFluidCapacity(MotorbikeFluid.FUEL));
		Print("[SBT] spawned " + type + " at " + pos.ToString());
	}
}
