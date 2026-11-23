#include "AetheriaGameMode.h"
#include "UI/SAetheriaLoadingScreen.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "AetheriaWorldDirector.h"
#include "AetheriaSanctumGate.h"
#include "AetheriaPackVFX.h"
#include "AetheriaGameState.h"
#include "AetheriaPlayerState.h"
#include "AetheriaPlayerController.h"
#include "AetheriaWizardCharacter.h"
#include "AetheriaAnimInstance.h"
#include "AetheriaMobCharacter.h"
#include "AetheriaBotAIController.h"
#include "AetheriaSlateHUD.h"
#include "AetheriaBiomeBuilder.h"
#include "Components/PrimitiveComponent.h"
#include "AetheriaBiomeDatabase.h"
#include "AetheriaSpellActors.h"
#include "AetheriaMaterialLibrary.h"
#include "AetheriaSaveGame.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "GameFramework/Character.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/WorldSettings.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMeshSocket.h"

#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/WorldSettings.h"
#include "Components/LightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/LightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"

AAetheriaGameMode::AAetheriaGameMode()
{
	// Hard-references every dynamically loaded asset for the cooker.
	static ConstructorHelpers::FObjectFinder<UAetheriaCookRefs> AetheriaCookRefsObj(TEXT("/Game/AetheriaCookRefs.AetheriaCookRefs"));
	if (AetheriaCookRefsObj.Succeeded())
	{
		CookRefs = AetheriaCookRefsObj.Object;
	}

	GameStateClass = AAetheriaGameState::StaticClass();
	PlayerStateClass = AAetheriaPlayerState::StaticClass();
	PlayerControllerClass = AAetheriaPlayerController::StaticClass();
	DefaultPawnClass = AAetheriaWizardCharacter::StaticClass();
	HUDClass = AAetheriaHUD::StaticClass();
	bUseSeamlessTravel = false;
	MaxPlayers = 6;
}

namespace
{
	EAetheriaWorldId GAetheriaPendingWorld = EAetheriaWorldId::Bastion;
	bool GAetheriaSkipMainMenu = false;
	int32 GAetheriaSmokePhase = 0;
static int32 GAetheriaStaffShotPhase = 0;
}


namespace
{
	const FName GBastionDressingTag(TEXT("AetheriaBastionDressing"));

