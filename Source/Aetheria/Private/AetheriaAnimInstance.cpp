#include "AetheriaAnimInstance.h"

#include "AetheriaMaterialLibrary.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimCompositeBase.h"
#include "AnimationRuntime.h"
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
	static bool IsPoseOrAdditiveName(const FString& AssetName)
	{
		const FString L = AssetName.ToLower();
		return L.Contains(TEXT("additive"))
			|| L.Contains(TEXT("_ao_"))
			|| L.Contains(TEXT("aimoffset"))
			|| L.Contains(TEXT("_pose"))
			|| L.EndsWith(TEXT("pose"))
			|| L.Contains(TEXT("_msa"));
	}

	static UAnimSequence* ExtractSequenceFromMontage(const UAnimMontage* Montage)
	{
		if (!Montage)
		{
			return nullptr;
		}
		for (const FSlotAnimationTrack& SlotTrack : Montage->SlotAnimTracks)
		{
			for (const FAnimSegment& Seg : SlotTrack.AnimTrack.AnimSegments)
			{
				if (UAnimSequence* Seq = Cast<UAnimSequence>(Seg.GetAnimReference()))
				{
					if (!Seq->IsValidAdditive())
					{
						return Seq;
					}
				}
			}
		}
		return nullptr;
	}
}

struct FAetheriaAnimInstanceProxy : public FAnimInstanceProxy
{
	explicit FAetheriaAnimInstanceProxy(UAnimInstance* Instance)
		: FAnimInstanceProxy(Instance)
	{
	}

	UAnimSequence* CurrentLocSeq = nullptr;
	float LocTime = 0.0f;

	UAnimSequence* PrevLocSeq = nullptr;
	float PrevLocTime = 0.0f;
	float LocBlendAlpha = 1.0f;
	static constexpr float LocBlendDuration = 0.18f;

	UAnimSequence* ActionSeq = nullptr;
	float ActionTime = 0.0f;
	float ActionPlayRate = 1.0f;
	float ActionBlendIn = 0.12f;
	float ActionBlendOut = 0.22f;
	bool bActionHoldEnd = false;

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

		UAetheriaAnimInstance* Inst = Cast<UAetheriaAnimInstance>(InAnimInstance);
		if (!Inst)
		{
			return;
		}

		UAnimSequence* Desired = Inst->DesiredLocomotionSeq.Get();
		if (Desired && (!IsValid(Desired) || Desired->IsValidAdditive()))
		{
			Desired = nullptr;
		}

		if (Desired != CurrentLocSeq)
		{
			if (CurrentLocSeq && IsValid(CurrentLocSeq))
			{
				PrevLocSeq = CurrentLocSeq;
				PrevLocTime = LocTime;
				LocBlendAlpha = 0.0f;
			}
			else
			{
				PrevLocSeq = nullptr;
				LocBlendAlpha = 1.0f;
			}
			CurrentLocSeq = Desired;
			if (CurrentLocSeq && PrevLocSeq == nullptr)
			{
				const float Len = CurrentLocSeq->GetPlayLength();
				const APawn* OwnerPawn = Inst->TryGetPawnOwner();
				const bool bIsPlayer = OwnerPawn && OwnerPawn->IsPlayerControlled();
				LocTime = (!bIsPlayer && Len > 0.1f) ? FMath::FRandRange(0.0f, Len * 0.75f) : 0.0f;
			}
			else
			{
				LocTime = 0.0f;
			}
		}

		if (Inst->bPendingResetDeath)
		{
			Inst->bPendingResetDeath = false;
			ActionSeq = nullptr;
			ActionTime = 0.0f;
			bActionHoldEnd = false;
			PrevLocSeq = nullptr;
			LocBlendAlpha = 1.0f;
			Inst->ActiveActionSeq = nullptr;
			Inst->bProxyActionPlaying = false;
		}

		if (Inst->bPendingActionRequest)
		{
			Inst->bPendingActionRequest = false;
			UAnimSequence* ReqSeq = Inst->PendingActionSeq.Get();
			if (ReqSeq && IsValid(ReqSeq))
			{
				ActionSeq = ReqSeq;
				ActionTime = 0.0f;
				ActionPlayRate = FMath::Max(0.1f, Inst->PendingActionPlayRate);
				const float EffectiveLen = FMath::Max(0.15f, ReqSeq->GetPlayLength() / ActionPlayRate);
				// Smooth BlendIn = 0.12s, BlendOut = 0.22s (clamped proportionally for very short clips)
				ActionBlendIn = FMath::Min(0.12f, EffectiveLen * 0.30f);
				ActionBlendOut = Inst->bPendingActionHoldEnd ? 0.0f : FMath::Min(0.22f, EffectiveLen * 0.42f);
				bActionHoldEnd = Inst->bPendingActionHoldEnd;
				Inst->ActiveActionSeq = ReqSeq;
			}
			Inst->PendingActionSeq = nullptr;
		}

