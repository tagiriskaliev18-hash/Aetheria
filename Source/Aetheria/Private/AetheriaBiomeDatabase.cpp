#include "AetheriaBiomeDatabase.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "CollisionQueryParams.h"

namespace
{
	FAetheriaMobArchetype MakeMob(
		FName Id,
		const TCHAR* Name,
		EAetheriaMobKind Kind,
		float HP,
		float SpeedUU,
		float Damage,
		float RadiusUU,
		float RangeUU,
		FLinearColor BodyColor,
		FLinearColor AccentColor,
		bool bExplodeOnDeath = false,
		bool bCastsOrbs = false)
	{
		FAetheriaMobArchetype M;
		M.Id = Id;
		M.DisplayName = Name;
		M.Kind = Kind;
		M.MaxHP = HP;
		M.MoveSpeedUU = SpeedUU;
		M.Damage = Damage;
		M.CollisionRadiusUU = RadiusUU;
		M.AttackRangeUU = RangeUU;
		M.BodyColor = BodyColor;
		M.AccentColor = AccentColor;
		M.bExplodeOnDeath = bExplodeOnDeath;
		M.bCastsOrbs = bCastsOrbs;
		return M;
	}

	TArray<FAetheriaTerrainZone> BuildBiomeTerrainZones(int32 Index, float ArenaRadius)
	{
		TArray<FAetheriaTerrainZone> Zones;
		Zones.Reserve(14);

		// Sanctuary Ring at (0, 0)
		FAetheriaTerrainZone Sanct;
		Sanct.Kind = EAetheriaTerrainZoneKind::SanctuaryRing;
		Sanct.Center = FVector2D::ZeroVector;
		Sanct.RadiusUU = 640.0f;
		Sanct.ElevationUU = 0.0f;
		Zones.Add(Sanct);

		const float BaseAngle = Index * 37.0f;

		// 3 Elevated Plateaus around R=2650 (gentle walkable steps)
		const float PlateauElev = 42.0f + (Index % 3) * 6.0f;
		for (int32 i = 0; i < 3; ++i)
		{
			const float Deg = BaseAngle + i * 120.0f;
			const float Rad = FMath::DegreesToRadians(Deg);
			FAetheriaTerrainZone P;
			P.Kind = EAetheriaTerrainZoneKind::ElevatedPlateau;
			P.Center = FVector2D(FMath::Cos(Rad) * 2650.0f, FMath::Sin(Rad) * 2650.0f);
			P.RadiusUU = 660.0f + (Index % 2) * 30.0f;
			P.ElevationUU = PlateauElev;
			P.RampWidthUU = 420.0f;
			P.YawDeg = Deg;
			Zones.Add(P);
		}

		// 2 Sunken Trenches at R=1300
		const float TrenchDeg1 = BaseAngle + 55.0f;
		const float TrenchDeg2 = BaseAngle + 235.0f;
		const float TrenchElev = -45.0f - (Index % 2) * 10.0f;

		const float RadT1 = FMath::DegreesToRadians(TrenchDeg1);
		const FVector2D TrenchPos1(FMath::Cos(RadT1) * 1300.0f, FMath::Sin(RadT1) * 1300.0f);
		FAetheriaTerrainZone T1;
		T1.Kind = EAetheriaTerrainZoneKind::SunkenTrench;
		T1.Center = TrenchPos1;
		T1.RadiusUU = 500.0f;
		T1.WidthUU = 580.0f;
		T1.LengthUU = 1650.0f;
		T1.YawDeg = TrenchDeg1 + 90.0f;
		T1.ElevationUU = TrenchElev;
		T1.RampWidthUU = 220.0f;
		Zones.Add(T1);

		const float RadT2 = FMath::DegreesToRadians(TrenchDeg2);
		const FVector2D TrenchPos2(FMath::Cos(RadT2) * 1300.0f, FMath::Sin(RadT2) * 1300.0f);
		FAetheriaTerrainZone T2;
		T2.Kind = EAetheriaTerrainZoneKind::SunkenTrench;
		T2.Center = TrenchPos2;
		T2.RadiusUU = 500.0f;
		T2.WidthUU = 580.0f;
		T2.LengthUU = 1650.0f;
		T2.YawDeg = TrenchDeg2 + 90.0f;
		T2.ElevationUU = TrenchElev;
		T2.RampWidthUU = 220.0f;
		Zones.Add(T2);

		// 2 Chokepoint Bridges crossing the trenches
		FAetheriaTerrainZone B1;
		B1.Kind = EAetheriaTerrainZoneKind::ChokepointBridge;
		B1.Center = TrenchPos1;
		B1.WidthUU = 360.0f;
		B1.LengthUU = 860.0f;
		B1.YawDeg = TrenchDeg1;
		B1.ElevationUU = 6.0f;
		Zones.Add(B1);

		FAetheriaTerrainZone B2;
		B2.Kind = EAetheriaTerrainZoneKind::ChokepointBridge;
		B2.Center = TrenchPos2;
		B2.WidthUU = 360.0f;
		B2.LengthUU = 860.0f;
		B2.YawDeg = TrenchDeg2;
		B2.ElevationUU = 6.0f;
		Zones.Add(B2);

		// 6 Outer Cover Parapets at R=2350 UU (keeping central Sanctuary & spawn ring completely clear)
		static const float OffsetAngles[6] = { 35.0f, 85.0f, 145.0f, 215.0f, 265.0f, 325.0f };
		for (float Offset : OffsetAngles)
		{
			const float Deg = BaseAngle + Offset;
			const float Rad = FMath::DegreesToRadians(Deg);
			FAetheriaTerrainZone C;
			C.Kind = EAetheriaTerrainZoneKind::CoverParapet;
			C.Center = FVector2D(FMath::Cos(Rad) * 2350.0f, FMath::Sin(Rad) * 2350.0f);
			C.WidthUU = 260.0f;
			C.LengthUU = 110.0f;
			C.YawDeg = Deg + 90.0f;
			C.ElevationUU = 125.0f;
			Zones.Add(C);
		}

		return Zones;
	}

