class CfgPatches
{
	class StuntBikesTest
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"StuntBikes"};
	};
};

class CfgMods
{
	class StuntBikesTest
	{
		dir = "StuntBikesTest";
		name = "Stunt Bikes Test Helper";
		author = "V";
		version = "0.1";
		type = "mod";
		dependencies[] = {"Mission"};
		class defs
		{
			class missionScriptModule
			{
				value = "";
				files[] = {"StuntBikesTest/scripts/5_Mission"};
			};
		};
	};
};