		if (ActionSeq && !IsValid(ActionSeq))
		{
			ActionSeq = nullptr;
		}
		if (!ActionSeq)
		{
			Inst->ActiveActionSeq = nullptr;
		}
		Inst->bProxyActionPlaying = (ActionSeq != nullptr);
	}

	virtual void Update(float DeltaSeconds) override
	{
		if (CurrentLocSeq && IsValid(CurrentLocSeq))
		{
			const float Len = CurrentLocSeq->GetPlayLength();
			if (Len > KINDA_SMALL_NUMBER)
			{
				LocTime = FMath::Fmod(LocTime + DeltaSeconds, Len);
				if (LocTime < 0.0f)
				{
					LocTime += Len;
				}
			}
		}

		if (PrevLocSeq && IsValid(PrevLocSeq) && LocBlendAlpha < 1.0f)
		{
			const float PrevLen = PrevLocSeq->GetPlayLength();
			if (PrevLen > KINDA_SMALL_NUMBER)
			{
				PrevLocTime = FMath::Fmod(PrevLocTime + DeltaSeconds, PrevLen);
				if (PrevLocTime < 0.0f)
				{
					PrevLocTime += PrevLen;
				}
			}
			LocBlendAlpha = FMath::Min(1.0f, LocBlendAlpha + (DeltaSeconds / LocBlendDuration));
			if (LocBlendAlpha >= 1.0f)
			{
				PrevLocSeq = nullptr;
			}
		}

		if (ActionSeq && IsValid(ActionSeq))
		{
			const float Len = FMath::Max(0.05f, ActionSeq->GetPlayLength());
			ActionTime += DeltaSeconds * ActionPlayRate;
			if (ActionTime >= Len)
			{
				if (bActionHoldEnd)
				{
					ActionTime = FMath::Max(0.0f, Len - 0.01f);
				}
				else
				{
					ActionSeq = nullptr;
					ActionTime = 0.0f;
				}
			}
		}
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		if (!GetRequiredBones().IsValid() || Output.Pose.GetNumBones() <= 0)
		{
			return false;
		}

		USkeleton* ActiveSkel = GetSkeleton();
		auto CanSample = [ActiveSkel](UAnimSequence* Seq, bool bAllowAdditive) -> bool
		{
			if (!Seq || !IsValid(Seq) || (!bAllowAdditive && Seq->IsValidAdditive()))
			{
				return false;
			}
			USkeleton* SeqSkel = Seq->GetSkeleton();
			if (!SeqSkel)
			{
				return false;
			}
			return (!ActiveSkel || SeqSkel == ActiveSkel);
		};

		const bool bHasLoc = CanSample(CurrentLocSeq, false);
		const bool bHasPrev = CanSample(PrevLocSeq, false) && LocBlendAlpha < 0.999f;
		const bool bHasAction = CanSample(ActionSeq, true);

		if (!bHasLoc && !bHasAction)
		{
			return false;
		}

		FAnimationPoseData OutData(Output);

		auto EvalLocomotion = [&](FPoseContext& TargetCtx)
		{
			if (!bHasLoc)
			{
				TargetCtx.ResetToRefPose();
				return;
			}
			FAnimationPoseData TargetData(TargetCtx);
			if (bHasPrev)
			{
				FPoseContext PrevCtx(TargetCtx);
				FPoseContext CurCtx(TargetCtx);
				FAnimationPoseData PrevData(PrevCtx);
				FAnimationPoseData CurData(CurCtx);
				PrevLocSeq->GetAnimationPose(PrevData, FAnimExtractContext(static_cast<double>(PrevLocTime), false));
				CurrentLocSeq->GetAnimationPose(CurData, FAnimExtractContext(static_cast<double>(LocTime), false));
				const float SmoothW = FMath::Clamp(LocBlendAlpha * LocBlendAlpha * (3.0f - 2.0f * LocBlendAlpha), 0.0f, 1.0f);
				FAnimationRuntime::BlendTwoPosesTogether(CurData, PrevData, SmoothW, TargetData);
			}
			else
			{
				CurrentLocSeq->GetAnimationPose(TargetData, FAnimExtractContext(static_cast<double>(LocTime), false));
			}
		};

		if (!bHasAction)
		{
			EvalLocomotion(Output);
			return true;
		}

		const float ActionLen = FMath::Max(0.05f, ActionSeq->GetPlayLength());
		const float BlendInSeq = ActionBlendIn * ActionPlayRate;
		const float BlendOutSeq = ActionBlendOut * ActionPlayRate;
		float ActionWeight = 1.0f;
		if (BlendInSeq > KINDA_SMALL_NUMBER && ActionTime < BlendInSeq)
		{
			ActionWeight = FMath::Clamp(ActionTime / BlendInSeq, 0.0f, 1.0f);
		}
		if (!bActionHoldEnd && BlendOutSeq > KINDA_SMALL_NUMBER && (ActionLen - ActionTime) < BlendOutSeq)
		{
			ActionWeight = FMath::Min(ActionWeight, FMath::Clamp((ActionLen - ActionTime) / BlendOutSeq, 0.0f, 1.0f));
		}
		ActionWeight = ActionWeight * ActionWeight * (3.0f - 2.0f * ActionWeight);

		if (ActionSeq->IsValidAdditive())
		{
			EvalLocomotion(Output);
			if (ActionWeight > KINDA_SMALL_NUMBER)
			{
				FPoseContext AdditiveCtx(Output, /*bInOverrideExpectsAdditivePose=*/true);
				AdditiveCtx.ResetToAdditiveIdentity();
				FAnimationPoseData AdditiveData(AdditiveCtx);
				ActionSeq->GetAnimationPose(AdditiveData, FAnimExtractContext(static_cast<double>(ActionTime), false));
				FAnimationRuntime::AccumulateAdditivePose(OutData, AdditiveData, ActionWeight, ActionSeq->GetAdditiveAnimType());
				Output.Pose.NormalizeRotations();
			}
			return true;
		}

		if (ActionWeight <= KINDA_SMALL_NUMBER && bHasLoc)
		{
			EvalLocomotion(Output);
			return true;
		}

		FPoseContext BaseCtx(Output);
		if (bHasLoc)
		{
			EvalLocomotion(BaseCtx);
		}
		else
		{
			BaseCtx.ResetToRefPose();
		}

		if (ActionWeight >= 0.999f || !bHasLoc)
		{
			ActionSeq->GetAnimationPose(OutData, FAnimExtractContext(static_cast<double>(ActionTime), false));
			// Suppress root-bone Z-drift and crooked horizontal lurching on Paragon cast/action sequences!
			if (!bActionHoldEnd && Output.Pose.GetNumBones() > 0 && BaseCtx.Pose.GetNumBones() > 0)
			{
				const FCompactPoseBoneIndex RootIdx(0);
				const FTransform BaseRoot = BaseCtx.Pose[RootIdx];
				FTransform ActionRoot = Output.Pose[RootIdx];
				ActionRoot.SetTranslation(BaseRoot.GetTranslation());
				ActionRoot.SetRotation(BaseRoot.GetRotation());
				Output.Pose[RootIdx] = ActionRoot;
			}
			return true;
		}

		FPoseContext ActionCtx(Output);
		FAnimationPoseData ActionData(ActionCtx);
		ActionSeq->GetAnimationPose(ActionData, FAnimExtractContext(static_cast<double>(ActionTime), false));

		// Suppress root-bone Z-drift and crooked horizontal lurching on Paragon cast/action sequences!
		if (!bActionHoldEnd && ActionCtx.Pose.GetNumBones() > 0 && BaseCtx.Pose.GetNumBones() > 0)
		{
			const FCompactPoseBoneIndex RootIdx(0);
			const FTransform BaseRoot = BaseCtx.Pose[RootIdx];
			FTransform ActionRoot = ActionCtx.Pose[RootIdx];
			ActionRoot.SetTranslation(BaseRoot.GetTranslation());
			ActionRoot.SetRotation(BaseRoot.GetRotation());
			ActionCtx.Pose[RootIdx] = ActionRoot;
		}

		FAnimationPoseData BaseData(BaseCtx);
		FAnimationRuntime::BlendTwoPosesTogether(ActionData, BaseData, ActionWeight, OutData);
		return true;
	}
};

