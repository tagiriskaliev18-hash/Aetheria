#include "AetheriaAnimInstance.h"

#include "AetheriaMaterialLibrary.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/PlatformProcess.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/PackageName.h"

namespace AetheriaAnimPrivate
{
	static FString NormalizeName(const FString& In)
	{
		FString Out = In;
		Out.ReplaceInline(TEXT(" "), TEXT(""));
		Out.ReplaceInline(TEXT("_"), TEXT(""));
		Out.ReplaceInline(TEXT("-"), TEXT(""));
		return Out.ToLower();
	}

	static bool NameMatches(const FString& AssetName, const TArray<FString>& Aliases)
	{
		const FString Norm = NormalizeName(AssetName);
		for (const FString& Alias : Aliases)
		{
			const FString Needle = NormalizeName(Alias);
			if (Norm.Equals(Needle) || Norm.EndsWith(Needle))
			{
				return true;
			}
		}
		return false;
	}
}

void UAetheriaAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	ResolveAssetsIfNeeded();
}

void UAetheriaAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	APawn* Pawn = TryGetPawnOwner();
	if (!Pawn)
	{
		return;
	}

	const FVector Vel = Pawn->GetVelocity();
	Speed = Vel.Size2D();
	bIsMoving = Speed > 15.0f;

	const FVector Local = Pawn->GetActorRotation().UnrotateVector(Vel);
	const float MaxSpeed = FMath::Max(1.0f, Pawn->GetMovementComponent() ? Pawn->GetMovementComponent()->GetMaxSpeed() : 660.0f);
	StrafeY = FMath::Clamp(Local.X / MaxSpeed, -1.0f, 1.0f);
	StrafeX = FMath::Clamp(Local.Y / MaxSpeed, -1.0f, 1.0f);
	Direction = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));

	if (ACharacter* Char = Cast<ACharacter>(Pawn))
	{
		if (Char->GetMesh() && Char->GetMesh()->GetAnimationMode() != EAnimationMode::AnimationBlueprint)
		{
			// Keep AnimInstance-driven updates available even if the mesh started in single-node mode.
		}
	}

	TickLocomotionPlayback();
}

void UAetheriaAnimInstance::ConfigureFromFolder(const FString& ContentFolder)
{
	AssetFolder = ContentFolder;
	bAssetsResolved = false;
	FolderAssets.Reset();
	ResolveAssetsIfNeeded();
}

