#include "AetheriaPlayerController.h"
#include "AetheriaWizardCharacter.h"
#include "AetheriaGameMode.h"
#include "AetheriaGameState.h"
#include "AetheriaPlayerState.h"
#include "AetheriaSpellDatabase.h"
#include "AetheriaMaterialLibrary.h"
#include "AetheriaSlateHUD.h"
#include "AetheriaSaveGame.h"
#include "AetheriaBiomeDatabase.h"

#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"
#include "HAL/PlatformMisc.h"

AAetheriaTargetingReticle::AAetheriaTargetingReticle()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("ReticleRoot"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	RingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RingMesh"));
	RingMesh->SetupAttachment(Root);
	if (PlaneFinder.Succeeded())
	{
		RingMesh->SetStaticMesh(PlaneFinder.Object);
	}
	RingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RingMesh->SetCastShadow(false);

	LineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LineMesh"));
	LineMesh->SetupAttachment(Root);
	if (CylinderFinder.Succeeded())
	{
		LineMesh->SetStaticMesh(CylinderFinder.Object);
	}
	LineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LineMesh->SetCastShadow(false);
}

void AAetheriaTargetingReticle::UpdateReticle(
	bool bVisible,
	EAetheriaTargetShape Shape,
	const FVector& Origin,
	const FVector& AimPoint,
	float RadiusUU,
	float RangeUU,
	FLinearColor Color)
{
	SetActorHiddenInGame(!bVisible);
	if (!bVisible) return;

	if (!ReticleDynamicMat)
	{
		UMaterialInterface* RingBase = UAetheriaMaterialLibrary::GetArcaneRingMaterial(this);
		ReticleDynamicMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(this, RingBase, Color, Color, 3.8f, 0.90f);
		if (ReticleDynamicMat)
		{
			RingMesh->SetMaterial(0, ReticleDynamicMat);
		}
		UMaterialInterface* EmissiveBase = UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this);
		if (UMaterialInstanceDynamic* BeamMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(this, EmissiveBase, Color, Color, 4.5f, 0.72f))
		{
			LineMesh->SetMaterial(0, BeamMat);
		}
	}
	else
	{
		ReticleDynamicMat->SetVectorParameterValue(TEXT("Color"), Color);
		ReticleDynamicMat->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
	}

	RingMesh->SetVisibility(false);
	LineMesh->SetVisibility(false);

	FVector Dir = AimPoint - Origin;
	Dir.Z = 0.0f;
	const float Dist = Dir.Size();
	if (!Dir.Normalize())
	{
		Dir = FVector::ForwardVector;
	}

	switch (Shape)
	{
	case EAetheriaTargetShape::PointAoE:
	{
		RingMesh->SetVisibility(true);
		const float ClampedDist = FMath::Min(Dist, RangeUU);
		const FVector Loc = Origin + Dir * ClampedDist;
		RingMesh->SetWorldLocation(FVector(Loc.X, Loc.Y, AimPoint.Z + 6.0f));
		const float DiameterScale = FMath::Max(1.0f, (RadiusUU * 2.0f) / 100.0f);
		RingMesh->SetWorldScale3D(FVector(DiameterScale, DiameterScale, 1.0f));
		break;
	}
	case EAetheriaTargetShape::Direction:
	{
		LineMesh->SetVisibility(true);
		const float Len = FMath::Max(400.0f, RangeUU);
		const FVector Mid = Origin + Dir * (Len * 0.5f);
		LineMesh->SetWorldLocation(FVector(Mid.X, Mid.Y, Origin.Z + 8.0f));
		LineMesh->SetWorldRotation(FRotator(90.0f, Dir.Rotation().Yaw, 0.0f));
		LineMesh->SetWorldScale3D(FVector(0.14f, 0.14f, Len / 100.0f));

		RingMesh->SetVisibility(true);
		const FVector TipLoc = Origin + Dir * Len;
		RingMesh->SetWorldLocation(FVector(TipLoc.X, TipLoc.Y, Origin.Z + 6.0f));
		RingMesh->SetWorldScale3D(FVector(1.8f, 1.8f, 1.0f));
		break;
	}
	case EAetheriaTargetShape::LineWall:
	{
		LineMesh->SetVisibility(true);
		const float ClampedDist = FMath::Min(Dist, RangeUU);
		const FVector Loc = Origin + Dir * ClampedDist;
		LineMesh->SetWorldLocation(FVector(Loc.X, Loc.Y, AimPoint.Z + 8.0f));
		LineMesh->SetWorldRotation(FRotator(90.0f, Dir.Rotation().Yaw + 90.0f, 0.0f));
		LineMesh->SetWorldScale3D(FVector(0.18f, 0.18f, 9.0f));
		break;
	}
	case EAetheriaTargetShape::SelfNova:
	{
		RingMesh->SetVisibility(true);
		RingMesh->SetWorldLocation(FVector(Origin.X, Origin.Y, Origin.Z + 6.0f));
		const float DiameterScale = FMath::Max(2.0f, (RadiusUU * 2.0f) / 100.0f);
		RingMesh->SetWorldScale3D(FVector(DiameterScale, DiameterScale, 1.0f));
		break;
	}
	default:
		break;
	}
}