	UStaticMesh* LoadBastionMesh(const TCHAR* ShortName)
	{
		const FString Candidates[] = {
			FString::Printf(TEXT("/Game/Props/%s.%s"), ShortName, ShortName),
			FString::Printf(TEXT("/Game/Props/%s/%s.%s"), ShortName, ShortName, ShortName),
		};
		for (const FString& Path : Candidates)
		{
			if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return Mesh;
			}
		}
		return nullptr;
	}

	AStaticMeshActor* SpawnBastionMesh(UWorld* World, UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot, const FVector& Scale)
	{
		if (!World || !Mesh) return nullptr;
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Loc, Rot, Sp);
		if (!Actor) return nullptr;
		Actor->Tags.Add(GBastionDressingTag);
		if (UStaticMeshComponent* SMC = Actor->GetStaticMeshComponent())
		{
			SMC->SetMobility(EComponentMobility::Movable);
			SMC->SetStaticMesh(Mesh);
			SMC->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		}
		Actor->SetActorScale3D(Scale);
		return Actor;
	}

	void SpawnAetherBastionDressing(UWorld* World)
	{
		// 04:29: procedural props/walls rejected. Artist layouts come from WorldDirector.
		if (!World) return;
		return;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(GBastionDressingTag))
			{
				return; // already dressed (BeginPlay retriggers presentation)
			}
		}

		// Godot DungeonGenerator._place_braziers: (X, Y-up, Z-forward) m -> UE (X, Z, Y) * 100
		// Six shadowed key lights, twelve fill lights. Warm torch color (1.0, 0.52, 0.18).
		struct FBrazierSpec { float X, Y, Height, Intensity, Radius; bool bShadow; };
		const FBrazierSpec Braziers[] = {
			{-650.f,  1850.f, 148.f, 56.f, 1450.f, true},
			{ 650.f,  1850.f, 148.f, 56.f, 1450.f, true},
			{-1600.f,    0.f, 148.f, 60.f, 1500.f, true},
			{ 1600.f,    0.f, 148.f, 60.f, 1500.f, true},
			{-720.f, -2700.f, 148.f, 62.f, 1550.f, true},
			{ 550.f, -5100.f, 198.f, 64.f, 1600.f, true},
			{-1400.f, 3600.f, 148.f, 32.f, 1200.f, false},
			{ 1400.f, 3600.f, 148.f, 32.f, 1200.f, false},
			{-680.f,  1200.f, 148.f, 30.f, 1150.f, false},
			{ 680.f,  1200.f, 148.f, 30.f, 1150.f, false},
			{-1500.f,-1100.f, 148.f, 34.f, 1250.f, false},
			{ 1500.f,-1100.f, 148.f, 34.f, 1250.f, false},
			{-1500.f, 1100.f, 148.f, 34.f, 1250.f, false},
			{ 1500.f, 1100.f, 148.f, 34.f, 1250.f, false},
			{-2700.f,-1400.f, 148.f, 36.f, 1300.f, false},
			{-2700.f, 1400.f, 148.f, 36.f, 1300.f, false},
			{ 2700.f,-1400.f, 148.f, 36.f, 1300.f, false},
			{ 2700.f, 1400.f, 148.f, 36.f, 1300.f, false},
			{ 720.f, -2700.f, 148.f, 36.f, 1250.f, false},
			{-550.f, -5100.f, 198.f, 36.f, 1250.f, false},
		};

		UStaticMesh* BrazierMesh = LoadBastionMesh(TEXT("SM_PropBrazier"));
		const FLinearColor TorchColor(1.0f, 0.52f, 0.18f);

		for (const FBrazierSpec& Spec : Braziers)
		{
			FActorSpawnParameters Sp;
			Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector LightLoc(Spec.X, Spec.Y, Spec.Height);
			if (APointLight* PL = World->SpawnActor<APointLight>(LightLoc, FRotator::ZeroRotator, Sp))
			{
				PL->Tags.Add(GBastionDressingTag);
				if (UPointLightComponent* C = Cast<UPointLightComponent>(PL->GetLightComponent()))
				{
					C->SetMobility(EComponentMobility::Movable);
					C->SetIntensityUnits(ELightUnits::Candelas);
					C->SetLightColor(TorchColor);
					// Inverse-square candelas: ~moon 28 lux is drowned unless I is thousands
					C->SetIntensity(Spec.bShadow ? 12000.0f : 7000.0f);
					C->SetAttenuationRadius(Spec.Radius);
					C->SetCastShadows(Spec.bShadow);
					C->SetVolumetricScatteringIntensity(Spec.bShadow ? 1.8f : 1.0f);
					C->SetSourceRadius(20.0f);
					C->SetSpecularScale(0.85f);
				}
			}
			SpawnBastionMesh(World, BrazierMesh, FVector(Spec.X, Spec.Y, 0.0f), FRotator::ZeroRotator, FVector(1.0f));
		}

		UStaticMesh* ArchMesh = LoadBastionMesh(TEXT("SM_PropArch"));
		UStaticMesh* GateMesh = LoadBastionMesh(TEXT("SM_PropIronGate"));
		UStaticMesh* PillarMesh = LoadBastionMesh(TEXT("SM_PropPillar"));
		// Nave / north approach (in front of the default -52/-45 camera look)
		SpawnBastionMesh(World, ArchMesh, FVector(900.f, -900.f, 0.f), FRotator(0.f, 45.f, 0.f), FVector(1.0f));
		SpawnBastionMesh(World, GateMesh, FVector(1100.f, -1100.f, 0.f), FRotator(0.f, 45.f, 0.f), FVector(1.0f));
		SpawnBastionMesh(World, ArchMesh, FVector(0.f, 1600.f, 0.f), FRotator(0.f, 90.f, 0.f), FVector(1.0f));
		SpawnBastionMesh(World, PillarMesh, FVector(-1100.f, 900.f, 0.f), FRotator::ZeroRotator, FVector(1.0f));
		SpawnBastionMesh(World, PillarMesh, FVector(1100.f, 900.f, 0.f), FRotator::ZeroRotator, FVector(1.0f));
		SpawnBastionMesh(World, PillarMesh, FVector(-1100.f, -900.f, 0.f), FRotator::ZeroRotator, FVector(1.0f));
		SpawnBastionMesh(World, PillarMesh, FVector(1100.f, -900.f, 0.f), FRotator::ZeroRotator, FVector(1.0f));

		// Soft overhead fill + warm key so Gideon reads from the Dota-style camera.
		{
			FActorSpawnParameters Sp;
			Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (APointLight* Fill = World->SpawnActor<APointLight>(FVector(0.f, 0.f, 720.f), FRotator::ZeroRotator, Sp))
			{
				Fill->Tags.Add(GBastionDressingTag);
				if (UPointLightComponent* C = Cast<UPointLightComponent>(Fill->GetLightComponent()))
				{
					C->SetMobility(EComponentMobility::Movable);
					C->SetIntensityUnits(ELightUnits::Candelas);
					C->SetLightColor(FLinearColor(0.82f, 0.88f, 1.0f));
					C->SetIntensity(20000.0f);
					C->SetAttenuationRadius(4800.0f);
					C->SetCastShadows(false);
					C->SetSourceRadius(120.0f);
					C->SetVolumetricScatteringIntensity(0.35f);
				}
			}
			if (APointLight* Key = World->SpawnActor<APointLight>(FVector(700.f, -500.f, 380.f), FRotator::ZeroRotator, Sp))
			{
				Key->Tags.Add(GBastionDressingTag);
				if (UPointLightComponent* C = Cast<UPointLightComponent>(Key->GetLightComponent()))
				{
					C->SetMobility(EComponentMobility::Movable);
					C->SetIntensityUnits(ELightUnits::Candelas);
					C->SetLightColor(FLinearColor(1.0f, 0.72f, 0.42f));
					C->SetIntensity(14000.0f);
					C->SetAttenuationRadius(2800.0f);
					C->SetCastShadows(false);
					C->SetSourceRadius(60.0f);
					C->SetVolumetricScatteringIntensity(0.8f);
				}
			}
		}

		auto LoadSM = [](const TCHAR* Pkg) -> UStaticMesh*
		{
			const FString Obj = FString::Printf(TEXT("%s.%s"), Pkg, *FPackageName::GetShortName(Pkg));
			return LoadObject<UStaticMesh>(nullptr, *Obj, nullptr, LOAD_NoWarn | LOAD_Quiet);
		};

		UStaticMesh* GothicBuilding = LoadSM(TEXT("/Game/Gothic_Environment/Meshes/Gothic_buildings/SM_Gothic_building_09"));
		UStaticMesh* JungleArch = LoadSM(TEXT("/Game/ParagonProps/Monolith/Ruins/Meshes/JungleArch_01A"));
		UStaticMesh* JungleWall = LoadSM(TEXT("/Game/ParagonProps/Monolith/Ruins/Meshes/JungleWall_01A"));
		UStaticMesh* JungleWallB = LoadSM(TEXT("/Game/ParagonProps/Monolith/Ruins/Meshes/JungleWall_02A"));
		UStaticMesh* JunglePillar = LoadSM(TEXT("/Game/ParagonProps/Monolith/Ruins/Meshes/JunglePillarBlock_01A"));
		UStaticMesh* DamagedStatue = LoadSM(TEXT("/Game/ParagonProps/Monolith/Ruins/Meshes/MonoStatue_Damaged"));
		UStaticMesh* EvilGate = LoadSM(TEXT("/Game/ParagonProps/Monolith/Dusk/Meshes/Evil_Gate_A"));
		UStaticMesh* DawnGate = LoadSM(TEXT("/Game/ParagonProps/Monolith/Dawn/Meshes/Dawn_Gate"));
		UStaticMesh* DawnPillar = LoadSM(TEXT("/Game/ParagonProps/Monolith/Dawn/Meshes/DawnSide_Pillar_Large"));
		UStaticMesh* NordicRock = LoadSM(TEXT("/Game/ParagonProps/Monolith/Rocks/Meshes/SM_RockNordic_03"));
		UStaticMesh* Hadrian = LoadSM(TEXT("/Game/ParagonProps/Agora/Rocks/Meshes/SM_HadriansWall"));
		const FVector RuinScale(0.50f);
		const FVector WallScale(0.45f);
		const FVector GothicScale(0.80f);

		// Camera sits near (-936, 936). Keep the near-camera quadrant (-X,+Y) clear
		// so large kits do not swallow Gideon. Look direction is +X -Y.
		if (GothicBuilding)
		{
			SpawnBastionMesh(World, GothicBuilding, FVector(1600.f, -1250.f, 0.f), FRotator(0.f, 35.f, 0.f), GothicScale);
			SpawnBastionMesh(World, GothicBuilding, FVector(-800.f, -1550.f, 0.f), FRotator(0.f, -20.f, 0.f), GothicScale);
		}
		if (JungleArch)
		{
			SpawnBastionMesh(World, JungleArch, FVector(1150.f, -1150.f, 0.f), FRotator(0.f, 45.f, 0.f), RuinScale);
			SpawnBastionMesh(World, JungleArch, FVector(-900.f, -1500.f, 0.f), FRotator(0.f, 0.f, 0.f), RuinScale);
			SpawnBastionMesh(World, JungleArch, FVector(1650.f, 250.f, 0.f), FRotator(0.f, 90.f, 0.f), RuinScale);
		}
		if (EvilGate)
		{
			SpawnBastionMesh(World, EvilGate, FVector(400.f, -2000.f, 0.f), FRotator(0.f, 45.f, 0.f), FVector(0.40f));
		}
		else if (DawnGate)
		{
			SpawnBastionMesh(World, DawnGate, FVector(400.f, -2000.f, 0.f), FRotator(0.f, 45.f, 0.f), FVector(0.40f));
		}
		if (JungleWall)
		{
			SpawnBastionMesh(World, JungleWall, FVector(1750.f, -500.f, 0.f), FRotator(0.f, 90.f, 0.f), WallScale);
			SpawnBastionMesh(World, JungleWall, FVector(1750.f, 400.f, 0.f), FRotator(0.f, 90.f, 0.f), WallScale);
			SpawnBastionMesh(World, JungleWall, FVector(-400.f, -1750.f, 0.f), FRotator::ZeroRotator, WallScale);
			SpawnBastionMesh(World, JungleWall, FVector(600.f, -1750.f, 0.f), FRotator::ZeroRotator, WallScale);
			SpawnBastionMesh(World, JungleWall, FVector(-1600.f, -900.f, 0.f), FRotator(0.f, 90.f, 0.f), WallScale);
		}
		if (JungleWallB)
		{
			SpawnBastionMesh(World, JungleWallB, FVector(200.f, 1750.f, 0.f), FRotator::ZeroRotator, WallScale);
		}
		if (JunglePillar)
		{
			SpawnBastionMesh(World, JunglePillar, FVector(-1400.f, -1400.f, 0.f), FRotator::ZeroRotator, RuinScale);
			SpawnBastionMesh(World, JunglePillar, FVector(1400.f, -1400.f, 0.f), FRotator::ZeroRotator, RuinScale);
			SpawnBastionMesh(World, JunglePillar, FVector(1400.f, 1400.f, 0.f), FRotator::ZeroRotator, RuinScale);
		}
		if (DawnPillar)
		{
			SpawnBastionMesh(World, DawnPillar, FVector(700.f, -1550.f, 0.f), FRotator::ZeroRotator, FVector(0.45f));
			SpawnBastionMesh(World, DawnPillar, FVector(1550.f, -700.f, 0.f), FRotator::ZeroRotator, FVector(0.45f));
		}
		if (DamagedStatue)
		{
			SpawnBastionMesh(World, DamagedStatue, FVector(750.f, -650.f, 0.f), FRotator(0.f, -30.f, 0.f), FVector(0.65f));
			SpawnBastionMesh(World, DamagedStatue, FVector(-550.f, -850.f, 0.f), FRotator(0.f, 40.f, 0.f), FVector(0.65f));
		}
		// Skip SM_HadriansWall: its 8K albedo is too heavy for the 4060 / 16 GB budget.
		if (NordicRock)
		{
			SpawnBastionMesh(World, NordicRock, FVector(-1100.f, -700.f, 0.f), FRotator(0.f, 15.f, 0.f), FVector(0.75f));
			SpawnBastionMesh(World, NordicRock, FVector(1250.f, 450.f, 0.f), FRotator(0.f, 70.f, 0.f), FVector(0.65f));
		}

		// Looping torch fire (Stylish Fire VFX). Do not rewrite the spell Niagara track here.
		UNiagaraSystem* FireNS = LoadObject<UNiagaraSystem>(
			nullptr,
			TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_1.NS_Stylish_Fire_1"),
			nullptr,
			LOAD_NoWarn | LOAD_Quiet);
		if (FireNS)
		{
			for (const FBrazierSpec& Spec : Braziers)
			{
				if (UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					World, FireNS, FVector(Spec.X, Spec.Y, Spec.Height), FRotator::ZeroRotator,
					FVector(2.40f), false, true, ENCPoolMethod::None, true))
				{
					if (AActor* Owner = NC->GetOwner())
					{
						Owner->Tags.Add(GBastionDressingTag);
					}
				}
			}
		}
	}
}


	void HideProceduralBiomeVisuals(AAetheriaBiomeBuilder* Builder)
	{
		if (!IsValid(Builder)) return;
		Builder->SetActorHiddenInGame(true);
		Builder->SetActorEnableCollision(false);
		TArray<UPrimitiveComponent*> Prim;
		Builder->GetComponents<UPrimitiveComponent>(Prim);
		for (UPrimitiveComponent* C : Prim)
		{
			C->SetVisibility(false, true);
			C->SetHiddenInGame(true, true);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

void AAetheriaGameMode::BeginPlay()
{
	Super::BeginPlay();
	FAetheriaLoadingScreenController::EnsureInitialized();
	if (GEngine) { GEngine->bEnableOnScreenDebugMessages = false; }
	extern ENGINE_API bool GAreScreenMessagesEnabled;
	GAreScreenMessagesEnabled = false;

	UWorld* World = GetWorld();
	if (!World) return;

	LoadLootTablesFromJson();
	BiomeBuilder = World->SpawnActor<AAetheriaBiomeBuilder>(FVector::ZeroVector, FRotator::ZeroRotator);
	HideProceduralBiomeVisuals(BiomeBuilder);

	int32 StartLoc = 0;
	int32 StartBots = 0;
	int32 PacingInt = static_cast<int32>(EAetheriaPacingMode::DemoBlitz);

	if (!OptionsString.IsEmpty())
	{
		if (UGameplayStatics::HasOption(OptionsString, TEXT("Loc")))
		{
			StartLoc = UGameplayStatics::GetIntOption(OptionsString, TEXT("Loc"), 0);
		}
		if (UGameplayStatics::HasOption(OptionsString, TEXT("Bots")))
		{
			StartBots = UGameplayStatics::GetIntOption(OptionsString, TEXT("Bots"), 0);
		}
		if (UGameplayStatics::HasOption(OptionsString, TEXT("Pacing")))
		{
			PacingInt = UGameplayStatics::GetIntOption(OptionsString, TEXT("Pacing"), PacingInt);
		}
	}

	SetPacingMode(static_cast<EAetheriaPacingMode>(FMath::Clamp(PacingInt, 0, 1)));
	WarpToLocation(StartLoc, 1);

	{
		const FString MapName = World->GetMapName();
		if (GAetheriaSkipMainMenu || AetheriaWorldDirector::IsBakedWorldMap(MapName))
		{
			bInMainMenu = false;
			if (AetheriaWorldDirector::IsBakedWorldMap(MapName))
			{
				ActiveWorld = AetheriaWorldDirector::WorldIdFromMapName(MapName);
			}
			else
			{
				ActiveWorld = GAetheriaPendingWorld;
			}
			GAetheriaSkipMainMenu = false;
			UE_LOG(LogTemp, Display, TEXT("[Aetheria] Boot baked world %s as %s"), *MapName, AetheriaWorldDirector::WorldDisplayName(ActiveWorld));
		}
	}

	// Bastion playable-map pass: start in the ARPG camera (not the lobby close-up),
	// and lift lighting so the sanctum is moody but readable.
	auto ForceGameplayPresentation = [World, this]()
	{
		if (!World) return;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC) continue;
			if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(PC->GetHUD()))
			{
				if (!bInMainMenu)
				{
					Hud->HideLobby();
					Hud->SetShowMainMenu(false);
				}
			}
			if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
			{
				Wiz->SetLobbyCameraMode(bInMainMenu);
			}
		}

		// Lighting, fog, exposure and saturation have one owner:
		// AetheriaWorldDirector::ApplyLighting. This lambda runs at BeginPlay
		// and again at +0.35s; writing a hardcoded Bastion palette here used
		// to overwrite per-world art direction (and the menu courtyard look).
		if (AAetheriaGameMode* GM = World->GetAuthGameMode<AAetheriaGameMode>())
		{
			if (!GM->IsInMainMenu())
			{
				GM->ApplyWorldPresentation();
			}
		}
	};
	ForceGameplayPresentation();
	// Weak lambda: if the GameMode is torn down during level transition the
	// retry simply never fires instead of touching a dead object.
	FTimerDelegate PresentationRetry;
	PresentationRetry.BindWeakLambda(this, ForceGameplayPresentation);
	World->GetTimerManager().SetTimer(PresentationTimerHandle, PresentationRetry, 0.35f, false);

	for (int32 i = 0; i < FMath::Clamp(StartBots, 0, 5); ++i)
	{
		AddBotWizard();
	}

	World->GetTimerManager().SetTimer(
		SimulationTimerHandle,
		this,
		&AAetheriaGameMode::OnSimulationTick,
		0.05f,
		true);

	// Keeps every wizard on walkable ground: retries placement while level
	// collision streams in and rescues anyone falling through the world.
	World->GetTimerManager().SetTimer(
		GroundWatchdogHandle,
		this,
		&AAetheriaGameMode::TickGroundWatchdog,
		0.2f,
		true);


	if (FParse::Param(FCommandLine::Get(), TEXT("GrokWorldShot")))
	{
		// Lighting/framing iteration: boot any map, wait for streams and
		// tone-mapping to settle, one screenshot, quit. Windowed so the
		// shot matches what the player sees (RenderOffscreen changes EV ramp).
		const FString ShotDir2 = FPaths::ProjectSavedDir() / TEXT("GrokShots");
		IFileManager::Get().MakeDirectory(*ShotDir2, true);
		const FString MapTag = World->GetMapName().Replace(TEXT("World_"), TEXT(""));
		FTimerHandle HShot1, HQuit1;
		World->GetTimerManager().SetTimer(HShot1, [ShotDir2, MapTag]()
		{
			const FString Path = ShotDir2 / FString::Printf(TEXT("worldshot_%s_%s.png"), *MapTag, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
			FScreenshotRequest::RequestScreenshot(Path, true, false);
			UE_LOG(LogTemp, Display, TEXT("[Aetheria] World shot %s"), *Path);
		}, 6.0f, false);
		World->GetTimerManager().SetTimer(HQuit1, []() { FGenericPlatformMisc::RequestExit(false); }, 8.5f, false);
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("GrokCombatTest")))
	{
		// Gameplay-loop check: pushes the real combat RPCs (orbs -> R -> D/F
		// cast, basic attack, dash), releases the gate so waves spawn, then
		// logs a [CombatTest] verdict. Run on a baked world map.
		TWeakObjectPtr<AAetheriaGameMode> WeakGM(this);
		auto FindHero = [World]() -> AAetheriaWizardCharacter*
		{
			for (TActorIterator<AAetheriaWizardCharacter> It(World); It; ++It)
			{
				if (It->IsPlayerControlled()) return *It;
			}
			return nullptr;
		};
		struct FCombatTestState
		{
			bool bOrbs = false;
			bool bInvoke = false;
			bool bCast = false;
			bool bWaves = false;
			bool bReviveReset = false;
			int32 StaticBuildingsMoved = 0;
			TMap<TWeakObjectPtr<AStaticMeshActor>, FVector> StaticSnapshot;
			float ManaBeforeCast = -1.0f;
		};
		TSharedRef<FCombatTestState> CT = MakeShared<FCombatTestState>();
		FTimerHandle HC1, HCGargantua, HCGargantuaCheck, HC2, HC3, HC4, HC6, HC7;
		World->GetTimerManager().SetTimer(HC1, [WeakGM, FindHero, CT]()
		{
			AAetheriaWizardCharacter* H = FindHero();
			if (!H) { UE_LOG(LogTemp, Error, TEXT("[CombatTest] FAIL no hero")); return; }
			const FVector Aim = H->GetActorLocation() + H->GetActorForwardVector() * 800.f;
			H->ServerPushOrb(EAetheriaOrb::Ignis);
			H->ServerPushOrb(EAetheriaOrb::Tempest);
			H->ServerPushOrb(EAetheriaOrb::Graviton);
			CT->bOrbs = (H->ActiveOrbs.Num() == 3);
			UE_LOG(LogTemp, Display, TEXT("[CombatTest] orbs pushed=%d num=%d"), (int32)CT->bOrbs, H->ActiveOrbs.Num());
			CT->ManaBeforeCast = H->Mana;
			H->ServerInvoke(false, Aim);
			CT->bInvoke = (H->SlotD != EAetheriaSpellId::None);
			UE_LOG(LogTemp, Display, TEXT("[CombatTest] invoke ok=%d slotD=%d"), (int32)CT->bInvoke, (int32)H->SlotD);
			H->ServerCastSlot(0, Aim);
			const bool bManaSpent = H->Mana < CT->ManaBeforeCast;
			CT->bCast = bManaSpent || H->SlotDCooldownRemaining > 0.f;
			UE_LOG(LogTemp, Display, TEXT("[CombatTest] cast slotD ok=%d mana %.1f->%.1f cd=%.1f"),
				(int32)CT->bCast, CT->ManaBeforeCast, H->Mana, H->SlotDCooldownRemaining);
			H->ServerBasicAttack(Aim);

			// Test death -> revive reset BEFORE ServerDash i-frames are active
			H->ApplyDamageToWizard(9999.0f);
			const bool bWasDowned = !H->bIsAlive;
			H->ReviveWizard(1.0f);
			const UAetheriaAnimInstance* Anim = H->GetMesh() ? Cast<UAetheriaAnimInstance>(H->GetMesh()->GetAnimInstance()) : nullptr;
			const UCharacterMovementComponent* Mov = H->GetCharacterMovement();
			CT->bReviveReset = bWasDowned && H->bIsAlive && (!Anim || !Anim->bIsDead) && (!Mov || Mov->MovementMode == MOVE_Walking);
			UE_LOG(LogTemp, Display, TEXT("[CombatTest] revive_anim_reset=%d alive=%d animDead=%d movMode=%d"),
				CT->bReviveReset ? 1 : 0, H->bIsAlive ? 1 : 0, (Anim && Anim->bIsDead) ? 1 : 0, Mov ? (int32)Mov->MovementMode : -1);

			H->ServerDash(Aim + FVector(200.f, 0.f, 0.f));

			if (AAetheriaGameMode* GM = WeakGM.Get()) { GM->OpenActiveGate(); }
		}, 2.5f, false);
		World->GetTimerManager().SetTimer(HCGargantua, [World, FindHero, CT]()
		{
			AAetheriaWizardCharacter* H = FindHero();
			if (!H || !World) return;

			CT->StaticSnapshot.Reset();
			for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
			{
				if (AStaticMeshActor* SMA = *It)
				{
					if (!SMA->IsActorBeingDestroyed())
					{
						CT->StaticSnapshot.Add(SMA, SMA->GetActorLocation());
					}
				}
			}

			FVector Aim = H->GetActorLocation() + H->GetActorForwardVector() * 480.0f;
			float BestDistSq = MAX_FLT;
			for (TActorIterator<AAetheriaMobCharacter> It(World); It; ++It)
			{
				AAetheriaMobCharacter* M = *It;
				if (!M || M->IsActorBeingDestroyed() || M->HP <= 0.0f) continue;
				const float DSq = FVector::DistSquared(H->GetActorLocation(), M->GetActorLocation());
				if (DSq < BestDistSq)
				{
					BestDistSq = DSq;
					Aim = M->GetActorLocation();
				}
			}

			// Cast BlackHole (EEE)
			H->Mana = H->MaxMana;
			H->SpellCooldowns.Init(0.0f, 17);
			H->ServerPushOrb(EAetheriaOrb::Graviton);
			H->ServerPushOrb(EAetheriaOrb::Graviton);
			H->ServerPushOrb(EAetheriaOrb::Graviton);
			H->ServerInvoke(false, Aim);
			H->ServerCastSlot(0, Aim);

			// Cast ChaosMeteor (QQE)
			H->Mana = H->MaxMana;
			H->SpellCooldowns.Init(0.0f, 17);
			const FVector MeteorAim = Aim + FVector(220.0f, 0.0f, 0.0f);
			H->ServerPushOrb(EAetheriaOrb::Ignis);
			H->ServerPushOrb(EAetheriaOrb::Ignis);
			H->ServerPushOrb(EAetheriaOrb::Graviton);
			H->ServerInvoke(false, MeteorAim);
			H->ServerCastSlot(0, MeteorAim);

			UE_LOG(LogTemp, Display, TEXT("[CombatTest] cast BlackHole(EEE) + ChaosMeteor(QQE) snapshotStatic=%d aim=%s"),
				CT->StaticSnapshot.Num(), *Aim.ToCompactString());
		}, 14.5f, false);
		World->GetTimerManager().SetTimer(HCGargantuaCheck, [CT]()
		{
			int32 Moved = 0;
			for (const TPair<TWeakObjectPtr<AStaticMeshActor>, FVector>& KV : CT->StaticSnapshot)
			{
				if (AStaticMeshActor* SMA = KV.Key.Get())
				{
					if (!SMA->IsActorBeingDestroyed() && FVector::DistSquared(KV.Value, SMA->GetActorLocation()) > 4.0f)
					{
						++Moved;
					}
				}
			}
			CT->StaticBuildingsMoved = Moved;
			UE_LOG(LogTemp, Display, TEXT("[CombatTest] static_buildings_moved=%d"), Moved);

			const FString ShotDir = FPaths::ProjectSavedDir() / TEXT("GrokShots");
			IFileManager::Get().MakeDirectory(*ShotDir, true);
			const FString ShotPath = ShotDir / FString::Printf(TEXT("gargantua_meteor_%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
			FScreenshotRequest::RequestScreenshot(ShotPath, true, false);
			UE_LOG(LogTemp, Display, TEXT("[Aetheria] Gargantua/Meteor shot %s"), *ShotPath);
		}, 16.3f, false);
		World->GetTimerManager().SetTimer(HC2, [WeakGM, World, FindHero]()
		{
			AAetheriaWizardCharacter* H = FindHero();
			if (!H || !World) return;

			AAetheriaMobCharacter* NearestMob = nullptr;
			float BestDistSq = MAX_FLT;
			for (TActorIterator<AAetheriaMobCharacter> It(World); It; ++It)
			{
				AAetheriaMobCharacter* M = *It;
				if (!M || M->IsActorBeingDestroyed() || M->HP <= 0.0f) continue;
				const float DSq = FVector::DistSquared(H->GetActorLocation(), M->GetActorLocation());
				if (DSq < BestDistSq)
				{
					BestDistSq = DSq;
					NearestMob = M;
				}
			}

			FVector Aim = H->GetActorLocation() + H->GetActorForwardVector() * 650.0f;
			if (NearestMob)
			{
				const FVector MobLoc = NearestMob->GetActorLocation();
				if (FVector::Dist2D(H->GetActorLocation(), MobLoc) > 420.0f)
				{
					if (AAetheriaGameMode* GM = WeakGM.Get())
					{
						GM->PlaceCharacterOnGround(H, MobLoc + FVector(-340.0f, -120.0f, 0.0f), TEXT("CombatTestApproach"));
					}
				}
				Aim = MobLoc;
			}

			H->Mana = H->MaxMana;
			H->SpellCooldowns.Init(0.0f, 17);
			// Fireball (QQQ)
			H->ServerPushOrb(EAetheriaOrb::Ignis);
			H->ServerPushOrb(EAetheriaOrb::Ignis);
			H->ServerPushOrb(EAetheriaOrb::Ignis);
			H->ServerInvoke(false, Aim);
			H->ServerCastSlot(0, Aim);

			// ChainLightning (QWW)
			H->Mana = H->MaxMana;
			H->ServerPushOrb(EAetheriaOrb::Ignis);
			H->ServerPushOrb(EAetheriaOrb::Tempest);
			H->ServerPushOrb(EAetheriaOrb::Tempest);
			H->ServerInvoke(false, Aim);
			H->ServerCastSlot(0, Aim);
		}, 17.45f, false);
		World->GetTimerManager().SetTimer(HC7, [World, CT]()
		{
			int32 Mobs = 0;
			for (TActorIterator<AAetheriaMobCharacter> It(World); It; ++It)
			{
				if (It->IsActorBeingDestroyed()) continue;
				++Mobs;
			}
			CT->bWaves = Mobs > 0;
			UE_LOG(LogTemp, Display, TEXT("[CombatTest] mobs alive=%d ok=%d"), Mobs, (int32)CT->bWaves);
			const bool bPass = CT->bOrbs && CT->bInvoke && CT->bCast && CT->bWaves && CT->bReviveReset && (CT->StaticBuildingsMoved == 0);
			UE_LOG(LogTemp, Display, TEXT("[CombatTest] VERDICT %s orbs=%d invoke=%d cast=%d waves=%d revive=%d static_buildings_moved=%d"),
				bPass ? TEXT("PASS") : TEXT("FAIL"), (int32)CT->bOrbs, (int32)CT->bInvoke, (int32)CT->bCast, (int32)CT->bWaves, (int32)CT->bReviveReset, CT->StaticBuildingsMoved);
			const FString CombatShotDir = FPaths::ProjectSavedDir() / TEXT("GrokShots");
			IFileManager::Get().MakeDirectory(*CombatShotDir, true);
			const FString CombatShotPath = CombatShotDir / FString::Printf(TEXT("combat_mobs_%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
			FScreenshotRequest::RequestScreenshot(CombatShotPath, true, false);
			UE_LOG(LogTemp, Display, TEXT("[Aetheria] Combat shot %s"), *CombatShotPath);
		}, 18.0f, false);
		World->GetTimerManager().SetTimer(HC3, [WeakGM, World]()
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			if (!PC) return;
			const EAetheriaWorldId WId = WeakGM.IsValid() ? WeakGM->GetActiveWorld() : EAetheriaWorldId::Bastion;
			const FVector Base = AetheriaWorldDirector::PlayerStartForWorld(World, WId);
			const FVector FloorCamPos = Base + FVector(-480.0f, -480.0f, 760.0f);
			const FVector FloorLook = Base + FVector(320.0f, 180.0f, -60.0f);
			FActorSpawnParameters Sp;
			Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ACameraActor* FloorCam = World->SpawnActor<ACameraActor>(FloorCamPos, (FloorLook - FloorCamPos).Rotation(), Sp))
			{
				if (UCameraComponent* CC = FloorCam->GetCameraComponent())
				{
					CC->SetFieldOfView(54.0f);
				}
				PC->SetViewTarget(FloorCam);
			}
		}, 19.2f, false);
		World->GetTimerManager().SetTimer(HC4, []()
		{
			const FString FloorShotDir = FPaths::ProjectSavedDir() / TEXT("GrokShots");
			IFileManager::Get().MakeDirectory(*FloorShotDir, true);
			const FString FloorShotPath = FloorShotDir / FString::Printf(TEXT("bastion_floor_%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
			FScreenshotRequest::RequestScreenshot(FloorShotPath, true, false);
			UE_LOG(LogTemp, Display, TEXT("[Aetheria] Bastion floor shot %s"), *FloorShotPath);
		}, 19.8f, false);
		World->GetTimerManager().SetTimer(HC6, []() { FGenericPlatformMisc::RequestExit(false); }, 22.0f, false);
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("GrokStaffShot")))
	{
		const FString ShotDir = FPaths::ProjectSavedDir() / TEXT("GrokShots");
		IFileManager::Get().MakeDirectory(*ShotDir, true);
		auto Shot = [ShotDir](const TCHAR* Prefix)
		{
			const FString Path = ShotDir / FString::Printf(TEXT("%s_%s.png"), Prefix, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
			FScreenshotRequest::RequestScreenshot(Path, true, false);
			UE_LOG(LogTemp, Display, TEXT("[Aetheria] Staff shot %s"), *Path);
		};

		auto FindWiz = [World]() -> AAetheriaWizardCharacter*
		{
			for (TActorIterator<AAetheriaWizardCharacter> It(World); It; ++It) { return *It; }
			return nullptr;
		};

		auto FrameHand = [World, FindWiz]()
		{
			AAetheriaWizardCharacter* Wiz = FindWiz();
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			if (!Wiz || !PC) return;
			FVector Hand = Wiz->GetActorLocation() + FVector(0.f, 0.f, 70.f);
			if (USkeletalMeshComponent* Mesh = Wiz->GetMesh())
			{
				if (Mesh->DoesSocketExist(FName(TEXT("Muzzle_01"))))
				{
					Hand = Mesh->GetSocketLocation(FName(TEXT("Muzzle_01")));
				}
				else if (Mesh->DoesSocketExist(FName(TEXT("hand_r"))))
				{
					Hand = Mesh->GetSocketLocation(FName(TEXT("hand_r")));
				}
			}
			if (USkeletalMeshComponent* M2 = Wiz->GetMesh())
			{
				const FTransform HT = M2->GetSocketTransform(FName(TEXT("hand_r")), RTS_World);
				const FTransform AT = Wiz->GetActorTransform();
				UE_LOG(LogTemp, Display, TEXT("[Aetheria] StaffDbg hand_r actorLocal pos=%s X=%s Y=%s Z=%s"),
					*AT.InverseTransformPosition(HT.GetLocation()).ToCompactString(),
					*AT.InverseTransformVectorNoScale(HT.GetUnitAxis(EAxis::X)).ToCompactString(),
					*AT.InverseTransformVectorNoScale(HT.GetUnitAxis(EAxis::Y)).ToCompactString(),
					*AT.InverseTransformVectorNoScale(HT.GetUnitAxis(EAxis::Z)).ToCompactString());
				if (Wiz->StaffMesh)
				{
					UE_LOG(LogTemp, Display, TEXT("[Aetheria] StaffDbg staff actorLocal up=%s pos=%s"),
						*AT.InverseTransformVectorNoScale(Wiz->StaffMesh->GetComponentTransform().GetUnitAxis(EAxis::Z)).ToCompactString(),
						*AT.InverseTransformPosition(Wiz->StaffMesh->GetComponentLocation()).ToCompactString());
				}
			}
			// Medium front-right shot of the right hand + torso, lit by a
			// test-only fill light so the grip is readable in dark worlds.
			const FVector Look = FMath::Lerp(Hand, Wiz->GetActorLocation() + FVector(0.f, 0.f, 10.f), 0.5f);
			const FVector CamPos = Look
				+ Wiz->GetActorForwardVector() * 170.f
				+ Wiz->GetActorRightVector() * 520.f
				+ FVector(0.f, 0.f, 25.f);
			{
				APointLight* Fill = nullptr;
				for (TActorIterator<APointLight> It(World); It; ++It)
				{
					if (It->GetFName().ToString().Contains(TEXT("StaffShotFill"))) { Fill = *It; break; }
				}
				if (!Fill)
				{
					FActorSpawnParameters Lp;
					Lp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					Lp.Name = FName(TEXT("StaffShotFill"));
					Fill = World->SpawnActor<APointLight>(CamPos, FRotator::ZeroRotator, Lp);
					if (Fill && Fill->PointLightComponent)
					{
						Fill->PointLightComponent->SetMobility(EComponentMobility::Movable);
						Fill->PointLightComponent->SetIntensity(90000.f);
						Fill->PointLightComponent->SetAttenuationRadius(1600.f);
						Fill->PointLightComponent->SetCastShadows(false);
					}
				}
				if (Fill) { Fill->SetActorLocation(CamPos + FVector(0.f, 0.f, 40.f)); }
			}
			ACameraActor* Cam = nullptr;
			for (TActorIterator<ACameraActor> It(World); It; ++It)
			{
				if (It->GetFName().ToString().Contains(TEXT("StaffCloseCam"))) { Cam = *It; break; }
			}
			if (!Cam)
			{
				FActorSpawnParameters Sp;
				Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Sp.Name = FName(TEXT("StaffCloseCam"));
				Cam = World->SpawnActor<ACameraActor>(CamPos, (Look - CamPos).Rotation(), Sp);
				if (Cam && Cam->GetCameraComponent())
				{
					Cam->GetCameraComponent()->SetFieldOfView(34.0f);
					Cam->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureMethod = true;
					Cam->GetCameraComponent()->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
					Cam->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureBias = true;
					Cam->GetCameraComponent()->PostProcessSettings.AutoExposureBias = 6.5f;
					Cam->GetCameraComponent()->bConstrainAspectRatio = false;
				}
			}
			else
			{
				Cam->SetActorLocation(CamPos);
				Cam->SetActorRotation((Look - CamPos).Rotation());
			}
			if (Cam)
			{
				PC->SetViewTarget(Cam);
			}
			if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(PC->GetHUD()))
			{
				Hud->SetShowMainMenu(false);
				Hud->HideLobby();
				Hud->SetLoadingScreen(false, 1.0f, TEXT(""), TEXT(""));
			}
		};

		TWeakObjectPtr<AAetheriaGameMode> WeakThis(this);
		if (GAetheriaStaffShotPhase == 0)
		{
			StartMainMenu();
			FTimerHandle HFrame, HShot, HGo;
			World->GetTimerManager().SetTimer(HFrame, [FrameHand]() { FrameHand(); }, 1.6f, false);
			World->GetTimerManager().SetTimer(HShot, [Shot]() { Shot(TEXT("staff_menu")); }, 2.2f, false);
			World->GetTimerManager().SetTimer(HGo, [WeakThis]()
			{
				GAetheriaStaffShotPhase = 1;
				if (AAetheriaGameMode* GM = WeakThis.Get()) { GM->StartWorld(EAetheriaWorldId::Bastion, true); }
			}, 3.0f, false);
			FTimerHandle HQuit;
			World->GetTimerManager().SetTimer(HQuit, []() { FGenericPlatformMisc::RequestExit(false); }, 40.0f, false);
		}
		else
		{
			if (bInMainMenu) { bInMainMenu = false; }
			StartWorld(EAetheriaWorldId::Bastion, true);
			FTimerHandle HIdleFrame, HIdle, HRun, HRunShot, HCast, HCastShot, HQuit;
			World->GetTimerManager().SetTimer(HIdleFrame, [FrameHand]() { FrameHand(); }, 1.4f, false);
			World->GetTimerManager().SetTimer(HIdle, [Shot]() { Shot(TEXT("staff_idle")); }, 2.0f, false);
			TSharedRef<FTimerHandle> RunPulse(new FTimerHandle());
			World->GetTimerManager().SetTimer(*RunPulse, [FindWiz, FrameHand, World, RunPulse]()
			{
				if (AAetheriaWizardCharacter* Wiz = FindWiz())
				{
					if (UCharacterMovementComponent* Move = Wiz->GetCharacterMovement())
					{
						Move->GravityScale = 1.0f;
						Move->SetMovementMode(MOVE_Walking);
						Move->Velocity = Wiz->GetActorForwardVector() * 420.0f;
					}
					Wiz->SetLobbyCameraMode(false);
					Wiz->AddMovementInput(Wiz->GetActorForwardVector(), 1.0f);
				}
				FrameHand();
			}, 0.10f, true, 2.4f);
			World->GetTimerManager().SetTimer(HRunShot, [Shot]() { Shot(TEXT("staff_run")); }, 4.0f, false);
			World->GetTimerManager().SetTimer(HCast, [FindWiz, FrameHand, World, RunPulse]()
			{
				World->GetTimerManager().ClearTimer(*RunPulse);
				if (AAetheriaWizardCharacter* Wiz = FindWiz())
				{
					if (UCharacterMovementComponent* Move = Wiz->GetCharacterMovement())
					{
						Move->StopMovementImmediately();
					}
					Wiz->PlaySpellCastAnimation(EAetheriaSpellId::Fireball);
					const FVector O = Wiz->GetActorLocation() + FVector(0.f, 0.f, 80.f);
					UAetheriaPackVFX::PlaySpell(World, EAetheriaSpellId::Fireball, O, O + Wiz->GetActorForwardVector() * 600.f, {});
				}
				FrameHand();
			}, 4.6f, false);
			World->GetTimerManager().SetTimer(HCastShot, [Shot]() { Shot(TEXT("staff_cast")); }, 5.4f, false);
			World->GetTimerManager().SetTimer(HQuit, []() { FGenericPlatformMisc::RequestExit(false); }, 7.0f, false);
		}
	}
	else 	if (FParse::Param(FCommandLine::Get(), TEXT("GrokSmoke")))
	{
		const FString ShotDir = FPaths::ProjectSavedDir() / TEXT("GrokShots");
		IFileManager::Get().MakeDirectory(*ShotDir, true);
		auto Shot = [ShotDir](const TCHAR* Prefix)
		{
			const FString Path = ShotDir / FString::Printf(TEXT("%s_%s.png"), Prefix, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
			FScreenshotRequest::RequestScreenshot(Path, true, false);
			UE_LOG(LogTemp, Display, TEXT("[Aetheria] Smoke shot %s"), *Path);
		};

		TWeakObjectPtr<AAetheriaGameMode> WeakThis(this);
		auto ArmQuit = [World](float Seconds)
		{
			FTimerHandle HQuit;
			World->GetTimerManager().SetTimer(HQuit, []()
			{
				FGenericPlatformMisc::RequestExit(false);
			}, Seconds, false);
		};

		if (GAetheriaSmokePhase == 0)
		{
			StartMainMenu();
			FTimerHandle HMenu, HLoad, HLoadShot, HGo;
			World->GetTimerManager().SetTimer(HMenu, [Shot]() { Shot(TEXT("menu")); }, 3.0f, false);
			World->GetTimerManager().SetTimer(HLoad, [WeakThis]()
			{
				if (AAetheriaGameMode* GM = WeakThis.Get())
				{
					if (UWorld* W = GM->GetWorld())
					{
						for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
						{
							APlayerController* PCIt = It->Get();
							if (AAetheriaHUD* Hud = PCIt ? Cast<AAetheriaHUD>(PCIt->GetHUD()) : nullptr)
							{
								Hud->SetLoadingScreen(true, 0.35f, TEXT("БАСТИОН АОТЕРА"), TEXT("Жаровни зажигаются. Врата ещё закрыты — стражи спят."));
							}
						}
					}
				}
			}, 5.0f, false);
			World->GetTimerManager().SetTimer(HLoadShot, [WeakThis, Shot]()
			{
				if (AAetheriaGameMode* GM = WeakThis.Get())
				{
					if (UWorld* W = GM->GetWorld())
					{
						for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
						{
							APlayerController* PCIt = It->Get();
							if (AAetheriaHUD* Hud = PCIt ? Cast<AAetheriaHUD>(PCIt->GetHUD()) : nullptr)
							{
								Hud->SetLoadingScreen(true, 0.82f, TEXT("БАСТИОН АОТЕРА"), TEXT("Готический двор и залы собираются в памяти."));
							}
						}
					}
				}
				Shot(TEXT("loading"));
			}, 6.5f, false);
			World->GetTimerManager().SetTimer(HGo, [WeakThis]()
			{
				GAetheriaSmokePhase = 1;
				if (AAetheriaGameMode* GM = WeakThis.Get()) { GM->StartWorld(EAetheriaWorldId::Bastion, true); }
			}, 8.0f, false);
			ArmQuit(70.0f);
		}
		else if (GAetheriaSmokePhase == 1)
		{
			StartWorld(EAetheriaWorldId::Bastion, true);
			FTimerHandle HShot, HGo;
			World->GetTimerManager().SetTimer(HShot, [Shot]() { Shot(TEXT("world_bastion")); }, 5.0f, false);
			World->GetTimerManager().SetTimer(HGo, [WeakThis]()
			{
				GAetheriaSmokePhase = 2;
				if (AAetheriaGameMode* GM = WeakThis.Get()) { GM->StartWorld(EAetheriaWorldId::RuinsTemple, true); }
			}, 8.0f, false);
			ArmQuit(40.0f);
		}
		else if (GAetheriaSmokePhase == 2)
		{
			StartWorld(EAetheriaWorldId::RuinsTemple, true);
			FTimerHandle HShot, HGo;
			World->GetTimerManager().SetTimer(HShot, [Shot]() { Shot(TEXT("world_ruins")); }, 5.0f, false);
			World->GetTimerManager().SetTimer(HGo, [WeakThis]()
			{
				GAetheriaSmokePhase = 3;
				if (AAetheriaGameMode* GM = WeakThis.Get()) { GM->StartWorld(EAetheriaWorldId::VoidRealm, true); }
			}, 8.0f, false);
			ArmQuit(40.0f);
		}
		else if (GAetheriaSmokePhase == 3)
		{
			StartWorld(EAetheriaWorldId::VoidRealm, true);
			FTimerHandle HShot, HGo;
			World->GetTimerManager().SetTimer(HShot, [Shot]() { Shot(TEXT("world_void")); }, 5.0f, false);
			World->GetTimerManager().SetTimer(HGo, [WeakThis]()
			{
				GAetheriaSmokePhase = 4;
				if (AAetheriaGameMode* GM = WeakThis.Get()) { GM->StartWorld(EAetheriaWorldId::BossSanctum, true); }
			}, 8.0f, false);
			ArmQuit(40.0f);
		}
		else
		{
			StartWorld(EAetheriaWorldId::BossSanctum, true);
			OpenActiveGate();
			FTimerHandle HShot, HCast, HSpellShot, HScene;
			World->GetTimerManager().SetTimer(HShot, [Shot]() { Shot(TEXT("world_sanctum")); }, 5.0f, false);
			World->GetTimerManager().SetTimer(HCast, [WeakThis]()
			{
				if (AAetheriaGameMode* GM = WeakThis.Get())
				{
					if (UWorld* W = GM->GetWorld())
					{
						AAetheriaWizardCharacter* Wiz = nullptr;
						for (TActorIterator<AAetheriaWizardCharacter> It(W); It; ++It) { Wiz = *It; break; }
						if (Wiz)
						{
							const FVector O = Wiz->GetActorLocation() + FVector(120.f, -120.f, 80.f);
							const FVector Fwd = FVector(0.72f, -0.72f, 0.f).GetSafeNormal();
							TArray<FVector> Chain = { O, O + Fwd * 280.f + FVector(0, 120, 0), O + Fwd * 480.f + FVector(0, -80, 0) };
							UAetheriaPackVFX::PlaySpell(W, EAetheriaSpellId::Fireball, O, O + Fwd * 520.f, {});
							UAetheriaPackVFX::PlaySpell(W, EAetheriaSpellId::ChaosMeteor, O + FVector(0, 0, 280), O + Fwd * 360.f, {});
							UAetheriaPackVFX::PlaySpell(W, EAetheriaSpellId::ChainLightning, O, O + Fwd * 480.f, Chain);
							UAetheriaPackVFX::PlaySpell(W, EAetheriaSpellId::BlackHole, O + Fwd * 220.f, O + Fwd * 220.f, {});
							UAetheriaPackVFX::PlaySpell(W, EAetheriaSpellId::SupernovaBeam, O, O + Fwd * 640.f, {});
							UAetheriaPackVFX::PlaySpell(W, EAetheriaSpellId::EyeOfTheStorm, O + Fwd * 160.f, O + Fwd * 160.f + FVector(0, 0, 120), {});
						}
					}
				}
			}, 7.0f, false);
			World->GetTimerManager().SetTimer(HSpellShot, [Shot]() { Shot(TEXT("spells")); }, 8.6f, false);
			World->GetTimerManager().SetTimer(HScene, [Shot]() { Shot(TEXT("scene")); }, 10.0f, false);
			ArmQuit(16.0f);
		}
	}
	else if (bInMainMenu)
	{
		StartMainMenu();
	}
	else
	{
		StartWorld(ActiveWorld, true);
	}
}

void AAetheriaGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!NewPlayer || !GS) return;

	const int32 Index = GS->ActivePlayerCount;
	GS->ActivePlayerCount++;

	if (AAetheriaPlayerState* PS = NewPlayer->GetPlayerState<AAetheriaPlayerState>())
	{
		PS->PartyColorIndex = Index % 6;
		PS->bIsBotMage = false;
		PS->WizardDisplayName = FString::Printf(TEXT("Маг %d"), Index + 1);
	}

	if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(NewPlayer->GetHUD()))
	{
		if (bInMainMenu)
		{
			Hud->ShowMainMenu();
		}
		else
		{
			Hud->HideLobby();
			Hud->SetShowMainMenu(false);
		}
	}
	if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(NewPlayer->GetPawn()))
	{
		Wiz->SetLobbyCameraMode(bInMainMenu);
		Wiz->PartyColorIndex = Index % 6;
		Wiz->WizardName = FString::Printf(TEXT("Маг %d"), Index + 1);
		const float Angle = (2.0f * PI * Index) / 6.0f;
		const float X = FMath::Cos(Angle) * 320.0f;
		const float Y = FMath::Sin(Angle) * 320.0f;
		// Baked worlds: spawn around the map's PlayerStart, never at the old
		// procedural-terrain height (that left heroes in the void).
		const FVector Base = AetheriaWorldDirector::PlayerStartForWorld(GetWorld(), ActiveWorld);
		const FVector Offset = Index == 0 ? FVector::ZeroVector : FVector(X * 0.5f, Y * 0.5f, 0.0f);
		PlaceCharacterOnGround(Wiz, Base + Offset, TEXT("PostLogin"));
	}
}