UAetheriaAnimInstance::UAetheriaAnimInstance()
{
	bUseMultiThreadedAnimationUpdate = false;
}

FAnimInstanceProxy* UAetheriaAnimInstance::CreateAnimInstanceProxy()
{
	return new FAetheriaAnimInstanceProxy(this);
}

void UAetheriaAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void UAetheriaAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	// Disable root-motion capsule drift from Paragon cast montages
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	if (ACharacter* Char = Cast<ACharacter>(TryGetPawnOwner()))
	{
		if (UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement())
		{
			MoveComp->bAllowPhysicsRotationDuringAnimRootMotion = false;
		}
	}
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

USkeleton* UAetheriaAnimInstance::GetTargetSkeleton() const
{
	if (CurrentSkeleton)
	{
		return CurrentSkeleton;
	}
	if (USkeletalMeshComponent* SkelComp = GetSkelMeshComponentUnchecked())
	{
		if (USkeletalMesh* SkelMesh = SkelComp->GetSkeletalMeshAsset())
		{
			return SkelMesh->GetSkeleton();
		}
	}
	return nullptr;
}

bool UAetheriaAnimInstance::IsSkeletonCompatible(const USkeleton* AssetSkel, const USkeleton* TargetSkel) const
{
	if (!AssetSkel || !TargetSkel)
	{
		return false;
	}
	if (AssetSkel == TargetSkel)
	{
		return true;
	}
#if WITH_EDITORONLY_DATA
	if (TargetSkel->IsCompatibleForEditor(AssetSkel))
	{
		return true;
	}
#endif
	for (const TSoftObjectPtr<USkeleton>& Compat : TargetSkel->GetCompatibleSkeletons())
	{
		if (Compat.Get() == AssetSkel)
		{
			return true;
		}
	}
	return false;
}