AAetheriaPlayerController::AAetheriaPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

AAetheriaMoveMarker::AAetheriaMoveMarker()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("MoveMarkerRoot"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	for (int32 i = 0; i < 4; ++i)
	{
		const FName MeshName(*FString::Printf(TEXT("ArrowMesh_%d"), i));
		ArrowMeshes[i] = CreateDefaultSubobject<UStaticMeshComponent>(MeshName);
		ArrowMeshes[i]->SetupAttachment(Root);
		if (ConeFinder.Succeeded())
		{
			ArrowMeshes[i]->SetStaticMesh(ConeFinder.Object);
		}
		ArrowMeshes[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ArrowMeshes[i]->SetCastShadow(false);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));

	GroundRingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundRingMesh"));
	GroundRingMesh->SetupAttachment(Root);
	if (PlaneFinder.Succeeded())
	{
		GroundRingMesh->SetStaticMesh(PlaneFinder.Object);
	}
	else if (CylinderFinder.Succeeded())
	{
		GroundRingMesh->SetStaticMesh(CylinderFinder.Object);
	}
	GroundRingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroundRingMesh->SetCastShadow(false);
	GroundRingMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 3.0f));
	GroundRingMesh->SetRelativeScale3D(FVector(1.05f, 1.05f, 1.0f));

	MarkerLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MarkerLight"));
	MarkerLight->SetupAttachment(Root);
	MarkerLight->SetRelativeLocation(FVector(0.0f, 0.0f, 48.0f));
	MarkerLight->SetLightColor(AetheriaMindColors::MindCyan);
	MarkerLight->SetIntensity(1450.0f);
	MarkerLight->SetAttenuationRadius(150.0f);
	MarkerLight->SetCastShadows(false);
}

void AAetheriaMoveMarker::BeginPlay()
{
	Super::BeginPlay();

	if (UStaticMesh* Chevron = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_StrategyMoveArrow"), TEXT("")))
	{
		bUsingChevronMesh = true;
		for (int32 i = 0; i < 4; ++i)
		{
			if (ArrowMeshes[i])
			{
				ArrowMeshes[i]->SetStaticMesh(Chevron);
			}
		}
	}

	UMaterialInterface* EmissiveBase = UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this);
	UMaterialInterface* RingBase = UAetheriaMaterialLibrary::GetArcaneRingMaterial(this);
	const FLinearColor CyanArrows = FLinearColor(0.0f, 0.96f, 1.0f, 1.0f);
	const FLinearColor AntiqueGold = FLinearColor(0.94f, 0.74f, 0.32f, 1.0f);
	MarkerDynMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(this, EmissiveBase, CyanArrows, CyanArrows, 3.6f, 0.92f);
	if (MarkerDynMat)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			if (ArrowMeshes[i]) ArrowMeshes[i]->SetMaterial(0, MarkerDynMat);
		}
	}
	if (UMaterialInstanceDynamic* RingMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(this, RingBase ? RingBase : EmissiveBase, AntiqueGold, AntiqueGold, 4.2f, 0.92f))
	{
		if (GroundRingMesh) GroundRingMesh->SetMaterial(0, RingMat);
	}

	SetActorHiddenInGame(true);
}

void AAetheriaMoveMarker::PlayClickOrder(const FVector& TargetLocation)
{
	int32 LocIdx = 0;
	if (UWorld* World = GetWorld())
	{
		if (AAetheriaGameState* GS = World->GetGameState<AAetheriaGameState>())
		{
			LocIdx = GS->CurrentLocationIndex;
		}
	}

	const float ZCenter = UAetheriaBiomeDatabase::GroundZAt(this, LocIdx, FVector2D(TargetLocation.X, TargetLocation.Y));
	const float ZXPlus = UAetheriaBiomeDatabase::GroundZAt(this, LocIdx, FVector2D(TargetLocation.X + 28.0f, TargetLocation.Y));
	const float ZYPlus = UAetheriaBiomeDatabase::GroundZAt(this, LocIdx, FVector2D(TargetLocation.X, TargetLocation.Y + 28.0f));
	const FVector TangentX(28.0f, 0.0f, ZXPlus - ZCenter);
	const FVector TangentY(0.0f, 28.0f, ZYPlus - ZCenter);
	const FVector SurfaceNormal = FVector::CrossProduct(TangentX, TangentY).GetSafeNormal();
	const FRotator SlopeRot = SurfaceNormal.IsNearlyZero()
		? FRotator::ZeroRotator
		: FRotationMatrix::MakeFromZX(SurfaceNormal, FVector::ForwardVector).Rotator();

	SetActorLocation(FVector(TargetLocation.X, TargetLocation.Y, ZCenter + 2.5f));
	SetActorRotation(SlopeRot);
	SetActorHiddenInGame(false);
	ConvergeTimer = 0.0f;
	bAnimConverging = true;
	bMoveDestinationActive = true;

	for (int32 i = 0; i < 4; ++i)
	{
		if (ArrowMeshes[i])
		{
			ArrowMeshes[i]->SetVisibility(true);
		}
	}
	if (GroundRingMesh)
	{
		GroundRingMesh->SetVisibility(true);
	}
}