	TArray<FAetheriaLocationDef> BuildAllLocations()
	{
		TArray<FAetheriaLocationDef> List;
		List.Reserve(10);

		// 01. Shattered Aether Bastion
		{
			FAetheriaLocationDef L;
			L.Index = 0;
			L.Id = FName("aether_bastion");
			L.DisplayName = TEXT("01. Расколотый Бастион Эфира");
			L.Subtitle = TEXT("Внешние врата Хроно-Печати");
			L.StoryHook = TEXT("Первый осколок Печати пробудил эфирные гейзеры. Захватите рунные маяки, чтобы прорваться вглубь разлома.");
			L.Theme = EAetheriaBiomeTheme::Bastion;
			L.HazardKind = EAetheriaHazardKind::EtherGeyser;
			L.HazardName = TEXT("Эфирные гейзеры");
			L.HazardDescription = TEXT("Гейзеры ускоряют восстановление маны (+15/с), но усиливают натиск порождений Бездны.");
			L.ArenaRadiusUU = 12000.0f;
			L.GracePeriodSeconds = 5.5f;

			// SanctuaryRing at (0,0), RadiusUU=720, ElevationUU=0
			FAetheriaTerrainZone S;
			S.Kind = EAetheriaTerrainZoneKind::SanctuaryRing;
			S.Center = FVector2D::ZeroVector;
			S.RadiusUU = 720.0f;
			S.ElevationUU = 0.0f;
			L.TerrainZones.Add(S);

			// 3 ElevatedPlateau at angles 0, 120, 240 deg, R=2600 UU, RadiusUU=720, ElevationUU=95, RampWidthUU=380
			static const float BastionPlateauAngles[3] = { 0.0f, 120.0f, 240.0f };
			for (float Deg : BastionPlateauAngles)
			{
				const float Rad = FMath::DegreesToRadians(Deg);
				FAetheriaTerrainZone P;
				P.Kind = EAetheriaTerrainZoneKind::ElevatedPlateau;
				P.Center = FVector2D(FMath::Cos(Rad) * 2600.0f, FMath::Sin(Rad) * 2600.0f);
				P.RadiusUU = 720.0f;
				P.ElevationUU = 95.0f;
				P.RampWidthUU = 380.0f;
				P.YawDeg = Deg;
				L.TerrainZones.Add(P);
			}

			// 2 SunkenTrench channels: centered at (1300, 950) and (-1100, -1050), RadiusUU=520, WidthUU=620, LengthUU=1650, ElevationUU=-45, RampWidthUU=220
			FAetheriaTerrainZone T1;
			T1.Kind = EAetheriaTerrainZoneKind::SunkenTrench;
			T1.Center = FVector2D(1300.0f, 950.0f);
			T1.RadiusUU = 520.0f;
			T1.WidthUU = 620.0f;
			T1.LengthUU = 1650.0f;
			T1.YawDeg = 36.0f;
			T1.ElevationUU = -45.0f;
			T1.RampWidthUU = 220.0f;
			L.TerrainZones.Add(T1);

			FAetheriaTerrainZone T2;
			T2.Kind = EAetheriaTerrainZoneKind::SunkenTrench;
			T2.Center = FVector2D(-1100.0f, -1050.0f);
			T2.RadiusUU = 520.0f;
			T2.WidthUU = 620.0f;
			T2.LengthUU = 1650.0f;
			T2.YawDeg = 216.0f;
			T2.ElevationUU = -45.0f;
			T2.RampWidthUU = 220.0f;
			L.TerrainZones.Add(T2);

			// 2 ChokepointBridge crossing those trenches at ElevationUU=5, WidthUU=380, LengthUU=900
			FAetheriaTerrainZone B1;
			B1.Kind = EAetheriaTerrainZoneKind::ChokepointBridge;
			B1.Center = FVector2D(1300.0f, 950.0f);
			B1.WidthUU = 380.0f;
			B1.LengthUU = 900.0f;
			B1.YawDeg = 126.0f;
			B1.ElevationUU = 5.0f;
			L.TerrainZones.Add(B1);

			FAetheriaTerrainZone B2;
			B2.Kind = EAetheriaTerrainZoneKind::ChokepointBridge;
			B2.Center = FVector2D(-1100.0f, -1050.0f);
			B2.WidthUU = 380.0f;
			B2.LengthUU = 900.0f;
			B2.YawDeg = 306.0f;
			B2.ElevationUU = 5.0f;
			L.TerrainZones.Add(B2);

			// 6 CoverParapet at angles 35, 85, 145, 215, 265, 325 deg at R=1850 UU, WidthUU=340, LengthUU=150, ElevationUU=145
			static const float BastionParapets[6] = { 35.0f, 85.0f, 145.0f, 215.0f, 265.0f, 325.0f };
			for (float Deg : BastionParapets)
			{
				const float Rad = FMath::DegreesToRadians(Deg);
				FAetheriaTerrainZone C;
				C.Kind = EAetheriaTerrainZoneKind::CoverParapet;
				C.Center = FVector2D(FMath::Cos(Rad) * 1850.0f, FMath::Sin(Rad) * 1850.0f);
				C.WidthUU = 340.0f;
				C.LengthUU = 150.0f;
				C.YawDeg = Deg + 90.0f;
				C.ElevationUU = 145.0f;
				L.TerrainZones.Add(C);
			}

			L.GroundColor = FLinearColor(0.018f, 0.020f, 0.042f, 1.0f);
			L.FogColor = FLinearColor(0.012f, 0.012f, 0.025f, 1.0f);
			L.RimColor = AetheriaMindColors::MindIndigo;
			L.AccentColor = AetheriaMindColors::MindCyan;
			L.Mobs = {
				MakeMob("void_hound", TEXT("Гончая Пустоты"), EAetheriaMobKind::Swarm, 140, 640, 14, 90, 200, FLinearColor(0.05f, 0.06f, 0.14f), AetheriaMindColors::MindViolet),
				MakeMob("shard_golem", TEXT("Осколочный Голем"), EAetheriaMobKind::Bruiser, 440, 340, 28, 165, 290, FLinearColor(0.07f, 0.08f, 0.16f), AetheriaMindColors::MindIndigo),
				MakeMob("aether_sniper", TEXT("Эфирный Снайпер"), EAetheriaMobKind::Ranged, 190, 410, 20, 100, 1600, FLinearColor(0.04f, 0.08f, 0.16f), AetheriaMindColors::MindCyan)
			};
			L.Boss = MakeMob("archon_rift", TEXT("Архонт Разлома"), EAetheriaMobKind::Boss, 5200, 420, 42, 280, 360, FLinearColor(0.04f, 0.04f, 0.10f), AetheriaMindColors::MindCyan);
			List.Add(L);
		}

		// 02. Ash Caldera of Ignis
		{
			FAetheriaLocationDef L;
			L.Index = 1;
			L.Id = FName("ignis_caldera");
			L.DisplayName = TEXT("02. Пепельная Кальдера Игниса");
			L.Subtitle = TEXT("Плазменные кратеры Древней Кузни");
			L.StoryHook = TEXT("Раскалённые приливы магмы прожигают броню. Призовите сферу Темпеста (W), чтобы остудить жар.");
			L.Theme = EAetheriaBiomeTheme::Caldera;
			L.HazardKind = EAetheriaHazardKind::MagmaTide;
			L.HazardName = TEXT("Магмовые приливы");
			L.HazardDescription = TEXT("Огненные разломы обжигают стоящих рядом; активная сфера W снижает урон.");
			L.ArenaRadiusUU = 12500.0f;
			L.GracePeriodSeconds = 3.5f;

			// SanctuaryRing at (0,0), RadiusUU=620, ElevationUU=0
			FAetheriaTerrainZone S;
			S.Kind = EAetheriaTerrainZoneKind::SanctuaryRing;
			S.Center = FVector2D::ZeroVector;
			S.RadiusUU = 620.0f;
			S.ElevationUU = 0.0f;
			L.TerrainZones.Add(S);

			// 3 ElevatedPlateau obsidian cliffs at angles 60, 180, 300 deg, R=2500 UU, RadiusUU=680, ElevationUU=115, RampWidthUU=380
			static const float CalderaPlateaus[3] = { 60.0f, 180.0f, 300.0f };
			for (float Deg : CalderaPlateaus)
			{
				const float Rad = FMath::DegreesToRadians(Deg);
				FAetheriaTerrainZone P;
				P.Kind = EAetheriaTerrainZoneKind::ElevatedPlateau;
				P.Center = FVector2D(FMath::Cos(Rad) * 2500.0f, FMath::Sin(Rad) * 2500.0f);
				P.RadiusUU = 680.0f;
				P.ElevationUU = 115.0f;
				P.RampWidthUU = 380.0f;
				P.YawDeg = Deg;
				L.TerrainZones.Add(P);
			}

			// 2 SunkenTrench magma rifts at R=1350 UU (angles 90 and 270 deg), WidthUU=560, LengthUU=1800, ElevationUU=-60, RampWidthUU=200
			FAetheriaTerrainZone T1;
			T1.Kind = EAetheriaTerrainZoneKind::SunkenTrench;
			T1.Center = FVector2D(0.0f, 1350.0f);
			T1.RadiusUU = 540.0f;
			T1.WidthUU = 560.0f;
			T1.LengthUU = 1800.0f;
			T1.YawDeg = 0.0f;
			T1.ElevationUU = -60.0f;
			T1.RampWidthUU = 200.0f;
			L.TerrainZones.Add(T1);

			FAetheriaTerrainZone T2;
			T2.Kind = EAetheriaTerrainZoneKind::SunkenTrench;
			T2.Center = FVector2D(0.0f, -1350.0f);
			T2.RadiusUU = 540.0f;
			T2.WidthUU = 560.0f;
			T2.LengthUU = 1800.0f;
			T2.YawDeg = 0.0f;
			T2.ElevationUU = -60.0f;
			T2.RampWidthUU = 200.0f;
			L.TerrainZones.Add(T2);

			// 3 ChokepointBridge basalt bridges across the magma rifts, WidthUU=340, LengthUU=880, ElevationUU=8
			FAetheriaTerrainZone B1;
			B1.Kind = EAetheriaTerrainZoneKind::ChokepointBridge;
			B1.Center = FVector2D(0.0f, 1350.0f);
			B1.WidthUU = 340.0f;
			B1.LengthUU = 880.0f;
			B1.YawDeg = 90.0f;
			B1.ElevationUU = 8.0f;
			L.TerrainZones.Add(B1);

			FAetheriaTerrainZone B2;
			B2.Kind = EAetheriaTerrainZoneKind::ChokepointBridge;
			B2.Center = FVector2D(-450.0f, -1350.0f);
			B2.WidthUU = 340.0f;
			B2.LengthUU = 880.0f;
			B2.YawDeg = 90.0f;
			B2.ElevationUU = 8.0f;
			L.TerrainZones.Add(B2);

			FAetheriaTerrainZone B3;
			B3.Kind = EAetheriaTerrainZoneKind::ChokepointBridge;
			B3.Center = FVector2D(450.0f, -1350.0f);
			B3.WidthUU = 340.0f;
			B3.LengthUU = 880.0f;
			B3.YawDeg = 90.0f;
			B3.ElevationUU = 8.0f;
			L.TerrainZones.Add(B3);

			// 5 CoverParapet at R=1950 UU (angles 30, 110, 165, 245, 315 deg), WidthUU=340, LengthUU=150, ElevationUU=145
			static const float CalderaParapets[5] = { 30.0f, 110.0f, 165.0f, 245.0f, 315.0f };
			for (float Deg : CalderaParapets)
			{
				const float Rad = FMath::DegreesToRadians(Deg);
				FAetheriaTerrainZone C;
				C.Kind = EAetheriaTerrainZoneKind::CoverParapet;
				C.Center = FVector2D(FMath::Cos(Rad) * 1950.0f, FMath::Sin(Rad) * 1950.0f);
				C.WidthUU = 340.0f;
				C.LengthUU = 150.0f;
				C.YawDeg = Deg + 90.0f;
				C.ElevationUU = 145.0f;
				L.TerrainZones.Add(C);
			}

			L.GroundColor = FLinearColor(0.045f, 0.015f, 0.012f, 1.0f);
			L.FogColor = FLinearColor(0.025f, 0.008f, 0.008f, 1.0f);
			L.RimColor = AetheriaMindColors::IgnisPlasma;
			L.AccentColor = FLinearColor(1.0f, 0.48f, 0.08f, 1.0f);
			L.Mobs = {
				MakeMob("magma_crawler", TEXT("Магмовый Ползун"), EAetheriaMobKind::Swarm, 165, 590, 16, 95, 200, FLinearColor(0.12f, 0.03f, 0.02f), AetheriaMindColors::IgnisPlasma, true),
				MakeMob("obsidian_juggernaut", TEXT("Обсидиановый Джаггернаут"), EAetheriaMobKind::Bruiser, 540, 310, 34, 180, 290, FLinearColor(0.04f, 0.03f, 0.03f), FLinearColor(1.0f, 0.25f, 0.02f)),
				MakeMob("pyro_cultist", TEXT("Пиро-Культист"), EAetheriaMobKind::Ranged, 220, 400, 24, 100, 1700, FLinearColor(0.14f, 0.03f, 0.02f), FLinearColor(1.0f, 0.5f, 0.1f))
			};
			L.Boss = MakeMob("titan_pyros", TEXT("Титан Пирос"), EAetheriaMobKind::Boss, 6400, 390, 48, 310, 380, FLinearColor(0.08f, 0.02f, 0.015f), AetheriaMindColors::IgnisPlasma);
			List.Add(L);
		}

		// 03. Crystal Glacier of Chronos
		{
			FAetheriaLocationDef L;
			L.Index = 2;
			L.Id = FName("chrono_glacier");
			L.DisplayName = TEXT("03. Кристальный Ледник Хроноса");
			L.Subtitle = TEXT("Замёрзшие монументы вечности");
			L.StoryHook = TEXT("Вечная стужа накапливает стаки Холода. Сбрасывайте оледенение сферой Игниса (Q).");
			L.Theme = EAetheriaBiomeTheme::Glacier;
			L.HazardKind = EAetheriaHazardKind::FrostDrift;
			L.HazardName = TEXT("Ледяная стужа");
			L.HazardDescription = TEXT("Накапливает замедляющий холод; сфера Q мгновенно согревает мага.");
			L.ArenaRadiusUU = 12500.0f;
			L.GracePeriodSeconds = 3.5f;

			// SanctuaryRing at (0,0), RadiusUU=600, ElevationUU=0
			FAetheriaTerrainZone S;
			S.Kind = EAetheriaTerrainZoneKind::SanctuaryRing;
			S.Center = FVector2D::ZeroVector;
			S.RadiusUU = 600.0f;
			S.ElevationUU = 0.0f;
			L.TerrainZones.Add(S);

			// Central/ring SunkenTrench frost basin around R=1250 UU (2 arcs at 45 and 225 deg), WidthUU=620, LengthUU=1600, ElevationUU=-45, RampWidthUU=240
			const float Rad45 = FMath::DegreesToRadians(45.0f);
			FAetheriaTerrainZone T1;
			T1.Kind = EAetheriaTerrainZoneKind::SunkenTrench;
			T1.Center = FVector2D(FMath::Cos(Rad45) * 1250.0f, FMath::Sin(Rad45) * 1250.0f);
			T1.RadiusUU = 520.0f;
			T1.WidthUU = 620.0f;
			T1.LengthUU = 1600.0f;
			T1.YawDeg = 135.0f;
			T1.ElevationUU = -45.0f;
			T1.RampWidthUU = 240.0f;
			L.TerrainZones.Add(T1);

			const float Rad225 = FMath::DegreesToRadians(225.0f);
			FAetheriaTerrainZone T2;
			T2.Kind = EAetheriaTerrainZoneKind::SunkenTrench;
			T2.Center = FVector2D(FMath::Cos(Rad225) * 1250.0f, FMath::Sin(Rad225) * 1250.0f);
			T2.RadiusUU = 520.0f;
			T2.WidthUU = 620.0f;
			T2.LengthUU = 1600.0f;
			T2.YawDeg = 315.0f;
			T2.ElevationUU = -45.0f;
			T2.RampWidthUU = 240.0f;
			L.TerrainZones.Add(T2);

			// 3 ElevatedPlateau thermal plateaus at angles 0, 120, 240 deg, R=2550 UU, RadiusUU=700, ElevationUU=105, RampWidthUU=380
			static const float GlacierPlateaus[3] = { 0.0f, 120.0f, 240.0f };
			for (float Deg : GlacierPlateaus)
			{
				const float Rad = FMath::DegreesToRadians(Deg);
				FAetheriaTerrainZone P;
				P.Kind = EAetheriaTerrainZoneKind::ElevatedPlateau;
				P.Center = FVector2D(FMath::Cos(Rad) * 2550.0f, FMath::Sin(Rad) * 2550.0f);
				P.RadiusUU = 700.0f;
				P.ElevationUU = 105.0f;
				P.RampWidthUU = 380.0f;
				P.YawDeg = Deg;
				L.TerrainZones.Add(P);
			}

			// 3 ChokepointBridge ice bridges, WidthUU=360, LengthUU=840, ElevationUU=5
			FAetheriaTerrainZone B1;
			B1.Kind = EAetheriaTerrainZoneKind::ChokepointBridge;
			B1.Center = T1.Center;
			B1.WidthUU = 360.0f;
			B1.LengthUU = 840.0f;
			B1.YawDeg = 45.0f;
			B1.ElevationUU = 5.0f;
			L.TerrainZones.Add(B1);

			FAetheriaTerrainZone B2;
			B2.Kind = EAetheriaTerrainZoneKind::ChokepointBridge;
			B2.Center = T2.Center;
			B2.WidthUU = 360.0f;
			B2.LengthUU = 840.0f;
			B2.YawDeg = 225.0f;
			B2.ElevationUU = 5.0f;
			L.TerrainZones.Add(B2);

			FAetheriaTerrainZone B3;
			B3.Kind = EAetheriaTerrainZoneKind::ChokepointBridge;
			B3.Center = FVector2D(0.0f, 1250.0f);
			B3.WidthUU = 360.0f;
			B3.LengthUU = 840.0f;
			B3.YawDeg = 90.0f;
			B3.ElevationUU = 5.0f;
			L.TerrainZones.Add(B3);

			// 6 CoverParapet crystal monoliths at R=1850 UU (35, 85, 145, 215, 265, 325 deg), WidthUU=320, LengthUU=150, ElevationUU=145
			static const float GlacierParapets[6] = { 35.0f, 85.0f, 145.0f, 215.0f, 265.0f, 325.0f };
			for (float Deg : GlacierParapets)
			{
				const float Rad = FMath::DegreesToRadians(Deg);
				FAetheriaTerrainZone C;
				C.Kind = EAetheriaTerrainZoneKind::CoverParapet;
				C.Center = FVector2D(FMath::Cos(Rad) * 1850.0f, FMath::Sin(Rad) * 1850.0f);
				C.WidthUU = 320.0f;
				C.LengthUU = 150.0f;
				C.YawDeg = Deg + 90.0f;
				C.ElevationUU = 145.0f;
				L.TerrainZones.Add(C);
			}

			L.GroundColor = FLinearColor(0.015f, 0.035f, 0.075f, 1.0f);
			L.FogColor = FLinearColor(0.010f, 0.020f, 0.045f, 1.0f);
			L.RimColor = AetheriaMindColors::MindCobalt;
			L.AccentColor = AetheriaMindColors::MindCyan;
			L.Mobs = {
				MakeMob("ice_wraith", TEXT("Ледяной Призрак"), EAetheriaMobKind::Swarm, 185, 670, 18, 90, 200, FLinearColor(0.04f, 0.14f, 0.26f), AetheriaMindColors::MindCyan),
				MakeMob("cryo_shielder", TEXT("Крио-Щитоносец"), EAetheriaMobKind::Bruiser, 620, 330, 32, 175, 290, FLinearColor(0.03f, 0.09f, 0.18f), FLinearColor(0.25f, 0.95f, 1.0f)),
				MakeMob("shard_wyvern", TEXT("Осколочная Виверна"), EAetheriaMobKind::Ranged, 250, 450, 25, 110, 1800, FLinearColor(0.05f, 0.16f, 0.30f), AetheriaMindColors::MindCobalt)
			};
			L.Boss = MakeMob("lich_glacius", TEXT("Хроно-Лич Гласиус"), EAetheriaMobKind::Boss, 7500, 420, 52, 300, 370, FLinearColor(0.025f, 0.08f, 0.18f), AetheriaMindColors::MindCyan);
			List.Add(L);
		}

		// 04. Gravity Gardens of Void
		{
			FAetheriaLocationDef L;
			L.Index = 3;
			L.Id = FName("gravity_gardens");
			L.DisplayName = TEXT("04. Гравитационные Сады Пустоты");
			L.Subtitle = TEXT("Парящие террасы искажённого притяжения");
			L.StoryHook = TEXT("Расколотое притяжение рождает грави-колодцы. Используйте их, чтобы сгруппировать врагов под свои заклинания.");
			L.Theme = EAetheriaBiomeTheme::GravityGardens;
			L.HazardKind = EAetheriaHazardKind::GravWell;
			L.HazardName = TEXT("Грави-колодцы");
			L.HazardDescription = TEXT("Аномалии притяжения стягивают бойцов к точкам искажения.");
			L.ArenaRadiusUU = 13000.0f;
			L.GracePeriodSeconds = 3.5f;
			L.TerrainZones = BuildBiomeTerrainZones(L.Index, L.ArenaRadiusUU);
			L.GroundColor = FLinearColor(0.028f, 0.015f, 0.065f, 1.0f);
			L.FogColor = FLinearColor(0.016f, 0.008f, 0.038f, 1.0f);
			L.RimColor = AetheriaMindColors::MindViolet;
			L.AccentColor = FLinearColor(0.60f, 0.25f, 1.0f, 1.0f);
			L.Mobs = {
				MakeMob("astral_manta", TEXT("Астральный Скат"), EAetheriaMobKind::Swarm, 200, 700, 20, 100, 210, FLinearColor(0.08f, 0.03f, 0.20f), FLinearColor(0.65f, 0.30f, 1.0f)),
				MakeMob("gravity_devourer", TEXT("Пожиратель Гравитации"), EAetheriaMobKind::Bruiser, 700, 350, 38, 185, 300, FLinearColor(0.06f, 0.02f, 0.16f), AetheriaMindColors::MindViolet),
				MakeMob("shadow_duelist", TEXT("Теневой Дуэлянт"), EAetheriaMobKind::Ranged, 275, 480, 28, 100, 1700, FLinearColor(0.09f, 0.04f, 0.22f), FLinearColor(0.80f, 0.45f, 1.0f))
			};
			L.Boss = MakeMob("warden_null", TEXT("Смотритель Нулл"), EAetheriaMobKind::Boss, 8700, 440, 56, 310, 380, FLinearColor(0.04f, 0.015f, 0.12f), AetheriaMindColors::MindViolet);
			List.Add(L);
		}

		// 05. Storm Necropolis
		{
			FAetheriaLocationDef L;
			L.Index = 4;
			L.Id = FName("storm_necropolis");
			L.DisplayName = TEXT("05. Грозовой Некрополь Бурь");
			L.Subtitle = TEXT("Медные обелиски вечного шторма");
			L.StoryHook = TEXT("Обелиски аккумулируют разряды бури и выпускают импульсы молний по арене.");
			L.Theme = EAetheriaBiomeTheme::StormNecropolis;
			L.HazardKind = EAetheriaHazardKind::StormArc;
			L.HazardName = TEXT("Штормовые разряды");
			L.HazardDescription = TEXT("Шпили периодически бьют электрическим импульсом по ближайшим бойцам.");
			L.ArenaRadiusUU = 13000.0f;
			L.GracePeriodSeconds = 3.5f;
			L.TerrainZones = BuildBiomeTerrainZones(L.Index, L.ArenaRadiusUU);
			L.GroundColor = FLinearColor(0.018f, 0.030f, 0.050f, 1.0f);
			L.FogColor = FLinearColor(0.010f, 0.016f, 0.028f, 1.0f);
			L.RimColor = AetheriaMindColors::MindCyan;
			L.AccentColor = FLinearColor(0.25f, 0.90f, 0.80f, 1.0f);
			L.Mobs = {
				MakeMob("chain_phantom", TEXT("Цепной Фантом"), EAetheriaMobKind::Swarm, 225, 720, 22, 90, 200, FLinearColor(0.04f, 0.09f, 0.14f), AetheriaMindColors::MindCyan),
				MakeMob("storm_automaton", TEXT("Грозовой Автоматон"), EAetheriaMobKind::Bruiser, 780, 360, 40, 185, 300, FLinearColor(0.06f, 0.11f, 0.16f), FLinearColor(0.15f, 0.65f, 0.90f)),
				MakeMob("tempest_priest", TEXT("Жрец Бури"), EAetheriaMobKind::Ranged, 310, 430, 31, 105, 1900, FLinearColor(0.03f, 0.09f, 0.14f), FLinearColor(0.35f, 0.85f, 1.0f))
			};
			L.Boss = MakeMob("colossus_volt", TEXT("Колосс Вольт"), EAetheriaMobKind::Boss, 9900, 430, 60, 320, 390, FLinearColor(0.03f, 0.08f, 0.13f), AetheriaMindColors::MindCyan);
			List.Add(L);
		}

		// 06. Mirror Labyrinth
		{
			FAetheriaLocationDef L;
			L.Index = 5;
			L.Id = FName("mirror_labyrinth");
			L.DisplayName = TEXT("06. Зеркальный Лабиринт Миражей");
			L.Subtitle = TEXT("Хрустальные призмы отражений");
			L.StoryHook = TEXT("Зеркальные монолиты усиливают магию: союзные снаряды в зоне призм получают +35% урона.");
			L.Theme = EAetheriaBiomeTheme::MirrorLabyrinth;
			L.HazardKind = EAetheriaHazardKind::MirrorPylon;
			L.HazardName = TEXT("Зеркальные пилоны");
			L.HazardDescription = TEXT("Пилоны усиливают урон союзных заклинаний и расщепляют потоки энергии.");
			L.ArenaRadiusUU = 13000.0f;
			L.GracePeriodSeconds = 3.5f;
			L.TerrainZones = BuildBiomeTerrainZones(L.Index, L.ArenaRadiusUU);
			L.GroundColor = FLinearColor(0.022f, 0.026f, 0.055f, 1.0f);
			L.FogColor = FLinearColor(0.012f, 0.014f, 0.030f, 1.0f);
			L.RimColor = AetheriaMindColors::MindCobalt;
			L.AccentColor = AetheriaMindColors::CrispAlabaster;
			L.Mobs = {
				MakeMob("prism_scorpion", TEXT("Призменный Скорпион"), EAetheriaMobKind::Swarm, 255, 710, 25, 100, 210, FLinearColor(0.06f, 0.08f, 0.17f), AetheriaMindColors::CrispAlabaster),
				MakeMob("mirror_guard", TEXT("Зеркальный Страж"), EAetheriaMobKind::Bruiser, 860, 370, 44, 190, 300, FLinearColor(0.07f, 0.10f, 0.22f), AetheriaMindColors::MindCyan),
				MakeMob("illusion_weaver", TEXT("Ткач Иллюзий"), EAetheriaMobKind::Ranged, 340, 460, 34, 105, 1900, FLinearColor(0.05f, 0.07f, 0.18f), AetheriaMindColors::MindViolet)
			};
			L.Boss = MakeMob("architect_spectrum", TEXT("Архитектор Спектр"), EAetheriaMobKind::Boss, 11400, 460, 64, 310, 380, FLinearColor(0.05f, 0.07f, 0.17f), AetheriaMindColors::CrispAlabaster);
			List.Add(L);
		}

		// 07. Sunken Astrolabe Abyss
		{
			FAetheriaLocationDef L;
			L.Index = 6;
			L.Id = FName("sunken_astrolabe");
			L.DisplayName = TEXT("07. Бездна Затонувшего Астролябия");
			L.Subtitle = TEXT("Древний звёздный механизм");
			L.StoryHook = TEXT("Огромные латунные орбиты фокусируют лучи: маги внутри фокус-кольца наносят +50% урона.");
			L.Theme = EAetheriaBiomeTheme::SunkenAstrolabe;
			L.HazardKind = EAetheriaHazardKind::AstrolabeRay;
			L.HazardName = TEXT("Резонанс Астролябия");
			L.HazardDescription = TEXT("Фокус-лучи усиливают урон всех заклинаний отряда на +50%.");
			L.ArenaRadiusUU = 13500.0f;
			L.GracePeriodSeconds = 3.5f;
			L.TerrainZones = BuildBiomeTerrainZones(L.Index, L.ArenaRadiusUU);
			L.GroundColor = FLinearColor(0.012f, 0.024f, 0.045f, 1.0f);
			L.FogColor = FLinearColor(0.008f, 0.015f, 0.025f, 1.0f);
			L.RimColor = AetheriaMindColors::MindIndigo;
			L.AccentColor = AetheriaMindColors::MindCyan;
			L.Mobs = {
				MakeMob("abyss_hunter", TEXT("Охотник Бездны"), EAetheriaMobKind::Swarm, 285, 730, 27, 100, 210, FLinearColor(0.03f, 0.07f, 0.12f), AetheriaMindColors::MindCyan),
				MakeMob("gear_sentinel", TEXT("Часовой Шестерён"), EAetheriaMobKind::Bruiser, 960, 360, 48, 195, 310, FLinearColor(0.07f, 0.09f, 0.13f), FLinearColor(1.0f, 0.55f, 0.12f)),
				MakeMob("depth_siren", TEXT("Сирена Глубин"), EAetheriaMobKind::Ranged, 370, 450, 36, 110, 2000, FLinearColor(0.03f, 0.08f, 0.16f), AetheriaMindColors::MindIndigo)
			};
			L.Boss = MakeMob("leviathan_clockwork", TEXT("Левиафан Механизма"), EAetheriaMobKind::Boss, 13000, 440, 70, 330, 400, FLinearColor(0.04f, 0.07f, 0.12f), FLinearColor(1.0f, 0.55f, 0.12f));
			List.Add(L);
		}

		// 08. Obsidian Titan Forge
		{
			FAetheriaLocationDef L;
			L.Index = 7;
			L.Id = FName("titan_forge");
			L.DisplayName = TEXT("08. Обсидиановая Кузня Титанов");
			L.Subtitle = TEXT("Молот звёзд и расплавленного металла");
			L.StoryHook = TEXT("Небесные наковальни обрушивают ударные волны по всей арене. Выходите из колец рывком (Space) или Блинком (WWE).");
			L.Theme = EAetheriaBiomeTheme::TitanForge;
			L.HazardKind = EAetheriaHazardKind::TitanAnvil;
			L.HazardName = TEXT("Удары Кузни");
			L.HazardDescription = TEXT("Регулярные сейсмические волны от наковален отбрасывают зазевавшихся.");
			L.ArenaRadiusUU = 13500.0f;
			L.GracePeriodSeconds = 3.5f;
			L.TerrainZones = BuildBiomeTerrainZones(L.Index, L.ArenaRadiusUU);
			L.GroundColor = FLinearColor(0.036f, 0.014f, 0.020f, 1.0f);
			L.FogColor = FLinearColor(0.020f, 0.008f, 0.012f, 1.0f);
			L.RimColor = AetheriaMindColors::IgnisPlasma;
			L.AccentColor = FLinearColor(1.0f, 0.35f, 0.08f, 1.0f);
			L.Mobs = {
				MakeMob("slag_berserker", TEXT("Берсерк Шлака"), EAetheriaMobKind::Swarm, 320, 740, 30, 105, 220, FLinearColor(0.11f, 0.03f, 0.04f), AetheriaMindColors::IgnisPlasma, true),
				MakeMob("forged_golem", TEXT("Кованый Голем"), EAetheriaMobKind::Bruiser, 1100, 370, 54, 200, 320, FLinearColor(0.08f, 0.04f, 0.05f), FLinearColor(1.0f, 0.35f, 0.08f)),
				MakeMob("plasma_engineer", TEXT("Плазменный Инженер"), EAetheriaMobKind::Ranged, 400, 440, 39, 110, 1900, FLinearColor(0.12f, 0.04f, 0.07f), AetheriaMindColors::MindCyan)
			};
			L.Boss = MakeMob("smith_hephaestion", TEXT("Кузнец Гефестион"), EAetheriaMobKind::Boss, 14800, 450, 76, 340, 410, FLinearColor(0.08f, 0.025f, 0.03f), AetheriaMindColors::IgnisPlasma);
			List.Add(L);
		}

		// 09. Frozen Time Temple
		{
			FAetheriaLocationDef L;
			L.Index = 8;
			L.Id = FName("frozen_time_temple");
			L.DisplayName = TEXT("09. Храм Остановившегося Времени");
			L.Subtitle = TEXT("Осколки застывших эпох");
			L.StoryHook = TEXT("Искажённые пузыри времени меняют скорость отката умений и движений.");
			L.Theme = EAetheriaBiomeTheme::TimeTemple;
			L.HazardKind = EAetheriaHazardKind::TimeDilation;
			L.HazardName = TEXT("Временные пузыри");
			L.HazardDescription = TEXT("Блуждающие зоны замедляют врагов и ускоряют восстановление заклинаний.");
			L.ArenaRadiusUU = 13500.0f;
			L.GracePeriodSeconds = 3.5f;
			L.TerrainZones = BuildBiomeTerrainZones(L.Index, L.ArenaRadiusUU);
			L.GroundColor = FLinearColor(0.018f, 0.022f, 0.055f, 1.0f);
			L.FogColor = FLinearColor(0.010f, 0.012f, 0.028f, 1.0f);
			L.RimColor = AetheriaMindColors::MindCyan;
			L.AccentColor = AetheriaMindColors::MindViolet;
			L.Mobs = {
				MakeMob("chrono_reaper", TEXT("Хроно-Жнец"), EAetheriaMobKind::Swarm, 355, 770, 34, 100, 220, FLinearColor(0.05f, 0.06f, 0.16f), AetheriaMindColors::MindCyan),
				MakeMob("paradox_guardian", TEXT("Страж Парадокса"), EAetheriaMobKind::Bruiser, 1240, 390, 58, 200, 320, FLinearColor(0.06f, 0.04f, 0.15f), AetheriaMindColors::MindViolet),
				MakeMob("entropy_herald", TEXT("Вестник Энтропии"), EAetheriaMobKind::Ranged, 440, 470, 42, 115, 2000, FLinearColor(0.04f, 0.07f, 0.18f), AetheriaMindColors::MindCobalt)
			};
			L.Boss = MakeMob("chronoarch", TEXT("Хроноарх"), EAetheriaMobKind::Boss, 17000, 470, 82, 340, 410, FLinearColor(0.04f, 0.05f, 0.14f), AetheriaMindColors::MindCyan);
			List.Add(L);
		}

		// 10. Heart of the Chrono-Seal
		{
			FAetheriaLocationDef L;
			L.Index = 9;
			L.Id = FName("chrono_seal_heart");
			L.DisplayName = TEXT("10. Сердце Расколотой Хроно-Печати");
			L.Subtitle = TEXT("Финальный алтарь Первозданной Триады");
			L.StoryHook = TEXT("Первозданный Архимаг сам владеет сферами Q, W и E. Восстановите Печать в решающей битве отряда!");
			L.Theme = EAetheriaBiomeTheme::ChronoSealHeart;
			L.HazardKind = EAetheriaHazardKind::TriadPhase;
			L.HazardName = TEXT("Пульсация Триады");
			L.HazardDescription = TEXT("Алтарь циклически усиливает стихии Q -> W -> E для заклинаний (+35% урона).");
			L.ArenaRadiusUU = 14000.0f;
			L.GracePeriodSeconds = 3.5f;
			L.TerrainZones = BuildBiomeTerrainZones(L.Index, L.ArenaRadiusUU);
			L.GroundColor = FLinearColor(0.015f, 0.015f, 0.030f, 1.0f);
			L.FogColor = FLinearColor(0.008f, 0.008f, 0.018f, 1.0f);
			L.RimColor = AetheriaMindColors::MindViolet;
			L.AccentColor = AetheriaMindColors::MindCyan;
			L.Mobs = {
				MakeMob("chaos_avatar", TEXT("Аватар Хаоса"), EAetheriaMobKind::Swarm, 410, 790, 38, 105, 230, FLinearColor(0.09f, 0.03f, 0.14f), AetheriaMindColors::IgnisPlasma),
				MakeMob("void_archon", TEXT("Архонт Пустоты"), EAetheriaMobKind::Bruiser, 1400, 400, 64, 210, 330, FLinearColor(0.05f, 0.03f, 0.12f), AetheriaMindColors::MindViolet),
				MakeMob("chrono_seraph", TEXT("Хроно-Серафим"), EAetheriaMobKind::Ranged, 490, 490, 46, 120, 2100, FLinearColor(0.04f, 0.08f, 0.17f), AetheriaMindColors::MindCyan)
			};
			L.Boss = MakeMob("primal_archmage", TEXT("Первозданный Архимаг Раскола"), EAetheriaMobKind::Boss, 22000, 490, 90, 350, 420, FLinearColor(0.03f, 0.02f, 0.08f), AetheriaMindColors::MindCyan, false, true);
			List.Add(L);
		}

		return List;
	}
}