void UAetheriaAnimInstance::ResolveAssetsIfNeeded()
{
	if (bAssetsResolved)
	{
		return;
	}

	if (AssetFolder.IsEmpty())
	{
		return;
	}

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	// Early spawn can beat the async registry scan: without this the folder
	// reads as empty, HasLocomotionSequences() fails and the hero drops to a
	// native Paragon BP that nothing drives (frozen idle). Cold boots on big
	// worlds need the full scan; this runs during the loading screen anyway.
	for (int32 WaitIter = 0; ARM.Get().IsLoadingAssets() && WaitIter < 2000; ++WaitIter)
	{
		FPlatformProcess::Sleep(0.01f);
	}
	TArray<FAssetData> Assets;
	ARM.Get().GetAssetsByPath(FName(*AssetFolder), Assets, true);

	for (const FAssetData& Data : Assets)
	{
		if (UAnimationAsset* Anim = Cast<UAnimationAsset>(Data.GetAsset()))
		{
			FolderAssets.Add(Data.AssetName.ToString(), Anim);
		}
		else if (UBlendSpace* BS = Cast<UBlendSpace>(Data.GetAsset()))
		{
			if (!LocomotionBlendSpace)
			{
				LocomotionBlendSpace = BS;
			}
		}
	}

	IdleSeq = FindSequenceByNames({ TEXT("Idle"), TEXT("Idle_Relaxed"), TEXT("Idle_Long") });
	RunFwdSeq = FindSequenceByNames({ TEXT("Run_Fwd"), TEXT("RunFwd"), TEXT("Run_Forward"), TEXT("Jog_Fwd") });
	RunBackSeq = FindSequenceByNames({ TEXT("Run_Back"), TEXT("RunBack"), TEXT("Run_Backward"), TEXT("Run_Bwd"), TEXT("Jog_Bwd") });
	RunStrafeLSeq = FindSequenceByNames({ TEXT("Run_StrafeL"), TEXT("RunStrafeL"), TEXT("StrafeL"), TEXT("Run_Lft"), TEXT("Jog_Left") });
	RunStrafeRSeq = FindSequenceByNames({ TEXT("Run_StrafeR"), TEXT("RunStrafeR"), TEXT("StrafeR"), TEXT("Run_Rt"), TEXT("Jog_Right") });

	CastPrimaryMontage = FindMontageByNames({ TEXT("AM_Cast_Primary"), TEXT("Cast_Primary"), TEXT("Cast"), TEXT("RMB_Cast"), TEXT("Cosmic_Rift"), TEXT("Primary_Attack_A_Medium_Montage") });
	CastHeavyMontage = FindMontageByNames({ TEXT("AM_Cast_Heavy"), TEXT("Cast_Heavy"), TEXT("Blackhole_Start"), TEXT("BlackHole_Loop"), TEXT("CosmicRift_Execute"), TEXT("Torn_Space"), TEXT("Torn_Space_v2") });
	DashMontage = FindMontageByNames({ TEXT("AM_Dash"), TEXT("Dash") });
	AttackMeleeMontage = FindMontageByNames({ TEXT("AM_Attack_Melee"), TEXT("Attack_Melee"), TEXT("Primary_Attack_A_Medium_Montage"), TEXT("Primary_Attack_B_Medium_Montage") });
	AttackSlamMontage = FindMontageByNames({ TEXT("AM_Attack_Slam"), TEXT("Attack_Slam"), TEXT("Primary_Attack_C_Medium_Montage") });
	HitReactMontage = FindMontageByNames({ TEXT("AM_Hit_React"), TEXT("Hit_React"), TEXT("HitReact_Front") });
	DeathMontage = FindMontageByNames({ TEXT("AM_Death"), TEXT("Death"), TEXT("Death_Fwd"), TEXT("Death_Back") });

	CastPrimaryMontage = EnsureMontage(CastPrimaryMontage, FindSequenceByNames({ TEXT("Cast_Primary"), TEXT("Cast"), TEXT("RMB_Cast"), TEXT("Cosmic_Rift") }), TEXT("DefaultSlot"));
	CastHeavyMontage = EnsureMontage(CastHeavyMontage, FindSequenceByNames({ TEXT("Cast_Heavy"), TEXT("Blackhole_Start"), TEXT("Torn_Space"), TEXT("CosmicRift_Execute") }), TEXT("DefaultSlot"));
	DashMontage = EnsureMontage(DashMontage, FindSequenceByNames({ TEXT("Dash") }), TEXT("DefaultSlot"));
	AttackMeleeMontage = EnsureMontage(AttackMeleeMontage, FindSequenceByNames({ TEXT("Attack_Melee") }), TEXT("DefaultSlot"));
	AttackSlamMontage = EnsureMontage(AttackSlamMontage, FindSequenceByNames({ TEXT("Attack_Slam") }), TEXT("DefaultSlot"));
	HitReactMontage = EnsureMontage(HitReactMontage, FindSequenceByNames({ TEXT("Hit_React") }), TEXT("DefaultSlot"));
	DeathMontage = EnsureMontage(DeathMontage, FindSequenceByNames({ TEXT("Death") }), TEXT("DefaultSlot"));

	if (!LocomotionBlendSpace)
	{
		LocomotionBlendSpace = Cast<UBlendSpace>(FindSequenceByNames({ TEXT("BS_Locomotion") })); // will be null if not a sequence
		for (const TPair<FString, TWeakObjectPtr<UAnimationAsset>>& Pair : FolderAssets)
		{
			if (AetheriaAnimPrivate::NameMatches(Pair.Key, { TEXT("BS_Locomotion"), TEXT("Locomotion") }))
			{
				if (UBlendSpace* BS = Cast<UBlendSpace>(Pair.Value.Get()))
				{
					LocomotionBlendSpace = BS;
					break;
				}
			}
		}
	}

	bAssetsResolved = true;
}