void AAetheriaMoveMarker::SetMovingActive(bool bActive)
{
	bMoveDestinationActive = bActive;
	if (!bActive && !bAnimConverging)
	{
		SetActorHiddenInGame(true);
	}
}

void AAetheriaMoveMarker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsHidden()) return;

	if (bAnimConverging)
	{
		ConvergeTimer += DeltaTime;
		const float RawT = FMath::Clamp(ConvergeTimer / ConvergeDuration, 0.0f, 1.0f);
		// Cubic ease-out for crisp RTS click snap
		const float Eased = 1.0f - FMath::Pow(1.0f - RawT, 3.0f);

		const float Radius = FMath::Lerp(95.0f, 18.0f, Eased);
		const float ZPos = FMath::Lerp(34.0f, 5.0f, Eased);
		const float ArrowScale = FMath::Lerp(0.34f, 0.22f, Eased);

		for (int32 i = 0; i < 4; ++i)
		{
			if (!ArrowMeshes[i]) continue;
			const float AngleDeg = i * 90.0f;
			const float AngleRad = FMath::DegreesToRadians(AngleDeg);
			const FVector RelLoc(FMath::Cos(AngleRad) * Radius, FMath::Sin(AngleRad) * Radius, ZPos);
			ArrowMeshes[i]->SetRelativeLocation(RelLoc);

			if (bUsingChevronMesh)
			{
				ArrowMeshes[i]->SetRelativeRotation(FRotator(-22.0f, AngleDeg + 180.0f, 0.0f));
				ArrowMeshes[i]->SetRelativeScale3D(FVector(ArrowScale));
			}
			else
			{
				ArrowMeshes[i]->SetRelativeRotation(FRotator(-112.0f, AngleDeg, 0.0f));
				ArrowMeshes[i]->SetRelativeScale3D(FVector(0.24f, 0.24f, 0.48f) * (ArrowScale / 0.6f));
			}
		}

		if (GroundRingMesh)
		{
			const float RingScale = FMath::Lerp(0.85f, 0.42f, Eased);
			GroundRingMesh->SetRelativeScale3D(FVector(RingScale, RingScale, 0.015f));
		}

		if (RawT >= 1.0f)
		{
			bAnimConverging = false;
			for (int32 i = 0; i < 4; ++i)
			{
				if (ArrowMeshes[i]) ArrowMeshes[i]->SetVisibility(false);
			}
			if (!bMoveDestinationActive)
			{
				SetActorHiddenInGame(true);
			}
		}
	}
	else if (bMoveDestinationActive)
	{
		const float TimeSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		const float Pulse = 0.42f + 0.05f * FMath::Sin(TimeSec * 6.0f);
		if (GroundRingMesh)
		{
			GroundRingMesh->SetRelativeScale3D(FVector(Pulse, Pulse, 0.015f));
		}
	}
}

void AAetheriaPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		bShowMouseCursor = true;

		if (GEngine && !FParse::Param(FCommandLine::Get(), TEXT("Windowed")))
		{
			if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
			{
				const FIntPoint DeskRes = Settings->GetDesktopResolution();
				if (DeskRes.X >= 800 && DeskRes.Y >= 600)
				{
					Settings->SetScreenResolution(DeskRes);
				}
				if (Settings->GetFullscreenMode() != EWindowMode::WindowedFullscreen)
				{
					Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
				}
				Settings->ApplySettings(false);
			}
		}

		if (UAetheriaSaveGame* Save = UAetheriaSaveGame::LoadOrCreateSave())
		{
			GraphicsPresetIndex = Save->SavedGraphicsPreset;
			UI_ApplyGraphicsPreset(Save->SavedGraphicsPreset);
		}
		UAetheriaMaterialLibrary::SetGlobalAudioVolume(MasterVolume);

		if (UWorld* World = GetWorld())
		{
			TargetingReticle = World->SpawnActor<AAetheriaTargetingReticle>(FVector::ZeroVector, FRotator::ZeroRotator);
			if (TargetingReticle)
			{
				TargetingReticle->SetActorHiddenInGame(true);
			}

			MoveMarker = World->SpawnActor<AAetheriaMoveMarker>(FVector::ZeroVector, FRotator::ZeroRotator);
		}
	}
}

void AAetheriaPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent) return;

	InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AAetheriaPlayerController::OnKeyOrbQ);
	InputComponent->BindKey(EKeys::W, IE_Pressed, this, &AAetheriaPlayerController::OnKeyOrbW);
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AAetheriaPlayerController::OnKeyOrbE);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AAetheriaPlayerController::OnKeyInvokeR);
	InputComponent->BindKey(EKeys::D, IE_Pressed, this, &AAetheriaPlayerController::OnKeySlotD);
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AAetheriaPlayerController::OnKeySlotF);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AAetheriaPlayerController::OnKeyDashSpace);
	InputComponent->BindKey(EKeys::C, IE_Pressed, this, &AAetheriaPlayerController::OnKeyCharacterEditorC);
	InputComponent->BindKey(EKeys::H, IE_Pressed, this, &AAetheriaPlayerController::OnKeyUseFlask);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AAetheriaPlayerController::OnKeyUseFlask);
	InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &AAetheriaPlayerController::OnKeySelfCast);
	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AAetheriaPlayerController::OnKeyEmpowerEssence);
	InputComponent->BindKey(EKeys::F6, IE_Pressed, this, &AAetheriaPlayerController::OnKeyToggleFriendlyFire);

	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AAetheriaPlayerController::OnLeftMouseDown);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AAetheriaPlayerController::OnRightMouseDown);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AAetheriaPlayerController::OnRightMouseUp);

	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AAetheriaPlayerController::OnKeyGrimoireTab);
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AAetheriaPlayerController::OnKeyWorldMapM);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AAetheriaPlayerController::OnKeyEscape);
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AAetheriaPlayerController::OnKeyEnterPortal);
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AAetheriaPlayerController::OnMouseWheelUp);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AAetheriaPlayerController::OnMouseWheelDown);
}

void AAetheriaPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsLocalController()) return;

	UpdateGroundAimRay();
	if (AAetheriaWizardCharacter* AimWiz = Cast<AAetheriaWizardCharacter>(GetPawn()))
	{
		AimWiz->SetCursorAimPoint(CachedGroundAim);
	}
	UpdateMovementInput(DeltaTime);

	if (MoveMarker)
	{
		MoveMarker->SetMovingActive(bClickMoveActive);
	}

	// 200ms Input Buffer flush (input-buffering-handler)
	if (InputBufferExpireTime > 0.0 && GetWorld())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now > InputBufferExpireTime)
		{
			InputBufferExpireTime = 0.0;
			BufferedSlotIndex = -1;
			bBufferedInvoke = false;
		}
		else if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn()))
		{
			if (bBufferedInvoke)
			{
				bBufferedInvoke = false;
				InputBufferExpireTime = 0.0;
				UI_InvokeOrCast();
			}
			else if (BufferedSlotIndex >= 0 && Wiz->GetSlotCooldown(BufferedSlotIndex) <= 0.02f)
			{
				const int32 SlotToCast = BufferedSlotIndex;
				BufferedSlotIndex = -1;
				InputBufferExpireTime = 0.0;
				Wiz->ServerCastSlot(SlotToCast, CachedGroundAim);
			}
		}
	}

	if (TargetingReticle)
	{
		AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn());
		if (Wiz && ArmedSlotIndex >= 0 && PreferredCastMode == EAetheriaCastMode::InvokeThenAim)
		{
			const EAetheriaSpellId SpellId = (ArmedSlotIndex == 0) ? Wiz->SlotD : Wiz->SlotF;
			if (SpellId != EAetheriaSpellId::None)
			{
				const FAetheriaSpellDef Def = UAetheriaSpellDatabase::GetSpellDef(SpellId);
				TargetingReticle->UpdateReticle(
					true,
					Def.TargetShape,
					Wiz->GetActorLocation(),
					CachedGroundAim,
					Def.RadiusUU,
					Def.RangeUU,
					Def.PrimaryColor);
				return;
			}
		}
		TargetingReticle->UpdateReticle(false, EAetheriaTargetShape::PointAoE, FVector::ZeroVector, FVector::ZeroVector, 100.f, 100.f, AetheriaMindColors::MindCyan);
	}
}

void AAetheriaPlayerController::UpdateGroundAimRay()
{
	FVector WorldOrigin, WorldDir;
	if (DeprojectMousePositionToWorld(WorldOrigin, WorldDir))
	{
		const FVector TraceEnd = WorldOrigin + WorldDir * 50000.0f;
		FHitResult Hit;
		FCollisionQueryParams QueryParams;
		if (APawn* MyPawn = GetPawn())
		{
			QueryParams.AddIgnoredActor(MyPawn);
		}

		TArray<AActor*> AllPawns;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APawn::StaticClass(), AllPawns);
		QueryParams.AddIgnoredActors(AllPawns);

		bool bHit = GetWorld() ? GetWorld()->LineTraceSingleByChannel(Hit, WorldOrigin, TraceEnd, ECC_Visibility, QueryParams) : false;
		if (bHit && Hit.bBlockingHit)
		{
			CachedGroundAim = Hit.ImpactPoint;
		}
		else
		{
			int32 LocIdx = 0;
			if (GetWorld())
			{
				if (AAetheriaGameState* GS = GetWorld()->GetGameState<AAetheriaGameState>())
				{
					LocIdx = GS->CurrentLocationIndex;
				}
			}

			if (FMath::Abs(WorldDir.Z) > 0.001f)
			{
				const float T = -WorldOrigin.Z / WorldDir.Z;
				if (T > 0.0f && T < 50000.0f)
				{
					CachedGroundAim = WorldOrigin + WorldDir * T;
				}
			}
			CachedGroundAim.Z = UAetheriaBiomeDatabase::GroundZAt(this, LocIdx, FVector2D(CachedGroundAim.X, CachedGroundAim.Y));
		}
	}
}