void UAetheriaAnimInstance::ConfigureFromFolder(const FString& ContentFolder)
{
	AssetFolder = ContentFolder;
	bAssetsResolved = false;
	FolderAssets.Reset();
	CachedFolderAssets.Reset();
	LocomotionBlendSpace = nullptr;
	DesiredLocomotionSeq = nullptr;
	PendingActionSeq = nullptr;
	ActiveActionSeq = nullptr;
	bPendingActionRequest = false;
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

	USkeleton* TargetSkel = GetTargetSkeleton();
	for (const FAssetData& Data : Assets)
	{
		const FString PkgPath = Data.PackageName.ToString();
		if (PkgPath.Contains(TEXT("/AimOffsets/")))
		{
			continue;
		}
		if (UAnimationAsset* Anim = Cast<UAnimationAsset>(Data.GetAsset()))
		{
			USkeleton* AnimSkel = Anim->GetSkeleton();
			if (TargetSkel && (!AnimSkel || !IsSkeletonCompatible(AnimSkel, TargetSkel)))
			{
				continue;
			}
			const FString Key = Data.AssetName.ToString();
			if (const TWeakObjectPtr<UAnimationAsset>* Existing = FolderAssets.Find(Key))
			{
				if (Existing->IsValid() && TargetSkel && Existing->Get()->GetSkeleton() == TargetSkel && AnimSkel != TargetSkel)
				{
					continue;
				}
			}
			FolderAssets.Add(Key, Anim);
			CachedFolderAssets.AddUnique(Anim);
		}
	}

	IdleSeq = FindSequenceByNames({ TEXT("Idle"), TEXT("Idle_A"), TEXT("NonCombat_Idle"), TEXT("Idle_Relaxed"), TEXT("Idle_Relaxed_A"), TEXT("Idle_Long"), TEXT("Idle_Straight"), TEXT("Idle_Variant_01") });
	RunFwdSeq = FindSequenceByNames({ TEXT("Run_Fwd"), TEXT("RunFwd"), TEXT("Run_Forward"), TEXT("Jog_Fwd"), TEXT("Combat_JogFwd"), TEXT("Jog_Fwd_Combat_A"), TEXT("Fly_Fwd_FullSpeed"), TEXT("Fly_Fwd_MidSpeed"), TEXT("NonCombat_JogFwd") });
	RunBackSeq = FindSequenceByNames({ TEXT("Run_Back"), TEXT("RunBack"), TEXT("Run_Backward"), TEXT("Run_Bwd"), TEXT("Jog_Bwd"), TEXT("Fly_Fwd_MidSpeed") });
	RunStrafeLSeq = FindSequenceByNames({ TEXT("Run_StrafeL"), TEXT("RunStrafeL"), TEXT("StrafeL"), TEXT("Run_Lft"), TEXT("Run_Left"), TEXT("Jog_Left"), TEXT("Jog_Lft"), TEXT("Jog_Left_Combat_A") });
	RunStrafeRSeq = FindSequenceByNames({ TEXT("Run_StrafeR"), TEXT("RunStrafeR"), TEXT("StrafeR"), TEXT("Run_Rt"), TEXT("Run_Right"), TEXT("Jog_Right"), TEXT("Jog_Rt"), TEXT("Jog_Right_Combat_A") });

	CastPrimaryMontage = FindMontageByNames({ TEXT("AM_Cast_Primary"), TEXT("Cast_Primary"), TEXT("Cast"), TEXT("RMB_Cast"), TEXT("Fire_A"), TEXT("Fire"), TEXT("Fire_B"), TEXT("Cosmic_Rift"), TEXT("Primary_Attack_A_Medium_Montage") });
	CastHeavyMontage = FindMontageByNames({ TEXT("AM_Cast_Heavy"), TEXT("Cast_Heavy"), TEXT("Blackhole_Start"), TEXT("BlackHole_Loop"), TEXT("CosmicRift_Execute"), TEXT("Torn_Space"), TEXT("Torn_Space_v2") });
	DashMontage = FindMontageByNames({ TEXT("AM_Dash"), TEXT("Dash") });
	AttackMeleeMontage = FindMontageByNames({ TEXT("AM_Attack_Melee"), TEXT("Attack_Melee"), TEXT("Primary_Attack_A_Medium_Montage"), TEXT("Primary_Attack_B_Medium_Montage") });
	AttackSlamMontage = FindMontageByNames({ TEXT("AM_Attack_Slam"), TEXT("Attack_Slam"), TEXT("Primary_Attack_C_Medium_Montage") });
	HitReactMontage = FindMontageByNames({ TEXT("AM_Hit_React"), TEXT("Hit_React"), TEXT("HitReact_Front") });
	DeathMontage = FindMontageByNames({ TEXT("AM_Death"), TEXT("Death"), TEXT("Death_Fwd"), TEXT("Death_Back") });

	CastPrimarySeq = FindSequenceByNames({ TEXT("Cast_Primary"), TEXT("RMB_Cast"), TEXT("Cast"), TEXT("Cosmic_Rift"), TEXT("Primary_Attack_A_Medium"), TEXT("Ability_Q"), TEXT("Ability_RMB"), TEXT("PrimaryAttack_A_Slow"), TEXT("Fire_A"), TEXT("Fire"), TEXT("Spit_Attack"), TEXT("Attack_Uppercut") }, false);
	if (!CastPrimarySeq) CastPrimarySeq = AetheriaAnimPrivate::ExtractSequenceFromMontage(CastPrimaryMontage);
	if (!CastPrimarySeq) CastPrimarySeq = FindSequenceByNames({ TEXT("Cast_Primary"), TEXT("RMB_Cast"), TEXT("Cast"), TEXT("Primary_Attack_A_Medium") }, true);

	CastHeavySeq = FindSequenceByNames({ TEXT("Cast_Heavy"), TEXT("CosmicRift_Execute"), TEXT("Torn_Space_v2"), TEXT("Torn_Space"), TEXT("PortalBlast"), TEXT("Burden"), TEXT("Ability_R"), TEXT("Ability_E"), TEXT("Ultimate_Swing_120fps"), TEXT("Primary_Attack_C_Medium"), TEXT("Blackhole_Start"), TEXT("Fire_B"), TEXT("Attack_BigSmash") }, false);
	if (!CastHeavySeq) CastHeavySeq = AetheriaAnimPrivate::ExtractSequenceFromMontage(CastHeavyMontage);
	if (!CastHeavySeq) CastHeavySeq = FindSequenceByNames({ TEXT("Cast_Heavy"), TEXT("Blackhole_Start"), TEXT("Torn_Space"), TEXT("Primary_Attack_C_Medium") }, true);

	DashSeq = FindSequenceByNames({ TEXT("Dash"), TEXT("Torn_Space"), TEXT("Jump_Idle_Start"), TEXT("Spawn") });
	if (!DashSeq) DashSeq = AetheriaAnimPrivate::ExtractSequenceFromMontage(DashMontage);

	AttackMeleeSeq = FindSequenceByNames({ TEXT("Attack_Melee"), TEXT("BiteAttack_A"), TEXT("BiteAttack_Fast"), TEXT("Attack_Punch_01"), TEXT("Attack_SmallCombo"), TEXT("Primary_Attack_A_Medium"), TEXT("Attack_A_SetA"), TEXT("Fire_A"), TEXT("Fire"), TEXT("Primary_Attack_B_Medium") }, true);
	if (!AttackMeleeSeq) AttackMeleeSeq = AetheriaAnimPrivate::ExtractSequenceFromMontage(AttackMeleeMontage);

	AttackSlamSeq = FindSequenceByNames({ TEXT("Attack_Slam"), TEXT("Attack_BigSmash"), TEXT("Attack_Uppercut"), TEXT("BiteAttack_B"), TEXT("Primary_Attack_C_Medium"), TEXT("Torn_Space"), TEXT("Attack_Punch_02"), TEXT("Fire_B") }, true);
	if (!AttackSlamSeq) AttackSlamSeq = AetheriaAnimPrivate::ExtractSequenceFromMontage(AttackSlamMontage);

	HitReactSeq = FindSequenceByNames({ TEXT("Hit_React"), TEXT("HitReact_Front"), TEXT("HitReaction_FWD"), TEXT("Hit_Front"), TEXT("Hitreat_Fwd"), TEXT("HitReact_Front_A"), TEXT("HitReact_Back") }, true);
	if (!HitReactSeq) HitReactSeq = AetheriaAnimPrivate::ExtractSequenceFromMontage(HitReactMontage);

	DeathSeq = FindSequenceByNames({ TEXT("Death"), TEXT("Death_Fwd"), TEXT("Death_Front"), TEXT("Death_front"), TEXT("Death_A"), TEXT("Death_Front_A"), TEXT("Death_Back"), TEXT("Death_back") });
	if (!DeathSeq) DeathSeq = AetheriaAnimPrivate::ExtractSequenceFromMontage(DeathMontage);

	CastPrimaryMontage = EnsureMontage(CastPrimaryMontage, CastPrimarySeq, TEXT("DefaultSlot"));
	CastHeavyMontage = EnsureMontage(CastHeavyMontage, CastHeavySeq, TEXT("DefaultSlot"));
	DashMontage = EnsureMontage(DashMontage, DashSeq, TEXT("DefaultSlot"));
	AttackMeleeMontage = EnsureMontage(AttackMeleeMontage, AttackMeleeSeq, TEXT("DefaultSlot"));
	AttackSlamMontage = EnsureMontage(AttackSlamMontage, AttackSlamSeq, TEXT("DefaultSlot"));
	HitReactMontage = EnsureMontage(HitReactMontage, HitReactSeq, TEXT("DefaultSlot"));
	DeathMontage = EnsureMontage(DeathMontage, DeathSeq, TEXT("DefaultSlot"));

	if (!LocomotionBlendSpace)
	{
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

	DesiredLocomotionSeq = IdleSeq ? IdleSeq : RunFwdSeq;
	bAssetsResolved = true;

	UE_LOG(LogTemp, Display, TEXT("[Aetheria] AnimResolve folder=%s skel=%s Idle=%s RunFwd=%s Cast=%s Melee=%s Slam=%s Hit=%s Death=%s"),
		*AssetFolder,
		*GetNameSafe(TargetSkel),
		*GetNameSafe(IdleSeq),
		*GetNameSafe(RunFwdSeq),
		*GetNameSafe(CastPrimarySeq),
		*GetNameSafe(AttackMeleeSeq),
		*GetNameSafe(AttackSlamSeq),
		*GetNameSafe(HitReactSeq),
		*GetNameSafe(DeathSeq));
}

UAnimSequence* UAetheriaAnimInstance::FindSequenceByNames(const TArray<FString>& Names, bool bAllowAdditive) const
{
	USkeleton* TargetSkel = GetTargetSkeleton();
	auto SearchPass = [this, &Names, TargetSkel](bool bWantAdditive) -> UAnimSequence*
	{
		UAnimSequence* ExactCompat = nullptr;
		UAnimSequence* LooseExactSkel = nullptr;
		UAnimSequence* LooseCompat = nullptr;

		for (const FString& Alias : Names)
		{
			const FString Needle = AetheriaAnimPrivate::NormalizeName(Alias);
			for (const TPair<FString, TWeakObjectPtr<UAnimationAsset>>& Pair : FolderAssets)
			{
				if (UAnimSequence* Seq = Cast<UAnimSequence>(Pair.Value.Get()))
				{
					if (Seq->IsValidAdditive() != bWantAdditive || AetheriaAnimPrivate::IsPoseOrAdditiveName(Pair.Key))
					{
						continue;
					}
					USkeleton* SeqSkel = Seq->GetSkeleton();
					if (TargetSkel && (!SeqSkel || !IsSkeletonCompatible(SeqSkel, TargetSkel)))
					{
						continue;
					}
					const bool bExactSkel = (!TargetSkel || SeqSkel == TargetSkel);
					const FString Norm = AetheriaAnimPrivate::NormalizeName(Pair.Key);
					if (Norm.Equals(Needle))
					{
						if (bExactSkel)
						{
							return Seq;
						}
						if (!ExactCompat)
						{
							ExactCompat = Seq;
						}
					}
					else if (Norm.EndsWith(Needle))
					{
						if (bExactSkel && !LooseExactSkel)
						{
							LooseExactSkel = Seq;
						}
						else if (!LooseCompat)
						{
							LooseCompat = Seq;
						}
					}
				}
			}
		}
		if (ExactCompat)
		{
			return ExactCompat;
		}
		return LooseExactSkel ? LooseExactSkel : LooseCompat;
	};

	if (UAnimSequence* NonAdditive = SearchPass(false))
	{
		return NonAdditive;
	}
	return bAllowAdditive ? SearchPass(true) : nullptr;
}

UAnimMontage* UAetheriaAnimInstance::FindMontageByNames(const TArray<FString>& Names) const
{
	USkeleton* TargetSkel = GetTargetSkeleton();
	UAnimMontage* CompatFallback = nullptr;
	for (const TPair<FString, TWeakObjectPtr<UAnimationAsset>>& Pair : FolderAssets)
	{
		if (UAnimMontage* Montage = Cast<UAnimMontage>(Pair.Value.Get()))
		{
			USkeleton* MontageSkel = Montage->GetSkeleton();
			if (TargetSkel && (!MontageSkel || !IsSkeletonCompatible(MontageSkel, TargetSkel)))
			{
				continue;
			}
			if (AetheriaAnimPrivate::NameMatches(Pair.Key, Names))
			{
				if (!TargetSkel || MontageSkel == TargetSkel)
				{
					return Montage;
				}
				if (!CompatFallback)
				{
					CompatFallback = Montage;
				}
			}
		}
	}
	return CompatFallback;
}

UAnimMontage* UAetheriaAnimInstance::EnsureMontage(UAnimMontage* Montage, UAnimSequence* Source, const TCHAR* SlotName)
{
	USkeleton* TargetSkel = GetTargetSkeleton();
	if (Montage)
	{
		USkeleton* MontageSkel = Montage->GetSkeleton();
		if (MontageSkel && (!TargetSkel || IsSkeletonCompatible(MontageSkel, TargetSkel)))
		{
			return Montage;
		}
		Montage = nullptr;
	}
	if (!Source || Source->IsValidAdditive())
	{
		return nullptr;
	}
	USkeleton* SourceSkel = Source->GetSkeleton();
	if (!SourceSkel || (TargetSkel && !IsSkeletonCompatible(SourceSkel, TargetSkel)))
	{
		return nullptr;
	}
	UAnimMontage* DynamicMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Source, FName(SlotName), 0.12f, 0.22f, 1.0f, 1);
	if (!DynamicMontage)
	{
		return nullptr;
	}
	if (!DynamicMontage->GetSkeleton())
	{
		USkeleton* SkelToSet = TargetSkel ? TargetSkel : SourceSkel;
		if (SkelToSet && IsSkeletonCompatible(SourceSkel, SkelToSet))
		{
			DynamicMontage->SetSkeleton(SkelToSet);
		}
		else
		{
			return nullptr;
		}
	}
	if (TargetSkel && !IsSkeletonCompatible(DynamicMontage->GetSkeleton(), TargetSkel))
	{
		return nullptr;
	}
	return DynamicMontage;
}