const TArray<FAetheriaLocationDef>& UAetheriaBiomeDatabase::GetAllLocations()
{
	static const TArray<FAetheriaLocationDef> Cached = BuildAllLocations();
	return Cached;
}

FAetheriaLocationDef UAetheriaBiomeDatabase::GetLocation(int32 LocationIndex)
{
	const TArray<FAetheriaLocationDef>& All = GetAllLocations();
	const int32 Clamped = FMath::Clamp(LocationIndex, 0, All.Num() - 1);
	return All[Clamped];
}

FAetheriaActTimingConfig UAetheriaBiomeDatabase::GetActTiming(EAetheriaPacingMode Pacing, int32 PlayerCount)
{
	FAetheriaActTimingConfig Cfg;
	const float PartyScale = 1.0f + FMath::Max(0, PlayerCount - 1) * 0.45f;

	if (Pacing == EAetheriaPacingMode::Epic45Min)
	{
		Cfg.EstimatedMinutes = 48;
		Cfg.BeaconCaptureSeconds = 180.0f;
		Cfg.BeaconWaveCount = 6;
		Cfg.ObeliskChargeSeconds = 420.0f;
		Cfg.PylonHPScale = 2.6f * PartyScale;
		Cfg.LegionKillsTarget = FMath::RoundToInt(48.0f * (1.0f + FMath::Max(0, PlayerCount - 1) * 0.30f));
		Cfg.BossHPScale = 2.2f * PartyScale;
	}
	else
	{
		Cfg.EstimatedMinutes = 6;
		Cfg.BeaconCaptureSeconds = 11.0f;
		Cfg.BeaconWaveCount = 2;
		Cfg.ObeliskChargeSeconds = 18.0f;
		Cfg.PylonHPScale = 0.55f * PartyScale;
		Cfg.LegionKillsTarget = FMath::RoundToInt(10.0f * (1.0f + FMath::Max(0, PlayerCount - 1) * 0.25f));
		Cfg.BossHPScale = 0.65f * PartyScale;
	}
	return Cfg;
}