void AAetheriaGameMode::Logout(AController* Exiting)
{
	if (AAetheriaGameState* GS = GetGameState<AAetheriaGameState>())
	{
		GS->ActivePlayerCount = FMath::Max(0, GS->ActivePlayerCount - 1);
	}
	Super::Logout(Exiting);
}

void AAetheriaGameMode::AddBotWizard()
{
	UWorld* World = GetWorld();
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!World || !GS) return;

	if (GS->ActivePlayerCount + GS->ActiveBotCount >= MaxPlayers)
	{
		return;
	}

	static const TCHAR* BotNames[6] = { TEXT("Лира"), TEXT("Кай"), TEXT("Векс"), TEXT("Сола"), TEXT("Нова"), TEXT("Орион") };
	const int32 BotIdx = GS->ActiveBotCount;
	const float Angle = (2.0f * PI * (BotIdx + 1)) / 6.0f;
	const float X = FMath::Cos(Angle) * 420.0f;
	const float Y = FMath::Sin(Angle) * 420.0f;
	const float Z = UAetheriaBiomeDatabase::GroundZAt(this, GS->CurrentLocationIndex, FVector2D(X, Y)) + 100.0f;
	const FVector SpawnLoc(X, Y, Z);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AAetheriaBotAIController* BotCtrl = World->SpawnActor<AAetheriaBotAIController>(SpawnLoc, FRotator::ZeroRotator, Params);
	AAetheriaWizardCharacter* BotWiz = World->SpawnActor<AAetheriaWizardCharacter>(SpawnLoc, FRotator::ZeroRotator, Params);
	if (BotCtrl && BotWiz)
	{
		const int32 ColorIdx = (GS->ActivePlayerCount + BotIdx) % 6;
		BotWiz->PartyColorIndex = ColorIdx;
		BotWiz->WizardName = FString::Printf(TEXT("%s (AI)"), BotNames[BotIdx % 6]);
		BotWiz->Customization.BodyArchetypeIndex = (BotIdx + 1) % 10;
		BotWiz->Customization.RobeDyeIndex = 0;
		BotWiz->Customization.HeadgearIndex = (BotIdx % 4);
		BotWiz->Customization.RuneGlowIndex = (BotIdx == 0) ? 2 : (BotIdx % 4);
		BotWiz->Customization.TrimMetalIndex = (BotIdx == 0) ? 3 : (BotIdx % 4);
		BotWiz->ApplyCustomizationVisuals();
		BotCtrl->Possess(BotWiz);
		ActiveBotControllers.Add(BotCtrl);
		GS->ActiveBotCount++;
	}
}

void AAetheriaGameMode::RemoveBotWizard()
{
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!GS || ActiveBotControllers.Num() == 0) return;

	AAetheriaBotAIController* LastBot = ActiveBotControllers.Pop();
	if (IsValid(LastBot))
	{
		if (APawn* P = LastBot->GetPawn())
		{
			P->Destroy();
		}
		LastBot->Destroy();
	}
	GS->ActiveBotCount = FMath::Max(0, GS->ActiveBotCount - 1);
}

void AAetheriaGameMode::WarpToLocation(int32 NewLocationIndex, int32 OptionalAct)
{
	UWorld* World = GetWorld();
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!World || !GS) return;

	const int32 ClampedLoc = FMath::Clamp(NewLocationIndex, 0, 9);

	// Clear active mobs, zones, and projectiles
	for (TActorIterator<AAetheriaMobCharacter> It(World); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<AAetheriaSpellZone> It(World); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<AAetheriaSpellProjectile> It(World); It; ++It)
	{
		It->Destroy();
	}

	ActiveBossMob.Reset();

	if (IsValid(BiomeBuilder))
	{
		// 04:29: do not procedurally scatter biome meshes. Worlds are artist layouts.
		UE_LOG(LogTemp, Display, TEXT("[Aetheria] Skipping BiomeBuilder::BuildLocation; using streamed demo layouts"));
		HideProceduralBiomeVisuals(BiomeBuilder);
	}

	GS->CurrentLocationIndex = ClampedLoc;
	GS->LocationElapsedSeconds = 0.0f;
	GS->bBossSpawned = false;
	GS->bBossDefeated = false;
	GS->bPortalOpen = false;
	GS->bCampaignComplete = false;
	GS->BossName.Empty();
	GS->BossHP = 0.0f;
	GS->BossMaxHP = 0.0f;
	GS->BossOrbs.Empty();
	GS->ClearSynergyBanner();
	GS->ClearAmbushAlert();
	GS->CurrentWaveNumber = 0;

	PendingAmbushes.Empty();
	CounteredObjectiveIds.Empty();
	bConstructAwakeningTriggered = false;
	LastTriggeredBossPhase = 1;
	BreatherTimer = 0.0f;
	bBreatherReady = false;
	AmbushCooldownTimer = (GS->PacingMode == EAetheriaPacingMode::DemoBlitz) ? 14.0f : 22.0f;

	int32 Index = 0;
	for (TActorIterator<AAetheriaWizardCharacter> It(World); It; ++It)
	{
		AAetheriaWizardCharacter* Wiz = *It;
		if (!Wiz) continue;
		const float Angle = (2.0f * PI * Index) / 6.0f;
		const float X = FMath::Cos(Angle) * 320.0f;
		const float Y = FMath::Sin(Angle) * 320.0f;
		const FVector Base = AetheriaWorldDirector::PlayerStartForWorld(World, ActiveWorld);
		const FVector Offset = Index == 0 ? FVector::ZeroVector : FVector(X * 0.5f, Y * 0.5f, 0.0f);
		PlaceCharacterOnGround(Wiz, Base + Offset, TEXT("Warp"));
		Wiz->HP = Wiz->MaxHP;
		Wiz->Mana = Wiz->MaxMana;
		Wiz->bIsAlive = true;
		Wiz->ResetPresentationDeath();
		Wiz->ColdStacks = 0;
		Wiz->RespawnTimer = 0.0f;
		if (UCharacterMovementComponent* MoveComp = Wiz->GetCharacterMovement())
		{
			MoveComp->SetMovementMode(MOVE_Walking);
		}
		Index++;
	}

	SetAct(FMath::Clamp(OptionalAct, 1, 5));
}

void AAetheriaGameMode::SetPacingMode(EAetheriaPacingMode NewPacing)
{
	if (AAetheriaGameState* GS = GetGameState<AAetheriaGameState>())
	{
		GS->PacingMode = NewPacing;
		const int32 TotalMages = FMath::Max(1, GS->ActivePlayerCount + GS->ActiveBotCount);
		const FAetheriaActTimingConfig Timing = UAetheriaBiomeDatabase::GetActTiming(NewPacing, TotalMages);
		GS->EstimatedLocationMinutes = Timing.EstimatedMinutes;
		LegionKillsTarget = Timing.LegionKillsTarget;
	}
}