void UAetheriaAnimInstance::PlaySequenceAction(UAnimSequence* Seq, float PlayRate, bool bHoldEnd)
{
	if (!Seq || !IsValid(Seq))
	{
		return;
	}
	USkeleton* TargetSkel = GetTargetSkeleton();
	if (!TargetSkel || Seq->GetSkeleton() != TargetSkel)
	{
		return;
	}
	const float SafeRate = FMath::Clamp(PlayRate, 0.15f, 2.25f);
	PendingActionSeq = Seq;
	PendingActionPlayRate = SafeRate;
	bPendingActionHoldEnd = bHoldEnd;
	bPendingActionRequest = true;
	bProxyActionPlaying = true;

	// Drive smooth dynamic slot montage in lockstep (BlendIn=0.12s, BlendOut=0.22s) with root-motion drift suppressed
	if (!Seq->IsValidAdditive())
	{
		const float BlendIn = 0.12f;
		const float BlendOut = bHoldEnd ? 0.0f : 0.22f;
		if (UAnimMontage* DynMontage = PlaySlotAnimationAsDynamicMontage(Seq, FName(TEXT("DefaultSlot")), BlendIn, BlendOut, SafeRate, 1))
		{
			if (FAnimMontageInstance* ActiveInst = GetActiveInstanceForMontage(DynMontage))
			{
				ActiveInst->PushDisableRootMotion();
			}
		}
	}
}

