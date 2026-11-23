#pragma once

#include "CoreMinimal.h"
#include "AetheriaTypes.generated.h"

UENUM(BlueprintType)
enum class EAetheriaOrb : uint8
{
	Ignis UMETA(DisplayName = "Q - Ignis"),
	Tempest UMETA(DisplayName = "W - Tempest"),
	Graviton UMETA(DisplayName = "E - Graviton")
};

UENUM(BlueprintType)
enum class EAetheriaCastMode : uint8
{
	InvokeThenAim UMETA(DisplayName = "Invoke to Slot (D/F) -> Aim -> LMB"),
	InstantOnCombo UMETA(DisplayName = "Instant Cast on 3-Orb Combo")
};

UENUM(BlueprintType)
enum class EAetheriaPacingMode : uint8
{
	Epic45Min UMETA(DisplayName = "Epic Campaign (40-60 min / Location)"),
	DemoBlitz UMETA(DisplayName = "Demo Blitz (5-7 min / Location)")
};

UENUM(BlueprintType)
enum class EAetheriaSpellId : uint8
{
	None UMETA(DisplayName = "None"),
	Fireball UMETA(DisplayName = "QQQ - Solar Cataclysm Fireball"),
	BlackHole UMETA(DisplayName = "EEE - Event Horizon Black Hole"),
	Chronosphere UMETA(DisplayName = "WWW - Chronosphere"),
	ChaosMeteor UMETA(DisplayName = "QQE - Chaos Meteor"),
	ChainLightning UMETA(DisplayName = "QWW - Arcane Chain Lightning"),
	SupernovaBeam UMETA(DisplayName = "QQW - Solar Supernova Beam"),
	ChronoBlink UMETA(DisplayName = "WWE - Chrono-Blink Hologram"),
	GravityCataclysm UMETA(DisplayName = "QEE - Gravity Cataclysm"),
	PrismWall UMETA(DisplayName = "WEE - Cryo-Void Prism Wall"),
	PrimalOverload UMETA(DisplayName = "QWE - Primal Triad Overload"),
	Macropyre UMETA(DisplayName = "QEQ - Macropyre Magma Corridor"),
	PlasmaField UMETA(DisplayName = "WQW - Plasma Field Lightning Ring"),
	CataclysmNova UMETA(DisplayName = "EQE - Cataclysmic Supernova Explosion"),
	SunStrike UMETA(DisplayName = "QWQ - Helios Sun Strike"),
	EyeOfTheStorm UMETA(DisplayName = "WEW - Eye of the Storm"),
	RequiemBlast UMETA(DisplayName = "EWE - Requiem of Shattered Souls")
};

UENUM(BlueprintType)
enum class EAetheriaTargetShape : uint8
{
	Direction UMETA(DisplayName = "Direction Vector"),
	PointAoE UMETA(DisplayName = "Ground AoE Circle"),
	LineWall UMETA(DisplayName = "Perpendicular Barrier Line"),
	SelfNova UMETA(DisplayName = "Centered Nova")
};

UENUM(BlueprintType)
enum class EAetheriaMobKind : uint8
{
	Swarm UMETA(DisplayName = "Swarm"),
	Bruiser UMETA(DisplayName = "Bruiser"),
	Ranged UMETA(DisplayName = "Ranged"),
	Boss UMETA(DisplayName = "Boss")
};

UENUM(BlueprintType)
enum class EAetheriaBiomeTheme : uint8
{
	Bastion UMETA(DisplayName = "01 Shattered Aether Bastion"),
	Caldera UMETA(DisplayName = "02 Ash Caldera of Ignis"),
	Glacier UMETA(DisplayName = "03 Crystal Glacier of Chronos"),
	GravityGardens UMETA(DisplayName = "04 Gravity Gardens of Void"),
	StormNecropolis UMETA(DisplayName = "05 Storm Necropolis"),
	MirrorLabyrinth UMETA(DisplayName = "06 Mirror Labyrinth"),
	SunkenAstrolabe UMETA(DisplayName = "07 Sunken Astrolabe Abyss"),
	TitanForge UMETA(DisplayName = "08 Obsidian Titan Forge"),
	TimeTemple UMETA(DisplayName = "09 Frozen Time Temple"),
	ChronoSealHeart UMETA(DisplayName = "10 Heart of the Chrono-Seal")
};