float UAetheriaBiomeDatabase::GroundZAt(const UObject* WorldContextObject, int32 LocationIndex, const FVector2D& Pos2D, float BiasUU)
{
	const float Fallback = SampleTerrainHeightAt(LocationIndex, Pos2D);
	const UWorld* World = GEngine && WorldContextObject ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return Fallback + BiasUU;
	}
	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(AetheriaGroundZ), true);
	static const float TraceUp = 2500.0f;
	static const float TraceDown = 6000.0f;
	const FVector Start(Pos2D.X, Pos2D.Y, Fallback + TraceUp);
	const FVector End(Pos2D.X, Pos2D.Y, Fallback - TraceDown);
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, Params) && Hit.ImpactNormal.Z >= 0.6f)
	{
		return Hit.ImpactPoint.Z + BiasUU;
	}
	return Fallback + BiasUU;
}

float UAetheriaBiomeDatabase::SampleTerrainHeightAt(int32 LocationIndex, const FVector2D& Pos2D)
{
	const FAetheriaLocationDef Loc = GetLocation(LocationIndex);
	float MaxPlateauHeight = 0.0f;
	float MinTrenchElevation = 0.0f;
	float MaxBridgeElevation = -1000.0f;
	bool bOnBridge = false;

	for (const FAetheriaTerrainZone& Zone : Loc.TerrainZones)
	{
		if (Zone.Kind == EAetheriaTerrainZoneKind::ElevatedPlateau)
		{
			const float Dist = FVector2D::Distance(Pos2D, Zone.Center);
			if (Dist <= Zone.RadiusUU)
			{
				MaxPlateauHeight = FMath::Max(MaxPlateauHeight, Zone.ElevationUU);
			}
			else if (Zone.RampWidthUU > 0.0f && Dist < (Zone.RadiusUU + Zone.RampWidthUU))
			{
				const float T = 1.0f - ((Dist - Zone.RadiusUU) / Zone.RampWidthUU);
				const float S = T * T * (3.0f - 2.0f * T);
				MaxPlateauHeight = FMath::Max(MaxPlateauHeight, Zone.ElevationUU * S);
			}
		}
		else if (Zone.Kind == EAetheriaTerrainZoneKind::SunkenTrench)
		{
			const FVector2D Delta = Pos2D - Zone.Center;
			const float Rad = -FMath::DegreesToRadians(Zone.YawDeg);
			const float CosA = FMath::Cos(Rad);
			const float SinA = FMath::Sin(Rad);
			const float LocalX = Delta.X * CosA - Delta.Y * SinA;
			const float LocalY = Delta.X * SinA + Delta.Y * CosA;

			const float HalfL = Zone.LengthUU * 0.5f;
			const float HalfW = Zone.WidthUU * 0.5f;

			const float DistX = FMath::Max(0.0f, FMath::Abs(LocalX) - HalfL);
			const float DistY = FMath::Max(0.0f, FMath::Abs(LocalY) - HalfW);
			const float DistToBox = FMath::Sqrt(DistX * DistX + DistY * DistY);

			if (DistToBox <= 0.0f)
			{
				MinTrenchElevation = FMath::Min(MinTrenchElevation, Zone.ElevationUU);
			}
			else if (Zone.RampWidthUU > 0.0f && DistToBox < Zone.RampWidthUU)
			{
				const float T = 1.0f - (DistToBox / Zone.RampWidthUU);
				const float S = T * T * (3.0f - 2.0f * T);
				MinTrenchElevation = FMath::Min(MinTrenchElevation, Zone.ElevationUU * S);
			}
		}
		else if (Zone.Kind == EAetheriaTerrainZoneKind::ChokepointBridge)
		{
			const FVector2D Delta = Pos2D - Zone.Center;
			const float Rad = -FMath::DegreesToRadians(Zone.YawDeg);
			const float CosA = FMath::Cos(Rad);
			const float SinA = FMath::Sin(Rad);
			const float LocalX = Delta.X * CosA - Delta.Y * SinA;
			const float LocalY = Delta.X * SinA + Delta.Y * CosA;

			const float HalfL = Zone.LengthUU * 0.5f;
			const float HalfW = Zone.WidthUU * 0.5f;

			if (FMath::Abs(LocalX) <= HalfL && FMath::Abs(LocalY) <= HalfW)
			{
				bOnBridge = true;
				MaxBridgeElevation = FMath::Max(MaxBridgeElevation, Zone.ElevationUU);
			}
		}
	}

	float CurrentHeight = 0.0f;
	if (MaxPlateauHeight > 5.0f)
	{
		CurrentHeight = MaxPlateauHeight;
	}
	else if (MinTrenchElevation < -1.0f)
	{
		CurrentHeight = MinTrenchElevation;
	}

	if (bOnBridge)
	{
		CurrentHeight = FMath::Max(CurrentHeight, MaxBridgeElevation);
	}

	return CurrentHeight;
}