void UAetheriaAnimInstance::PlayAction(EAetheriaAnimAction Action, float PlayRate)
{
	if (bIsDead && Action != EAetheriaAnimAction::Death)
	{
		return;
	}

	UAnimSequence* Seq = nullptr;
	UAnimMontage* Montage = nullptr;
	bool bHoldEnd = false;
	switch (Action)
	{
	case EAetheriaAnimAction::CastPrimary: Seq = CastPrimarySeq; Montage = CastPrimaryMontage; break;
	case EAetheriaAnimAction::CastHeavy: Seq = CastHeavySeq; Montage = CastHeavyMontage; break;
	case EAetheriaAnimAction::Dash: Seq = DashSeq; Montage = DashMontage; break;
	case EAetheriaAnimAction::AttackMelee: Seq = AttackMeleeSeq; Montage = AttackMeleeMontage; break;
	case EAetheriaAnimAction::AttackSlam: Seq = AttackSlamSeq; Montage = AttackSlamMontage; break;
	case EAetheriaAnimAction::HitReact: Seq = HitReactSeq; Montage = HitReactMontage; break;
	case EAetheriaAnimAction::Death:
		bIsDead = true;
		bHoldEnd = true;
		Seq = DeathSeq;
		Montage = DeathMontage;
		break;
	}

	if (!Seq && Montage)
	{
		Seq = AetheriaAnimPrivate::ExtractSequenceFromMontage(Montage);
	}
	if (Seq)
	{
		PlaySequenceAction(Seq, PlayRate, bHoldEnd);
	}
}