UENUM(BlueprintType)
enum class EAetheriaHazardKind : uint8
{
	EtherGeyser,
	MagmaTide,
	FrostDrift,
	GravWell,
	StormArc,
	MirrorPylon,
	AstrolabeRay,
	TitanAnvil,
	TimeDilation,
	TriadPhase
};

UENUM(BlueprintType)
enum class EAetheriaWorldId : uint8
{
	Bastion UMETA(DisplayName = "Bastion of Aether"),
	RuinsTemple UMETA(DisplayName = "Monolith Ruins Temple"),
	VoidRealm UMETA(DisplayName = "Void Threshold"),
	BossSanctum UMETA(DisplayName = "Archon Sanctum")
};

UENUM(BlueprintType)
enum class EAetheriaObjectiveKind : uint8
{
	Beacon UMETA(DisplayName = "Runic Beacon"),
	Obelisk UMETA(DisplayName = "Ether Obelisk"),
	Pylon UMETA(DisplayName = "Shielded Pylon"),
	Portal UMETA(DisplayName = "Astral Portal")
};

UENUM(BlueprintType)
enum class EAetheriaGraphicsPreset : uint8
{
	Optimal3070 = 0 UMETA(DisplayName = "RTX 3070 Optimal"),
	UltraCinema = 1 UMETA(DisplayName = "Ultra Cinema"),
	Esports144 = 2 UMETA(DisplayName = "144 FPS Performance")
};

UENUM(BlueprintType)
enum class EAetheriaTerrainZoneKind : uint8
{
	ElevatedPlateau UMETA(DisplayName = "Elevated Plateau (+15% Damage)"),
	SunkenTrench UMETA(DisplayName = "Sunken Trench (-18% Walk Speed, +20% Shock)"),
	ChokepointBridge UMETA(DisplayName = "Chokepoint Bridge"),
	CoverParapet UMETA(DisplayName = "Cover Parapet (Blocks Enemy Shots / Wall Slam)"),
	SanctuaryRing UMETA(DisplayName = "Sanctuary Ring (+4 HP/s)")
};

UENUM(BlueprintType)
enum class EAetheriaWavePhase : uint8
{
	SanctuaryGrace UMETA(DisplayName = "Sanctuary Grace"),
	ScoutSkirmish UMETA(DisplayName = "Scout Skirmish"),
	TacticalEscalation UMETA(DisplayName = "Tactical Escalation"),
	ClimaxSiege UMETA(DisplayName = "Climax Siege"),
	Breather UMETA(DisplayName = "Breather")
};

UENUM(BlueprintType)
enum class EAetheriaAmbushKind : uint8
{
	None UMETA(DisplayName = "None"),
	RiftBreach UMETA(DisplayName = "Flanking Rift Breach"),
	SubterraneanEruption UMETA(DisplayName = "Subterranean Eruption"),
	DormantConstructAwakening UMETA(DisplayName = "Dormant Construct Awakening"),
	ObjectiveTrapCounter UMETA(DisplayName = "Objective Seal Counter-Ambush")
};

USTRUCT(BlueprintType)
struct FAetheriaTerrainZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	EAetheriaTerrainZoneKind Kind = EAetheriaTerrainZoneKind::ElevatedPlateau;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	FVector2D Center = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	float RadiusUU = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	float WidthUU = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	float LengthUU = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	float YawDeg = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	float ElevationUU = 95.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
	float RampWidthUU = 380.0f;
};

USTRUCT(BlueprintType)
struct FAetheriaPendingAmbush
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ambush")
	EAetheriaAmbushKind Kind = EAetheriaAmbushKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "Ambush")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ambush")
	float TelegraphRemaining = 0.85f;

	UPROPERTY(BlueprintReadOnly, Category = "Ambush")
	int32 SpawnCount = 3;

	UPROPERTY(BlueprintReadOnly, Category = "Ambush")
	int32 MobArchetypeIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ambush")
	bool bSpawnElite = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ambush")
	FString AlertText;
};