void AAetheriaPlayerController::UpdateMovementInput(float DeltaTime)
{
	AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn());
	if (!Wiz || !Wiz->bIsAlive)
	{
		bClickMoveActive = false;
		bRightMouseHeld = false;
		ArmedSlotIndex = -1;
		return;
	}

	if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(GetHUD()))
	{
		if (Hud->IsAnyModalOpen()) return;
	}

	if (bRightMouseHeld)
	{
		ClickMoveDestination = CachedGroundAim;
		bClickMoveActive = true;
	}

	// Arrow Key camera-relative movement overrides click move
	float ArrowForward = 0.0f;
	float ArrowRight = 0.0f;
	if (IsInputKeyDown(EKeys::Up)) ArrowForward += 1.0f;
	if (IsInputKeyDown(EKeys::Down)) ArrowForward -= 1.0f;
	if (IsInputKeyDown(EKeys::Right)) ArrowRight += 1.0f;
	if (IsInputKeyDown(EKeys::Left)) ArrowRight -= 1.0f;

	if (FMath::Abs(ArrowForward) > 0.1f || FMath::Abs(ArrowRight) > 0.1f)
	{
		bClickMoveActive = false;
		const FRotator CamRot = PlayerCameraManager ? PlayerCameraManager->GetCameraRotation() : FRotator(0.f, -45.f, 0.f);
		const FRotator YawRot(0.f, CamRot.Yaw, 0.f);
		const FVector ForwardDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
		const FVector RightDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
		FVector MoveVec = ForwardDir * ArrowForward + RightDir * ArrowRight;
		MoveVec.Normalize();
		Wiz->AddMovementInput(MoveVec, 1.0f);
		return;
	}

	if (bClickMoveActive)
	{
		FVector ToDest = ClickMoveDestination - Wiz->GetActorLocation();
		ToDest.Z = 0.0f;
		if (ToDest.Size() <= 65.0f)
		{
			bClickMoveActive = false;
		}
		else
		{
			ToDest.Normalize();
			Wiz->AddMovementInput(ToDest, 1.0f);
		}
	}
}

void AAetheriaPlayerController::OnKeyOrbQ() { UI_PushOrb(EAetheriaOrb::Ignis); }
void AAetheriaPlayerController::OnKeyOrbW() { UI_PushOrb(EAetheriaOrb::Tempest); }
void AAetheriaPlayerController::OnKeyOrbE() { UI_PushOrb(EAetheriaOrb::Graviton); }
void AAetheriaPlayerController::OnKeyInvokeR()
{
	if (IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt))
	{
		OnKeySelfCast();
		return;
	}
	UI_InvokeOrCast();
}
void AAetheriaPlayerController::OnKeySlotD() { UI_SelectOrCastSlot(0); }
void AAetheriaPlayerController::OnKeySlotF() { UI_SelectOrCastSlot(1); }
void AAetheriaPlayerController::OnKeyDashSpace() { UI_Dash(); }
void AAetheriaPlayerController::OnKeyCharacterEditorC()
{
	if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(GetHUD()))
	{
		Hud->ToggleCharacterEditor();
	}
}
void AAetheriaPlayerController::OnKeyUseFlask()
{
	if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(GetHUD()))
	{
		if (Hud->IsAnyModalOpen()) return;
	}
	if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn()))
	{
		Wiz->ServerUseHealingFlask();
	}
}
void AAetheriaPlayerController::OnKeySelfCast()
{
	if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(GetHUD()))
	{
		if (Hud->IsAnyModalOpen()) return;
	}
	if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn()))
	{
		Wiz->ServerSelfCast();
	}
}
void AAetheriaPlayerController::OnKeyEmpowerEssence()
{
	if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(GetHUD()))
	{
		if (Hud->IsAnyModalOpen()) return;
	}
	if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn()))
	{
		Wiz->ServerEmpowerWithEssence();
	}
}
void AAetheriaPlayerController::OnKeyToggleFriendlyFire()
{
	ServerToggleFriendlyFire();
}

void AAetheriaPlayerController::OnLeftMouseDown()
{
	if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(GetHUD()))
	{
		if (Hud->IsAnyModalOpen()) return;
	}

	AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn());
	if (!Wiz || !Wiz->bIsAlive) return;

	Wiz->FaceWorldPoint(CachedGroundAim);
	if (ArmedSlotIndex >= 0 && PreferredCastMode == EAetheriaCastMode::InvokeThenAim)
	{
		Wiz->ServerCastSlot(ArmedSlotIndex, CachedGroundAim);
		ArmedSlotIndex = -1;
	}
	else
	{
		Wiz->ServerBasicAttack(CachedGroundAim);
	}
}