bool UAetheriaBiomeDatabase::IsOnHighGround(int32 LocationIndex, const FVector& WorldPos)
{
	return SampleTerrainHeightAt(LocationIndex, FVector2D(WorldPos.X, WorldPos.Y)) >= 50.0f;
}

bool UAetheriaBiomeDatabase::IsInSunkenTrench(int32 LocationIndex, const FVector& WorldPos)
{
	return SampleTerrainHeightAt(LocationIndex, FVector2D(WorldPos.X, WorldPos.Y)) <= -25.0f;
}

bool UAetheriaBiomeDatabase::IsInSanctuary(int32 LocationIndex, const FVector& WorldPos)
{
	const FAetheriaLocationDef Loc = GetLocation(LocationIndex);
	const FVector2D Pos2D(WorldPos.X, WorldPos.Y);
	for (const FAetheriaTerrainZone& Zone : Loc.TerrainZones)
	{
		if (Zone.Kind == EAetheriaTerrainZoneKind::SanctuaryRing)
		{
			if (FVector2D::Distance(Pos2D, Zone.Center) <= Zone.RadiusUU)
			{
				return true;
			}
		}
	}
	return Pos2D.Size() <= 700.0f;
}

bool UAetheriaBiomeDatabase::SegmentHitsCoverParapet(int32 LocationIndex, const FVector2D& Start2D, const FVector2D& End2D, FVector2D& OutHit2D)
{
	const FAetheriaLocationDef Loc = GetLocation(LocationIndex);
	float ClosestHitT = 2.0f;
	bool bHitAny = false;

	const FVector2D SegDir = End2D - Start2D;
	const float SegLength = SegDir.Size();
	if (SegLength < 1e-3f)
	{
		OutHit2D = End2D;
		return false;
	}

	for (const FAetheriaTerrainZone& Zone : Loc.TerrainZones)
	{
		if (Zone.Kind != EAetheriaTerrainZoneKind::CoverParapet)
		{
			continue;
		}

		const float Rad = -FMath::DegreesToRadians(Zone.YawDeg);
		const float CosA = FMath::Cos(Rad);
		const float SinA = FMath::Sin(Rad);

		const FVector2D DeltaS = Start2D - Zone.Center;
		const FVector2D LocalS(DeltaS.X * CosA - DeltaS.Y * SinA, DeltaS.X * SinA + DeltaS.Y * CosA);

		const FVector2D DeltaE = End2D - Zone.Center;
		const FVector2D LocalE(DeltaE.X * CosA - DeltaE.Y * SinA, DeltaE.X * SinA + DeltaE.Y * CosA);

		const FVector2D LocalD = LocalE - LocalS;
		const float HalfL = Zone.LengthUU * 0.5f;
		const float HalfW = Zone.WidthUU * 0.5f;

		// 2D Ray/Segment vs AABB slab test
		float TMin = 0.0f;
		float TMax = 1.0f;
		bool bIntersectSlabs = true;

		// X Slab
		if (FMath::Abs(LocalD.X) < 1e-4f)
		{
			if (LocalS.X < -HalfL || LocalS.X > HalfL)
			{
				bIntersectSlabs = false;
			}
		}
		else
		{
			float T1 = (-HalfL - LocalS.X) / LocalD.X;
			float T2 = (HalfL - LocalS.X) / LocalD.X;
			if (T1 > T2) { const float Temp = T1; T1 = T2; T2 = Temp; }
			TMin = FMath::Max(TMin, T1);
			TMax = FMath::Min(TMax, T2);
			if (TMin > TMax)
			{
				bIntersectSlabs = false;
			}
		}

		// Y Slab
		if (bIntersectSlabs)
		{
			if (FMath::Abs(LocalD.Y) < 1e-4f)
			{
				if (LocalS.Y < -HalfW || LocalS.Y > HalfW)
				{
					bIntersectSlabs = false;
				}
			}
			else
			{
				float T1 = (-HalfW - LocalS.Y) / LocalD.Y;
				float T2 = (HalfW - LocalS.Y) / LocalD.Y;
				if (T1 > T2) { const float Temp = T1; T1 = T2; T2 = Temp; }
				TMin = FMath::Max(TMin, T1);
				TMax = FMath::Min(TMax, T2);
				if (TMin > TMax)
				{
					bIntersectSlabs = false;
				}
			}
		}

		if (bIntersectSlabs && TMin >= 0.0f && TMin <= 1.0f)
		{
			if (TMin < ClosestHitT)
			{
				ClosestHitT = TMin;
				bHitAny = true;
			}
		}
	}

	if (bHitAny)
	{
		OutHit2D = Start2D + SegDir * ClosestHitT;
		return true;
	}

	OutHit2D = End2D;
	return false;
}