void AAetheriaGameMode::EnterAstralPortal()
{
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!GS || !GS->bPortalOpen) return;

	// Progress seamlessly across all 4 baked worlds with our persistent dark-fantasy loading screen
	switch (ActiveWorld)
	{
	case EAetheriaWorldId::Bastion:
		StartWorld(EAetheriaWorldId::RuinsTemple, false);
		return;
	case EAetheriaWorldId::RuinsTemple:
		StartWorld(EAetheriaWorldId::VoidRealm, false);
		return;
	case EAetheriaWorldId::VoidRealm:
		StartWorld(EAetheriaWorldId::BossSanctum, false);
		return;
	case EAetheriaWorldId::BossSanctum:
	default:
		GS->bCampaignComplete = true;
		GS->ActTitle = TEXT("Хроно-Печать Восстановлена!");
		GS->ActDescription = TEXT("Все 4 мира пройдены! Выберите любой мир через Атлас [M].");
		GS->bPortalOpen = false;
		PlayAnnouncerVoice(TEXT("/Game/ParagonMaleAnnouncer/Sounds/SoundWaves/MAnnouncer_Player_QuestComplete_010"), 0.90f);
		return;
	}
}

void AAetheriaGameMode::SetAct(int32 ActIndex)
{
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	UWorld* World = GetWorld();
	if (!GS) return;

	ClearStageWorldVFX();

	CurrentAct = FMath::Clamp(ActIndex, 1, 5);
	GS->CurrentAct = CurrentAct;
	GS->ActProgress = 0.0f;
	GS->Objectives.Empty();
	LegionKillsCurrent = 0;
	BreatherTimer = 0.0f;
	bBreatherReady = false;

	const FAetheriaLocationDef LocDef = UAetheriaBiomeDatabase::GetLocation(GS->CurrentLocationIndex);
	const int32 TotalMages = FMath::Max(1, GS->ActivePlayerCount + GS->ActiveBotCount);
	const FAetheriaActTimingConfig Timing = UAetheriaBiomeDatabase::GetActTiming(GS->PacingMode, TotalMages);
	LegionKillsTarget = (GS->PacingMode == EAetheriaPacingMode::DemoBlitz) ? FMath::Clamp(10 + TotalMages * 2, 10, 18) : Timing.LegionKillsTarget;

	// Anchor every stage dynamically along the active world's playable route: PlayerStart -> Gate -> BossArena
	const FVector StartOrigin = AetheriaWorldDirector::PlayerStartForWorld(World, ActiveWorld);
	const FVector GatePos = AetheriaWorldDirector::GateLocationForWorld(ActiveWorld);
	FVector RouteDir = (GatePos - StartOrigin);
	RouteDir.Z = 0.0f;
	if (!RouteDir.Normalize())
	{
		RouteDir = FVector(0.7071f, -0.7071f, 0.0f);
	}
	const FVector RouteRight(-RouteDir.Y, RouteDir.X, 0.0f);

	if (CurrentAct == 1)
	{
		GraceTimer = LocDef.GracePeriodSeconds;
		GS->CurrentWavePhase = EAetheriaWavePhase::SanctuaryGrace;
		GS->CurrentWaveNumber = 0;
		bConstructAwakeningTriggered = false;
		bStageEntryAmbushSpawned = false;
		bGateOpenVFXTriggered = false;
		bVictoryPortalVFXTriggered = false;
		CounteredObjectiveIds.Empty();
		if (GS->CurrentLocationIndex == 0)
		{
			GS->SetSynergyBanner(TEXT("Этап I — Засада у Врат: ПКМ — движение, [Q/W/E] -> [R] — синтез заклинания"));
		}
	}
	else if (CurrentAct == 5)
	{
		LastTriggeredBossPhase = 1;
		GS->CurrentWavePhase = EAetheriaWavePhase::ClimaxSiege;
	}

	switch (CurrentAct)
	{
	case 1:
	{
		// Stage 1: Entry Ambush & Forward Runic Beacons
		GS->ActTitle = TEXT("Этап I — Засада у Врат");
		GS->ActDescription = TEXT("Прорвите авангардную засаду и активируйте 2 Рунных Маяка на подступах.");
		const FVector BeaconOffsets[2] = {
			StartOrigin + RouteDir * 480.0f + RouteRight * 220.0f,
			StartOrigin + RouteDir * 820.0f - RouteRight * 220.0f
		};
		for (int32 i = 0; i < 2; ++i)
		{
			const FVector Loc = ResolveGroundedStagePoint(BeaconOffsets[i], StartOrigin.Z);
			FAetheriaObjectiveInfo Obj;
			Obj.ObjectiveId = i + 1;
			Obj.Kind = EAetheriaObjectiveKind::Beacon;
			Obj.Label = FString::Printf(TEXT("Рунный Маяк %d"), i + 1);
			Obj.Location = Loc;
			Obj.RadiusUU = 560.0f;
			Obj.Progress = 0.0f;
			Obj.bCompleted = false;
			Obj.bActive = true;
			GS->Objectives.Add(Obj);
			SpawnStageWorldVFX(TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Circle"), Loc + FVector(0.0f, 0.0f, 8.0f), FRotator::ZeroRotator, FVector(2.2f));
		}
		break;
	}
	case 2:
	{
		// Stage 2: Obelisk Seal Encounter with glowing runic ring
		GS->ActTitle = TEXT("Этап II — Печать Эфирного Обелиска");
		GS->ActDescription = TEXT("Удерживайте сияющее руническое кольцо Обелиска (0% -> 100%) под натиском стражей.");
		GS->CurrentWavePhase = EAetheriaWavePhase::ClimaxSiege;
		const FVector ObeliskLoc = ResolveGroundedStagePoint(FMath::Lerp(StartOrigin, GatePos, 0.58f), StartOrigin.Z);
		FAetheriaObjectiveInfo Obj;
		Obj.ObjectiveId = 10;
		Obj.Kind = EAetheriaObjectiveKind::Obelisk;
		Obj.Label = TEXT("Печать Обелиска");
		Obj.Location = ObeliskLoc;
		Obj.RadiusUU = 680.0f;
		Obj.Progress = 0.0f;
		Obj.bCompleted = false;
		Obj.bActive = true;
		GS->Objectives.Add(Obj);

		SpawnStageWorldVFX(TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Circle"), ObeliskLoc + FVector(0.0f, 0.0f, 10.0f), FRotator::ZeroRotator, FVector(3.4f));
		SpawnStageWorldVFX(TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Mana"), ObeliskLoc + FVector(0.0f, 0.0f, 35.0f), FRotator::ZeroRotator, FVector(2.6f));
		GS->SetSynergyBanner(TEXT("ПЕЧАТЬ ОБЕЛИСКА ПРОБУЖДЕНА! Встаньте в рунный круг!"));
		PlayAnnouncerVoice(TEXT("/Game/ParagonMaleAnnouncer/Sounds/SoundWaves/MAnnouncer_Enemy_TowerUnderAttack_010"), 0.80f);
		break;
	}
	case 3:
	{
		// Stage 3: Gate Opens with /Game/Portals VFX & ParagonMaleAnnouncer callout + Shielded Seal Pylons
		GS->ActTitle = TEXT("Этап III — Врата Святилища Открыты");
		GS->ActDescription = TEXT("Врата разломаны! Уничтожьте 2 Пилона Печати заклинаниями на проходе.");
		GS->CurrentWavePhase = EAetheriaWavePhase::TacticalEscalation;

		OpenActiveGate();
		const FVector GroundGate = ResolveGroundedStagePoint(GatePos, StartOrigin.Z);
		if (!bGateOpenVFXTriggered)
		{
			bGateOpenVFXTriggered = true;
			const TCHAR* GatePortalVFX = (ActiveWorld == EAetheriaWorldId::BossSanctum)
				? TEXT("/Game/Portals/Rectangle/FirePortal/NS_FireRec")
				: TEXT("/Game/Portals/Rounded/LightningPortal/NS_Lightning");
			SpawnStageWorldVFX(GatePortalVFX, GroundGate + FVector(0.0f, 0.0f, 130.0f), FRotator(0.0f, 45.0f, 0.0f), FVector(2.0f));
			SpawnStageWorldVFX(TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Shockwave"), GroundGate + FVector(0.0f, 0.0f, 20.0f), FRotator::ZeroRotator, FVector(2.8f), false);
			UAetheriaMaterialLibrary::PlayGameSound2D(World, TEXT("SFX_Boss_Gate_Open"), 0.90f);
			PlayAnnouncerVoice(TEXT("/Game/ParagonMaleAnnouncer/Sounds/SoundWaves/MAnnouncer_Player_QuestUnlocked_010"), 0.90f);
			GS->SetSynergyBanner(TEXT("ВРАТА СВЯТИЛИЩА ОТКРЫТЫ! Сокрушите Пилоны Печати!"));
		}

		const float PylonHP = 720.0f * Timing.PylonHPScale;
		const FVector PylonOffsets[2] = {
			GroundGate + RouteDir * 360.0f + RouteRight * 340.0f,
			GroundGate + RouteDir * 360.0f - RouteRight * 340.0f
		};
		for (int32 i = 0; i < 2; ++i)
		{
			const FVector Loc = ResolveGroundedStagePoint(PylonOffsets[i], GroundGate.Z);
			FAetheriaObjectiveInfo Obj;
			Obj.ObjectiveId = 20 + i;
			Obj.Kind = EAetheriaObjectiveKind::Pylon;
			Obj.Label = FString::Printf(TEXT("Пилон Печати %d"), i + 1);
			Obj.Location = Loc;
			Obj.RadiusUU = 420.0f;
			Obj.HP = PylonHP;
			Obj.MaxHP = PylonHP;
			Obj.Progress = 0.0f;
			Obj.bCompleted = false;
			Obj.bActive = true;
			GS->Objectives.Add(Obj);
			SpawnStageWorldVFX(TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Fire"), Loc + FVector(0.0f, 0.0f, 20.0f), FRotator::ZeroRotator, FVector(2.0f));
		}
		break;
	}
	case 4:
	{
		// Stage 4: Elite Champions Vanguard
		GS->ActTitle = TEXT("Этап IV — Чемпионы Разлома");
		GS->ActDescription = FString::Printf(TEXT("Сокрушите Элитных Чемпионов и их легион (0 / %d)."), LegionKillsTarget);
		GS->CurrentWavePhase = EAetheriaWavePhase::ClimaxSiege;
		GS->SetSynergyBanner(TEXT("ЧЕМПИОНЫ РАЗЛОМА ВЫШЛИ НА АРЕНУ! Приготовьтесь к элитному бою!"));
		PlayAnnouncerVoice(TEXT("/Game/ParagonMaleAnnouncer/Sounds/SoundWaves/MAnnouncer_Enemy_OrbPrimeEnabled_010"), 0.90f);

		if (bCombatReleased && !LocDef.Mobs.IsEmpty())
		{
			int32 SwarmIdx = 0, BruiserIdx = 0, RangedIdx = 0;
			for (int32 i = 0; i < LocDef.Mobs.Num(); ++i)
			{
				if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Swarm) SwarmIdx = i;
				else if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Bruiser) BruiserIdx = i;
				else if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Ranged) RangedIdx = i;
			}
			const FVector EliteAnchor = ResolveGroundedStagePoint(GatePos + RouteDir * 420.0f, StartOrigin.Z);
			QueueAmbush(EAetheriaAmbushKind::ObjectiveTrapCounter, EliteAnchor + RouteRight * 260.0f, 1, BruiserIdx, true, TEXT("ЭЛИТНЫЙ ЧЕМПИОН ПРИБЫЛ!"));
			QueueAmbush(EAetheriaAmbushKind::ObjectiveTrapCounter, EliteAnchor - RouteRight * 260.0f, 1, BruiserIdx, true, TEXT("ЭЛИТНЫЙ ЧЕМПИОН ПРИБЫЛ!"));
			QueueAmbush(EAetheriaAmbushKind::RiftBreach, EliteAnchor + RouteDir * 240.0f, 2, RangedIdx, false, TEXT(""));
			QueueAmbush(EAetheriaAmbushKind::SubterraneanEruption, EliteAnchor, 3, SwarmIdx, false, TEXT(""));
		}
		break;
	}
	case 5:
	{
		// Stage 5: Boss Encounter (switches to BGM_Boss_Dread + ParagonMaleAnnouncer Epic callout)
		const TCHAR* WorldBossHero = AetheriaWorldDirector::BossHeroForWorld(ActiveWorld);
		GS->ActTitle = FString::Printf(TEXT("Этап V — Хранитель Мира: %s (%s)"), WorldBossHero, *LocDef.Boss.DisplayName);
		GS->ActDescription = FString::Printf(TEXT("Сразите Хранителя: %s, чтобы открыть Портал Победы!"), WorldBossHero);
		GS->CurrentWavePhase = EAetheriaWavePhase::ClimaxSiege;
		PlayBiomeBGM(ActiveWorld, true);
		PlayAnnouncerVoice(TEXT("/Game/ParagonMaleAnnouncer/Sounds/SoundWaves/MAnnouncer_Player_Epic_010"), 0.92f);
		SpawnBoss(LocDef, Timing.BossHPScale);
		break;
	}
	}
}

void AAetheriaGameMode::OnSimulationTick()
{
	UWorld* World = GetWorld();
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!World || !GS) return;

	const float Dt = 0.05f;
	GS->LocationElapsedSeconds += Dt;

	const FAetheriaLocationDef LocDef = UAetheriaBiomeDatabase::GetLocation(GS->CurrentLocationIndex);
	const int32 TotalMages = FMath::Max(1, GS->ActivePlayerCount + GS->ActiveBotCount);
	const FAetheriaActTimingConfig Timing = UAetheriaBiomeDatabase::GetActTiming(GS->PacingMode, TotalMages);

	// Count active mobs
	int32 MobCount = 0;
	for (TActorIterator<AAetheriaMobCharacter> It(World); It; ++It)
	{
		if (IsValid(*It) && It->HP > 0.0f)
		{
			MobCount++;
		}
	}

	// Simulate Biome Hazards
	SimulateHazards(LocDef, Dt);

	// Sync objective visuals
	if (IsValid(BiomeBuilder))
	{
		BiomeBuilder->SyncObjectives(GS->Objectives, Dt);
	}

	// Party Wipe & Co-op Ally Revive (Diablo / Magicka stakes)
	TArray<AAetheriaWizardCharacter*> AllWizards;
	TArray<AAetheriaWizardCharacter*> LivingWizards;
	for (TActorIterator<AAetheriaWizardCharacter> It(World); It; ++It)
	{
		if (AAetheriaWizardCharacter* Wiz = *It)
		{
			AllWizards.Add(Wiz);
			if (Wiz->bIsAlive)
			{
				LivingWizards.Add(Wiz);
			}
		}
	}

	if (AllWizards.Num() > 0 && LivingWizards.Num() == 0)
	{
		if (PartyWipeResetTimer <= 0.0f)
		{
			PartyWipeResetTimer = 2.4f;
			GS->SetSynergyBanner(TEXT("ОТРЯД ПАЛ! Поход начнётся сначала (Локация 1, Акт I)..."));
			UAetheriaMaterialLibrary::PlayGameSound2D(World, TEXT("SFX_Party_Wipe"), 0.95f);
		}
		else
		{
			PartyWipeResetTimer -= Dt;
			if (PartyWipeResetTimer <= 0.0f)
			{
				PartyWipeResetTimer = 0.0f;
				WarpToLocation(0, 1);
				return;
			}
		}
	}
	else
	{
		PartyWipeResetTimer = 0.0f;
		for (AAetheriaWizardCharacter* Wiz : AllWizards)
		{
			if (!Wiz || Wiz->bIsAlive) continue;

			if (Wiz->RespawnTimer <= 0.0f)
			{
				Wiz->RespawnTimer = 12.0f;
				continue;
			}

			bool bAllyAssisting = false;
			for (AAetheriaWizardCharacter* Ally : LivingWizards)
			{
				if (FVector::Dist2D(Ally->GetActorLocation(), Wiz->GetActorLocation()) <= 420.0f)
				{
					bAllyAssisting = true;
					break;
				}
			}

			// Ally within 420 UU channels a fast revive; alone the downed hero
			// still self-stabilises (slow tick) — otherwise a solo run dead-
			// locks with no one left to stand near the body.
			Wiz->RespawnTimer -= Dt * (bAllyAssisting ? 4.5f : 0.55f);
			{
				if (Wiz->RespawnTimer <= 0.0f)
				{
					Wiz->ReviveWizard(0.65f);
					UAetheriaMaterialLibrary::PlayGameSoundAtLocation(World, TEXT("SFX_Revive_Complete"), Wiz->GetActorLocation(), 0.90f);
					if (UWorld* W = GetWorld())
					{
						AAetheriaLightningArcVFX::SpawnShockwaveRing(W, Wiz->GetActorLocation() - FVector(0.0f, 0.0f, 40.0f), 320.0f, FLinearColor(1.0f, 0.85f, 0.25f, 1.0f), 0.60f);
						AAetheriaLightningArcVFX::SpawnShockwaveRing(W, Wiz->GetActorLocation() - FVector(0.0f, 0.0f, 30.0f), 210.0f, AetheriaMindColors::MindCyan, 0.45f);
					}
					GS->SetSynergyBanner(TEXT("Союзник возвращён в бой благодаря поддержке отряда!"));
				}
			}
		}
	}

	// Tick pending ambush visual telegraphs
	TickPendingAmbushes(Dt);

	// Progressive wave director grace / breather logic
	bool bGraceActive = false;
	if (CurrentAct == 1 && GraceTimer > 0.0f)
	{
		GraceTimer -= Dt;
		GS->CurrentWavePhase = EAetheriaWavePhase::SanctuaryGrace;
		bGraceActive = true;
	}
	else if (BreatherTimer > 0.0f)
	{
		BreatherTimer -= Dt;
		GS->CurrentWavePhase = EAetheriaWavePhase::Breather;
	}
	else if (GS->CurrentWaveNumber >= 2 && MobCount <= 2 && bBreatherReady)
	{
		bBreatherReady = false;
		BreatherTimer = (GS->PacingMode == EAetheriaPacingMode::DemoBlitz) ? 5.5f : 8.5f;
		GS->CurrentWavePhase = EAetheriaWavePhase::Breather;
		MobSpawnTimer = BreatherTimer;
	}

	MobSpawnTimer -= Dt;

	switch (CurrentAct)
	{
	case 1:
	{
		int32 CompletedCount = 0;
		float SumProgress = 0.0f;
		const float RatePerSec = 100.0f / FMath::Max(1.0f, Timing.BeaconCaptureSeconds);

		for (FAetheriaObjectiveInfo& Obj : GS->Objectives)
		{
			if (Obj.bCompleted)
			{
				CompletedCount++;
				SumProgress += 100.0f;
				continue;
			}

			int32 WizardsInside = 0;
			for (TActorIterator<AAetheriaWizardCharacter> It(World); It; ++It)
			{
				if (It->bIsAlive && FVector::Dist2D(It->GetActorLocation(), Obj.Location) <= Obj.RadiusUU)
				{
					WizardsInside++;
				}
			}
			if (WizardsInside > 0)
			{
				Obj.Progress = FMath::Min(100.0f, Obj.Progress + RatePerSec * (1.0f + 0.25f * (WizardsInside - 1)) * Dt);
				if (Obj.Progress >= 100.0f)
				{
					Obj.bCompleted = true;
					CompletedCount++;
					UAetheriaMaterialLibrary::PlayGameSound2D(World, TEXT("SFX_BeaconCapture"), 0.85f);
				}
			}
			SumProgress += Obj.Progress;
		}
		GS->ActProgress = GS->Objectives.Num() > 0 ? (SumProgress / GS->Objectives.Num()) : 0.0f;

		// Ambush Trigger 1: Act 1 any Beacon >= 55% -> DormantConstructAwakening
		if (!bConstructAwakeningTriggered)
		{
			for (const FAetheriaObjectiveInfo& Obj : GS->Objectives)
			{
				if (Obj.Kind == EAetheriaObjectiveKind::Beacon && Obj.Progress >= 55.0f && !Obj.bCompleted)
				{
					bConstructAwakeningTriggered = true;
					int32 SwarmIdx = 0, BruiserIdx = 1;
					for (int32 i = 0; i < LocDef.Mobs.Num(); ++i)
					{
						if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Bruiser) BruiserIdx = i;
						else if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Swarm) SwarmIdx = i;
					}
					QueueAmbush(EAetheriaAmbushKind::DormantConstructAwakening, Obj.Location + FVector(200.0f, 0.0f, 0.0f), 1, BruiserIdx, true, TEXT("ВНИМАНИЕ: Пробуждение Древнего Конструкта у Маяка!"));
					QueueAmbush(EAetheriaAmbushKind::DormantConstructAwakening, Obj.Location - FVector(200.0f, 0.0f, 0.0f), 2, SwarmIdx, false, TEXT("ВНИМАНИЕ: Пробуждение Древнего Конструкта у Маяка!"));
					UAetheriaMaterialLibrary::PlayGameSound2D(World, TEXT("SFX_BossRoar"), 0.75f);
					break;
				}
			}
		}

		// Ambush Trigger 2: Act 1 Beacon completed -> ObjectiveTrapCounter once per ObjectiveId
		for (const FAetheriaObjectiveInfo& Obj : GS->Objectives)
		{
			if (Obj.bCompleted && !CounteredObjectiveIds.Contains(Obj.ObjectiveId))
			{
				CounteredObjectiveIds.Add(Obj.ObjectiveId);
				int32 SwarmIdx = 0, RangedIdx = 2;
				for (int32 i = 0; i < LocDef.Mobs.Num(); ++i)
				{
					if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Ranged) RangedIdx = i;
					else if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Swarm) SwarmIdx = i;
				}
				FVector PlateauPos = UAetheriaBiomeDatabase::FindNearestPlateauLocation(GS->CurrentLocationIndex, Obj.Location);
				if (PlateauPos.IsNearlyZero())
				{
					PlateauPos = Obj.Location + FVector(450.0f, 450.0f, 0.0f);
				}
				QueueAmbush(EAetheriaAmbushKind::ObjectiveTrapCounter, PlateauPos, 2, RangedIdx, false, TEXT("ЗАСАДА: Контратака Печати Объекта!"));
				QueueAmbush(EAetheriaAmbushKind::ObjectiveTrapCounter, Obj.Location, 2, SwarmIdx, false, TEXT("ЗАСАДА: Контратака Печати Объекта!"));
			}
		}

		if (!bGraceActive && BreatherTimer <= 0.0f && MobSpawnTimer <= 0.0f && MobCount < MaxActiveMobs)
		{
			SpawnWaveAroundActiveObjective(LocDef, EAetheriaObjectiveKind::Beacon);
			MobSpawnTimer = 4.5f;
		}
		if (CompletedCount >= GS->Objectives.Num() && GS->Objectives.Num() > 0)
		{
			SpawnStageWorldVFX(TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Shockwave"),
				GS->Objectives.Last().Location + FVector(0.0f, 0.0f, 15.0f), FRotator::ZeroRotator, FVector(2.2f), false);
			SetAct(2);
		}
		break;
	}
	case 2:
	{
		if (GS->Objectives.Num() > 0)
		{
			FAetheriaObjectiveInfo& Obelisk = GS->Objectives[0];
			const float RatePerSec = 100.0f / FMath::Max(1.0f, Timing.ObeliskChargeSeconds);
			bool bWizardNear = false;
			for (TActorIterator<AAetheriaWizardCharacter> It(World); It; ++It)
			{
				if (It->bIsAlive && FVector::Dist2D(It->GetActorLocation(), Obelisk.Location) <= Obelisk.RadiusUU)
				{
					bWizardNear = true;
					break;
				}
			}
			if (bWizardNear)
			{
				Obelisk.Progress = FMath::Min(100.0f, Obelisk.Progress + RatePerSec * Dt);
			}
			GS->ActProgress = Obelisk.Progress;
			if (BreatherTimer <= 0.0f && MobSpawnTimer <= 0.0f && MobCount < MaxActiveMobs)
			{
				SpawnSiegeWave(LocDef);
				MobSpawnTimer = 3.8f;
			}
			if (Obelisk.Progress >= 100.0f)
			{
				Obelisk.bCompleted = true;
				SpawnStageWorldVFX(TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Shockwave"), Obelisk.Location + FVector(0.0f, 0.0f, 20.0f), FRotator::ZeroRotator, FVector(3.0f), false);
				UAetheriaMaterialLibrary::PlayGameSound2D(World, TEXT("SFX_Obelisk_Complete"), 0.92f);
				SetAct(3);
			}
		}
		break;
	}
	case 3:
	{
		// Check overlapping player spells damaging Pylons
		for (TActorIterator<AAetheriaSpellProjectile> PIt(World); PIt; ++PIt)
		{
			if (PIt->bIsEnemyProjectile) continue;
			for (FAetheriaObjectiveInfo& Obj : GS->Objectives)
			{
				if (!Obj.bCompleted && FVector::Dist2D(PIt->GetActorLocation(), Obj.Location) <= 280.0f)
				{
					NotifyPylonDamaged(Obj.ObjectiveId, PIt->Damage * 0.65f, PIt->OwnerWizard.Get());
					PIt->Detonate();
					break;
				}
			}
		}
		for (TActorIterator<AAetheriaSpellZone> ZIt(World); ZIt; ++ZIt)
		{
			if (ZIt->bIsEnemy) continue;
			for (FAetheriaObjectiveInfo& Obj : GS->Objectives)
			{
				if (!Obj.bCompleted && FVector::Dist2D(ZIt->GetActorLocation(), Obj.Location) <= (ZIt->RadiusUU + 220.0f))
				{
					NotifyPylonDamaged(Obj.ObjectiveId, ZIt->TickDamage * Dt * 2.5f, ZIt->OwnerWizard.Get());
				}
			}
		}

		int32 Destroyed = 0;
		for (const FAetheriaObjectiveInfo& Obj : GS->Objectives)
		{
			if (Obj.bCompleted)
			{
				Destroyed++;

				// Ambush Trigger 2 (Act 3 Pylon destroyed -> ObjectiveTrapCounter)
				if (!CounteredObjectiveIds.Contains(Obj.ObjectiveId))
				{
					CounteredObjectiveIds.Add(Obj.ObjectiveId);
					int32 SwarmIdx = 0, RangedIdx = 2;
					for (int32 i = 0; i < LocDef.Mobs.Num(); ++i)
					{
						if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Ranged) RangedIdx = i;
						else if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Swarm) SwarmIdx = i;
					}
					FVector PlateauPos = UAetheriaBiomeDatabase::FindNearestPlateauLocation(GS->CurrentLocationIndex, Obj.Location);
					if (PlateauPos.IsNearlyZero())
					{
						PlateauPos = Obj.Location + FVector(450.0f, 450.0f, 0.0f);
					}
					QueueAmbush(EAetheriaAmbushKind::ObjectiveTrapCounter, PlateauPos, 2, RangedIdx, false, TEXT("ЗАСАДА: Контратака Печати Объекта!"));
					QueueAmbush(EAetheriaAmbushKind::ObjectiveTrapCounter, Obj.Location, 2, SwarmIdx, false, TEXT("ЗАСАДА: Контратака Печати Объекта!"));
				}
			}
		}
		GS->ActProgress = GS->Objectives.Num() > 0 ? (100.0f * Destroyed / GS->Objectives.Num()) : 0.0f;

		if (BreatherTimer <= 0.0f && MobSpawnTimer <= 0.0f && MobCount < MaxActiveMobs)
		{
			SpawnWaveAroundActiveObjective(LocDef, EAetheriaObjectiveKind::Pylon);
			MobSpawnTimer = 4.5f;
		}
		if (Destroyed >= GS->Objectives.Num() && GS->Objectives.Num() > 0)
		{
			SetAct(4);
		}
		break;
	}
	case 4:
	{
		GS->ActDescription = FString::Printf(TEXT("Уничтожьте элитный легион разлома (%d / %d)."), LegionKillsCurrent, LegionKillsTarget);
		GS->ActProgress = LegionKillsTarget > 0 ? FMath::Clamp(100.0f * LegionKillsCurrent / LegionKillsTarget, 0.0f, 100.0f) : 100.0f;

		if (BreatherTimer <= 0.0f && MobSpawnTimer <= 0.0f && MobCount < MaxActiveMobs)
		{
			SpawnLegionWave(LocDef);
			MobSpawnTimer = 3.2f;
		}
		if (LegionKillsCurrent >= LegionKillsTarget)
		{
			SetAct(5);
		}
		break;
	}
	case 5:
	{
		if (ActiveBossMob.IsValid() && !GS->bBossDefeated)
		{
			GS->BossHP = FMath::Max(0.0f, ActiveBossMob->HP);
			GS->BossMaxHP = ActiveBossMob->MaxHP;
			GS->BossOrbs = ActiveBossMob->BossOrbs;
			GS->ActProgress = GS->BossMaxHP > 0.0f ? (1.0f - GS->BossHP / GS->BossMaxHP) * 100.0f : 0.0f;

			// Ambush Trigger 4: Act 5 Boss Phase transitions (Phase 2 at 66%, Phase 3 at 33%) -> Twin RiftBreach reinforcements
			if (ActiveBossMob->BossPhase > LastTriggeredBossPhase)
			{
				LastTriggeredBossPhase = ActiveBossMob->BossPhase;
				int32 SwarmIdx = 0, BruiserIdx = 1;
				for (int32 i = 0; i < LocDef.Mobs.Num(); ++i)
				{
					if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Bruiser) BruiserIdx = i;
					else if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Swarm) SwarmIdx = i;
				}
				const FVector BossLoc = ActiveBossMob->GetActorLocation();
				const FVector LeftFlank = BossLoc + FVector(-900.0f, 600.0f, 0.0f);
				const FVector RightFlank = BossLoc + FVector(900.0f, 600.0f, 0.0f);

				QueueAmbush(
					EAetheriaAmbushKind::RiftBreach,
					LeftFlank,
					2,
					SwarmIdx,
					false,
					FString::Printf(TEXT("ФАЗА %d: Двойной Прорыв Разлома!"), LastTriggeredBossPhase)
				);
				QueueAmbush(
					EAetheriaAmbushKind::RiftBreach,
					RightFlank,
					1,
					BruiserIdx,
					true,
					FString::Printf(TEXT("ФАЗА %d: Двойной Прорыв Разлома!"), LastTriggeredBossPhase)
				);
				UAetheriaMaterialLibrary::PlayGameSound2D(World, TEXT("SFX_BossRoar"), 0.95f);
			}
		}
		break;
	}
	}

	// Ambush Trigger 3: Periodic tactical ambush during active combat (AmbushCooldownTimer)
	if (!bGraceActive && BreatherTimer <= 0.0f && CurrentAct <= 5)
	{
		AmbushCooldownTimer -= Dt;
		if (AmbushCooldownTimer <= 0.0f)
		{
			AmbushCooldownTimer = (GS->PacingMode == EAetheriaPacingMode::DemoBlitz) ? 14.0f : 22.0f;
			AAetheriaWizardCharacter* TargetWizard = nullptr;
			for (TActorIterator<AAetheriaWizardCharacter> It(World); It; ++It)
			{
				if (It->bIsAlive)
				{
					TargetWizard = *It;
					break;
				}
			}
			if (TargetWizard && MobCount < MaxActiveMobs)
			{
				int32 SwarmIdx = 0;
				for (int32 i = 0; i < LocDef.Mobs.Num(); ++i)
				{
					if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Swarm) SwarmIdx = i;
				}
				const bool bTrenchLoc = (GS->CurrentLocationIndex == 1 || GS->CurrentLocationIndex == 2 || GS->CurrentLocationIndex == 5 || GS->CurrentLocationIndex == 7);
				if (TargetWizard->bInSunkenTrench || bTrenchLoc)
				{
					QueueAmbush(
						EAetheriaAmbushKind::SubterraneanEruption,
						TargetWizard->GetActorLocation(),
						3,
						SwarmIdx,
						false,
						TEXT("ТРЕВОГА: Подземное Извержение под магами!")
					);
				}
				else
				{
					const float FlankSign = (FMath::FRand() < 0.5f) ? 1.0f : -1.0f;
					const FRotator WizRot = TargetWizard->GetActorRotation();
					const FRotator FlankRot = WizRot + FRotator(0.0f, FlankSign * 110.0f, 0.0f);
					const FVector FlankPos = TargetWizard->GetActorLocation() + FlankRot.Vector() * 850.0f;
					QueueAmbush(
						EAetheriaAmbushKind::RiftBreach,
						FlankPos,
						3,
						SwarmIdx,
						false,
						TEXT("ТРЕВОГА: Фланговый Прорыв Разлома!")
					);
				}
			}
		}
	}
}