UAnimSequence* UAetheriaAnimInstance::FindSequenceByNames(const TArray<FString>& Names) const
{
	UAnimSequence* Loose = nullptr;
	for (const TPair<FString, TWeakObjectPtr<UAnimationAsset>>& Pair : FolderAssets)
	{
		if (UAnimSequence* Seq = Cast<UAnimSequence>(Pair.Value.Get()))
		{
			const FString Norm = AetheriaAnimPrivate::NormalizeName(Pair.Key);
			bool bExact = false;
			for (const FString& Alias : Names)
			{
				if (Norm.Equals(AetheriaAnimPrivate::NormalizeName(Alias)))
				{
					bExact = true;
					break;
				}
			}
			if (bExact)
			{
				return Seq;
			}
			if (!Loose && AetheriaAnimPrivate::NameMatches(Pair.Key, Names))
			{
				Loose = Seq;
			}
		}
	}
	return Loose;
}

UAnimMontage* UAetheriaAnimInstance::FindMontageByNames(const TArray<FString>& Names) const
{
	for (const TPair<FString, TWeakObjectPtr<UAnimationAsset>>& Pair : FolderAssets)
	{
		if (UAnimMontage* Montage = Cast<UAnimMontage>(Pair.Value.Get()))
		{
			if (AetheriaAnimPrivate::NameMatches(Pair.Key, Names))
			{
				return Montage;
			}
		}
	}
	return nullptr;
}

UAnimMontage* UAetheriaAnimInstance::EnsureMontage(UAnimMontage* Montage, UAnimSequence* Source, const TCHAR* SlotName)
{
	if (Montage)
	{
		return Montage;
	}
	if (!Source)
	{
		return nullptr;
	}
	Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Source, FName(SlotName), 0.15f, 0.15f, 1.0f, 1);
	return Montage;
}

void UAetheriaAnimInstance::PlayAction(EAetheriaAnimAction Action, float PlayRate)
{
	if (bIsDead && Action != EAetheriaAnimAction::Death)
	{
		return;
	}

	UAnimMontage* Montage = nullptr;
	switch (Action)
	{
	case EAetheriaAnimAction::CastPrimary: Montage = CastPrimaryMontage; break;
	case EAetheriaAnimAction::CastHeavy: Montage = CastHeavyMontage; break;
	case EAetheriaAnimAction::Dash: Montage = DashMontage; break;
	case EAetheriaAnimAction::AttackMelee: Montage = AttackMeleeMontage; break;
	case EAetheriaAnimAction::AttackSlam: Montage = AttackSlamMontage; break;
	case EAetheriaAnimAction::HitReact: Montage = HitReactMontage; break;
	case EAetheriaAnimAction::Death:
		bIsDead = true;
		Montage = DeathMontage;
		break;
	}

	if (!Montage)
	{
		return;
	}

	Montage_Play(Montage, PlayRate);
}

bool UAetheriaAnimInstance::IsActionPlaying() const
{
	return IsAnyMontagePlaying();
}

bool UAetheriaAnimInstance::HasLocomotionSequences() const
{
	return IdleSeq || RunFwdSeq || RunBackSeq || RunStrafeLSeq || RunStrafeRSeq || LocomotionBlendSpace;
}

UAnimSequence* UAetheriaAnimInstance::PickLocomotionSequence() const
{
	if (!bIsMoving || Speed <= 15.0f)
	{
		return IdleSeq;
	}

	if (FMath::Abs(StrafeY) >= FMath::Abs(StrafeX))
	{
		return (StrafeY >= 0.0f) ? (RunFwdSeq ? RunFwdSeq : IdleSeq) : (RunBackSeq ? RunBackSeq : IdleSeq);
	}
	return (StrafeX >= 0.0f) ? (RunStrafeRSeq ? RunStrafeRSeq : IdleSeq) : (RunStrafeLSeq ? RunStrafeLSeq : IdleSeq);
}

