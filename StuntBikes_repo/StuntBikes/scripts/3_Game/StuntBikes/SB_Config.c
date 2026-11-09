// Stunt Bikes - settings shared by server and client.
// Server file: $profile:StuntBikes/config.json (created with defaults on first start).

const int SB_RPC_CONFIG = 5963777;
const int SB_CONFIG_VERSION = 5;
const int SB_RPC_STATE = 5963778;

class SB_ParkObject
{
	string Type;
	vector Position;
	vector Orientation;

	void SB_ParkObject(string type = "", vector pos = "0 0 0", vector ori = "0 0 0")
	{
		Type = type;
		Position = pos;
		Orientation = ori;
	}
}

class SB_Config
{
	int Version = SB_CONFIG_VERSION;
	bool Enabled = true;
	bool DebugLog = false;

	// --- rider control (realistic stunt physics) ---
	float LeanResponse = 5.0;         // how fast the rider shifts body weight (1/s)
	float ThrottleResponse = 3.0;     // how fast gas/brake inputs ramp (keys are on/off) 1/s
	float EngageLean = 0.4;           // lean needed to pop a wheelie / start a stoppie
	float TakeOverAngle = 15.0;       // deg on the rear wheel after a jump where stunt physics take over
	float ForceResponse = 6.0;        // how smoothly stunt forces change 1/s
	float GroundMaxPitchRate = 2.5;   // rad/s cap for wheelie / stoppie rotation
	float SettleRate = 1.5;           // rad/s max speed of a wheel coming back down
	float ThrottleLiftAccel = 9.0;    // rad/s^2 nose-up from full gas (engine torque lifting the front)
	float ThrottleLiftFadeSpeed = 90.0; // km/h, gas lifts less the faster you go (higher gears)
	float ThrottleLiftMinFactor = 0.35; // minimum lift factor at high speed
	float GravityPitchAccel = 7.0;    // rad/s^2, gravity pulling a wheelie back down (and over, past balance)
	float WheelieBalanceAngle = 55.0; // deg, balance point: above it the bike keeps going over (loop out)
	float WheelieStartAngle = 2.0;    // deg, from here the bike is considered on the rear wheel
	float RearBrakeDropAccel = 8.0;   // rad/s^2 nose-down from the rear brake (save a loop out)
	float GroundLeanAccel = 2.5;      // rad/s^2 from body lean on the ground (small: throttle is the skill)
	float GroundDamping = 0.6;        // 1/s pitch damping on one wheel
	float FrontBrakeLiftAccel = 10.0; // rad/s^2 rear-lift from the front brake at speed (stoppie)
	float StoppieFullSpeed = 30.0;    // km/h for full front-brake lift
	float StoppieBalanceAngle = 50.0; // deg, stoppie balance point
	float AirLeanAccel = 3.5;         // rad/s^2 from body lean in the air
	float AirThrottleAccel = 3.0;     // rad/s^2 nose-up from gas in the air (rear wheel spin)
	float AirBrakeAccel = 4.0;        // rad/s^2 nose-down from rear brake in the air
	float AirDamping = 0.05;          // 1/s, air rotation keeps its momentum
	float MaxPitchRate = 6.0;         // rad/s safety cap
	float ResyncRate = 3.0;           // rad/s, follow real physics after a hard hit
	float FollowGain = 10.0;          // server: how hard the server copy follows the rider pitch (1/s)
	float StateSendInterval = 0.05;   // s between pitch updates sent to the server
	float StateTimeout = 0.4;         // s, older pitch updates are ignored
	float LoopOutAngle = 80.0;        // deg on the rear wheel = rider falls
	float EndoAngle = 80.0;           // deg on the front wheel = rider falls
	float MinStuntSpeed = 5.0;        // km/h, below this no wheelie / stoppie can be started

	// --- crash handling (server) ---
	bool SafeLandings = true;         // ignore landing impacts while the bike lands upright
	float SafeLandUprightAngle = 40.0;// deg from world up
	float SafeLandMaxDamage = 200.0;  // contact damage (impulse * coef) ignored on a safe landing

	// --- optional stunt park (server spawns these objects on start) ---
	ref array<ref SB_ParkObject> ParkObjects = new array<ref SB_ParkObject>;
}

class SB_Settings
{
	protected static ref SB_Config s_Config;
	protected static bool s_ServerLoaded;

	static string GetPath()
	{
		return "$profile:StuntBikes/config.json";
	}

	static SB_Config Get()
	{
		if (!s_Config)
			s_Config = new SB_Config();

		if (!s_ServerLoaded && GetGame() && GetGame().IsServer())
		{
			s_ServerLoaded = true;
			LoadServer();
		}
		return s_Config;
	}

	static void Set(SB_Config cfg)
	{
		if (cfg)
			s_Config = cfg;
	}

	static void LoadServer()
	{
		string err;
		if (!FileExist("$profile:StuntBikes"))
			MakeDirectory("$profile:StuntBikes");

		if (FileExist(GetPath()))
		{
			SB_Config loaded;
			if (JsonFileLoader<SB_Config>.LoadFile(GetPath(), loaded, err) && loaded)
			{
				s_Config = loaded;
				if (!s_Config.ParkObjects)
					s_Config.ParkObjects = new array<ref SB_ParkObject>;
			}
			else
			{
				Print("[StuntBikes] config.json could not be read, using defaults: " + err);
			}
		}

		// fields missing in the file come back as 0: restore their defaults
		Validate(s_Config);

		// rewrite so new fields appear with their defaults
		s_Config.Version = SB_CONFIG_VERSION;
		JsonFileLoader<SB_Config>.SaveFile(GetPath(), s_Config, err);
		Print("[StuntBikes] config loaded, enabled=" + s_Config.Enabled);
	}