void AAetheriaGameMode::SimulateHazards(const FAetheriaLocationDef& LocDef, float DeltaTime)
{
	UWorld* World = GetWorld();
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!World || !GS) return;

	HazardTimer += DeltaTime;

	if (LocDef.HazardKind == EAetheriaHazardKind::TriadPhase)
	{
		TriadPhaseTimer += DeltaTime;
		if (TriadPhaseTimer >= 12.0f)
		{
			TriadPhaseTimer = 0.0f;
			const uint8 NextOrb = (static_cast<uint8>(GS->ActiveTriadPhaseOrb) + 1) % 3;
			GS->ActiveTriadPhaseOrb = static_cast<EAetheriaOrb>(NextOrb);
			GS->SetSynergyBanner(FString::Printf(TEXT("Фаза Триады: усилена стихия [%s] (+35%% урона)!"),
				*AetheriaMindColors::GetOrbLetter(GS->ActiveTriadPhaseOrb)));
		}
	}

	if (HazardTimer >= 3.0f)
	{
		HazardTimer = 0.0f;

		if (LocDef.HazardKind == EAetheriaHazardKind::TitanAnvil)
		{
			if (IsValid(BiomeBuilder))
			{
				BiomeBuilder->TriggerBossSlamVisual(FVector::ZeroVector, 1400.0f, LocDef.AccentColor);
			}
		}
		else if (LocDef.HazardKind == EAetheriaHazardKind::MirrorPylon)
		{
			AAetheriaLightningArcVFX::SpawnShockwaveRing(World, FVector::ZeroVector, 650.0f, AetheriaMindColors::MindViolet, 0.6f);
		}

		for (TActorIterator<AAetheriaWizardCharacter> It(World); It; ++It)
		{
			AAetheriaWizardCharacter* Wiz = *It;
			if (!Wiz || !Wiz->bIsAlive) continue;

			switch (LocDef.HazardKind)
			{
			case EAetheriaHazardKind::EtherGeyser:
			{
				const float ManaGain = Wiz->bInSunkenTrench ? 7.5f : 5.0f;
				Wiz->Mana = FMath::Min(Wiz->MaxMana, Wiz->Mana + ManaGain);
				break;
			}
			case EAetheriaHazardKind::MagmaTide:
			{
				if (Wiz->bOnHighGround)
				{
					break;
				}
				const bool bHasTempest = Wiz->ActiveOrbs.Contains(EAetheriaOrb::Tempest);
				Wiz->ApplyDamageToWizard(bHasTempest ? 4.0f : 12.0f);
				break;
			}
			case EAetheriaHazardKind::FrostDrift:
			{
				if (Wiz->ActiveOrbs.Contains(EAetheriaOrb::Ignis))
				{
					Wiz->ColdStacks = 0;
				}
				else
				{
					Wiz->ColdStacks = FMath::Min(5, Wiz->ColdStacks + 1);
				}
				if (UCharacterMovementComponent* MoveComp = Wiz->GetCharacterMovement())
				{
					MoveComp->MaxWalkSpeed = 660.0f * (1.0f - 0.08f * Wiz->ColdStacks);
				}
				break;
			}
			case EAetheriaHazardKind::GravWell:
			{
				const FVector ToCenter = (FVector::ZeroVector - Wiz->GetActorLocation());
				const FVector PullDir = ToCenter.GetSafeNormal2D();
				if (!PullDir.IsNearlyZero() && ToCenter.Size2D() > 250.0f)
				{
					Wiz->AddMovementInput(PullDir, 0.40f);
				}
				break;
			}
			case EAetheriaHazardKind::StormArc:
			{
				Wiz->ApplyDamageToWizard(8.0f);
				AAetheriaLightningArcVFX::SpawnShockwaveRing(World, Wiz->GetActorLocation(), 260.0f, AetheriaMindColors::MindCyan, 0.35f);
				break;
			}
			case EAetheriaHazardKind::MirrorPylon:
			{
				// Subtle resonance pulse handled above
				break;
			}
			case EAetheriaHazardKind::AstrolabeRay:
			{
				if (Wiz->GetActorLocation().Size2D() <= 750.0f)
				{
					Wiz->AddShield(12.0f);
					Wiz->Mana = FMath::Min(Wiz->MaxMana, Wiz->Mana + 15.0f);
				}
				break;
			}
			case EAetheriaHazardKind::TitanAnvil:
			{
				FVector KnockDir = (Wiz->GetActorLocation() - FVector::ZeroVector).GetSafeNormal2D();
				if (KnockDir.IsNearlyZero())
				{
					KnockDir = Wiz->GetActorForwardVector();
				}
				Wiz->LaunchCharacter(KnockDir * 450.0f + FVector(0.0f, 0.0f, 150.0f), false, false);
				break;
			}
			case EAetheriaHazardKind::TimeDilation:
			{
				for (float& CD : Wiz->SpellCooldowns)
				{
					CD = FMath::Max(0.0f, CD - 0.4f);
				}
				Wiz->SlotDCooldownRemaining = Wiz->GetSpellCooldownRemaining(Wiz->SlotD);
				Wiz->SlotFCooldownRemaining = Wiz->GetSpellCooldownRemaining(Wiz->SlotF);
				break;
			}
			default:
				break;
			}
		}
	}
}

void AAetheriaGameMode::SpawnBoss(const FAetheriaLocationDef& LocDef, float BossHPScale)
{
	if (!bCombatReleased) return;

	UWorld* World = GetWorld();
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!World || !GS) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	FVector BossSpot = AetheriaWorldDirector::BossArenaLocationForWorld(ActiveWorld);
	// If any living player is far from the default boss marker, place the boss cleanly ahead along the approach route
	for (TActorIterator<AAetheriaWizardCharacter> It(World); It; ++It)
	{
		if (It->bIsAlive)
		{
			if (FVector::Dist2D(It->GetActorLocation(), BossSpot) > 3200.0f)
			{
				const FVector Fwd = FVector(0.7071f, -0.7071f, 0.0f);
				BossSpot = It->GetActorLocation() + Fwd * 1050.0f;
			}
			break;
		}
	}
	const FVector GroundedBoss = ResolveGroundedStagePoint(BossSpot, BossSpot.Z);
	const FVector SpawnLoc(GroundedBoss.X, GroundedBoss.Y, GroundedBoss.Z + 305.0f);

	SpawnStageWorldVFX(TEXT("/Game/Portals/Rectangle/FirePortal/NS_FireRec"), GroundedBoss + FVector(0.0f, 0.0f, 120.0f), FRotator(0.0f, 45.0f, 0.0f), FVector(2.2f), false);
	SpawnStageWorldVFX(TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Shockwave"), GroundedBoss + FVector(0.0f, 0.0f, 20.0f), FRotator::ZeroRotator, FVector(3.2f), false);
	UAetheriaPackVFX::PlaySpawnTelegraph(this, EAetheriaMobKind::Boss, GroundedBoss, false);

	if (AAetheriaMobCharacter* Boss = World->SpawnActor<AAetheriaMobCharacter>(SpawnLoc, FRotator::ZeroRotator, Params))
	{
		Boss->InitFromArchetype(LocDef.Boss, BossHPScale, 1.0f, GS->CurrentLocationIndex);
		Boss->InitAmbushEmergence(EAetheriaAmbushKind::None, false);
		ActiveBossMob = Boss;
		GS->bBossSpawned = true;
		GS->bBossDefeated = false;
		GS->BossName = FString::Printf(TEXT("%s — %s"), AetheriaWorldDirector::BossHeroForWorld(ActiveWorld), *LocDef.Boss.DisplayName);
		GS->BossHP = Boss->HP;
		GS->BossMaxHP = Boss->MaxHP;
		GS->BossOrbs = Boss->BossOrbs;
		UAetheriaMaterialLibrary::PlayGameSound2D(World, TEXT("SFX_BossRoar"), 0.95f);
	}
}