void UAetheriaAnimInstance::ReviveFromDeath()
{
	bIsDead = false;
	bPendingResetDeath = true;
	bPendingActionRequest = false;
	PendingActionSeq = nullptr;
	ActiveActionSeq = nullptr;
	bProxyActionPlaying = false;
	DesiredLocomotionSeq = IdleSeq ? IdleSeq : RunFwdSeq;
}

bool UAetheriaAnimInstance::IsActionPlaying() const
{
	return bProxyActionPlaying || bPendingActionRequest || IsAnyMontagePlaying();
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

	UAnimSequence* Desired = PickLocomotionSequence();
	if (!Desired)
	{
		return;
	}

	USkeleton* TargetSkel = GetTargetSkeleton();
	if (!TargetSkel || Desired->GetSkeleton() != TargetSkel)
	{
		return;
	}

	CurrentLocomotion = Desired;
	DesiredLocomotionSeq = Desired;
	LastLocomotionDirection = Direction;
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
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->bPauseAnims = false;
	Mesh->bNoSkeletonUpdate = false;
	Mesh->SetComponentTickEnabled(true);
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
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Mesh->bPauseAnims = false;
	Mesh->bNoSkeletonUpdate = false;
	Mesh->SetComponentTickEnabled(true);
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
	if (bIsDead)
	{
		return;
	}

	// Curated full-follow-through Paragon cast sequences (no truncated *_Intro / *_Start crooked half-poses!)
	// plus cross-skeleton fallbacks (Gideon -> Aurora -> Sevarog -> Morigesh) and cast-window-matched pacing.
	TArray<FString> Candidates;
	float TargetCastWindow = 0.62f;
	bool bHeavySpell = false;

	switch (SpellId)
	{
	case EAetheriaSpellId::None:
		Candidates = { TEXT("Primary_Attack_A_Medium"), TEXT("Primary_Attack_B_Medium"), TEXT("PrimaryAttack_A_Slow"), TEXT("Cast"), TEXT("Ability_Q") };
		TargetCastWindow = 0.46f;
		break;
	case EAetheriaSpellId::Fireball:
		Candidates = { TEXT("RMB_Cast"), TEXT("Primary_Attack_B_Medium"), TEXT("Ability_RMB"), TEXT("Cast") };
		TargetCastWindow = 0.54f;
		break;
	case EAetheriaSpellId::BlackHole:
		Candidates = { TEXT("CosmicRift_Execute"), TEXT("Blackhole_Start"), TEXT("Ability_R"), TEXT("Ultimate_Swing_120fps"), TEXT("Cast") };
		TargetCastWindow = 0.88f;
		bHeavySpell = true;
		break;
	case EAetheriaSpellId::Chronosphere:
		Candidates = { TEXT("Cosmic_Rift"), TEXT("CosmicRift"), TEXT("Ability_R"), TEXT("Cast") };
		TargetCastWindow = 0.76f;
		bHeavySpell = true;
		break;
	case EAetheriaSpellId::ChaosMeteor:
		Candidates = { TEXT("Torn_Space_v2"), TEXT("Torn_Space"), TEXT("CosmicRift_Execute"), TEXT("Ability_R"), TEXT("Ultimate_Swing_120fps") };
		TargetCastWindow = 0.84f;
		bHeavySpell = true;
		break;
	case EAetheriaSpellId::ChainLightning:
		Candidates = { TEXT("Cast"), TEXT("portal_blast_fast"), TEXT("Primary_Attack_C_Medium"), TEXT("Ability_Q") };
		TargetCastWindow = 0.52f;
		break;
	case EAetheriaSpellId::SupernovaBeam:
		Candidates = { TEXT("PortalBlast"), TEXT("portal_blast_medium"), TEXT("Ability_E"), TEXT("Cast") };
		TargetCastWindow = 0.72f;
		bHeavySpell = true;
		break;
	case EAetheriaSpellId::ChronoBlink:
		Candidates = { TEXT("portal_blast_fast"), TEXT("Torn_Space"), TEXT("Ability_RMB_Fwd"), TEXT("Cast") };
		TargetCastWindow = 0.44f;
		break;
	case EAetheriaSpellId::GravityCataclysm:
		Candidates = { TEXT("Burden"), TEXT("Burden_Fast"), TEXT("Ability_E"), TEXT("Ultimate_Swing_120fps"), TEXT("Cast") };
		TargetCastWindow = 0.78f;
		bHeavySpell = true;
		break;
	case EAetheriaSpellId::PrismWall:
		Candidates = { TEXT("Cosmic_Rift"), TEXT("RMB_Cast"), TEXT("Ability_E"), TEXT("Cast") };
		TargetCastWindow = 0.58f;
		break;
	case EAetheriaSpellId::PrimalOverload:
		Candidates = { TEXT("Torn_Space_v2"), TEXT("PortalBlast"), TEXT("Ability_R"), TEXT("Cast") };
		TargetCastWindow = 0.78f;
		bHeavySpell = true;
		break;
	case EAetheriaSpellId::Macropyre:
		Candidates = { TEXT("portal_blast_medium"), TEXT("PortalBlast"), TEXT("Ability_Q"), TEXT("Cast") };
		TargetCastWindow = 0.64f;
		break;
	case EAetheriaSpellId::PlasmaField:
		Candidates = { TEXT("Burden_Fast"), TEXT("Cast"), TEXT("Ability_E"), TEXT("Primary_Attack_C_Medium") };
		TargetCastWindow = 0.60f;
		break;
	case EAetheriaSpellId::CataclysmNova:
		Candidates = { TEXT("CosmicRift_Execute"), TEXT("Torn_Space_v2"), TEXT("Ability_R"), TEXT("Cast") };
		TargetCastWindow = 0.80f;
		bHeavySpell = true;
		break;
	case EAetheriaSpellId::SunStrike:
		Candidates = { TEXT("portal_blast_fast"), TEXT("RMB_Cast"), TEXT("Ability_Q"), TEXT("Cast") };
		TargetCastWindow = 0.56f;
		break;
	case EAetheriaSpellId::EyeOfTheStorm:
		Candidates = { TEXT("Torn_Space"), TEXT("Cosmic_Rift"), TEXT("Ability_E"), TEXT("Cast") };
		TargetCastWindow = 0.68f;
		break;
	case EAetheriaSpellId::RequiemBlast:
		Candidates = { TEXT("Burden"), TEXT("CosmicRift_Execute"), TEXT("Ability_R"), TEXT("Ultimate_Swing_120fps"), TEXT("Cast") };
		TargetCastWindow = 0.86f;
		bHeavySpell = true;
		break;
	default:
		Candidates = { TEXT("Cast"), TEXT("RMB_Cast"), TEXT("Primary_Attack_A_Medium") };
		break;
	}

	// Prefer full-body non-additive sequences first so casts never look twisted or incomplete
	UAnimSequence* Seq = FindSequenceByNames(Candidates, false);
	if (!Seq)
	{
		if (UAnimMontage* Montage = FindMontageByNames(Candidates))
		{
			Seq = AetheriaAnimPrivate::ExtractSequenceFromMontage(Montage);
		}
	}

	USkeleton* TargetSkel = GetTargetSkeleton();
	if (Seq && TargetSkel && Seq->GetSkeleton() == TargetSkel)
	{
		const float SeqLen = FMath::Max(0.25f, Seq->GetPlayLength());
		const float ScaledRate = FMath::Clamp(SeqLen / FMath::Max(0.25f, TargetCastWindow), 0.85f, 1.65f);
		PlaySequenceAction(Seq, ScaledRate, false);
		return;
	}

	PlayAction(bHeavySpell ? EAetheriaAnimAction::CastHeavy : EAetheriaAnimAction::CastPrimary, bHeavySpell ? 1.05f : 1.20f);
}
