// Angle bar on the right side of the screen while a wheel is up.
// Novice: green band around the held angle. Rider: green zone to stay in, red zone at the top.
// Pro: green around the balance point, red near the loop-out.

class SB_Hud
{
	protected Widget m_Root;
	protected Widget m_Green;
	protected Widget m_Red;
	protected Widget m_Marker;
	protected TextWidget m_Label;

	static const float BAR_W = 22;
	static const float BAR_H = 240;
	static const float BAR_MAX = 75; // degrees at the top of the bar

	void SB_Hud()
	{
		m_Root = GetGame().GetWorkspace().CreateWidgets("StuntBikes/data/sb_hud.layout");
		if (!m_Root)
		{
			Print("[StuntBikes] HUD layout not found");
			return;
		}
		m_Green = m_Root.FindAnyWidget("SB_Green");
		m_Red = m_Root.FindAnyWidget("SB_Red");
		m_Marker = m_Root.FindAnyWidget("SB_Marker");
		m_Label = TextWidget.Cast(m_Root.FindAnyWidget("SB_Label"));
		m_Root.Show(false);
	}

	void ~SB_Hud()
	{
		if (m_Root)
			m_Root.Unlink();
	}

	protected float Y(float angle)
	{
		return BAR_H - Math.Clamp(angle / BAR_MAX, 0, 1) * BAR_H;
	}

	protected void Zone(Widget w, float from, float to)
	{
		if (!w)
			return;
		if (to <= from)
		{
			w.Show(false);
			return;
		}
		w.Show(true);
		w.SetPos(0, Y(to));
		w.SetSize(BAR_W, Y(from) - Y(to));
	}

	protected MotorbikeScript LocalBike()
	{
		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		if (!player)
			return null;
		HumanCommandVehicle hcv = player.GetCommand_Vehicle();
		if (!hcv)
			return null;
		return MotorbikeScript.Cast(hcv.GetTransport());
	}

	void Update()
	{
		if (!m_Root)
			return;

		SB_Config cfg = SB_Settings.Get();
		MotorbikeScript bike = LocalBike();
		if (!cfg.Enabled || !cfg.ShowAngleHud || !bike || !bike.SB_IsLocallyControlled())
		{
			m_Root.Show(false);
			return;
		}

		SB_Control ctl = bike.SB_GetControlPublic();
		int mode = ctl.GetMode();
		if (mode != SB_PitchMode.WHEELIE && mode != SB_PitchMode.STOPPIE)
		{
			m_Root.Show(false);
			return;
		}

		bool wheelie = mode == SB_PitchMode.WHEELIE;
		float lift = ctl.GetLastPitch();
		if (!wheelie)
			lift = -lift;
		int level = ctl.GetLevel();

		float gMin;
		float gMax;
		float red = BAR_MAX + 1;
		if (level == 0)
		{
			float held = cfg.AssistWheelieAngle;
			if (!wheelie)
				held = cfg.AssistStoppieAngle;
			gMin = held - 5;
			gMax = held + 5;
		}
		else if (level == 1)
		{
			gMin = cfg.RiderGreenMin;
			gMax = cfg.RiderGreenMax;
			red = cfg.RiderRedAngle;
			if (!wheelie)
			{
				gMin = cfg.RiderStoppieGreenMin;
				gMax = cfg.RiderStoppieGreenMax;
				red = cfg.RiderStoppieRedAngle;
			}
		}
		else
		{
			gMin = 25;
			gMax = 42;
			red = 60;
			if (!wheelie)
			{
				gMin = 10;
				gMax = 30;
				red = 45;
			}
		}

		Zone(m_Green, gMin, gMax);
		Zone(m_Red, red, BAR_MAX);
		if (m_Marker)
			m_Marker.SetPos(-5, Y(lift) - 2);

		if (m_Label)
		{
			string what = "ВИЛЛИ";
			if (!wheelie)
				what = "СТОППИ";
			if (ctl.IsCatching())
				what = "ЛОВЛЮ!";
			m_Label.SetText(what + " " + Math.Round(lift).ToString() + "°");
			if (ctl.IsCatching() || lift >= red)
				m_Label.SetColor(ARGB(255, 255, 80, 80));
			else if (lift >= gMin && lift <= gMax)
				m_Label.SetColor(ARGB(255, 90, 230, 110));
			else
				m_Label.SetColor(ARGB(255, 255, 255, 255));
		}
		m_Root.Show(true);
	}
}