void AAetheriaGameMode::SpawnPortalObjective()
{
	UWorld* World = GetWorld();
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!GS) return;

	ClearStageWorldVFX();

	GS->bPortalOpen = true;
	GS->ActTitle = TEXT("Хранитель Повержен — Портал Победы Открыт!");
	GS->ActDescription = TEXT("Нажмите [Enter] или войдите в сияющий Портал, чтобы перейти в следующий мир.");
	GS->ActProgress = 100.0f;
	GS->SetSynergyBanner(TEXT("ПОБЕДА! Портал в следующий мир открыт!"));

	FVector PortalAnchor = AetheriaWorldDirector::PlayerStartForWorld(World, ActiveWorld);
	if (World)
	{
		for (TActorIterator<AAetheriaWizardCharacter> It(World); It; ++It)
		{
			if (It->bIsAlive)
			{
				PortalAnchor = It->GetActorLocation() + FVector(320.0f, -320.0f, 0.0f);
				// Celebratory NS_Free_Spells_LevelUp burst on every living wizard
				SpawnStageWorldVFX(TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_LevelUp"),
					It->GetActorLocation() - FVector(0.0f, 0.0f, 60.0f), FRotator::ZeroRotator, FVector(1.8f), false);
			}
		}
	}
	const FVector GroundPortal = ResolveGroundedStagePoint(PortalAnchor, PortalAnchor.Z);

	GS->Objectives.Empty();
	FAetheriaObjectiveInfo Portal;
	Portal.ObjectiveId = 999;
	Portal.Kind = EAetheriaObjectiveKind::Portal;
	Portal.Label = TEXT("Портал Победы");
	Portal.Location = GroundPortal;
	Portal.RadiusUU = 550.0f;
	Portal.bActive = true;
	Portal.bCompleted = false;
	GS->Objectives.Add(Portal);

	if (!bVictoryPortalVFXTriggered)
	{
		bVictoryPortalVFXTriggered = true;
		SpawnStageWorldVFX(TEXT("/Game/Portals/Rounded/WaterPortal/NS_WaterPortal"),
			GroundPortal + FVector(0.0f, 0.0f, 140.0f), FRotator(0.0f, 45.0f, 0.0f), FVector(2.2f), true);
		SpawnStageWorldVFX(TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_LevelUp"),
			GroundPortal + FVector(0.0f, 0.0f, 20.0f), FRotator::ZeroRotator, FVector(2.4f), false);
	}

	UAetheriaMaterialLibrary::PlayGameSound2D(World, TEXT("SFX_PortalOpen"), 0.90f);
	PlayAnnouncerVoice(TEXT("/Game/ParagonMaleAnnouncer/Sounds/SoundWaves/MAnnouncer_Spectator_Victory_010"), 0.92f);
	PlayBiomeBGM(ActiveWorld, false);

	if (UAetheriaSaveGame* Save = UAetheriaSaveGame::LoadOrCreateSave())
	{
		Save->UnlockedHighestLocation = FMath::Max(Save->UnlockedHighestLocation, FMath::Min(9, GS->CurrentLocationIndex + 1));
		UAetheriaSaveGame::PersistSave(Save);
	}
}

void AAetheriaGameMode::NotifyMobKilled(AAetheriaMobCharacter* Mob, AActor* Killer)
{
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!GS) return;

	if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(Killer))
	{
		Wiz->Kills++;
		Wiz->Mana = FMath::Min(Wiz->MaxMana, Wiz->Mana + 6.0f);

		const EAetheriaMobKind Kind = Mob ? Mob->MobKind : EAetheriaMobKind::Swarm;
		int32 XPReward = 45;
		int32 EssenceReward = 3;
		switch (Kind)
		{
		case EAetheriaMobKind::Boss:    XPReward = 620; EssenceReward = 55; break;
		case EAetheriaMobKind::Bruiser: XPReward = 150; EssenceReward = 14; break;
		case EAetheriaMobKind::Ranged:  XPReward = 75;  EssenceReward = 6;  break;
		default:                        XPReward = 45;  EssenceReward = 3;  break;
		}

		const bool bBossKill = (Kind == EAetheriaMobKind::Boss);
		const bool bEliteKill = (Kind == EAetheriaMobKind::Bruiser);

		Wiz->RefillFlaskFromKill(bBossKill || bEliteKill);

		// Health Globe heal pulse for nearby living allies on Boss / Elite kill
		if ((bBossKill || bEliteKill) && Mob)
		{
			const FVector KillLoc = Mob->GetActorLocation();
			for (TActorIterator<AAetheriaWizardCharacter> WizIt(GetWorld()); WizIt; ++WizIt)
			{
				if (AAetheriaWizardCharacter* Ally = *WizIt)
				{
					if (Ally->bIsAlive && FVector::Dist2D(KillLoc, Ally->GetActorLocation()) <= 1400.0f)
					{
						Ally->HP = FMath::Clamp(Ally->HP + Ally->MaxHP * 0.15f, 0.0f, Ally->MaxHP);
					}
				}
			}
		}

		// Weighted Rarity Loot Drop + Pity Timer from Data/LootTables.json
		if (LoadedRarities.IsEmpty() || LoadedRelics.IsEmpty())
		{
			LoadLootTablesFromJson();
			// Cooked build without Data/LootTables.json: seed one harmless
			// entry per table so the roll math below can never index empty.
			if (LoadedRarities.IsEmpty())
			{
				FAetheriaRarityEntry Common;
				Common.Id = TEXT("Common");
				Common.NameRu = TEXT("Обычный");
				Common.Weight = 100;
				Common.PowerMult = 1.0f;
				LoadedRarities.Add(Common);
			}
			if (LoadedRelics.IsEmpty())
			{
				FAetheriaRelicEntry Generic;
				Generic.Id = TEXT("generic_shard");
				Generic.Name = TEXT("Осколок Хроно-Печати");
				Generic.Bonus = TEXT("незначительное усиление");
				LoadedRelics.Add(Generic);
			}
		}

		LootPityCounter++;
		const FAetheriaRelicEntry& Relic = LoadedRelics[FMath::RandRange(0, LoadedRelics.Num() - 1)];

		int32 ChosenRarityIdx = 0;
		if (LootPityCounter >= PityThresholdKills || bBossKill)
		{
			ChosenRarityIdx = LoadedRarities.Num() - 1; // Ancient guaranteed
			LootPityCounter = 0;
		}
		else
		{
			int32 TotalWeight = 0;
			for (const FAetheriaRarityEntry& R : LoadedRarities)
			{
				TotalWeight += FMath::Max(1, R.Weight);
			}
			int32 Roll = FMath::RandRange(1, FMath::Max(1, TotalWeight));
			int32 Accum = 0;
			for (int32 r = 0; r < LoadedRarities.Num(); ++r)
			{
				Accum += FMath::Max(1, LoadedRarities[r].Weight);
				if (Roll <= Accum)
				{
					ChosenRarityIdx = r;
					break;
				}
			}
		}

		const FAetheriaRarityEntry& Rarity = LoadedRarities[FMath::Clamp(ChosenRarityIdx, 0, LoadedRarities.Num() - 1)];

		if (Rarity.Id == TEXT("Ancient") || Rarity.Id == TEXT("Mythic"))
		{
			LootPityCounter = 0;
			EssenceReward += (Rarity.Id == TEXT("Ancient")) ? (bBossKill ? 40 : 25) : 15;
			Wiz->GrantRelicBonus(Relic.Id, Relic.Name, Rarity.PowerMult);
			GS->SetSynergyBanner(FString::Printf(TEXT("%s [%s — %s]: +%d XP, +%d Эссенции!"),
				*Rarity.NameRu.ToUpper(), *Relic.Name, *Relic.Bonus, XPReward, EssenceReward));
			UAetheriaMaterialLibrary::PlayGameSound2D(GetWorld(), TEXT("SFX_RelicDrop"), 0.90f);
		}
		else if (Rarity.Id == TEXT("Epic"))
		{
			LootPityCounter = 0;
			EssenceReward += 8;
			Wiz->GrantRelicBonus(Relic.Id, Relic.Name, Rarity.PowerMult);
			if (bEliteKill)
			{
				GS->SetSynergyBanner(FString::Printf(TEXT("%s [%s — %s]: +%d XP"),
					*Rarity.NameRu.ToUpper(), *Relic.Name, *Relic.Bonus, XPReward));
			}
			UAetheriaMaterialLibrary::PlayGameSound2D(GetWorld(), TEXT("SFX_RelicDrop"), 0.85f);
		}
		else if (bEliteKill)
		{
			GS->SetSynergyBanner(FString::Printf(TEXT("Элитный страж повержен: +%d XP, +%d Эссенции"), XPReward, EssenceReward));
		}

		Wiz->AwardProgression(XPReward, EssenceReward);
	}

	if (CurrentAct == 4)
	{
		LegionKillsCurrent++;
	}

	if (Mob && Mob == ActiveBossMob.Get())
	{
		GS->bBossDefeated = true;
		GS->BossHP = 0.0f;
		ActiveBossMob.Reset();

		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AAetheriaMobCharacter> It(World); It; ++It)
			{
				if (*It != Mob)
				{
					It->Destroy();
				}
			}
		}
		SpawnPortalObjective();
	}
}

void AAetheriaGameMode::NotifyPylonDamaged(int32 PylonIndex, float Damage, AActor* InstigatorWizard)
{
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!GS) return;

	for (FAetheriaObjectiveInfo& Obj : GS->Objectives)
	{
		if (Obj.ObjectiveId == PylonIndex && !Obj.bCompleted)
		{
			Obj.HP = FMath::Max(0.0f, Obj.HP - Damage);
			Obj.Progress = Obj.MaxHP > 0.0f ? (1.0f - Obj.HP / Obj.MaxHP) * 100.0f : 100.0f;
			if (Obj.HP <= 0.0f)
			{
				Obj.bCompleted = true;
				UAetheriaMaterialLibrary::PlayGameSound2D(GetWorld(), TEXT("SFX_Pylon_Shatter"), 0.90f);
			}
			break;
		}
	}
}

AAetheriaBiomeBuilder* AAetheriaGameMode::GetBiomeBuilder() const
{
	return BiomeBuilder;
}

void AAetheriaGameMode::SpawnWaveAroundActiveObjective(const FAetheriaLocationDef& LocDef, EAetheriaObjectiveKind ObjKind)
{
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!GS) return;

	FVector Anchor = FVector::ZeroVector;
	for (const FAetheriaObjectiveInfo& Obj : GS->Objectives)
	{
		if (!Obj.bCompleted && Obj.bActive)
		{
			Anchor = Obj.Location;
			break;
		}
	}
	SpawnProgressiveWave(LocDef, Anchor);
}

void AAetheriaGameMode::SpawnSiegeWave(const FAetheriaLocationDef& LocDef)
{
	if (!bCombatReleased) return;

	FVector Anchor = AetheriaWorldDirector::PlayerStartForWorld(GetWorld(), ActiveWorld);
	if (AAetheriaGameState* GS = GetGameState<AAetheriaGameState>())
	{
		if (GS->Objectives.Num() > 0)
		{
			Anchor = GS->Objectives[0].Location;
		}
	}
	SpawnProgressiveWave(LocDef, Anchor);
}

void AAetheriaGameMode::SpawnLegionWave(const FAetheriaLocationDef& LocDef)
{
	if (!bCombatReleased) return;

	UWorld* World = GetWorld();
	FVector Anchor = FVector::ZeroVector;
	if (World)
	{
		for (TActorIterator<AAetheriaWizardCharacter> It(World); It; ++It)
		{
			if (It->bIsAlive)
			{
				Anchor = It->GetActorLocation();
				break;
			}
		}
	}
	SpawnProgressiveWave(LocDef, Anchor);
}

int32 AAetheriaGameMode::SelectProgressiveArchetype(const FAetheriaLocationDef& LocDef, float ActProgress, int32 WaveNumber)
{
	if (LocDef.Mobs.IsEmpty()) return 0;

	int32 SwarmIdx = 0;
	int32 BruiserIdx = FMath::Min(1, LocDef.Mobs.Num() - 1);
	int32 RangedIdx = FMath::Min(2, LocDef.Mobs.Num() - 1);

	for (int32 i = 0; i < LocDef.Mobs.Num(); ++i)
	{
		if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Swarm) SwarmIdx = i;
		else if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Bruiser) BruiserIdx = i;
		else if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Ranged) RangedIdx = i;
	}

	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	const int32 LocIdx = GS ? GS->CurrentLocationIndex : 0;
	const int32 Act = GS ? GS->CurrentAct : 1;

	if (LocIdx == 0 && Act == 1)
	{
		// Loc 0 Act 1: mix Swarm (Buff_Black), Ranged (Buff_Blue), and Bruiser (Buff_Red)
		// right from wave 1 so every encounter has distinct visual silhouettes & combat variety.
		const float Roll = FMath::FRand();
		if (ActProgress < 35.0f)
		{
			if (Roll < 0.60f) return SwarmIdx;
			if (Roll < 0.85f) return RangedIdx;
			return BruiserIdx;
		}
		else if (ActProgress < 75.0f)
		{
			if (Roll < 0.50f) return SwarmIdx;
			if (Roll < 0.80f) return RangedIdx;
			return BruiserIdx;
		}
		else
		{
			if (Roll < 0.40f) return SwarmIdx;
			if (Roll < 0.70f) return RangedIdx;
			return BruiserIdx;
		}
	}
	else
	{
		// Loc 1..9:
		// Wave 1-2 -> mostly Swarm + 1 Ranged;
		// Wave 3+ -> Swarm + Ranged + Bruiser.
		if (WaveNumber <= 2)
		{
			return (FMath::FRand() < 0.75f) ? SwarmIdx : RangedIdx;
		}
		else
		{
			const float Roll = FMath::FRand();
			if (Roll < 0.48f) return SwarmIdx;
			if (Roll < 0.78f) return RangedIdx;
			return BruiserIdx;
		}
	}
}

void AAetheriaGameMode::SpawnProgressiveWave(const FAetheriaLocationDef& LocDef, const FVector& AnchorLocation)
{
	if (!bCombatReleased) return;

	UWorld* World = GetWorld();
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!World || !GS || LocDef.Mobs.IsEmpty()) return;

	GS->CurrentWaveNumber++;
	bBreatherReady = true;

	// Determine wave phase
	if (CurrentAct == 2 || CurrentAct == 4 || GS->ActProgress >= 70.0f)
	{
		GS->CurrentWavePhase = EAetheriaWavePhase::ClimaxSiege;
	}
	else if (CurrentAct == 1 && GS->ActProgress < 35.0f && GS->CurrentWaveNumber <= 2)
	{
		GS->CurrentWavePhase = EAetheriaWavePhase::ScoutSkirmish;
	}
	else
	{
		GS->CurrentWavePhase = EAetheriaWavePhase::TacticalEscalation;
	}

	// Threat budget
	float BaseThreat = 6.0f;
	if (CurrentAct == 1)
	{
		BaseThreat = (GS->ActProgress < 35.0f) ? 6.0f : ((GS->ActProgress < 75.0f) ? 8.5f : 12.0f);
	}
	else if (CurrentAct == 2)
	{
		BaseThreat = 11.0f;
	}
	else if (CurrentAct == 3)
	{
		BaseThreat = 9.0f;
	}
	else if (CurrentAct == 4)
	{
		BaseThreat = 13.0f;
	}

	const int32 TotalMages = FMath::Max(1, GS->ActivePlayerCount + GS->ActiveBotCount);

	float TotalLivingHP = 0.0f;
	float TotalLivingMaxHP = 0.0f;
	int32 LivingCount = 0;
	for (TActorIterator<AAetheriaWizardCharacter> It(World); It; ++It)
	{
		if (It->bIsAlive)
		{
			TotalLivingHP += It->HP;
			TotalLivingMaxHP += It->MaxHP;
			LivingCount++;
		}
	}
	const float AvgLivingHPPercent = (LivingCount > 0 && TotalLivingMaxHP > 0.0f) ? (TotalLivingHP / TotalLivingMaxHP) : 1.0f;
	float MercyFactor = 1.0f;
	if (AvgLivingHPPercent < 0.35f)
	{
		MercyFactor = 0.75f;
	}
	else if (AvgLivingHPPercent > 0.85f)
	{
		MercyFactor = 1.12f;
	}

	float ThreatBudget = BaseThreat * (1.0f + 0.38f * (TotalMages - 1)) * MercyFactor;

	// Spawn ahead along the tactical arena corridor (grounded cleanly inside playable space)
	const FVector LookFwd(0.7071f, -0.7071f, 0.0f);
	const FVector LookRight(0.7071f, 0.7071f, 0.0f);
	const float SectorSide = (FMath::RandBool() ? 1.0f : -1.0f);
	const FVector EffectiveAnchor = AnchorLocation.IsNearlyZero()
		? AetheriaWorldDirector::PlayerStartForWorld(World, ActiveWorld)
		: AnchorLocation;
	const FVector RiftCenter = ResolveGroundedStagePoint(
		EffectiveAnchor + LookFwd * FMath::FRandRange(640.0f, 980.0f) + LookRight * SectorSide * FMath::FRandRange(90.0f, 340.0f),
		EffectiveAnchor.Z);

	auto CountKind = [World](EAetheriaMobKind K) -> int32
	{
		int32 N = 0;
		for (TActorIterator<AAetheriaMobCharacter> It(World); It; ++It)
		{
			if (IsValid(*It) && It->HP > 0.0f && It->MobKind == K) ++N;
		}
		return N;
	};
	const int32 WaveN = GS->CurrentWaveNumber;
	const int32 CapSwarm = FMath::Clamp(8 + WaveN * 2, 8, 14);
	const int32 CapRanged = FMath::Clamp(2 + WaveN / 2, 2, 5);
	const int32 CapBruiser = FMath::Clamp(1 + (WaveN >= 3 ? WaveN / 3 : 0), 1, 3);
	ThreatBudget *= (1.0f + 0.12f * FMath::Max(0, WaveN - 1));

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const bool bIsClimax = (GS->CurrentWavePhase == EAetheriaWavePhase::ClimaxSiege);
	AAetheriaMobCharacter* HeaviestMob = nullptr;
	float HeaviestCost = -1.0f;

	int32 SafetyCount = 0;
	while (ThreatBudget >= 1.0f && SafetyCount < 16)
	{
		SafetyCount++;
		const int32 ArchIdx = SelectProgressiveArchetype(LocDef, GS->ActProgress, GS->CurrentWaveNumber);
		const FAetheriaMobArchetype& Arch = LocDef.Mobs[FMath::Clamp(ArchIdx, 0, LocDef.Mobs.Num() - 1)];

		float Cost = 1.0f;
		if (Arch.Kind == EAetheriaMobKind::Bruiser) Cost = 3.2f;
		else if (Arch.Kind == EAetheriaMobKind::Ranged) Cost = 1.8f;
		else Cost = 1.0f;

		if (Cost > ThreatBudget && Arch.Kind != EAetheriaMobKind::Swarm)
		{
			int32 SwarmIdx = 0;
			for (int32 i = 0; i < LocDef.Mobs.Num(); ++i)
			{
				if (LocDef.Mobs[i].Kind == EAetheriaMobKind::Swarm) { SwarmIdx = i; break; }
			}
			const FAetheriaMobArchetype& SwarmArch = LocDef.Mobs[SwarmIdx];
			Cost = 1.0f;
			ThreatBudget -= Cost;

			const float OffsetAngle = FMath::FRandRange(0.0f, 2.0f * PI);
			const float OffsetDist = FMath::FRandRange(40.0f, 260.0f);
			FVector SpawnPos = RiftCenter + FVector(FMath::Cos(OffsetAngle) * OffsetDist, FMath::Sin(OffsetAngle) * OffsetDist, 0.0f);
			SpawnPos.Z = UAetheriaBiomeDatabase::GroundZAt(this, GS->CurrentLocationIndex, FVector2D(SpawnPos.X, SpawnPos.Y)) + 100.0f;

			if (CountKind(EAetheriaMobKind::Swarm) < CapSwarm)
			{
				QueueAmbush(EAetheriaAmbushKind::SubterraneanEruption, SpawnPos, 1, SwarmIdx, false, TEXT(""));
			}
			continue;
		}

		ThreatBudget -= Cost;

		FVector SpawnPos;
		if (Arch.Kind == EAetheriaMobKind::Ranged)
		{
			const FVector PlateauPos = UAetheriaBiomeDatabase::FindNearestPlateauLocation(GS->CurrentLocationIndex, RiftCenter);
			if (!PlateauPos.IsNearlyZero())
			{
				const float PlateauAngle = FMath::FRandRange(0.0f, 2.0f * PI);
				const float PlateauDist = FMath::FRandRange(30.0f, 160.0f);
				SpawnPos = PlateauPos + FVector(FMath::Cos(PlateauAngle) * PlateauDist, FMath::Sin(PlateauAngle) * PlateauDist, 0.0f);
			}
			else
			{
				const float OffsetAngle = FMath::FRandRange(0.0f, 2.0f * PI);
				const float OffsetDist = FMath::FRandRange(60.0f, 280.0f);
				SpawnPos = RiftCenter + FVector(FMath::Cos(OffsetAngle) * OffsetDist, FMath::Sin(OffsetAngle) * OffsetDist, 0.0f);
			}
		}
		else
		{
			const float OffsetAngle = FMath::FRandRange(0.0f, 2.0f * PI);
			const float OffsetDist = FMath::FRandRange(60.0f, 280.0f);
			SpawnPos = RiftCenter + FVector(FMath::Cos(OffsetAngle) * OffsetDist, FMath::Sin(OffsetAngle) * OffsetDist, 0.0f);
		}

		SpawnPos.Z = UAetheriaBiomeDatabase::GroundZAt(this, GS->CurrentLocationIndex, FVector2D(SpawnPos.X, SpawnPos.Y)) + 100.0f;

		const bool bEliteWave = (WaveN > 0 && (WaveN % 3) == 0 && Arch.Kind != EAetheriaMobKind::Swarm);
		int32 Living = CountKind(Arch.Kind);
		const int32 Cap = (Arch.Kind == EAetheriaMobKind::Bruiser) ? CapBruiser
			: (Arch.Kind == EAetheriaMobKind::Ranged) ? CapRanged : CapSwarm;
		if (Living >= Cap)
		{
			continue;
		}
		EAetheriaAmbushKind AmbKind = EAetheriaAmbushKind::SubterraneanEruption;
		if (Arch.Kind == EAetheriaMobKind::Ranged) AmbKind = EAetheriaAmbushKind::RiftBreach;
		else if (Arch.Kind == EAetheriaMobKind::Bruiser) AmbKind = EAetheriaAmbushKind::DormantConstructAwakening;
		if (bEliteWave) AmbKind = EAetheriaAmbushKind::ObjectiveTrapCounter;
		QueueAmbush(AmbKind, SpawnPos, 1, ArchIdx, bEliteWave, TEXT(""));
		if (Cost > HeaviestCost)
		{
			HeaviestCost = Cost;
		}
	}
}