USTRUCT(BlueprintType)
struct FAetheriaCharacterCustomization
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	int32 BodyArchetypeIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	int32 HeadgearIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	int32 StaffIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	int32 FamiliarIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	int32 BackIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	int32 RobeDyeIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	int32 TrimMetalIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	int32 RuneGlowIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	float BodyScale = 1.0f;
};

USTRUCT(BlueprintType)
struct FAetheriaSpellDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	EAetheriaSpellId SpellId = EAetheriaSpellId::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	FString ComboKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	TArray<EAetheriaOrb> RequiredOrbs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	FString EnglishName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	FString SynergyHint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	EAetheriaTargetShape TargetShape = EAetheriaTargetShape::Direction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	float Cooldown = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	float ManaCost = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	float RangeUU = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	float RadiusUU = 480.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	float Duration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	float BaseDamage = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	float CastDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	FLinearColor PrimaryColor = FLinearColor(0.0f, 0.96f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spell")
	FLinearColor AccentColor = FLinearColor(0.48f, 0.18f, 0.97f, 1.0f);
};

USTRUCT(BlueprintType)
struct FAetheriaMobArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mob")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mob")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mob")
	EAetheriaMobKind Kind = EAetheriaMobKind::Swarm;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mob")
	float MaxHP = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mob")
	float MoveSpeedUU = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mob")
	float Damage = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mob")
	float CollisionRadiusUU = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mob")
	float AttackRangeUU = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mob")
	FLinearColor BodyColor = FLinearColor(0.12f, 0.14f, 0.22f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mob")
	FLinearColor AccentColor = FLinearColor(0.48f, 0.18f, 0.97f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mob")
	bool bExplodeOnDeath = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mob")
	bool bCastsOrbs = false;
};

USTRUCT(BlueprintType)
struct FAetheriaLocationDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	int32 Index = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FString Subtitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FString StoryHook;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	EAetheriaBiomeTheme Theme = EAetheriaBiomeTheme::Bastion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	EAetheriaHazardKind HazardKind = EAetheriaHazardKind::EtherGeyser;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FString HazardName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FString HazardDescription;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	float ArenaRadiusUU = 12000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	float GracePeriodSeconds = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	TArray<FAetheriaTerrainZone> TerrainZones;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FLinearColor GroundColor = FLinearColor(0.05f, 0.06f, 0.10f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FLinearColor FogColor = FLinearColor(0.035f, 0.035f, 0.06f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FLinearColor RimColor = FLinearColor(0.29f, 0.23f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FLinearColor AccentColor = FLinearColor(0.0f, 0.96f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	TArray<FAetheriaMobArchetype> Mobs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Location")
	FAetheriaMobArchetype Boss;
};

USTRUCT(BlueprintType)
struct FAetheriaObjectiveInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	int32 ObjectiveId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	EAetheriaObjectiveKind Kind = EAetheriaObjectiveKind::Beacon;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	float RadiusUU = 550.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	float Progress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	float HP = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	float MaxHP = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	bool bCompleted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	bool bActive = false;
};

USTRUCT(BlueprintType)
struct FAetheriaActTimingConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	int32 EstimatedMinutes = 48;

	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float BeaconCaptureSeconds = 180.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	int32 BeaconWaveCount = 6;

	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float ObeliskChargeSeconds = 420.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float PylonHPScale = 2.6f;

	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	int32 LegionKillsTarget = 48;

	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float BossHPScale = 2.2f;
};

namespace AetheriaMindColors
{
	inline const FLinearColor ObsidianVoid(0.035f, 0.035f, 0.050f, 1.0f);
	inline const FLinearColor SlateGlass(0.03f, 0.035f, 0.075f, 0.78f);
	inline const FLinearColor MindViolet(0.20f, 0.03f, 0.92f, 1.0f);   // #7B2FF7 linear approx
	inline const FLinearColor MindIndigo(0.07f, 0.04f, 1.0f, 1.0f);    // #4B3BFF
	inline const FLinearColor MindCobalt(0.015f, 0.28f, 1.0f, 1.0f);   // #1E90FF
	inline const FLinearColor MindCyan(0.0f, 0.92f, 1.0f, 1.0f);       // #00F5FF
	inline const FLinearColor IgnisPlasma(1.0f, 0.09f, 0.015f, 1.0f);  // #FF5520
	inline const FLinearColor CrispAlabaster(0.90f, 0.92f, 0.97f, 1.0f);

	inline FLinearColor GetOrbColor(EAetheriaOrb Orb)
	{
		switch (Orb)
		{
		case EAetheriaOrb::Ignis: return IgnisPlasma;
		case EAetheriaOrb::Tempest: return MindCyan;
		case EAetheriaOrb::Graviton: return MindViolet;
		default: return CrispAlabaster;
		}
	}

	inline FString GetOrbLetter(EAetheriaOrb Orb)
	{
		switch (Orb)
		{
		case EAetheriaOrb::Ignis: return TEXT("Q");
		case EAetheriaOrb::Tempest: return TEXT("W");
		case EAetheriaOrb::Graviton: return TEXT("E");
		default: return TEXT("?");
		}
	}

	inline FLinearColor GetPartyColor(int32 Index)
	{
		static const FLinearColor Colors[6] = {
			MindCyan,
			MindViolet,
			IgnisPlasma,
			MindCobalt,
			FLinearColor(0.03f, 0.72f, 0.20f, 1.0f),
			FLinearColor(1.0f, 0.42f, 0.02f, 1.0f)
		};
		return Colors[FMath::Abs(Index) % 6];
	}

	inline FLinearColor GetRobeDyeColor(int32 Index)
	{
		static const FLinearColor Dyes[8] = {
			FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),       // 0: Original Authentic Texture
			FLinearColor(0.045f, 0.075f, 0.095f, 1.0f), // 1: Hollow Slate-Teal (#141E26)
			FLinearColor(0.035f, 0.036f, 0.048f, 1.0f), // 2: Abyss Obsidian
			FLinearColor(0.120f, 0.035f, 0.042f, 1.0f), // 3: Blood Cathedral
			FLinearColor(0.035f, 0.075f, 0.060f, 1.0f), // 4: Lich Emerald
			FLinearColor(0.140f, 0.120f, 0.095f, 1.0f), // 5: Ashen Bone
			FLinearColor(0.040f, 0.042f, 0.120f, 1.0f), // 6: Midnight Indigo
			FLinearColor(0.120f, 0.055f, 0.035f, 1.0f)  // 7: Cinder Amber
		};
		return Dyes[FMath::Abs(Index) % 8];
	}

	inline FLinearColor GetTrimMetalColor(int32 Index)
	{
		static const FLinearColor Metals[4] = {
			FLinearColor(0.24f, 0.25f, 0.28f, 1.0f), // 0: Aged Steel
			FLinearColor(0.36f, 0.26f, 0.12f, 1.0f), // 1: Antique Bronze
			FLinearColor(0.08f, 0.085f, 0.095f, 1.0f), // 2: Blackened Iron
			FLinearColor(0.55f, 0.42f, 0.16f, 1.0f)  // 3: Pale Gold
		};
		return Metals[FMath::Abs(Index) % 4];
	}

	inline FLinearColor GetRuneGlowColor(int32 Index)
	{
		static const FLinearColor Glows[4] = {
			FLinearColor(0.92f, 0.74f, 0.46f, 1.0f), // 0: Warm Torchlight Gold (#EBBF75)
			MindCyan,                                 // 1: Mind Cyan (#00F5FF)
			IgnisPlasma,                              // 2: Ignis Ember (#FF5520)
			FLinearColor(0.32f, 0.06f, 0.96f, 1.0f)  // 3: Void Amethyst (#8B3DFF)
		};
		return Glows[FMath::Abs(Index) % 4];
	}
}