void AAetheriaPlayerController::OnRightMouseDown()
{
	if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(GetHUD()))
	{
		if (Hud->IsAnyModalOpen()) return;
	}

	AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn());
	if (!Wiz || !Wiz->bIsAlive) return;

	if (ArmedSlotIndex >= 0)
	{
		ArmedSlotIndex = -1;
		return;
	}

	bRightMouseHeld = true;
	ClickMoveDestination = CachedGroundAim;
	bClickMoveActive = true;

	if (MoveMarker)
	{
		MoveMarker->PlayClickOrder(CachedGroundAim);
	}
	UAetheriaMaterialLibrary::PlayGameSound2D(this, TEXT("SFX_MoveClick"), 0.52f);
}

void AAetheriaPlayerController::OnRightMouseUp()
{
	bRightMouseHeld = false;
}

void AAetheriaPlayerController::OnKeyGrimoireTab()
{
	if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(GetHUD()))
	{
		Hud->ToggleGrimoire();
	}
}

void AAetheriaPlayerController::OnKeyWorldMapM()
{
	if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(GetHUD()))
	{
		Hud->ToggleWorldMap();
	}
}

void AAetheriaPlayerController::OnKeyEscape()
{
	if (ArmedSlotIndex >= 0)
	{
		ArmedSlotIndex = -1;
		return;
	}
	if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(GetHUD()))
	{
		Hud->ToggleSettings();
	}
}

void AAetheriaPlayerController::OnKeyEnterPortal()
{
	ServerEnterPortal();
}

void AAetheriaPlayerController::UI_PushOrb(EAetheriaOrb Orb)
{
	AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn());
	if (!Wiz || !Wiz->bIsAlive) return;

	Wiz->ServerPushOrb(Orb);

	if (PreferredCastMode == EAetheriaCastMode::InstantOnCombo)
	{
		InstantComboCount++;
		if (InstantComboCount >= 3)
		{
			InstantComboCount = 0;
			Wiz->ServerInstantCastCombo(CachedGroundAim);
		}
	}
	else if (bAutoInvokeOnThree)
	{
		InstantComboCount++;
		if (InstantComboCount >= 3)
		{
			InstantComboCount = 0;
			Wiz->ServerInvoke(false, CachedGroundAim);
		}
	}
}

void AAetheriaPlayerController::UI_InvokeOrCast()
{
	AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn());
	if (!Wiz || !Wiz->bIsAlive) return;

	if (PreferredCastMode == EAetheriaCastMode::InstantOnCombo)
	{
		InstantComboCount = 0;
		Wiz->FaceWorldPoint(CachedGroundAim);
		Wiz->ServerInstantCastCombo(CachedGroundAim);
	}
	else
	{
		Wiz->ServerInvoke(false, CachedGroundAim);
	}
}

void AAetheriaPlayerController::UI_SelectOrCastSlot(int32 SlotIndex)
{
	AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn());
	if (!Wiz || !Wiz->bIsAlive) return;

	auto TryCastOrBuffer = [this, Wiz, SlotIndex]()
	{
		const float CD = Wiz->GetSlotCooldown(SlotIndex);
		if (CD > 0.02f && CD <= 0.25f && GetWorld())
		{
			BufferedSlotIndex = SlotIndex;
			InputBufferExpireTime = GetWorld()->GetTimeSeconds() + 0.22;
			return;
		}
		Wiz->FaceWorldPoint(CachedGroundAim);
		Wiz->ServerCastSlot(SlotIndex, CachedGroundAim);
	};

	if (PreferredCastMode == EAetheriaCastMode::InstantOnCombo)
	{
		TryCastOrBuffer();
		ArmedSlotIndex = -1;
		return;
	}

	if (ArmedSlotIndex == SlotIndex)
	{
		TryCastOrBuffer();
		ArmedSlotIndex = -1;
	}
	else
	{
		ArmedSlotIndex = SlotIndex;
	}
}

void AAetheriaPlayerController::UI_Dash()
{
	AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn());
	if (!Wiz || !Wiz->bIsAlive) return;

	Wiz->FaceWorldPoint(CachedGroundAim);
	Wiz->ServerDash(CachedGroundAim);
}

void AAetheriaPlayerController::UI_SetCastMode(EAetheriaCastMode NewMode)
{
	PreferredCastMode = NewMode;
	ArmedSlotIndex = -1;
	InstantComboCount = 0;
	SaveConfig();
}

void AAetheriaPlayerController::UI_SetAutoInvoke(bool bEnabled)
{
	bAutoInvokeOnThree = bEnabled;
	SaveConfig();
}

void AAetheriaPlayerController::UI_SetReducedMotion(bool bEnabled)
{
	bReducedMotion = bEnabled;
	SaveConfig();
}

void AAetheriaPlayerController::UI_SetVolume(float NewVolume)
{
	MasterVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
	SaveConfig();
	UAetheriaMaterialLibrary::SetGlobalAudioVolume(MasterVolume);
}

