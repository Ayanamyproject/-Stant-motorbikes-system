// Stunt Bikes - settings shared by server and client.
// Server file: $profile:StuntBikes/config.json (created with defaults on first start).
//
// Version 6: physics ported from MX Bikes (PiBoSo). Default numbers are the MSM 250XF
// from MX Bikes beta21 (bikes.pkz: 250xf.cfg / 250xf.engn / 250xf.geom, rider.cfg):
// engine torque curve, engine braking, clutch, gearbox, sprockets, masses, wheel friction.
// Values that MX Bikes keeps encrypted (rider.prm) or that DayZ handles differently are
// marked "tuned".

const int SB_RPC_CONFIG = 5963777;
const int SB_CONFIG_VERSION = 8;
const int SB_RPC_STATE = 5963778;

// automated test only (StuntBikesTest server mod): scripted rider inputs for a driverless bike
class SB_TestInputs
{
	float Throttle;
	float RearBrake;
	float FrontBrake;
	float Clutch;
	float Lean;
	float Speed = -1;     // m/s forced forward speed, < 0 = free
	bool ForceEngine = true;
	bool HoldUpright = true;
	int Level = -1;       // force a stunt level (-1 = server level)
}

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

	// --- stunt level ---
	// 0 = NOVICE: hold UP = wheelie, hold DOWN = stoppie. The bike lifts, holds a nice angle and
	//     comes down by itself. Cannot loop out.
	// 1 = RIDER: UP pops the front, then YOU hold the angle: UP raises, DOWN lowers, no key = it
	//     sinks. Keep the marker in the green zone of the HUD bar. In the red zone the system
	//     catches the bike and puts it back down instead of throwing you off.
	// 2 = PRO: MX Bikes physics (engine, clutch, rider weight), you can loop out. The front still
	//     only comes up while UP is held, never from gas alone.
	int StuntMode = 1;
	bool AllowPlayerModeSwitch = true; // players may cycle the level with the End key
	bool ShowAngleHud = true;          // angle bar on the right side of the screen
	float AssistMinSpeed = 8.0;       // km/h, below this the wheel comes down
	float AssistWheelieAngle = 35.0;  // deg held while UP is pressed
	float AssistStoppieAngle = 20.0;  // deg held while DOWN is pressed
	float AssistLiftRate = 1.4;       // rad/s max speed going up
	float AssistDropRate = 1.0;       // rad/s max speed coming down
	float AssistGain = 3.0;           // 1/s, how firmly the angle is held
	float AssistAccel = 5.0;          // rad/s^2, smoothness of every change
	float AssistWobble = 1.5;         // deg of natural wobble while holding (0 = perfectly still)
	float AssistAirRotate = 3.0;      // rad/s^2, UP/DOWN rotate the bike in the air
	bool AssistAirLevel = true;       // no key in the air: the bike levels itself for landing

	// --- RIDER level ---
	float RiderPopAngle = 18.0;       // deg the front pops up to when UP is pressed
	float RiderRaiseRate = 0.7;       // rad/s while the raise key is held
	float RiderLowerRate = 0.8;       // rad/s while the lower key is held
	float RiderSinkRate = 0.25;       // rad/s the wheel sinks with no key
	float RiderDrift = 0.15;          // rad/s random drift you have to correct (0 = none)
	float RiderGreenMin = 25.0;       // deg, wheelie green zone
	float RiderGreenMax = 45.0;
	float RiderRedAngle = 55.0;       // deg, red zone: the system catches the bike
	float RiderStoppieGreenMin = 10.0;
	float RiderStoppieGreenMax = 25.0;
	float RiderStoppieRedAngle = 32.0;
	float RiderCatchTime = 1.2;       // s the system takes over after a red zone

	// --- inputs (keyboard keys are on/off, so they ramp like in MX Bikes) ---
	float ThrottleResponse = 6.0;     // 1/s, gas and brakes
	float ClutchResponse = 12.0;      // 1/s, clutch lever (lower = slower release)

	// --- mass and geometry (MX Bikes 250XF + 80 kg rider) ---
	float BikeMass = 102.8;           // kg, 250xf.ini weight
	float RiderMass = 80.0;           // kg, rider.cfg mass
	float ComHeight = 0.80;           // m, bike+rider centre of mass above the ground (seated)
	float RearToCom = 0.72;           // m, rear contact patch -> centre of mass (along the bike)
	float FrontToCom = 0.76;          // m, centre of mass -> front contact patch
	float PitchInertia = 25.0;        // kg m^2 about the centre of mass (chassis 4.79 + rider + parts)
	float RollInertia = 18.0;         // kg m^2, air whips
	float YawInertia = 20.0;          // kg m^2, air scrubs
	float RearWheelRadius = 0.33;     // m
	float RearWheelInertia = 1.0;     // kg m^2, rear wheel + tyre (gyro effect in the air) - tuned
	float TyreFriction = 1.5;         // 250xf.cfg wheel Friction

	// --- rider body (fore/aft weight shift and up/down pumping) ---
	float RiderShift = 0.25;          // m the rider moves back (lean back) or forward (lean forward)
	float RiderBodyFreq = 10.0;       // rad/s, how fast the body follows the keys (critically damped)
	float BarPullArm = 0.6;           // m, lever of the pull/push on the bars while the body moves
	float RiderCrouch = 0.25;         // m the rider sinks when pumping (pump key)
	float PumpForceScale = 1.0;       // scale of the body reaction pushed into the suspension

	// --- engine (250xf.engn / 250xf.cfg) ---
	float EngineInertia = 0.0085;     // kg m^2, crank 0.006 + first axis 0.0025
	float IdleRPM = 2300;
	float LimiterRPM = 14300;
	float UpshiftRPM = 13200;         // virtual gearbox (MX Bikes autoshift)
	float DownshiftRPM = 4480;
	ref array<float> TorqueRPM = {30, 1619, 3208, 4767, 6776, 8005, 9594, 11183, 12052, 12802, 14301};
	ref array<float> TorqueNm = {0.23, 14.38, 15.91, 18.79, 26.38, 25.12, 23.39, 19.94, 5.56, 2.11, 0.0};
	ref array<float> EngineBrakeRPM = {0, 1619, 3178, 4767, 6386, 8575, 10703, 14391};
	ref array<float> EngineBrakeNm = {0, -1.15, -2.30, -4.41, -6.90, -11.12, -16.30, -27.08};

	// --- clutch and drivetrain ---
	float ClutchMaxTorque = 105.0;    // Nm at the clutch shaft
	float ClutchDeadZone = 0.25;      // first part of the lever that does nothing
	float PrimaryRatio = 3.0417;      // 73/24
	ref array<float> GearRatios = {2.4615, 2.0, 1.6471, 1.3684, 1.1905}; // 32/13 32/16 28/17 26/19 25/21
	float FinalRatio = 3.8462;        // 50/13 sprockets
	float DriveEfficiency = 0.92;
	bool UseGameGear = false;         // false: MX Bikes virtual gearbox, true: follow the DayZ gear

	// --- brakes (tuned: MX Bikes hydraulics expressed as max force at the tyre) ---
	float FrontBrakeMaxForce = 2600.0; // N
	float RearBrakeMaxForce = 2200.0;  // N (rear tyre carries the whole bike in a wheelie)

	// --- air ---
	float AirLeanAccel = 1.5;         // rad/s^2 from body english in the air
	float AirDamping = 0.05;          // 1/s, rotation keeps its momentum
	bool AirTricks = true;            // whips / scrubs with the steering keys in the air
	float AirRollTorque = 60.0;       // Nm from the rider throwing the bike sideways (negative = flip direction)
	float AirYawTorque = 45.0;        // Nm, scrub
	float AirTrickDamping = 1.2;      // 1/s

	// --- control limits ---
	float MinStuntSpeed = 2.0;        // km/h, below this no wheelie / stoppie can start
	float WheelieStartAngle = 2.0;    // deg, from here the bike is on one wheel
	float TakeOverAngle = 15.0;       // deg on the rear wheel (e.g. landing) where stunt physics take over
	float ForceResponse = 15.0;       // 1/s, smoothing of the stunt torque
	float GroundMaxPitchRate = 4.0;   // rad/s cap on one wheel
	float MaxPitchRate = 8.0;         // rad/s cap in the air
	float SettleRate = 1.5;           // rad/s max speed of a wheel coming back down
	float ResyncRate = 3.0;           // rad/s, follow real physics after a hard hit
	float LoopOutAngle = 80.0;        // deg on the rear wheel = rider falls
	float EndoAngle = 75.0;           // deg on the front wheel = rider falls

	// --- network ---
	float FollowGain = 10.0;          // server: how hard the server copy follows the rider (1/s)
	float StateSendInterval = 0.05;   // s between state updates sent to the server
	float StateTimeout = 0.4;         // s, older updates are ignored

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
	protected static int s_PlayerMode = -2; // -2 = not loaded, -1 = use the server level

	static string ModeName(int mode)
	{
		if (mode == 0)
			return "Новичок";
		if (mode == 1)
			return "Райдер";
		return "Профи";
	}

	static string PlayerModePath()
	{
		return "$profile:StuntBikes_level.txt";
	}

	// level used on this machine: the player's own choice if the server allows it
	static int GetMode(SB_Config cfg)
	{
		if (s_PlayerMode == -2)
		{
			s_PlayerMode = -1;
			if (GetGame() && !GetGame().IsDedicatedServer() && FileExist(PlayerModePath()))
			{
				FileHandle f = OpenFile(PlayerModePath(), FileMode.READ);
				if (f)
				{
					string line;
					FGets(f, line);
					CloseFile(f);
					line = line.Trim();
					if (line == "0" || line == "1" || line == "2")
						s_PlayerMode = line.ToInt();
				}
			}
		}
		if (cfg.AllowPlayerModeSwitch && s_PlayerMode >= 0)
			return s_PlayerMode;
		return cfg.StuntMode;
	}

	static void SetPlayerMode(int mode)
	{
		s_PlayerMode = mode;
		FileHandle f = OpenFile(PlayerModePath(), FileMode.WRITE);
		if (f)
		{
			FPrint(f, mode.ToString());
			CloseFile(f);
		}
	}

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
				if (loaded.Version < 6)
				{
					// physics changed completely in v6: keep only the general settings
					Print("[StuntBikes] config.json is version " + loaded.Version + ", physics reset to MX Bikes defaults");
					SB_Config fresh = new SB_Config();
					fresh.Enabled = loaded.Enabled;
					fresh.DebugLog = loaded.DebugLog;
					fresh.SafeLandings = loaded.SafeLandings;
					if (loaded.SafeLandUprightAngle > 0)
						fresh.SafeLandUprightAngle = loaded.SafeLandUprightAngle;
					if (loaded.SafeLandMaxDamage > 0)
						fresh.SafeLandMaxDamage = loaded.SafeLandMaxDamage;
					if (loaded.ParkObjects)
						fresh.ParkObjects = loaded.ParkObjects;
					s_Config = fresh;
				}
				else
				{
					s_Config = loaded;
				}
			}
			else
			{
				Print("[StuntBikes] config.json could not be read, using defaults: " + err);
			}
		}

		// fields missing in the file come back as 0 / empty: restore their defaults
		Validate(s_Config);

		// rewrite so new fields appear with their defaults
		s_Config.Version = SB_CONFIG_VERSION;
		JsonFileLoader<SB_Config>.SaveFile(GetPath(), s_Config, err);
		Print("[StuntBikes] config loaded, enabled=" + s_Config.Enabled);
	}

	// value must be > 0 (or >= 0 when allowZero)
	protected static float Fix(float value, float def, string name, bool allowZero = false)
	{
		if (value > 0 || (allowZero && value == 0))
			return value;
		Print("[StuntBikes] config: " + name + " missing or invalid, using " + def);
		return def;
	}

	protected static bool CurveOk(array<float> x, array<float> y)
	{
		if (!x || !y || x.Count() < 2 || x.Count() != y.Count())
			return false;
		for (int i = 1; i < x.Count(); i++)
		{
			if (x[i] <= x[i - 1])
				return false;
		}
		return true;
	}

	static void Validate(SB_Config c)
	{
		SB_Config d = new SB_Config();
		if (c.StuntMode < 0 || c.StuntMode > 2)
		{
			Print("[StuntBikes] config: StuntMode must be 0 (novice), 1 (rider) or 2 (pro), using 1");
			c.StuntMode = 1;
		}
		c.RiderPopAngle = Fix(c.RiderPopAngle, d.RiderPopAngle, "RiderPopAngle");
		c.RiderRaiseRate = Fix(c.RiderRaiseRate, d.RiderRaiseRate, "RiderRaiseRate");
		c.RiderLowerRate = Fix(c.RiderLowerRate, d.RiderLowerRate, "RiderLowerRate");
		c.RiderSinkRate = Fix(c.RiderSinkRate, d.RiderSinkRate, "RiderSinkRate", true);
		c.RiderDrift = Fix(c.RiderDrift, d.RiderDrift, "RiderDrift", true);
		c.RiderGreenMin = Fix(c.RiderGreenMin, d.RiderGreenMin, "RiderGreenMin");
		c.RiderGreenMax = Fix(c.RiderGreenMax, d.RiderGreenMax, "RiderGreenMax");
		c.RiderRedAngle = Fix(c.RiderRedAngle, d.RiderRedAngle, "RiderRedAngle");
		c.RiderStoppieGreenMin = Fix(c.RiderStoppieGreenMin, d.RiderStoppieGreenMin, "RiderStoppieGreenMin");
		c.RiderStoppieGreenMax = Fix(c.RiderStoppieGreenMax, d.RiderStoppieGreenMax, "RiderStoppieGreenMax");
		c.RiderStoppieRedAngle = Fix(c.RiderStoppieRedAngle, d.RiderStoppieRedAngle, "RiderStoppieRedAngle");
		c.RiderCatchTime = Fix(c.RiderCatchTime, d.RiderCatchTime, "RiderCatchTime");
		if (c.RiderGreenMax <= c.RiderGreenMin || c.RiderRedAngle <= c.RiderGreenMax)
		{
			Print("[StuntBikes] config: rider wheelie zones out of order, using defaults");
			c.RiderGreenMin = d.RiderGreenMin;
			c.RiderGreenMax = d.RiderGreenMax;
			c.RiderRedAngle = d.RiderRedAngle;
		}
		if (c.RiderStoppieGreenMax <= c.RiderStoppieGreenMin || c.RiderStoppieRedAngle <= c.RiderStoppieGreenMax)
		{
			Print("[StuntBikes] config: rider stoppie zones out of order, using defaults");
			c.RiderStoppieGreenMin = d.RiderStoppieGreenMin;
			c.RiderStoppieGreenMax = d.RiderStoppieGreenMax;
			c.RiderStoppieRedAngle = d.RiderStoppieRedAngle;
		}
		c.AssistMinSpeed = Fix(c.AssistMinSpeed, d.AssistMinSpeed, "AssistMinSpeed", true);
		c.AssistWheelieAngle = Fix(c.AssistWheelieAngle, d.AssistWheelieAngle, "AssistWheelieAngle");
		c.AssistStoppieAngle = Fix(c.AssistStoppieAngle, d.AssistStoppieAngle, "AssistStoppieAngle");
		c.AssistLiftRate = Fix(c.AssistLiftRate, d.AssistLiftRate, "AssistLiftRate");
		c.AssistDropRate = Fix(c.AssistDropRate, d.AssistDropRate, "AssistDropRate");
		c.AssistGain = Fix(c.AssistGain, d.AssistGain, "AssistGain");
		c.AssistAccel = Fix(c.AssistAccel, d.AssistAccel, "AssistAccel");
		c.AssistWobble = Fix(c.AssistWobble, d.AssistWobble, "AssistWobble", true);
		c.AssistAirRotate = Fix(c.AssistAirRotate, d.AssistAirRotate, "AssistAirRotate", true);
		if (c.AssistWheelieAngle > 60)
			c.AssistWheelieAngle = 60;
		if (c.AssistStoppieAngle > 40)
			c.AssistStoppieAngle = 40;
		c.ThrottleResponse = Fix(c.ThrottleResponse, d.ThrottleResponse, "ThrottleResponse");
		c.ClutchResponse = Fix(c.ClutchResponse, d.ClutchResponse, "ClutchResponse");

		c.BikeMass = Fix(c.BikeMass, d.BikeMass, "BikeMass");
		c.RiderMass = Fix(c.RiderMass, d.RiderMass, "RiderMass", true);
		c.ComHeight = Fix(c.ComHeight, d.ComHeight, "ComHeight");
		c.RearToCom = Fix(c.RearToCom, d.RearToCom, "RearToCom");
		c.FrontToCom = Fix(c.FrontToCom, d.FrontToCom, "FrontToCom");
		c.PitchInertia = Fix(c.PitchInertia, d.PitchInertia, "PitchInertia");
		c.RollInertia = Fix(c.RollInertia, d.RollInertia, "RollInertia");
		c.YawInertia = Fix(c.YawInertia, d.YawInertia, "YawInertia");
		c.RearWheelRadius = Fix(c.RearWheelRadius, d.RearWheelRadius, "RearWheelRadius");
		c.RearWheelInertia = Fix(c.RearWheelInertia, d.RearWheelInertia, "RearWheelInertia", true);
		c.TyreFriction = Fix(c.TyreFriction, d.TyreFriction, "TyreFriction");

		c.RiderShift = Fix(c.RiderShift, d.RiderShift, "RiderShift", true);
		c.RiderBodyFreq = Fix(c.RiderBodyFreq, d.RiderBodyFreq, "RiderBodyFreq");
		c.BarPullArm = Fix(c.BarPullArm, d.BarPullArm, "BarPullArm", true);
		c.RiderCrouch = Fix(c.RiderCrouch, d.RiderCrouch, "RiderCrouch", true);
		c.PumpForceScale = Fix(c.PumpForceScale, d.PumpForceScale, "PumpForceScale", true);

		c.EngineInertia = Fix(c.EngineInertia, d.EngineInertia, "EngineInertia");
		c.IdleRPM = Fix(c.IdleRPM, d.IdleRPM, "IdleRPM");
		c.LimiterRPM = Fix(c.LimiterRPM, d.LimiterRPM, "LimiterRPM");
		c.UpshiftRPM = Fix(c.UpshiftRPM, d.UpshiftRPM, "UpshiftRPM");
		c.DownshiftRPM = Fix(c.DownshiftRPM, d.DownshiftRPM, "DownshiftRPM");
		if (!CurveOk(c.TorqueRPM, c.TorqueNm))
		{
			Print("[StuntBikes] config: TorqueRPM / TorqueNm invalid, using MX Bikes 250XF curve");
			c.TorqueRPM = d.TorqueRPM;
			c.TorqueNm = d.TorqueNm;
		}
		if (!CurveOk(c.EngineBrakeRPM, c.EngineBrakeNm))
		{
			Print("[StuntBikes] config: EngineBrakeRPM / EngineBrakeNm invalid, using MX Bikes 250XF curve");
			c.EngineBrakeRPM = d.EngineBrakeRPM;
			c.EngineBrakeNm = d.EngineBrakeNm;
		}

		c.ClutchMaxTorque = Fix(c.ClutchMaxTorque, d.ClutchMaxTorque, "ClutchMaxTorque");
		c.ClutchDeadZone = Fix(c.ClutchDeadZone, d.ClutchDeadZone, "ClutchDeadZone", true);
		if (c.ClutchDeadZone > 0.9)
			c.ClutchDeadZone = 0.9;
		c.PrimaryRatio = Fix(c.PrimaryRatio, d.PrimaryRatio, "PrimaryRatio");
		if (!c.GearRatios || c.GearRatios.Count() == 0)
		{
			Print("[StuntBikes] config: GearRatios empty, using MX Bikes 250XF gearbox");
			c.GearRatios = d.GearRatios;
		}
		c.FinalRatio = Fix(c.FinalRatio, d.FinalRatio, "FinalRatio");
		c.DriveEfficiency = Fix(c.DriveEfficiency, d.DriveEfficiency, "DriveEfficiency");

		c.FrontBrakeMaxForce = Fix(c.FrontBrakeMaxForce, d.FrontBrakeMaxForce, "FrontBrakeMaxForce", true);
		c.RearBrakeMaxForce = Fix(c.RearBrakeMaxForce, d.RearBrakeMaxForce, "RearBrakeMaxForce", true);

		c.AirLeanAccel = Fix(c.AirLeanAccel, d.AirLeanAccel, "AirLeanAccel", true);
		c.AirDamping = Fix(c.AirDamping, d.AirDamping, "AirDamping", true);
		c.AirTrickDamping = Fix(c.AirTrickDamping, d.AirTrickDamping, "AirTrickDamping", true);

		c.MinStuntSpeed = Fix(c.MinStuntSpeed, d.MinStuntSpeed, "MinStuntSpeed", true);
		c.WheelieStartAngle = Fix(c.WheelieStartAngle, d.WheelieStartAngle, "WheelieStartAngle");
		c.TakeOverAngle = Fix(c.TakeOverAngle, d.TakeOverAngle, "TakeOverAngle");
		c.ForceResponse = Fix(c.ForceResponse, d.ForceResponse, "ForceResponse");
		c.GroundMaxPitchRate = Fix(c.GroundMaxPitchRate, d.GroundMaxPitchRate, "GroundMaxPitchRate");
		c.MaxPitchRate = Fix(c.MaxPitchRate, d.MaxPitchRate, "MaxPitchRate");
		c.SettleRate = Fix(c.SettleRate, d.SettleRate, "SettleRate");
		c.ResyncRate = Fix(c.ResyncRate, d.ResyncRate, "ResyncRate");
		c.LoopOutAngle = Fix(c.LoopOutAngle, d.LoopOutAngle, "LoopOutAngle");
		c.EndoAngle = Fix(c.EndoAngle, d.EndoAngle, "EndoAngle");

		c.FollowGain = Fix(c.FollowGain, d.FollowGain, "FollowGain");
		c.StateSendInterval = Fix(c.StateSendInterval, d.StateSendInterval, "StateSendInterval");
		c.StateTimeout = Fix(c.StateTimeout, d.StateTimeout, "StateTimeout");

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