void UAetheriaAnimInstance::TickLocomotionPlayback()
{
	if (bIsDead)
	{
		return;
	}
	if (IsAnyMontagePlaying())
	{
		return;
	}

	UAnimSequence* Desired = PickLocomotionSequence();
	if (!Desired)
	{
		return;
	}

	if (CurrentLocomotion.Get() == Desired && IsPlayingSlotAnimation(Desired, TEXT("DefaultSlot")))
	{
		return;
	}

	CurrentLocomotion = Desired;
	const float DirDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(LastLocomotionDirection, Direction));
	LastLocomotionDirection = Direction;
	const float BlendIn = (DirDelta > 35.0f) ? 0.35f : 0.18f;
	PlaySlotAnimationAsDynamicMontage(Desired, TEXT("DefaultSlot"), BlendIn, 0.22f, 1.0f, -1);
}

bool UAetheriaAnimInstance::BindSkeletalPresentation(
	ACharacter* Character,
	const FString& CharacterFolder,
	const FString& PreferredMeshName,
	float CapsuleHalfHeight)
{
	if (!Character)
	{
		return false;
	}

	USkeletalMesh* LoadedMesh = UAetheriaMaterialLibrary::LoadSkeletalMeshFromFolder(CharacterFolder, PreferredMeshName);
	if (!LoadedMesh)
	{
		return false;
	}

	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (!Mesh)
	{
		return false;
	}

	Mesh->SetSkeletalMesh(LoadedMesh);
	Mesh->SetAnimInstanceClass(UAetheriaAnimInstance::StaticClass());
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -CapsuleHalfHeight));
	Mesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);
	Mesh->bCastContactShadow = true;
	Mesh->SetHiddenInGame(false);
	Mesh->SetVisibility(true);
	Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);

	if (UAetheriaAnimInstance* Anim = Cast<UAetheriaAnimInstance>(Mesh->GetAnimInstance()))
	{
		Anim->ConfigureFromFolder(CharacterFolder);
	}
	else
	{
		Mesh->InitializeAnimScriptInstance(true);
		if (UAetheriaAnimInstance* Retry = Cast<UAetheriaAnimInstance>(Mesh->GetAnimInstance()))
		{
			Retry->ConfigureFromFolder(CharacterFolder);
		}
	}

	return true;
}

bool UAetheriaAnimInstance::BindExplicitPresentation(
	ACharacter* Character,
	const FString& MeshPackagePath,
	const FString& AnimFolder,
	const FString& AnimBlueprintPath,
	float CapsuleHalfHeight,
	float MeshScale)
{
	if (!Character || MeshPackagePath.IsEmpty())
	{
		return false;
	}

	const FString ObjectPath = MeshPackagePath + TEXT(".") + FPackageName::GetShortName(MeshPackagePath);
	USkeletalMesh* LoadedMesh = LoadObject<USkeletalMesh>(nullptr, *ObjectPath);
	if (!LoadedMesh)
	{
		return false;
	}

	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (!Mesh)
	{
		return false;
	}

	Mesh->SetSkeletalMesh(LoadedMesh);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -CapsuleHalfHeight));
	Mesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	Mesh->SetRelativeScale3D(FVector(MeshScale));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);
	Mesh->bCastContactShadow = true;
	Mesh->SetHiddenInGame(false);
	Mesh->SetVisibility(true);
	Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);

	// The native Paragon Gideon ABP is a full Paragon state machine whose
	// inputs our character never feeds, so it idles forever ("walking
	// animations gone"). Our UAetheriaAnimInstance drives locomotion/strafe/
	// casts from pawn velocity directly — prefer it whenever an anim folder
	// exists; the native BP stays only as a last-resort fallback.
	const FString EffectiveAnimFolder = AnimFolder.IsEmpty() ? FPackageName::GetLongPackagePath(MeshPackagePath) : AnimFolder;
	bool bUsedOurs = false;
	Mesh->SetAnimInstanceClass(UAetheriaAnimInstance::StaticClass());
	Mesh->InitializeAnimScriptInstance(true);
	if (UAetheriaAnimInstance* Anim = Cast<UAetheriaAnimInstance>(Mesh->GetAnimInstance()))
	{
		Anim->ConfigureFromFolder(EffectiveAnimFolder);
		bUsedOurs = Anim->HasLocomotionSequences();
	}

	if (!bUsedOurs && !AnimBlueprintPath.IsEmpty())
	{
		const FString ClassPath = AnimBlueprintPath + TEXT(".") + FPackageName::GetShortName(AnimBlueprintPath) + TEXT("_C");
		if (UClass* AnimClass = LoadClass<UAnimInstance>(nullptr, *ClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Mesh->SetAnimInstanceClass(AnimClass);
			Mesh->InitializeAnimScriptInstance(true);
			if (Mesh->GetAnimInstance())
			{
				UE_LOG(LogTemp, Display, TEXT("[Aetheria] Bound Paragon AnimBP %s on %s (no local clips)"), *AnimBlueprintPath, *Character->GetName());
				// Safety net: if the registry was still cold, swap back to the
				// procedural instance as soon as the clips resolvable.
				if (UWorld* World = Character->GetWorld())
				{
					TWeakObjectPtr<USkeletalMeshComponent> WeakMesh(Mesh);
					FTimerHandle RetryHandle;
					World->GetTimerManager().SetTimer(RetryHandle, [WeakMesh, EffectiveAnimFolder]()
					{
						USkeletalMeshComponent* M = WeakMesh.Get();
						if (!M) return;
						M->SetAnimInstanceClass(UAetheriaAnimInstance::StaticClass());
						M->InitializeAnimScriptInstance(true);
						if (UAetheriaAnimInstance* Anim = Cast<UAetheriaAnimInstance>(M->GetAnimInstance()))
						{
							Anim->ConfigureFromFolder(EffectiveAnimFolder);
							if (Anim->HasLocomotionSequences())
							{
								UE_LOG(LogTemp, Display, TEXT("[Aetheria] Late-swapped to UAetheriaAnimInstance folder %s"), *EffectiveAnimFolder);
							}
						}
					}, 5.0f, false);
				}
			}
		}
	}
	else if (bUsedOurs)
	{
		UE_LOG(LogTemp, Display, TEXT("[Aetheria] Bound mesh %s with UAetheriaAnimInstance folder %s"), *MeshPackagePath, *EffectiveAnimFolder);
	}

	return true;
}