void AAetheriaPlayerController::UI_ApplyGraphicsPreset(int32 PresetIdx)
{
	GraphicsPresetIndex = FMath::Clamp(PresetIdx, 0, 2);
	SaveConfig();

	if (UAetheriaSaveGame* Save = UAetheriaSaveGame::LoadOrCreateSave())
	{
		Save->SavedGraphicsPreset = GraphicsPresetIndex;
		UAetheriaSaveGame::PersistSave(Save);
	}

	switch (GraphicsPresetIndex)
	{
	case 0: // Optimal3070
		ConsoleCommand(TEXT("r.ScreenPercentage 100"));
		ConsoleCommand(TEXT("r.Streaming.PoolSize 3072"));
		ConsoleCommand(TEXT("r.MipMapLODBias -0.5"));
		ConsoleCommand(TEXT("r.Lumen.DiffuseIndirect.Allow 1"));
		ConsoleCommand(TEXT("r.Shadow.Virtual.Enable 1"));
		ConsoleCommand(TEXT("r.MaxAnisotropy 16"));
		ConsoleCommand(TEXT("r.VT.MaxAnisotropy 16"));
		ConsoleCommand(TEXT("r.ContactShadows 1"));
		ConsoleCommand(TEXT("r.Tonemapper.Sharpen 0.75"));
		ConsoleCommand(TEXT("t.MaxFPS 120"));
		break;
	case 1: // UltraCinema
		ConsoleCommand(TEXT("r.ScreenPercentage 100"));
		ConsoleCommand(TEXT("r.Streaming.PoolSize 3072"));
		ConsoleCommand(TEXT("r.MipMapLODBias -0.5"));
		ConsoleCommand(TEXT("r.Lumen.DiffuseIndirect.Allow 1"));
		ConsoleCommand(TEXT("r.Shadow.Virtual.Enable 1"));
		ConsoleCommand(TEXT("r.MaxAnisotropy 16"));
		ConsoleCommand(TEXT("r.VT.MaxAnisotropy 16"));
		ConsoleCommand(TEXT("r.ContactShadows 1"));
		ConsoleCommand(TEXT("r.Tonemapper.Sharpen 0.75"));
		ConsoleCommand(TEXT("t.MaxFPS 120"));
		break;
	case 2: // Esports144
		ConsoleCommand(TEXT("r.ScreenPercentage 75"));
		ConsoleCommand(TEXT("r.Lumen.DiffuseIndirect.Allow 0"));
		ConsoleCommand(TEXT("t.MaxFPS 144"));
		break;
	default:
		break;
	}
}

void AAetheriaPlayerController::UI_UpdateCustomization(const FAetheriaCharacterCustomization& NewCust, bool bPersistToDisk)
{
	if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn()))
	{
		Wiz->Customization = NewCust;
		Wiz->ApplyCustomizationVisuals();
		if (bPersistToDisk)
		{
			Wiz->ServerSetCustomization(NewCust);
		}
	}

	if (bPersistToDisk)
	{
		if (UAetheriaSaveGame* Save = UAetheriaSaveGame::LoadOrCreateSave())
		{
			Save->SavedCustomization = NewCust;
			UAetheriaSaveGame::PersistSave(Save);
		}
	}
}

void AAetheriaPlayerController::UI_SetEditorPreviewCamera(bool bOpen, float TurntableYaw)
{
	if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn()))
	{
		Wiz->SetEditorCameraMode(bOpen, TurntableYaw);
	}
}

void AAetheriaPlayerController::UI_ToggleFullscreen()
{
	bFullscreen = !bFullscreen;
	SaveConfig();
	if (GEngine)
	{
		if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
		{
			Settings->SetFullscreenMode(bFullscreen ? EWindowMode::Fullscreen : EWindowMode::Windowed);
			Settings->ApplySettings(false);
		}
	}
}

void AAetheriaPlayerController::UI_QuitToDesktop()
{
	FPlatformMisc::RequestExit(false);
}

void AAetheriaPlayerController::OnMouseWheelUp()
{
	if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(GetHUD()))
	{
		if (Hud->IsAnyModalOpen()) return;
	}
	if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn()))
	{
		Wiz->AdjustCameraZoom(+1.0f);
	}
}

void AAetheriaPlayerController::OnMouseWheelDown()
{
	if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(GetHUD()))
	{
		if (Hud->IsAnyModalOpen()) return;
	}
	if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn()))
	{
		Wiz->AdjustCameraZoom(-1.0f);
	}
}