FVector2D UAetheriaBiomeDatabase::ResolveCoverAvoidance2D(int32 LocationIndex, const FVector2D& Pos2D, float ActorRadiusUU)
{
	const FAetheriaLocationDef Loc = GetLocation(LocationIndex);
	FVector2D ResolvedPos = Pos2D;

	for (const FAetheriaTerrainZone& Zone : Loc.TerrainZones)
	{
		if (Zone.Kind != EAetheriaTerrainZoneKind::CoverParapet)
		{
			continue;
		}

		const float Rad = -FMath::DegreesToRadians(Zone.YawDeg);
		const float CosA = FMath::Cos(Rad);
		const float SinA = FMath::Sin(Rad);

		const FVector2D Delta = ResolvedPos - Zone.Center;
		FVector2D LocalP(Delta.X * CosA - Delta.Y * SinA, Delta.X * SinA + Delta.Y * CosA);

		const float HalfL = Zone.LengthUU * 0.5f + ActorRadiusUU;
		const float HalfW = Zone.WidthUU * 0.5f + ActorRadiusUU;

		if (FMath::Abs(LocalP.X) < HalfL && FMath::Abs(LocalP.Y) < HalfW)
		{
			const float OverlapX = HalfL - FMath::Abs(LocalP.X);
			const float OverlapY = HalfW - FMath::Abs(LocalP.Y);

			if (OverlapX < OverlapY)
			{
				LocalP.X = (LocalP.X >= 0.0f) ? HalfL : -HalfL;
			}
			else
			{
				LocalP.Y = (LocalP.Y >= 0.0f) ? HalfW : -HalfW;
			}

			const float InvRad = FMath::DegreesToRadians(Zone.YawDeg);
			const float InvCos = FMath::Cos(InvRad);
			const float InvSin = FMath::Sin(InvRad);
			ResolvedPos = Zone.Center + FVector2D(LocalP.X * InvCos - LocalP.Y * InvSin, LocalP.X * InvSin + LocalP.Y * InvCos);
		}
	}

	return ResolvedPos;
}

FVector UAetheriaBiomeDatabase::FindNearestPlateauLocation(int32 LocationIndex, const FVector& NearPos)
{
	const FAetheriaLocationDef Loc = GetLocation(LocationIndex);
	const FVector2D Near2D(NearPos.X, NearPos.Y);
	float BestDistSq = 1e18f;
	FVector BestPos = NearPos;
	bool bFound = false;

	for (const FAetheriaTerrainZone& Zone : Loc.TerrainZones)
	{
		if (Zone.Kind == EAetheriaTerrainZoneKind::ElevatedPlateau)
		{
			const float DistSq = FVector2D::DistSquared(Near2D, Zone.Center);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				const float Z = SampleTerrainHeightAt(LocationIndex, Zone.Center);
				BestPos = FVector(Zone.Center.X, Zone.Center.Y, Z);
				bFound = true;
			}
		}
	}

	if (!bFound)
	{
		BestPos.Z = SampleTerrainHeightAt(LocationIndex, Near2D);
	}

	return BestPos;
}