void AAetheriaGameMode::QueueAmbush(
	EAetheriaAmbushKind Kind,
	const FVector& Location,
	int32 SpawnCount,
	int32 ArchIndex,
	bool bElite,
	const FString& AlertText)
{
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!GS) return;

	FAetheriaPendingAmbush Ambush;
	Ambush.Kind = Kind;
	Ambush.Location = Location;
	Ambush.TelegraphRemaining = 1.0f;
	Ambush.SpawnCount = SpawnCount;
	Ambush.MobArchetypeIndex = ArchIndex;
	Ambush.bSpawnElite = bElite;
	Ambush.AlertText = AlertText;
	PendingAmbushes.Add(Ambush);

	const FAetheriaLocationDef LocDef = UAetheriaBiomeDatabase::GetLocation(GS->CurrentLocationIndex);
	if (IsValid(BiomeBuilder))
	{
		BiomeBuilder->TriggerAmbushTelegraphVisual(Location, 550.0f, LocDef.AccentColor);
	}
	{
		EAetheriaMobKind TelegraphKind = EAetheriaMobKind::Swarm;
		if (LocDef.Mobs.IsValidIndex(ArchIndex)) TelegraphKind = LocDef.Mobs[ArchIndex].Kind;
		UAetheriaPackVFX::PlaySpawnTelegraph(this, TelegraphKind, Location, bElite);
	}

	GS->ActiveAmbushAlert = AlertText;
}

void AAetheriaGameMode::TickPendingAmbushes(float DeltaTime)
{
	UWorld* World = GetWorld();
	AAetheriaGameState* GS = GetGameState<AAetheriaGameState>();
	if (!World || !GS || PendingAmbushes.IsEmpty()) return;

	const FAetheriaLocationDef LocDef = UAetheriaBiomeDatabase::GetLocation(GS->CurrentLocationIndex);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 i = PendingAmbushes.Num() - 1; i >= 0; --i)
	{
		FAetheriaPendingAmbush& Pending = PendingAmbushes[i];
		Pending.TelegraphRemaining -= DeltaTime;
		if (Pending.TelegraphRemaining <= 0.0f)
		{
			const int32 SafeArchIdx = FMath::Clamp(Pending.MobArchetypeIndex, 0, LocDef.Mobs.Num() - 1);
			const FAetheriaMobArchetype& Arch = LocDef.Mobs[SafeArchIdx];

			for (int32 k = 0; k < Pending.SpawnCount; ++k)
			{
				const float Angle = (2.0f * PI * k) / FMath::Max(1, Pending.SpawnCount) + FMath::FRandRange(-0.2f, 0.2f);
				const float MinSpread = (Pending.SpawnCount > 1) ? FMath::Max(160.0f, Arch.CollisionRadiusUU * 2.4f) : 40.0f;
				const float Dist = FMath::FRandRange(MinSpread, MinSpread + 160.0f);
				FVector SpawnPos = Pending.Location + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.0f);
				const float ExpectedHalfH = (Arch.Kind == EAetheriaMobKind::Boss)
					? 300.0f
					: (Arch.CollisionRadiusUU * (Arch.Kind == EAetheriaMobKind::Bruiser ? 2.0f : 1.4f) * (Pending.bSpawnElite ? 1.22f : 1.0f));
				SpawnPos.Z = UAetheriaBiomeDatabase::GroundZAt(this, GS->CurrentLocationIndex, FVector2D(SpawnPos.X, SpawnPos.Y)) + ExpectedHalfH + 16.0f;

				if (AAetheriaMobCharacter* Mob = World->SpawnActor<AAetheriaMobCharacter>(SpawnPos, FRotator::ZeroRotator, Params))
				{
					const float HPScale = Pending.bSpawnElite ? 1.45f : 1.0f;
					const float DmgScale = Pending.bSpawnElite ? 1.25f : 1.0f;
					Mob->InitFromArchetype(Arch, HPScale, DmgScale, GS->CurrentLocationIndex);
					Mob->InitAmbushEmergence(Pending.Kind, Pending.bSpawnElite);
				}
			}

			PendingAmbushes.RemoveAt(i);
		}
	}

	if (PendingAmbushes.IsEmpty())
	{
		GS->ActiveAmbushAlert.Empty();
	}
}

void AAetheriaGameMode::LoadLootTablesFromJson()
{
	LoadedRarities.Empty();
	LoadedRelics.Empty();

	FString JsonStr;
	const FString CandidateA = FPaths::ProjectDir() / TEXT("Data/LootTables.json");
	const FString CandidateB = FPaths::ProjectContentDir() / TEXT("Data/LootTables.json");

	if (!FFileHelper::LoadFileToString(JsonStr, *CandidateA))
	{
		FFileHelper::LoadFileToString(JsonStr, *CandidateB);
	}

	if (!JsonStr.IsEmpty())
	{
		TSharedPtr<FJsonObject> RootObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
		if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
		{
			if (RootObj->HasField(TEXT("pity_timer_kills")))
			{
				PityThresholdKills = FMath::Max(5, RootObj->GetIntegerField(TEXT("pity_timer_kills")));
			}

			const TArray<TSharedPtr<FJsonValue>>* RarityArr = nullptr;
			if (RootObj->TryGetArrayField(TEXT("rarities"), RarityArr) && RarityArr)
			{
				for (const TSharedPtr<FJsonValue>& Val : *RarityArr)
				{
					if (const TSharedPtr<FJsonObject> Obj = Val->AsObject())
					{
						FAetheriaRarityEntry Entry;
						Entry.Id = Obj->GetStringField(TEXT("id"));
						Entry.NameRu = Obj->GetStringField(TEXT("name_ru"));
						Entry.Weight = Obj->GetIntegerField(TEXT("weight"));
						Entry.PowerMult = static_cast<float>(Obj->GetNumberField(TEXT("power_mult")));
						LoadedRarities.Add(Entry);
					}
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* RelicArr = nullptr;
			if (RootObj->TryGetArrayField(TEXT("relics"), RelicArr) && RelicArr)
			{
				for (const TSharedPtr<FJsonValue>& Val : *RelicArr)
				{
					if (const TSharedPtr<FJsonObject> Obj = Val->AsObject())
					{
						FAetheriaRelicEntry Entry;
						Entry.Id = Obj->GetStringField(TEXT("id"));
						Entry.Name = Obj->GetStringField(TEXT("name"));
						Entry.Bonus = Obj->GetStringField(TEXT("bonus"));
						LoadedRelics.Add(Entry);
					}
				}
			}
		}
	}

	if (LoadedRarities.IsEmpty())
	{
		LoadedRarities = {
			{ TEXT("Common"),  TEXT("Обычная"),          55, 1.00f },
			{ TEXT("Rare"),    TEXT("Редкая"),           26, 1.18f },
			{ TEXT("Epic"),    TEXT("Эпическая"),        13, 1.38f },
			{ TEXT("Mythic"),  TEXT("Мифическая"),        5, 1.65f },
			{ TEXT("Ancient"), TEXT("Древняя Реликвия"),  1, 2.00f }
		};
	}
	if (LoadedRelics.IsEmpty())
	{
		LoadedRelics = {
			{ TEXT("ember_heart"),      TEXT("Угольное Сердце Кальдеры"),     TEXT("+12% урона Игниса (Q)") },
			{ TEXT("chrono_astrolabe"), TEXT("Малый Астролябий Вечности"),     TEXT("-10% перезарядки Хроносферы") },
			{ TEXT("singularity_seal"), TEXT("Печать Горизонта Событий"),     TEXT("+15% радиус Чёрной Дыры") },
			{ TEXT("prism_monocle"),    TEXT("Хрустальная Призма Лабиринта"), TEXT("+18% урона от расщепления") },
			{ TEXT("archon_crown"),     TEXT("Осколок Короны Архимага"),      TEXT("+25 Макс. Маны и +12% ко всем стихиям") }
		};
	}
}

void AAetheriaGameMode::ToggleFriendlyFire()
{
	if (AAetheriaGameState* GS = GetGameState<AAetheriaGameState>())
	{
		GS->bFriendlyFireEnabled = !GS->bFriendlyFireEnabled;
		GS->FriendlyFireScale = 0.30f;
		GS->SetSynergyBanner(GS->bFriendlyFireEnabled
			? TEXT("ОГОНЬ ПО СВОИМ [F6]: ВКЛЮЧЕН (30% урона союзникам)")
			: TEXT("ОГОНЬ ПО СВОИМ [F6]: ВЫКЛЮЧЕН (0% урона союзникам)"));
	}
}


namespace
{
	// Demo-map sky meshes (SM_SkyFloor: a 3.4 km box with BlockAllDynamic,
	// SM_SkySphere) came across with collision. The sky-floor box encloses
	// the playable area, so the hero spawned *inside* it: CharacterMovement
	// found no floor (start-penetrating) and the hero sank / fell through.
	int32 AetheriaDisableSkyCollision(UWorld* World)
	{
		int32 N = 0;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			UStaticMeshComponent* C = It->GetStaticMeshComponent();
			if (!C || !C->GetStaticMesh() || C->GetCollisionEnabled() == ECollisionEnabled::NoCollision) continue;
			const FString MeshName = C->GetStaticMesh()->GetName();
			const bool bSky = MeshName.Contains(TEXT("Sky")) || MeshName.Contains(TEXT("Skydome"));
			if (bSky)
			{
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				++N;
				UE_LOG(LogTemp, Display, TEXT("[Ground] disabled collision on sky/backdrop mesh %s (%s)"), *It->GetName(), *MeshName);
			}
		}
		return N;
	}
}

void AAetheriaGameMode::StartMainMenu()
{
	bInMainMenu = true;
	bCombatReleased = false;
	// Courtyard is baked into AetheriaWorld. Do not stream a dungeon under the menu.
	if (UWorld* World = GetWorld())
	{
		FAetheriaLoadingScreenController::DismissWhenReady(World, /*bImmediate=*/true);
		AetheriaWorldDirector::PatchBrokenMaterials(World, EAetheriaWorldId::Bastion);
		AetheriaWorldDirector::ApplyLighting(World, EAetheriaWorldId::Bastion);
		AetheriaDisableSkyCollision(World);
		PlayBiomeBGM(EAetheriaWorldId::Bastion, false);
		// ArtistMenu is a Void-lineage baked courtyard; do not destroy-and-respawn it at boot.
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC) continue;
			if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(PC->GetHUD()))
			{
				Hud->ShowMainMenu();
				Hud->SetLoadingScreen(false, 0.0f, TEXT(""), TEXT(""));
			}
			if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
			{
				Wiz->SetLobbyCameraMode(true);
				const FVector Floor = AetheriaWorldDirector::PlayerStartForWorld(World, EAetheriaWorldId::Bastion);
				PlaceCharacterOnGround(Wiz, Floor.Size2D() < 2500.f ? Floor : FVector(0.f, 0.f, 110.f), TEXT("Menu"));
			}
		}
	}
}

void AAetheriaGameMode::RestampCourtyardForMenu()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Baked courtyard actors can exist in the persistent menu map (iterator
	// counts them) and still fail to draw in -game — the same class of
	// bug as the old World_Void pack-map shell. Spawning fresh Movable
	// meshes is what made the dungeon visible. Clear and restamp.
	TArray<AStaticMeshActor*> Existing;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		Existing.Add(*It);
	}
	const int32 Cleared = Existing.Num();
	for (AStaticMeshActor* Actor : Existing)
	{
		if (Actor) { Actor->Destroy(); }
	}

	auto SpawnSM = [World](const TCHAR* Path, const FVector& Loc, const FRotator& Rot, const FVector& Scale, const TCHAR* Label)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
		if (!Mesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Aetheria] [Menu] missing mesh %s"), Path);
			return;
		}
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Loc, Rot, Sp);
		if (!Actor) return;
#if WITH_EDITOR
		Actor->SetActorLabel(Label);
#endif
		Actor->SetActorScale3D(Scale);
		if (UStaticMeshComponent* C = Actor->GetStaticMeshComponent())
		{
			C->SetMobility(EComponentMobility::Movable);
			C->SetStaticMesh(Mesh);
			C->SetWorldScale3D(Scale);
			C->SetVisibility(true, true);
			C->SetHiddenInGame(false, true);
			C->InvalidateLightingCache();
			C->MarkRenderStateDirty();
		}
	};

	const TCHAR* Floor = TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Floor.SM_Floor");
	const TCHAR* Wall = TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Dungeon_Wall.SM_Dungeon_Wall");
	const TCHAR* Pillar = TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Pillar.SM_Pillar");
	const TCHAR* Gothic = TEXT("/Game/Gothic_Environment/Meshes/Gothic_buildings/SM_Gothic_building_09.SM_Gothic_building_09");

	int32 N = 0;
	for (float X : { -450.f, 150.f, 750.f })
	{
		for (float Y : { -400.f, 200.f, 800.f })
		{
			SpawnSM(Floor, FVector(X, Y, 0.f), FRotator::ZeroRotator, FVector::OneVector, *FString::Printf(TEXT("MenuFloor_%d"), N++));
		}
	}
	SpawnSM(Wall, FVector(150.f, -1000.f, 0.f), FRotator(0.f, 90.f, 0.f), FVector::OneVector, TEXT("MenuWall_0"));
	SpawnSM(Wall, FVector(150.f, 1400.f, 0.f), FRotator(0.f, 90.f, 0.f), FVector::OneVector, TEXT("MenuWall_1"));
	SpawnSM(Wall, FVector(-1050.f, 200.f, 0.f), FRotator::ZeroRotator, FVector::OneVector, TEXT("MenuWall_2"));
	SpawnSM(Wall, FVector(1350.f, 200.f, 0.f), FRotator::ZeroRotator, FVector::OneVector, TEXT("MenuWall_3"));
	SpawnSM(Pillar, FVector(150.f, -400.f, 0.f), FRotator::ZeroRotator, FVector::OneVector, TEXT("MenuPillar_0"));
	SpawnSM(Pillar, FVector(150.f, 800.f, 0.f), FRotator::ZeroRotator, FVector::OneVector, TEXT("MenuPillar_1"));
	SpawnSM(Pillar, FVector(-450.f, 200.f, 0.f), FRotator::ZeroRotator, FVector::OneVector, TEXT("MenuPillar_2"));
	SpawnSM(Pillar, FVector(750.f, 200.f, 0.f), FRotator::ZeroRotator, FVector::OneVector, TEXT("MenuPillar_3"));
	SpawnSM(Gothic, FVector(900.f, -900.f, 0.f), FRotator(0.f, 45.f, 0.f), FVector::OneVector, TEXT("MenuGothicFacade"));

	UE_LOG(LogTemp, Display, TEXT("[Aetheria] [Menu] restamped courtyard (cleared %d baked meshes, spawned 18)"), Cleared);
}

void AAetheriaGameMode::StartWorld(EAetheriaWorldId WorldId, bool bFromMenu)
{
	bInMainMenu = false;
	bCombatReleased = false;
	ActiveWorld = WorldId;
	UWorld* World = GetWorld();
	if (!World) return;

	FAetheriaLoadingScreenController::EnsureInitialized();

	const FString MapName = World->GetMapName();
	if (!AetheriaWorldDirector::MapBelongsToWorld(MapName, WorldId))
	{
		GAetheriaSkipMainMenu = true;
		GAetheriaPendingWorld = WorldId;
		if (BiomeBGMComp && BiomeBGMComp->IsPlaying())
		{
			BiomeBGMComp->FadeOut(0.9f, 0.0f);
		}
		// Display persistent dark-fantasy loading screen overlay + arm IGameMoviePlayer BEFORE OpenLevel (ZERO black frames!)
		FAetheriaLoadingScreenController::BeginWorldTravel(
			World,
			AetheriaWorldDirector::WorldDisplayName(WorldId),
			AetheriaWorldDirector::WorldSubtitle(WorldId));
		const TCHAR* Path = AetheriaWorldDirector::PersistentMapForWorld(WorldId);
		UE_LOG(LogTemp, Display, TEXT("[Aetheria] OpenLevel baked world %s (from %s)"), Path, *MapName);
		UGameplayStatics::OpenLevel(World, FName(Path));
		return;
	}

	// Destination world loaded: flush streaming, dress/light the world, snap hero to ground, then fade loading screen smoothly.
	FAetheriaLoadingScreenController::UpdateProgress(0.76f, TEXT("Сборка архитектуры, света и порталов..."));
	World->FlushLevelStreaming(EFlushLevelStreamingType::Full);

	ApplyWorldPresentation();
	FAetheriaLoadingScreenController::UpdateProgress(0.90f, TEXT("Размещение героев и пробуждение стражей..."));

	const FVector Loc = AetheriaWorldDirector::PlayerStartForWorld(World, WorldId);
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;
		if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(PC->GetHUD()))
		{
			Hud->HideLobby();
			Hud->SetShowMainMenu(false);
			Hud->SetLoadingScreen(false, 1.0f, TEXT(""), TEXT(""));
		}
		if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
		{
			Wiz->SetLobbyCameraMode(false);
			PlaceCharacterOnGround(Wiz, Loc, TEXT("StartWorld"));
		}
	}

	// Sync world index & multi-stage ARPG progression objectives to this world's actual coordinates
	if (AAetheriaGameState* GS = GetGameState<AAetheriaGameState>())
	{
		GS->CurrentLocationIndex = static_cast<int32>(WorldId);
	}
	SetAct(1);
	PlayBiomeBGM(WorldId, WorldId == EAetheriaWorldId::BossSanctum);

	FAetheriaLoadingScreenController::DismissWhenReady(World, /*bImmediate=*/false);
	UE_LOG(LogTemp, Display, TEXT("[Aetheria] StartWorld %s at %s"), AetheriaWorldDirector::WorldDisplayName(WorldId), *Loc.ToCompactString());
}

