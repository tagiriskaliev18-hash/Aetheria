#include "AetheriaSanctumGate.h"
#include "AetheriaPackVFX.h"
#include "AetheriaGameMode.h"
#include "AetheriaWizardCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/PackageName.h"

AAetheriaSanctumGate::AAetheriaSanctumGate()
{
	PrimaryActorTick.bCanEverTick = true;
	GateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateMesh"));
	SetRootComponent(GateMesh);
	GateMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GateMesh->SetCollisionResponseToAllChannels(ECR_Block);
	GateMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	InteractSphere->SetupAttachment(GateMesh);
	InteractSphere->SetSphereRadius(420.0f);
	InteractSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractSphere->SetGenerateOverlapEvents(true);
}

void AAetheriaSanctumGate::BeginPlay()
{
	Super::BeginPlay();
	ClosedRot = GetActorRotation();
	const TCHAR* Paths[] = {
		TEXT("/Game/ParagonProps/Monolith/Dusk/Meshes/Evil_Gate_A"),
		TEXT("/Game/ParagonProps/Monolith/Dawn/Meshes/Dawn_Gate")
	};
	for (const TCHAR* Pkg : Paths)
	{
		const FString Obj = FString::Printf(TEXT("%s.%s"), Pkg, *FPackageName::GetShortName(Pkg));
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Obj, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			GateMesh->SetStaticMesh(Mesh);
			GateMesh->SetWorldScale3D(FVector(0.38f));
			break;
		}
	}
}

void AAetheriaSanctumGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPlayerNearby && !bIsOpen)
	{
		TryOpen();
	}
	if (bIsOpen && OpenAlpha < 1.0f)
	{
		OpenAlpha = FMath::Clamp(OpenAlpha + DeltaSeconds * 0.85f, 0.0f, 1.0f);
		SetActorRotation(ClosedRot + FRotator(0.f, OpenAlpha * 95.0f, 0.f));
		AddActorWorldOffset(FVector(0.f, 0.f, DeltaSeconds * 40.0f));
	}
}

void AAetheriaSanctumGate::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	if (Cast<AAetheriaWizardCharacter>(OtherActor))
	{
		bPlayerNearby = true;
	}
}

void AAetheriaSanctumGate::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	if (Cast<AAetheriaWizardCharacter>(OtherActor))
	{
		bPlayerNearby = false;
	}
}

void AAetheriaSanctumGate::TryOpen()
{
	if (bIsOpen) return;
	bIsOpen = true;
	UAetheriaPackVFX::PlaySpell(this, EAetheriaSpellId::ChronoBlink, GetActorLocation() + FVector(0, 0, 120), GetActorLocation() + FVector(0, 0, 400), TArray<FVector>());
	UAetheriaPackVFX::PlayTaggedLoop(this, TEXT("/Game/Portals/Rounded/LightningPortal/NS_Lightning"), GetActorLocation() + FVector(0, 0, 80), FRotator::ZeroRotator, FVector(1.4f), FName("AetheriaBastionDressing"));
	OnGateOpened.Broadcast();
	if (UWorld* World = GetWorld())
	{
		if (AAetheriaGameMode* GM = World->GetAuthGameMode<AAetheriaGameMode>())
		{
			GM->OpenActiveGate();
		}
	}
}