void AAetheriaPlayerController::UI_SetHeroArchetype(int32 ArchetypeIndex)
{
	AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn());
	if (!Wiz) return;

	static const int32 HeadPresets[6]     = { 0, 1, 2, 0, 1, 2 };
	static const int32 BackPresets[6]     = { 0, 1, 2, 0, 1, 2 };
	static const int32 StaffPresets[6]    = { 0, 0, 0, 0, 0, 0 };
	static const int32 FamiliarPresets[6] = { 0, 2, 1, 0, 2, 1 };
	static const int32 RobeDyes[6]        = { 1, 3, 6, 2, 0, 4 };
	static const int32 TrimMetals[6]      = { 0, 1, 2, 3, 0, 1 };
	static const int32 RuneGlows[6]       = { 1, 2, 0, 3, 0, 1 };

	const int32 Idx = FMath::Clamp(ArchetypeIndex, 0, 5);
	FAetheriaCharacterCustomization NewCust = Wiz->Customization;
	NewCust.BodyArchetypeIndex = Idx;
	NewCust.HeadgearIndex = HeadPresets[Idx];
	NewCust.BackIndex = BackPresets[Idx];
	NewCust.StaffIndex = StaffPresets[Idx];
	NewCust.FamiliarIndex = FamiliarPresets[Idx];
	NewCust.RobeDyeIndex = RobeDyes[Idx];
	NewCust.TrimMetalIndex = TrimMetals[Idx];
	NewCust.RuneGlowIndex = RuneGlows[Idx];
	UI_UpdateCustomization(NewCust, true);
}

void AAetheriaPlayerController::UI_StartGame(bool bHostCoop, const FString& PlayerName, int32 StartLocIndex, EAetheriaPacingMode Pacing, int32 BotCount)
{
	if (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(GetHUD()))
	{
		Hud->HideLobby();
	}
	if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn()))
	{
		Wiz->SetLobbyCameraMode(false);
	}

	if (bHostCoop && GetNetMode() == NM_Standalone)
	{
		const FString Opts = FString::Printf(TEXT("listen?Loc=%d?Bots=%d?Pacing=%d?Name=%s"),
			StartLocIndex,
			BotCount,
			static_cast<int32>(Pacing),
			*PlayerName);
		UGameplayStatics::OpenLevel(this, FName("/Engine/Maps/Entry"), true, Opts);
		return;
	}

	ServerConfigureMatch(PlayerName, StartLocIndex, Pacing, BotCount);
}

void AAetheriaPlayerController::UI_JoinByAddress(const FString& IpAddress, const FString& PlayerName)
{
	if (!IpAddress.IsEmpty())
	{
		if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn()))
		{
			Wiz->SetLobbyCameraMode(false);
		}
		ServerSetWizardName(PlayerName);
		ClientTravel(IpAddress, TRAVEL_Absolute);
	}
}

void AAetheriaPlayerController::ServerAddBot_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (AAetheriaGameMode* GM = Cast<AAetheriaGameMode>(World->GetAuthGameMode()))
		{
			GM->AddBotWizard();
		}
	}
}

void AAetheriaPlayerController::ServerRemoveBot_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (AAetheriaGameMode* GM = Cast<AAetheriaGameMode>(World->GetAuthGameMode()))
		{
			GM->RemoveBotWizard();
		}
	}
}

void AAetheriaPlayerController::ServerWarpLocation_Implementation(int32 LocationIndex, int32 Act)
{
	if (UWorld* World = GetWorld())
	{
		if (AAetheriaGameMode* GM = Cast<AAetheriaGameMode>(World->GetAuthGameMode()))
		{
			GM->WarpToLocation(LocationIndex, Act);
		}
	}
}

void AAetheriaPlayerController::ServerSetPacing_Implementation(EAetheriaPacingMode Pacing)
{
	if (UWorld* World = GetWorld())
	{
		if (AAetheriaGameMode* GM = Cast<AAetheriaGameMode>(World->GetAuthGameMode()))
		{
			GM->SetPacingMode(Pacing);
		}
	}
}

void AAetheriaPlayerController::ServerEnterPortal_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (AAetheriaGameMode* GM = Cast<AAetheriaGameMode>(World->GetAuthGameMode()))
		{
			GM->EnterAstralPortal();
		}
	}
}

void AAetheriaPlayerController::ServerSetWizardName_Implementation(const FString& NewName)
{
	if (NewName.IsEmpty()) return;
	if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(GetPawn()))
	{
		Wiz->WizardName = NewName;
	}
	if (AAetheriaPlayerState* PS = GetPlayerState<AAetheriaPlayerState>())
	{
		PS->WizardDisplayName = NewName;
	}
}

void AAetheriaPlayerController::ServerConfigureMatch_Implementation(const FString& PlayerName, int32 StartLocIndex, EAetheriaPacingMode Pacing, int32 BotCount)
{
	ServerSetWizardName_Implementation(PlayerName);

	if (UWorld* World = GetWorld())
	{
		if (AAetheriaGameMode* GM = Cast<AAetheriaGameMode>(World->GetAuthGameMode()))
		{
			GM->SetPacingMode(Pacing);
			GM->WarpToLocation(StartLocIndex, 1);

			for (int32 b = 0; b < 5; ++b)
			{
				GM->RemoveBotWizard();
			}
			for (int32 i = 0; i < FMath::Clamp(BotCount, 0, 5); ++i)
			{
				GM->AddBotWizard();
			}
		}
	}
}

void AAetheriaPlayerController::ServerToggleFriendlyFire_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (AAetheriaGameMode* GM = Cast<AAetheriaGameMode>(World->GetAuthGameMode()))
		{
			GM->ToggleFriendlyFire();
		}
	}
}