void AAetheriaGameMode::OpenActiveGate()
{
	const bool bFirstRelease = !bCombatReleased;
	bCombatReleased = true;
	bInMainMenu = false;
	if (ActiveGate.IsValid() && !ActiveGate->IsOpen())
	{
		ActiveGate->TryOpen();
	}

	// Stage 1 Entry Ambush: kick off immediate vanguard encounter + announcer welcome callout when combat starts
	if (bFirstRelease && CurrentAct == 1 && !bStageEntryAmbushSpawned)
	{
		bStageEntryAmbushSpawned = true;
		PlayAnnouncerVoice(TEXT("/Game/ParagonMaleAnnouncer/Sounds/SoundWaves/MAnnouncer_Player_Welcome_010"), 0.85f);
		if (AAetheriaGameState* GS = GetGameState<AAetheriaGameState>())
		{
			const FAetheriaLocationDef LocDef = UAetheriaBiomeDatabase::GetLocation(GS->CurrentLocationIndex);
			const FVector Anchor = (GS->Objectives.Num() > 0)
				? GS->Objectives[0].Location
				: AetheriaWorldDirector::PlayerStartForWorld(GetWorld(), ActiveWorld);
			SpawnProgressiveWave(LocDef, Anchor);
			MobSpawnTimer = 4.2f;
		}
	}
}

void AAetheriaGameMode::PlayBiomeBGM(EAetheriaWorldId WorldId, bool bBossEncounter)
{
	UWorld* World = GetWorld();
	if (!World || IsRunningDedicatedServer()) return;

	const TCHAR* TrackPath = TEXT("/Game/Audio/BGM_Sanctuary_Gothic.BGM_Sanctuary_Gothic");
	if (bBossEncounter || WorldId == EAetheriaWorldId::BossSanctum)
	{
		TrackPath = TEXT("/Game/Audio/BGM_Boss_Dread.BGM_Boss_Dread");
	}
	else if (WorldId == EAetheriaWorldId::VoidRealm)
	{
		TrackPath = TEXT("/Game/Audio/BGM_Abyssal_Expedition.BGM_Abyssal_Expedition");
	}

	USoundBase* NextTrack = LoadObject<USoundBase>(nullptr, TrackPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!NextTrack)
	{
		return;
	}

	if (ActiveBGMTrack == NextTrack && BiomeBGMComp && BiomeBGMComp->IsPlaying())
	{
		return;
	}

	if (BiomeBGMComp)
	{
		BiomeBGMComp->bAutoDestroy = true;
		BiomeBGMComp->FadeOut(1.5f, 0.0f);
		BiomeBGMComp = nullptr;
	}

	if (USoundWave* SW = Cast<USoundWave>(NextTrack))
	{
		SW->bLooping = true;
	}

	const float TargetVol = FMath::Clamp(0.38f * UAetheriaMaterialLibrary::GetGlobalAudioVolume(), 0.04f, 0.60f);
	BiomeBGMComp = UGameplayStatics::CreateSound2D(
		World,
		NextTrack,
		TargetVol,
		1.0f,
		0.0f,
		nullptr,
		/*bPersistAcrossLevelTransition=*/false,
		/*bAutoDestroy=*/false);
	if (BiomeBGMComp)
	{
		BiomeBGMComp->bIsUISound = true;
		BiomeBGMComp->bIgnoreForFlushing = true;
		BiomeBGMComp->FadeIn(1.8f, TargetVol);
		ActiveBGMTrack = NextTrack;
		UE_LOG(LogTemp, Display, TEXT("[Aetheria] Crossfading Biome BGM -> %s (vol=%.2f)"), TrackPath, TargetVol);
	}
}

void AAetheriaGameMode::PlayAnnouncerVoice(const TCHAR* SoundWavePkg, float Volume)
{
	UWorld* World = GetWorld();
	if (!World || !SoundWavePkg || IsRunningDedicatedServer()) return;
	const FString Obj = FString::Printf(TEXT("%s.%s"), SoundWavePkg, *FPackageName::GetShortName(SoundWavePkg));
	if (USoundBase* Voice = LoadObject<USoundBase>(nullptr, *Obj, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		const float EffectiveVol = FMath::Clamp(Volume * UAetheriaMaterialLibrary::GetGlobalAudioVolume(), 0.05f, 1.0f);
		UGameplayStatics::PlaySound2D(World, Voice, EffectiveVol);
	}
}

void AAetheriaGameMode::SpawnStageWorldVFX(const TCHAR* NiagaraPkg, const FVector& Loc, const FRotator& Rot, const FVector& Scale, bool bPersistentStageTag)
{
	UWorld* World = GetWorld();
	if (!World || !NiagaraPkg) return;
	const FString Obj = FString::Printf(TEXT("%s.%s"), NiagaraPkg, *FPackageName::GetShortName(NiagaraPkg));
	UNiagaraSystem* Sys = LoadObject<UNiagaraSystem>(nullptr, *Obj, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Sys) return;

	if (!bPersistentStageTag)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Sys, Loc, Rot, Scale, true, true, ENCPoolMethod::None, true);
		return;
	}

	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APointLight* Anchor = World->SpawnActor<APointLight>(Loc, Rot, Sp))
	{
		Anchor->Tags.Add(FName(TEXT("AetheriaStageVFX")));
		if (UPointLightComponent* PL = Cast<UPointLightComponent>(Anchor->GetLightComponent()))
		{
			PL->SetMobility(EComponentMobility::Movable);
			PL->SetIntensityUnits(ELightUnits::Candelas);
			PL->SetLightColor(FLinearColor(0.55f, 0.72f, 1.0f));
			PL->SetIntensity(24.0f);
			PL->SetAttenuationRadius(1100.0f);
			PL->SetCastShadows(false);
			PL->SetSpecularScale(0.18f);
		}
		if (UNiagaraComponent* NC = NewObject<UNiagaraComponent>(Anchor))
		{
			NC->SetAsset(Sys);
			NC->SetupAttachment(Anchor->GetRootComponent());
			NC->SetRelativeLocation(FVector::ZeroVector);
			NC->SetRelativeRotation(FRotator::ZeroRotator);
			NC->SetRelativeScale3D(Scale);
			NC->bAutoActivate = true;
			NC->RegisterComponent();
			NC->Activate(true);
		}
	}
}

void AAetheriaGameMode::ClearStageWorldVFX()
{
	UWorld* World = GetWorld();
	if (!World) return;
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(FName(TEXT("AetheriaStageVFX"))))
		{
			ToDestroy.Add(*It);
		}
	}
	for (AActor* A : ToDestroy)
	{
		A->Destroy();
	}
}

FVector AAetheriaGameMode::ResolveGroundedStagePoint(const FVector& DesiredXY, float DefaultZ) const
{
	UWorld* World = GetWorld();
	FVector Result(DesiredXY.X, DesiredXY.Y, DefaultZ);
	if (!World) return Result;

	FCollisionQueryParams Q(SCENE_QUERY_STAT(AetheriaStageGround), false);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		Q.AddIgnoredActor(*It);
	}
	FHitResult Hit;
	if (World->SweepSingleByChannel(
		Hit,
		FVector(DesiredXY.X, DesiredXY.Y, DefaultZ + 480.0f),
		FVector(DesiredXY.X, DesiredXY.Y, DefaultZ - 2600.0f),
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(24.0f),
		Q))
	{
		if (Hit.ImpactNormal.Z >= 0.7f && !Hit.bStartPenetrating)
		{
			Result.Z = Hit.ImpactPoint.Z;
			return Result;
		}
	}
	return Result;
}

void AAetheriaGameMode::ApplyWorldPresentation()
{
	UWorld* World = GetWorld();
	if (!World) return;
	AetheriaDisableSkyCollision(World);
	AetheriaWorldDirector::Dress(World, ActiveWorld);
	ActiveGate = AetheriaWorldDirector::SpawnGate(World, ActiveWorld);
}



namespace
{
	bool AetheriaTryFindGround(UWorld* World, ACharacter* C, const FVector& Desired, FVector& OutLoc, FString& OutWhat, float MinRing = 0.f)
	{
		const float Half = C->GetCapsuleComponent() ? C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.0f;
		const float Radius = C->GetCapsuleComponent() ? C->GetCapsuleComponent()->GetScaledCapsuleRadius() : 42.0f;
		// Simple collision only: exactly what the CharacterMovement capsule uses
		// (complex-only meshes such as grass would otherwise look like ground).
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AetheriaGround), false);
		Q.AddIgnoredActor(C);
		for (TActorIterator<APawn> It(World); It; ++It) { Q.AddIgnoredActor(*It); }
		static const float Rings[] = { 0.f, 120.f, 250.f, 450.f, 700.f, 1000.f, 1500.f, 2200.f };
		for (float R : Rings)
		{
			if (R < MinRing) continue;
			const int32 Steps = R <= 0.f ? 1 : 12;
			for (int32 i = 0; i < Steps; ++i)
			{
				const float A = (2.f * PI * i) / Steps;
				const FVector P = Desired + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.f);
				FHitResult Hit;
				// Start a little above the desired point (not from the sky:
				// Sanctum has vaults/roofs) and trace on the Pawn channel so
				// the result is exactly what CharacterMovement collides with.
				if (!World->SweepSingleByChannel(Hit, P + FVector(0.f, 0.f, 350.f), P - FVector(0.f, 0.f, 4000.f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(20.f), Q))
				{
					continue;
				}
				if (Hit.ImpactNormal.Z < 0.7f || Hit.bStartPenetrating)
				{
					continue;
				}
				const FVector Loc = Hit.ImpactPoint + FVector(0.f, 0.f, Half + 4.f);
				if (World->OverlapBlockingTestByChannel(Loc, FQuat::Identity, ECC_Pawn,
					FCollisionShape::MakeCapsule(Radius + 6.f, Half + 2.f), Q))
				{
					continue;
				}
				OutLoc = Loc;
				OutWhat = Hit.GetComponent() ? FString::Printf(TEXT("%s/%s"), *GetNameSafe(Hit.GetActor()), *Hit.GetComponent()->GetName()) : TEXT("?");
				return true;
			}
		}
		return false;
	}
}

void AAetheriaGameMode::PlaceCharacterOnGround(ACharacter* Character, const FVector& Desired, const TCHAR* Reason)
{
	UWorld* World = GetWorld();
	if (!World || !Character) return;
	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	const AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(Character);
	const bool bLobby = Wiz && Wiz->IsInLobbyCameraMode();

	FVector Loc;
	FString What;
	if (AetheriaTryFindGround(World, Character, Desired, Loc, What))
	{
		Character->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
		if (Move)
		{
			Move->StopMovementImmediately();
			if (!bLobby)
			{
				Move->GravityScale = 1.0f;
				Move->SetMovementMode(MOVE_Walking);
			}
		}
		LastSafeGround.Add(Character, Loc);
		FallingTime.Add(Character, 0.0f);
		PendingGround.Remove(Character);
		UE_LOG(LogTemp, Display, TEXT("[Ground] %s: placed %s at %s on %s (wanted %s)"),
			Reason, *Character->GetName(), *Loc.ToCompactString(), *What, *Desired.ToCompactString());
		return;
	}

	// No collision under or near the target yet: hold the hero still instead
	// of letting him fall, and let the watchdog retry.
	Character->SetActorLocation(Desired, false, nullptr, ETeleportType::TeleportPhysics);
	if (Move)
	{
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_Flying);
	}
	if (!PendingGround.Contains(Character))
	{
		PendingGroundClock = 0.0f;
	}
	PendingGround.Add(Character, Desired);
	UE_LOG(LogTemp, Warning, TEXT("[Ground] %s: NO walkable ground near %s for %s - holding and retrying"),
		Reason, *Desired.ToCompactString(), *Character->GetName());
}

void AAetheriaGameMode::TickGroundWatchdog()
{
	UWorld* World = GetWorld();
	if (!World) return;
	const float Dt = 0.2f;
	const AWorldSettings* WS = World->GetWorldSettings();
	const float KillZ = (WS && WS->bEnableWorldBoundsChecks) ? WS->KillZ : -1.0e9f;

	// Retry heroes waiting for collision.
	if (PendingGround.Num() > 0)
	{
		PendingGroundClock += Dt;
		TArray<TPair<TWeakObjectPtr<ACharacter>, FVector>> Retry;
		for (const auto& KV : PendingGround) { Retry.Add(TPair<TWeakObjectPtr<ACharacter>, FVector>(KV.Key, KV.Value)); }
		for (const auto& KV : Retry)
		{
			ACharacter* C = KV.Key.Get();
			if (!C) { PendingGround.Remove(KV.Key); continue; }
			FVector Loc;
			FString What;
			if (AetheriaTryFindGround(World, C, KV.Value, Loc, What))
			{
				PlaceCharacterOnGround(C, KV.Value, TEXT("Retry"));
			}
			else if (PendingGroundClock > 10.0f)
			{
				// Give up on this spot: fall back to the map's PlayerStart.
				const FVector Start = AetheriaWorldDirector::PlayerStartForWorld(World, ActiveWorld);
				PendingGround.Remove(KV.Key);
				UE_LOG(LogTemp, Error, TEXT("[Ground] no ground after 10s near %s, falling back to PlayerStart %s"),
					*KV.Value.ToCompactString(), *Start.ToCompactString());
				if (!Start.Equals(KV.Value, 1.0f)) { PlaceCharacterOnGround(C, Start, TEXT("Fallback")); }
			}
		}
	}

	AAetheriaWizardCharacter* Hero = nullptr;
	for (TActorIterator<AAetheriaWizardCharacter> It(World); It; ++It)
	{
		AAetheriaWizardCharacter* W = *It;
		if (!W || W->IsActorBeingDestroyed()) continue;
		if (!Hero && W->IsPlayerControlled()) { Hero = W; }
		if (PendingGround.Contains(W) || W->IsInLobbyCameraMode()) continue;
		UCharacterMovementComponent* Move = W->GetCharacterMovement();
		if (!Move) continue;
		const FVector L = W->GetActorLocation();
		if (Move->IsMovingOnGround() && Move->CurrentFloor.IsWalkableFloor())
		{
			LastSafeGround.Add(W, L);
			FallingTime.Add(W, 0.0f);
			continue;
		}
		if (Move->IsMovingOnGround())
		{
			// Walking but CharacterMovement has no floor: the capsule is
			// embedded in geometry. Move to the nearest free walkable spot.
			float& S = FallingTime.FindOrAdd(W);
			S += Dt;
			if (S > 0.6f)
			{
				FVector Loc;
				FString What;
				if (AetheriaTryFindGround(World, W, L, Loc, What, 120.f))
				{
					++GroundRescueCount;
					UE_LOG(LogTemp, Warning, TEXT("[Ground] UNSTICK %s at %s -> %s on %s"), *W->GetName(), *L.ToCompactString(), *Loc.ToCompactString(), *What);
					W->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
					Move->StopMovementImmediately();
					Move->SetMovementMode(MOVE_Walking);
					S = 0.0f;
				}
			}
			continue;
		}
		if (!Move->IsFalling()) continue;
		float& T = FallingTime.FindOrAdd(W);
		T += Dt;
		const FVector* Safe = LastSafeGround.Find(W);
		const bool bFellFar = Safe && L.Z < Safe->Z - 1200.0f;
		const bool bNearKill = L.Z < KillZ + 1500.0f;
		if (T > 1.6f || bFellFar || bNearKill)
		{
			++GroundRescueCount;
			const FVector Target = Safe ? *Safe : AetheriaWorldDirector::PlayerStartForWorld(World, ActiveWorld);
			UE_LOG(LogTemp, Warning, TEXT("[Ground] RESCUE %s falling %.1fs at %s -> %s"),
				*W->GetName(), T, *L.ToCompactString(), *Target.ToCompactString());
			PlaceCharacterOnGround(W, Target, TEXT("Rescue"));
		}
	}

	// Automated check for the screenshot tests: PASS once the hero has been
	// standing on walkable ground ~4s after the world started.
	static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("GrokSmoke"))
		|| FParse::Param(FCommandLine::Get(), TEXT("GrokStaffShot"))
		|| FParse::Param(FCommandLine::Get(), TEXT("GrokGroundTest"));
	if (!bTest || !Hero) return;
	GroundCheckClock += Dt;
	UCharacterMovementComponent* HM = Hero->GetCharacterMovement();
	const bool bOnFloor = HM && HM->IsMovingOnGround() && HM->CurrentFloor.IsWalkableFloor();
	if (FMath::Fmod(GroundCheckClock, 1.0f) < Dt * 0.5f)
	{
		UE_LOG(LogTemp, Display, TEXT("[GroundCheck] map=%s t=%.1f Z=%.0f mode=%d onFloor=%d lobby=%d rescues=%d"),
			*World->GetMapName(), GroundCheckClock, Hero->GetActorLocation().Z, HM ? (int32)HM->MovementMode : -1,
			bOnFloor ? 1 : 0, Hero->IsInLobbyCameraMode() ? 1 : 0, GroundRescueCount);
	}
	// Menu courtyard: the hero is parked (lobby, flying mode), so check that a
	// walkable simple-collision floor sits right under the capsule instead.
	if (!bGroundVerdictLogged && GroundCheckClock >= 3.0f && Hero->IsInLobbyCameraMode() && HM)
	{
		bGroundVerdictLogged = true;
		FFindFloorResult FF;
		HM->FindFloor(Hero->GetActorLocation(), FF, false);
		const bool bPass = FF.IsWalkableFloor() && FF.FloorDist < 25.0f && !FF.HitResult.bStartPenetrating;
		UE_LOG(LogTemp, Display, TEXT("[GroundCheck] VERDICT %s (menu floor under parked hero) map=%s Z=%.0f floor=%s dist=%.1f rescues=%d"),
			bPass ? TEXT("PASS") : TEXT("FAIL"), *World->GetMapName(), Hero->GetActorLocation().Z,
			FF.HitResult.GetActor() ? *FF.HitResult.GetActor()->GetName() : TEXT("none"), FF.FloorDist, GroundRescueCount);
	}
	if (!bGroundVerdictLogged && GroundCheckClock >= 4.0f && !Hero->IsInLobbyCameraMode())
	{
		bGroundVerdictLogged = true;
		const bool bPass = bOnFloor && PendingGround.Num() == 0;
		UE_LOG(LogTemp, Display, TEXT("[GroundCheck] VERDICT %s (%s) map=%s Z=%.0f floor=%s rescues=%d"),
			bPass ? TEXT("PASS") : TEXT("FAIL"), GroundRescueCount == 0 ? TEXT("clean spawn") : TEXT("needed rescue"), *World->GetMapName(), Hero->GetActorLocation().Z,
			(bOnFloor && HM->CurrentFloor.HitResult.GetComponent()) ? *HM->CurrentFloor.HitResult.GetComponent()->GetName() : TEXT("none"),
			GroundRescueCount);
		if (!bPass && HM)
		{
			FFindFloorResult FF;
			HM->FindFloor(Hero->GetActorLocation(), FF, false);
			const FHitResult& FH = FF.HitResult;
			UE_LOG(LogTemp, Warning, TEXT("[GroundCheck] DIAG tickEnabled=%d active=%d vel=%s cached(block=%d walk=%d dist=%.1f) fresh(block=%d walk=%d line=%d dist=%.1f n=%s comp=%s/%s startPen=%d) walkableZ=%.2f"),
				HM->IsComponentTickEnabled() ? 1 : 0, HM->IsActive() ? 1 : 0, *HM->Velocity.ToCompactString(),
				HM->CurrentFloor.bBlockingHit ? 1 : 0, HM->CurrentFloor.bWalkableFloor ? 1 : 0, HM->CurrentFloor.FloorDist,
				FF.bBlockingHit ? 1 : 0, FF.bWalkableFloor ? 1 : 0, FF.bLineTrace ? 1 : 0, FF.FloorDist, *FH.ImpactNormal.ToCompactString(),
				*GetNameSafe(FH.GetActor()), *GetNameSafe(FH.GetComponent()), FH.bStartPenetrating ? 1 : 0, HM->GetWalkableFloorZ());
			TArray<FOverlapResult> Ov;
			FCollisionQueryParams OQ(SCENE_QUERY_STAT(AetheriaGroundDiag), false, Hero);
			World->OverlapMultiByChannel(Ov, Hero->GetActorLocation(), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(160.f), OQ);
			for (const FOverlapResult& O : Ov)
			{
				if (UPrimitiveComponent* PC = O.GetComponent())
				{
					UE_LOG(LogTemp, Warning, TEXT("[GroundCheck] DIAG near %s/%s resp=%d coll=%d mob=%d"), *GetNameSafe(O.GetActor()), *PC->GetName(),
						(int32)PC->GetCollisionResponseToChannel(ECC_Pawn), (int32)PC->GetCollisionEnabled(), (int32)PC->Mobility);
				}
			}
		}
	}
}
