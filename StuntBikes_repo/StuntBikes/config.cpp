class CfgPatches
{
	class StuntBikes
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data", "DZ_Scripts", "DZ_Vehicles_Singletrack"};
	};
};

class CfgMods
{
	class StuntBikes
	{
		dir = "StuntBikes";
		name = "Stunt Bikes";
		author = "V";
		version = "1.0";
		type = "mod";
		inputs = "StuntBikes/data/inputs.xml";
		dependencies[] = {"Game", "World", "Mission"};
		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"StuntBikes/scripts/3_Game"};
			};
			class worldScriptModule
			{
				value = "";
				files[] = {"StuntBikes/scripts/4_World"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"StuntBikes/scripts/5_Mission"};
			};
		};
	};
};