bool UAetheriaAnimInstance::BindBestPresentation(
	ACharacter* Character,
	float CapsuleHalfHeight,
	const TArray<FString>& ParagonRoots,
	const FString& HeroName,
	const FString& FallbackFolder,
	const FString& FallbackMeshName)
{
	for (const FString& Root : ParagonRoots)
	{
		if (Root.IsEmpty())
		{
			continue;
		}

		TArray<FString> MeshCandidates;
		if (!HeroName.IsEmpty())
		{
			MeshCandidates.Add(Root / TEXT("Characters/Heroes") / HeroName / TEXT("Meshes") / HeroName);
			MeshCandidates.Add(Root / TEXT("Characters") / HeroName / TEXT("Meshes") / HeroName);
			MeshCandidates.Add(Root / TEXT("Characters/Heroes") / HeroName / HeroName);
		}

		for (const FString& MeshPath : MeshCandidates)
		{
			// In cooked/IoStore builds FPackageName::DoesPackageExist misses
			// container packages, so probe by loading: LoadObject is just as
			// cheap when the package is absent.
			if (!LoadObject<USkeletalMesh>(nullptr, *(MeshPath + TEXT(".") + FPackageName::GetShortName(MeshPath)), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				continue;
			}
			FString AnimFolder = FPackageName::GetLongPackagePath(MeshPath);
			AnimFolder.ReplaceInline(TEXT("/Meshes"), TEXT("/Animations"));
			FString BP;
			const FString AnimBP = Root / TEXT("Characters/Heroes") / HeroName / (HeroName + TEXT("_AnimBlueprint"));
			const FString AnimBPObject = AnimBP + TEXT(".") + FPackageName::GetShortName(AnimBP) + TEXT("_C");
			if (LoadClass<UAnimInstance>(nullptr, *AnimBPObject, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				BP = AnimBP;
			}
			if (BindExplicitPresentation(Character, MeshPath, AnimFolder, BP, CapsuleHalfHeight, 1.0f))
			{
				return true;
			}
		}

		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		AR.GetAssetsByPath(FName(*Root), Assets, true);
		FString FoundMesh;
		FString FoundFolder;
		for (const FAssetData& Data : Assets)
		{
			if (!(Data.GetClass() && Data.GetClass()->IsChildOf(USkeletalMesh::StaticClass())))
			{
				continue;
			}
			const FString N = Data.AssetName.ToString();
			if (N.Contains(TEXT("Physics")) || N.Contains(TEXT("CylShadow")) || N.Contains(TEXT("Preview")))
			{
				continue;
			}
			const bool bMinionHunt = HeroName.Equals(TEXT("Minion"), ESearchCase::IgnoreCase);
			if (!HeroName.IsEmpty() && !N.Contains(HeroName) && !(bMinionHunt && N.Contains(TEXT("Minion"))))
			{
				continue;
			}
			FoundMesh = Data.PackageName.ToString();
			FoundFolder = FPackageName::GetLongPackagePath(FoundMesh);
			break;
		}
		if (!FoundMesh.IsEmpty())
		{
			FString BP;
			for (const FAssetData& Data : Assets)
			{
				const FString N = Data.AssetName.ToString();
				if (N.Contains(TEXT("AnimBlueprint")) || N.Contains(TEXT("AnimBP")))
				{
					BP = Data.PackageName.ToString();
					break;
				}
			}
			if (BindExplicitPresentation(Character, FoundMesh, FoundFolder, BP, CapsuleHalfHeight, 1.0f))
			{
				return true;
			}
		}
	}

	return BindSkeletalPresentation(Character, FallbackFolder, FallbackMeshName, CapsuleHalfHeight);
}

int32 UAetheriaAnimInstance::FillLocomotionBlendSpaces()
{
	int32 Filled = 0;
#if WITH_EDITOR
	IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	AR.GetAssetsByPath(FName(TEXT("/Game/Characters")), Assets, /*bRecursive=*/true);

	TMap<FString, TArray<FAssetData>> ByFolder;
	for (const FAssetData& Asset : Assets)
	{
		const FString Pkg = Asset.PackageName.ToString();
		const FString Folder = FPackageName::GetLongPackagePath(Pkg);
		ByFolder.FindOrAdd(Folder).Add(Asset);
	}

	for (const TPair<FString, TArray<FAssetData>>& Pair : ByFolder)
	{
		UBlendSpace* BS = nullptr;
		UAnimSequence* Idle = nullptr;
		UAnimSequence* Fwd = nullptr;
		UAnimSequence* Back = nullptr;
		UAnimSequence* Left = nullptr;
		UAnimSequence* Right = nullptr;

		for (const FAssetData& Asset : Pair.Value)
		{
			const FString Name = Asset.AssetName.ToString();
			if (Name.Contains(TEXT("BS_Locomotion")))
			{
				BS = Cast<UBlendSpace>(Asset.GetAsset());
			}
			else if (UAnimSequence* Seq = Cast<UAnimSequence>(Asset.GetAsset()))
			{
				const FString L = Name.ToLower();
				if (L.Contains(TEXT("run_fwd")) || L.EndsWith(TEXT("runfwd")))
				{
					Fwd = Seq;
				}
				else if (L.Contains(TEXT("run_back")) || L.Contains(TEXT("runback")))
				{
					Back = Seq;
				}
				else if (L.Contains(TEXT("strafe_l")) || L.Contains(TEXT("strafel")) || L.Contains(TEXT("run_strafel")))
				{
					Left = Seq;
				}
				else if (L.Contains(TEXT("strafe_r")) || L.Contains(TEXT("strafer")) || L.Contains(TEXT("run_strafer")))
				{
					Right = Seq;
				}
				else if (L.Contains(TEXT("idle")) && !Idle)
				{
					Idle = Seq;
				}
			}
		}

		if (!BS || !Idle || !Fwd || !Back || !Left || !Right)
		{
			continue;
		}

		BS->Modify();
		while (BS->GetNumberOfBlendSamples() > 0)
		{
			BS->DeleteSample(0);
		}

		FBlendParameter& AxisX = const_cast<FBlendParameter&>(BS->GetBlendParameter(0));
		AxisX.DisplayName = TEXT("Strafe");
		AxisX.Min = -1.0f;
		AxisX.Max = 1.0f;
		AxisX.GridNum = 4;
		FBlendParameter& AxisY = const_cast<FBlendParameter&>(BS->GetBlendParameter(1));
		AxisY.DisplayName = TEXT("Forward");
		AxisY.Min = -1.0f;
		AxisY.Max = 1.0f;
		AxisY.GridNum = 4;

		auto Add = [BS](UAnimSequence* Seq, float X, float Y)
		{
			if (Seq)
			{
				BS->AddSample(Seq, FVector(X, Y, 0.0f));
			}
		};
		Add(Idle, 0.0f, 0.0f);
		Add(Fwd, 0.0f, 1.0f);
		Add(Back, 0.0f, -1.0f);
		Add(Left, -1.0f, 0.0f);
		Add(Right, 1.0f, 0.0f);

		if (BS->GetNumberOfBlendSamples() >= 5)
		{
			BS->ValidateSampleData();
			BS->ResampleData();
			BS->PostEditChange();
			BS->MarkPackageDirty();
			UPackage* Pkg = BS->GetOutermost();
			const FString Filename = FPackageName::LongPackageNameToFilename(Pkg->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			if (UPackage::SavePackage(Pkg, BS, *Filename, SaveArgs))
			{
				++Filled;
				UE_LOG(LogTemp, Display, TEXT("[Aetheria] Filled blendspace %s (%d samples)"), *Pkg->GetName(), BS->GetNumberOfBlendSamples());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Aetheria] Blendspace %s still has %d samples (AddSample may be unavailable)"),
				*BS->GetPathName(), BS->GetNumberOfBlendSamples());
		}
	}
#endif
	return Filled;
}


void UAetheriaAnimInstance::PlayCastForSpell(EAetheriaSpellId SpellId)
{
	const TCHAR* Name = TEXT("Cast");
	switch (SpellId)
	{
	case EAetheriaSpellId::None:             Name = TEXT("Primary_Attack_A_Medium_Montage"); break;
	case EAetheriaSpellId::Fireball:         Name = TEXT("RMB_Cast"); break;
	case EAetheriaSpellId::BlackHole:        Name = TEXT("Blackhole_Start"); break;
	case EAetheriaSpellId::Chronosphere:     Name = TEXT("Cosmic_Rift"); break;
	case EAetheriaSpellId::ChaosMeteor:      Name = TEXT("Torn_Space"); break;
	case EAetheriaSpellId::ChainLightning:   Name = TEXT("Cast"); break;
	case EAetheriaSpellId::SupernovaBeam:    Name = TEXT("PortalBlast"); break;
	case EAetheriaSpellId::ChronoBlink:      Name = TEXT("CosmicRift_Execute"); break;
	case EAetheriaSpellId::GravityCataclysm: Name = TEXT("Burden"); break;
	case EAetheriaSpellId::PrismWall:        Name = TEXT("CosmicRift_Start"); break;
	case EAetheriaSpellId::PrimalOverload:   Name = TEXT("Torn_Space_v2"); break;
	case EAetheriaSpellId::Macropyre:        Name = TEXT("portal_blast_medium"); break;
	case EAetheriaSpellId::PlasmaField:      Name = TEXT("PortalBlast_Start"); break;
	case EAetheriaSpellId::CataclysmNova:    Name = TEXT("Cosmic_Rift_Intro"); break;
	case EAetheriaSpellId::SunStrike:        Name = TEXT("portal_blast_fast"); break;
	case EAetheriaSpellId::EyeOfTheStorm:    Name = TEXT("Torn_Space_intro"); break;
	case EAetheriaSpellId::RequiemBlast:     Name = TEXT("Burden_Start"); break;
	default: break;
	}
	UAnimMontage* Montage = FindMontageByNames({ Name });
	if (!Montage)
	{
		Montage = EnsureMontage(nullptr, FindSequenceByNames({ Name }), TEXT("DefaultSlot"));
	}
	if (Montage)
	{
		Montage_Play(Montage, 1.0f);
	}
	else
	{
		PlayAction((SpellId == EAetheriaSpellId::BlackHole || SpellId == EAetheriaSpellId::ChaosMeteor
			|| SpellId == EAetheriaSpellId::GravityCataclysm || SpellId == EAetheriaSpellId::CataclysmNova
			|| SpellId == EAetheriaSpellId::RequiemBlast)
			? EAetheriaAnimAction::CastHeavy : EAetheriaAnimAction::CastPrimary);
	}
}
