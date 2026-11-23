#include "AetheriaPackVFX.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

namespace AetheriaPackVFXPrivate
{
	struct FSpellFX
	{
		const TCHAR* Cast = nullptr;
		const TCHAR* Travel = nullptr;
		const TCHAR* Impact = nullptr;
		float TravelScale = 1.0f;
		float ImpactScale = 1.2f;
		bool bOrientTravel = true;
	};

	// One coherent identity per spell. Each phase is a tightly matched sibling
	// from the SAME pack (or the same system at a different scale). Never mixed families.
	// Table index matches EAetheriaSpellId (None=0 ... RequiemBlast=16).
	static const FSpellFX& ForSpell(EAetheriaSpellId Id)
	{
		static const FSpellFX Table[17] = {
			/* None / Basic bolt: Free Stylized Laser Beam only */
			{ TEXT("/Game/FreeStylizedLaserBeamVFX/VFX/MuzzleFlash/NS_MuzzleFlash_01"),
			  TEXT("/Game/FreeStylizedLaserBeamVFX/VFX/NS_Laser_02"),
			  TEXT("/Game/FreeStylizedLaserBeamVFX/VFX/Hit/NS_Hit_01"), 1.0f, 1.1f, true },

			/* Fireball: Stylish Fire only */
			{ TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_2"),
			  TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_2"),
			  TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_3"), 1.35f, 1.7f, true },

			/* Black Hole: Gideon Ultimate family only */
			{ TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Ultimate/FX/P_Ultimate_Cast"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Ultimate/FX/P_Gideon_Ultimate"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Ultimate/FX/P_Gideon_BlackHole_Beam"), 0.85f, 0.9f, false },

			/* Chronosphere: Spline Magic only */
			{ TEXT("/Game/_SplineVFX/NS/NS_Spline_Magic"),
			  TEXT("/Game/_SplineVFX/NS/NS_Spline_Magic"),
			  TEXT("/Game/_SplineVFX/NS/NS_Spline_Magic"), 1.8f, 1.4f, false },

			/* Chaos Meteor: Gideon Meteor family only */
			{ TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Meteor/FX/P_Meteor_Cast"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Meteor/FX/P_Gideon_Meteor_Trail"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Meteor/FX/P_Gideon_MeteorBigImpact"), 1.5f, 2.2f, true },

			/* Chain Lightning: Vefects Zap only */
			{ TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_05_Purple"),
			  TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_05_Purple"),
			  TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_05_Purple"), 1.15f, 1.2f, true },

			/* Supernova Beam: Free Stylized Laser Beam only */
			{ TEXT("/Game/FreeStylizedLaserBeamVFX/VFX/MuzzleFlash/NS_MuzzleFlash_02"),
			  TEXT("/Game/FreeStylizedLaserBeamVFX/VFX/NS_Laser_01"),
			  TEXT("/Game/FreeStylizedLaserBeamVFX/VFX/Hit/NS_Hit_03"), 1.5f, 1.5f, true },

			/* Chrono Blink: Gideon Portal family only */
			{ TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Portal/FX/P_Portal_Teleport_Enter"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Portal/FX/P_Portal_Entrance"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Portal/FX/P_Portal_Teleport_Exit"), 1.2f, 1.2f, false },

			/* Gravity Cataclysm: Gideon Burden family only */
			{ TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Burden/FX/P_Burden_Cast"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Burden/FX/P_Burden_Cast"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Burden/FX/P_Gideon_Burden_HitWorld"), 1.6f, 1.8f, false },

			/* Prism Wall: Spline Holy only */
			{ TEXT("/Game/_SplineVFX/NS/NS_Spline_Holy"),
			  TEXT("/Game/_SplineVFX/NS/NS_Spline_Holy"),
			  TEXT("/Game/_SplineVFX/NS/NS_Spline_Holy"), 1.3f, 1.0f, false },

			/* Primal Overload: Stylish Fire only */
			{ TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_1"),
			  TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_1"),
			  TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_4"), 1.8f, 2.0f, false },

			/* Macropyre: Spline Fire only */
			{ TEXT("/Game/_SplineVFX/NS/NS_Spline_Fire"),
			  TEXT("/Game/_SplineVFX/NS/NS_Spline_Fire"),
			  TEXT("/Game/_SplineVFX/NS/NS_Spline_Fire"), 1.7f, 1.5f, true },

			/* Plasma Field: Zap White only */
			{ TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_06_White"),
			  TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_06_White"),
			  TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_06_White"), 1.6f, 1.3f, false },

			/* Cataclysm Nova: Gideon Ultimate family only */
			{ TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Ultimate/FX/P_Gideon_Ultimate_Cast"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Ultimate/FX/P_Gideon_Ultimate"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Ultimate/FX/P_Gideon_Ultimate"), 0.90f, 1.00f, false },

			/* Sun Strike: Spline Holy only (sky-drop at target) */
			{ TEXT("/Game/_SplineVFX/NS/NS_Spline_Holy"),
			  TEXT("/Game/_SplineVFX/NS/NS_Spline_Holy"),
			  TEXT("/Game/_SplineVFX/NS/NS_Spline_Holy"), 1.5f, 1.8f, true },

			/* Eye of the Storm: Vefects Wind only */
			{ TEXT("/Game/Vefects/Wind_VFX/VFX/Particles/NS_Wind_Flows_02_Fast"),
			  TEXT("/Game/Vefects/Wind_VFX/VFX/Particles/NS_Wind_Flows_02_Fast"),
			  TEXT("/Game/Vefects/Wind_VFX/VFX/Particles/NS_Wind_Flows_01"), 2.0f, 1.4f, false },

			/* Requiem Blast: Zap Black only */
			{ TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_07_Black"),
			  TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_07_Black"),
			  TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_07_Black"), 1.9f, 2.0f, false }
		};
		const int32 Index = static_cast<int32>(Id);
		if (Index >= 0 && Index < 17) return Table[Index];
		return Table[0];
	}

	static FString ObjectPath(const TCHAR* Package)
	{
		if (!Package || !Package[0]) return FString();
		const FString Pkg(Package);
		return FString::Printf(TEXT("%s.%s"), *Pkg, *FPackageName::GetShortName(Pkg));
	}
}

void UAetheriaPackVFX::PlayNiagara(UWorld* World, const TCHAR* Path, FVector Location, FRotator Rotation, FVector Scale, bool bAutoDestroy)
{
	PlayOne(World, Path, Location, Rotation, Scale, bAutoDestroy);
}

void UAetheriaPackVFX::PlayCascade(UWorld* World, const TCHAR* Path, FVector Location, FRotator Rotation, FVector Scale)
{
	if (!World || !Path) return;
	const FString Obj = AetheriaPackVFXPrivate::ObjectPath(Path);
	if (UParticleSystem* PS = LoadObject<UParticleSystem>(nullptr, *Obj, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::SpawnEmitterAtLocation(World, PS, Location, Rotation, Scale, true);
	}
}

bool UAetheriaPackVFX::PlayOne(UWorld* World, const TCHAR* Path, FVector Location, FRotator Rotation, FVector Scale, bool bAutoDestroy)
{
	if (!World || !Path || !Path[0]) return false;
	const FString Obj = AetheriaPackVFXPrivate::ObjectPath(Path);
	if (UNiagaraSystem* NS = LoadObject<UNiagaraSystem>(nullptr, *Obj, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, NS, Location, Rotation, Scale, bAutoDestroy, true, ENCPoolMethod::None, true);
		return true;
	}
	if (UParticleSystem* PS = LoadObject<UParticleSystem>(nullptr, *Obj, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::SpawnEmitterAtLocation(World, PS, Location, Rotation, Scale, true);
		return true;
	}
	return false;
}

void UAetheriaPackVFX::PlayTaggedLoop(UObject* WorldContext, const TCHAR* NiagaraPath, FVector Location, FRotator Rotation, FVector Scale, FName Tag)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || !NiagaraPath) return;
	const FString Obj = AetheriaPackVFXPrivate::ObjectPath(NiagaraPath);
	if (UNiagaraSystem* NS = LoadObject<UNiagaraSystem>(nullptr, *Obj, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		if (UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, NS, Location, Rotation, Scale, false, true, ENCPoolMethod::None, true))
		{
			if (AActor* Owner = NC->GetOwner())
			{
				Owner->Tags.Add(Tag);
			}
			NC->ComponentTags.Add(Tag);
		}
	}
}

void UAetheriaPackVFX::PlaySpell(UObject* WorldContext, EAetheriaSpellId SpellId, FVector Origin, FVector Target, const TArray<FVector>& ChainPoints)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return;
	const AetheriaPackVFXPrivate::FSpellFX& FX = AetheriaPackVFXPrivate::ForSpell(SpellId);
	const FVector Delta = Target - Origin;
	const FRotator AimRot = Delta.IsNearlyZero() ? FRotator::ZeroRotator : Delta.Rotation();

	// Cast at the staff tip / origin only.
	PlayOne(World, FX.Cast, Origin, AimRot, FVector(1.0f), true);

	if (SpellId == EAetheriaSpellId::ChainLightning && ChainPoints.Num() > 1)
	{
		for (int32 i = 0; i < ChainPoints.Num(); ++i)
		{
			const FRotator HopRot = (i + 1 < ChainPoints.Num()) ? (ChainPoints[i + 1] - ChainPoints[i]).Rotation() : AimRot;
			PlayOne(World, FX.Travel, ChainPoints[i], HopRot, FVector(FX.TravelScale), true);
			if (i + 1 < ChainPoints.Num())
			{
				PlayOne(World, FX.Impact, ChainPoints[i + 1], FRotator::ZeroRotator, FVector(FX.ImpactScale), true);
			}
		}
		return;
	}

	// Travel once (at origin, aimed), impact once (at target). No doubled main+impact stacks.
	PlayOne(World, FX.Travel, Origin, FX.bOrientTravel ? AimRot : FRotator::ZeroRotator, FVector(FX.TravelScale), true);
	if (!Target.Equals(Origin, 8.0f))
	{
		PlayOne(World, FX.Impact, Target, FRotator::ZeroRotator, FVector(FX.ImpactScale), true);
	}
}

void UAetheriaPackVFX::PlayHit(UObject* WorldContext, FVector Location, EAetheriaSpellId SpellId)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return;
	const AetheriaPackVFXPrivate::FSpellFX& FX = AetheriaPackVFXPrivate::ForSpell(SpellId);
	PlayOne(World, FX.Impact, Location, FRotator::ZeroRotator, FVector(FX.ImpactScale), true);
}

void UAetheriaPackVFX::PlayCastFlourish(UObject* WorldContext, FVector Location, EAetheriaSpellId SpellId)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return;
	const AetheriaPackVFXPrivate::FSpellFX& FX = AetheriaPackVFXPrivate::ForSpell(SpellId);
	PlayOne(World, FX.Cast, Location, FRotator::ZeroRotator, FVector(1.0f), true);
}

void UAetheriaPackVFX::PlaySpawnTelegraph(UObject* WorldContext, EAetheriaMobKind Kind, FVector Location, bool bElite)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return;
	const FVector Mark = Location + FVector(0.f, 0.f, 4.f);
	if (bElite || Kind == EAetheriaMobKind::Boss)
	{
		PlayOne(World, TEXT("/Game/_SplineVFX/NS/NS_Spline_Magic"), Mark, FRotator::ZeroRotator, FVector(1.6f), true);
		return;
	}
	switch (Kind)
	{
	case EAetheriaMobKind::Swarm:
		PlayOne(World, TEXT("/Game/Portals/Rounded/EarthPortal/NS_EarthPortal"), Mark, FRotator::ZeroRotator, FVector(0.7f), true);
		break;
	case EAetheriaMobKind::Ranged:
		PlayOne(World, TEXT("/Game/Vefects/VFX_Distortion/VFX/Particles/NS_Distortion_Flat_04"), Mark, FRotator::ZeroRotator, FVector(0.9f), true);
		break;
	case EAetheriaMobKind::Bruiser:
		PlayOne(World, TEXT("/Game/Portals/Rounded/EarthPortal/NS_EarthPortal"), Mark, FRotator::ZeroRotator, FVector(1.1f), true);
		break;
	default:
		PlayOne(World, TEXT("/Game/_SplineVFX/NS/NS_Spline_Magic"), Mark, FRotator::ZeroRotator, FVector(1.0f), true);
		break;
	}
}

void UAetheriaPackVFX::PlaySpawnEntrance(UObject* WorldContext, EAetheriaMobKind Kind, FVector Location, bool bElite, bool bIsBoss)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return;
	if (bIsBoss || Kind == EAetheriaMobKind::Boss)
	{
		PlayOne(World, TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Ultimate/FX/P_Gideon_Ultimate"), Location + FVector(0, 0, 40), FRotator::ZeroRotator, FVector(0.85f), true);
		return;
	}
	if (bElite)
	{
		PlayOne(World, TEXT("/Game/_SplineVFX/NS/NS_Spline_Magic"), Location, FRotator::ZeroRotator, FVector(1.8f), true);
		return;
	}
	switch (Kind)
	{
	case EAetheriaMobKind::Swarm:
		PlayOne(World, TEXT("/Game/Portals/Rounded/EarthPortal/NS_EarthPortal"), Location, FRotator::ZeroRotator, FVector(1.0f), true);
		break;
	case EAetheriaMobKind::Ranged:
		PlayOne(World, TEXT("/Game/Vefects/VFX_Distortion/VFX/Particles/NS_Distortion_Flat_08"), Location + FVector(0, 0, 40), FRotator::ZeroRotator, FVector(1.1f), true);
		break;
	case EAetheriaMobKind::Bruiser:
		PlayOne(World, TEXT("/Game/Portals/Rounded/EarthPortal/NS_EarthPortal"), Location, FRotator::ZeroRotator, FVector(1.4f), true);
		break;
	default:
		break;
	}
}