	protected static float Fix(float value, float def, string name)
	{
		if (value > 0)
			return value;
		Print("[StuntBikes] config: " + name + " missing or <= 0, using " + def);
		return def;
	}

	static void Validate(SB_Config c)
	{
		SB_Config d = new SB_Config();
		c.LeanResponse = Fix(c.LeanResponse, d.LeanResponse, "LeanResponse");
		c.ThrottleResponse = Fix(c.ThrottleResponse, d.ThrottleResponse, "ThrottleResponse");
		c.EngageLean = Fix(c.EngageLean, d.EngageLean, "EngageLean");
		c.TakeOverAngle = Fix(c.TakeOverAngle, d.TakeOverAngle, "TakeOverAngle");
		c.ForceResponse = Fix(c.ForceResponse, d.ForceResponse, "ForceResponse");
		c.GroundMaxPitchRate = Fix(c.GroundMaxPitchRate, d.GroundMaxPitchRate, "GroundMaxPitchRate");
		c.SettleRate = Fix(c.SettleRate, d.SettleRate, "SettleRate");
		c.ThrottleLiftAccel = Fix(c.ThrottleLiftAccel, d.ThrottleLiftAccel, "ThrottleLiftAccel");
		c.ThrottleLiftFadeSpeed = Fix(c.ThrottleLiftFadeSpeed, d.ThrottleLiftFadeSpeed, "ThrottleLiftFadeSpeed");
		c.ThrottleLiftMinFactor = Fix(c.ThrottleLiftMinFactor, d.ThrottleLiftMinFactor, "ThrottleLiftMinFactor");
		c.GravityPitchAccel = Fix(c.GravityPitchAccel, d.GravityPitchAccel, "GravityPitchAccel");
		c.WheelieBalanceAngle = Fix(c.WheelieBalanceAngle, d.WheelieBalanceAngle, "WheelieBalanceAngle");
		c.WheelieStartAngle = Fix(c.WheelieStartAngle, d.WheelieStartAngle, "WheelieStartAngle");
		c.RearBrakeDropAccel = Fix(c.RearBrakeDropAccel, d.RearBrakeDropAccel, "RearBrakeDropAccel");
		c.GroundLeanAccel = Fix(c.GroundLeanAccel, d.GroundLeanAccel, "GroundLeanAccel");
		c.GroundDamping = Fix(c.GroundDamping, d.GroundDamping, "GroundDamping");
		c.FrontBrakeLiftAccel = Fix(c.FrontBrakeLiftAccel, d.FrontBrakeLiftAccel, "FrontBrakeLiftAccel");
		c.StoppieFullSpeed = Fix(c.StoppieFullSpeed, d.StoppieFullSpeed, "StoppieFullSpeed");
		c.StoppieBalanceAngle = Fix(c.StoppieBalanceAngle, d.StoppieBalanceAngle, "StoppieBalanceAngle");
		c.AirLeanAccel = Fix(c.AirLeanAccel, d.AirLeanAccel, "AirLeanAccel");
		c.AirThrottleAccel = Fix(c.AirThrottleAccel, d.AirThrottleAccel, "AirThrottleAccel");
		c.AirBrakeAccel = Fix(c.AirBrakeAccel, d.AirBrakeAccel, "AirBrakeAccel");
		c.AirDamping = Fix(c.AirDamping, d.AirDamping, "AirDamping");
		c.MaxPitchRate = Fix(c.MaxPitchRate, d.MaxPitchRate, "MaxPitchRate");
		c.ResyncRate = Fix(c.ResyncRate, d.ResyncRate, "ResyncRate");
		c.FollowGain = Fix(c.FollowGain, d.FollowGain, "FollowGain");
		c.StateSendInterval = Fix(c.StateSendInterval, d.StateSendInterval, "StateSendInterval");
		c.StateTimeout = Fix(c.StateTimeout, d.StateTimeout, "StateTimeout");
		c.LoopOutAngle = Fix(c.LoopOutAngle, d.LoopOutAngle, "LoopOutAngle");
		c.EndoAngle = Fix(c.EndoAngle, d.EndoAngle, "EndoAngle");
		c.MinStuntSpeed = Fix(c.MinStuntSpeed, d.MinStuntSpeed, "MinStuntSpeed");
		c.SafeLandUprightAngle = Fix(c.SafeLandUprightAngle, d.SafeLandUprightAngle, "SafeLandUprightAngle");
		c.SafeLandMaxDamage = Fix(c.SafeLandMaxDamage, d.SafeLandMaxDamage, "SafeLandMaxDamage");
		if (!c.ParkObjects)
			c.ParkObjects = new array<ref SB_ParkObject>;
	}

	static string ToJson()
	{
		string data;
		string err;
		JsonFileLoader<SB_Config>.MakeData(Get(), data, err, false);
		return data;
	}

	static bool FromJson(string data)
	{
		SB_Config cfg;
		string err;
		if (JsonFileLoader<SB_Config>.LoadData(data, cfg, err) && cfg)
		{
			Validate(cfg);
			s_Config = cfg;
			return true;
		}
		Print("[StuntBikes] bad config from server: " + err);
		return false;
	}
}
