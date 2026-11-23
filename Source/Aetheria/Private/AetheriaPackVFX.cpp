#include "AetheriaPackVFX.h"
#include "AetheriaSpellActors.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/PackageName.h"

namespace AetheriaPackVFXPrivate
{
	struct FSpellFX
	{
		const TCHAR* Cast = nullptr;
		const TCHAR* CastAccent = nullptr;
		const TCHAR* Travel = nullptr;
		const TCHAR* Impact = nullptr;
		const TCHAR* Shockwave = nullptr;
		float CastScale = 1.0f;
		float TravelScale = 1.2f;
		float ImpactScale = 1.35f;
		float ShockwaveScale = 1.4f;
		bool bOrientTravel = true;
	};

	// Rich multi-layered Fab & Paragon VFX table covering ALL 16 EAetheriaSpellId spells (+ None basic bolt = 17 entries).
	// Every single /Game/... path is verified on disk under Content/.
	static const FSpellFX& ForSpell(EAetheriaSpellId Id)
	{
		static const FSpellFX Table[17] = {
			/* 0: None / Basic Arcane Bolt */
			{ TEXT("/Game/FreeStylizedLaserBeamVFX/VFX/MuzzleFlash/NS_MuzzleFlash_01"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Mana"),
			  TEXT("/Game/FreeStylizedLaserBeamVFX/VFX/NS_Laser_02"),
			  TEXT("/Game/FreeStylizedLaserBeamVFX/VFX/Hit/NS_Hit_01"),
			  TEXT("/Game/FXVarietyPack/Particles/P_ky_hit1"),
			  0.95f, 1.05f, 1.15f, 1.0f, true },

			/* 1: QQQ Fireball (Solar Cataclysm Fireball) */
			{ TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Fire"),
			  TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_2"),
			  TEXT("/Game/FXVarietyPack/Particles/P_ky_fireBall"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Explosion"),
			  TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_3"),
			  1.15f, 1.40f, 1.85f, 1.75f, true },

			/* 2: EEE BlackHole (Interstellar Gargantua Black Hole) */
			{ TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Ultimate/FX/P_Gideon_Ultimate_Cast"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Soul"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Ultimate/FX/P_Gideon_Ultimate"),
			  TEXT("/Game/Vefects/VFX_Distortion/VFX/Particles/NS_Distortion_Flat_05"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Ultimate/FX/P_Gideon_BlackHole_Beam"),
			  1.10f, 1.05f, 2.60f, 1.05f, false },

			/* 3: WWW Chronosphere (Space-Time Chronosphere) */
			{ TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Circle"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Mana"),
			  TEXT("/Game/_SplineVFX/NS/NS_Spline_Magic"),
			  TEXT("/Game/ParagonAurora/FX/Particles/Abilities/Ultimate/FX/P_Aurora_Ultimate_InitialBlast"),
			  TEXT("/Game/Vefects/VFX_Distortion/VFX/Particles/NS_Distortion_Flat_03"),
			  1.25f, 1.85f, 1.45f, 2.20f, false },

			/* 4: QQE ChaosMeteor (Dota 2 Invoker Rolling Chaos Meteor) */
			{ TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Meteor/FX/P_Meteor_Cast"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Circle"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Meteor/FX/P_Gideon_Meteor_Trail"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Meteor/FX/P_Gideon_MeteorBigImpact"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Explosion"),
			  1.20f, 1.55f, 2.10f, 2.00f, true },

			/* 5: QWW ChainLightning (Crimson Desert Forked Chain Lightning) */
			{ TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Lightning"),
			  TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_02_Blue"),
			  TEXT("/Game/_SplineVFX/NS/NS_Spline_ElectricLightning"),
			  TEXT("/Game/FXVarietyPack/Particles/P_ky_lightning1"),
			  TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_05_Purple"),
			  1.15f, 1.35f, 1.45f, 1.25f, true },

			/* 6: QQW SupernovaBeam (Solar Supernova Beam) */
			{ TEXT("/Game/FreeStylizedLaserBeamVFX/VFX/MuzzleFlash/NS_MuzzleFlash_02"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Fire"),
			  TEXT("/Game/FreeStylizedLaserBeamVFX/VFX/NS_Laser_01"),
			  TEXT("/Game/FreeStylizedLaserBeamVFX/VFX/Hit/NS_Hit_03"),
			  TEXT("/Game/FXVarietyPack/Particles/P_ky_explosion"),
			  1.25f, 1.65f, 1.65f, 1.50f, true },

			/* 7: WWE ChronoBlink (Chrono-Blink Rift Portal) */
			{ TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Portal/FX/P_Portal_Teleport_Enter"),
			  TEXT("/Game/Portals/Rounded/WindPortal/NS_WindPortal"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Portal/FX/P_Portal_Entrance"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Portal/FX/P_Portal_Teleport_Exit"),
			  TEXT("/Game/Vefects/VFX_Distortion/VFX/Particles/NS_Distortion_Flat_02"),
			  1.15f, 1.25f, 1.35f, 1.80f, false },

			/* 8: QEE GravityCataclysm (Abyssal Gravity Cataclysm) */
			{ TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Burden/FX/P_Burden_Cast"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Soul"),
			  TEXT("/Game/ParagonSevarog/FX/Particles/Abilities/Subjugate/FX/P_SubjugateSwirls"),
			  TEXT("/Game/ParagonSevarog/FX/Particles/Abilities/Subjugate/FX/P_Sevarog_Subjugate_Blast"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Shockwave"),
			  1.20f, 1.65f, 1.85f, 2.00f, false },

			/* 9: WEE PrismWall (Cryo-Void Prism Wall) */
			{ TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Mana"),
			  TEXT("/Game/_SplineVFX/NS/NS_Spline_Frost"),
			  TEXT("/Game/_SplineVFX/NS/NS_Spline_Holy"),
			  TEXT("/Game/ParagonAurora/FX/Particles/Abilities/Ultimate/FX/P_Aurora_Ultimate_Explode"),
			  TEXT("/Game/Vefects/VFX_Distortion/VFX/Particles/NS_Distortion_Flat_04"),
			  1.15f, 1.45f, 1.35f, 1.65f, false },

			/* 10: QWE PrimalOverload (Primal Triad Overload / Blazing Fire Tornado) */
			{ TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_LevelUp"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Fire"),
			  TEXT("/Game/FXVarietyPack/Particles/P_ky_fireStorm"),
			  TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_4"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Shockwave"),
			  1.35f, 1.95f, 2.15f, 2.10f, false },

			/* 11: QEQ Macropyre (Macropyre Magma Corridor) */
			{ TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Fire"),
			  TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_1"),
			  TEXT("/Game/_SplineVFX/NS/NS_Spline_Fire"),
			  TEXT("/Game/FXVarietyPack/Particles/P_ky_fireStorm"),
			  TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_3"),
			  1.25f, 1.80f, 1.65f, 1.60f, true },

			/* 12: WQW PlasmaField (Plasma Field Lightning Ring) */
			{ TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Buff_Lightning"),
			  TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_06_White"),
			  TEXT("/Game/FXVarietyPack/Particles/P_ky_thunderStorm"),
			  TEXT("/Game/FXVarietyPack/Particles/P_ky_ThunderBallHit"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Shockwave"),
			  1.20f, 1.65f, 1.55f, 1.85f, false },

			/* 13: EQE CataclysmNova (Cataclysmic Supernova Explosion) */
			{ TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Circle"),
			  TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Ultimate/FX/P_Gideon_Ultimate_Cast"),
			  TEXT("/Game/FXVarietyPack/Particles/P_ky_darkStorm"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Explosion"),
			  TEXT("/Game/Vefects/VFX_Distortion/VFX/Particles/NS_Distortion_Flat_07"),
			  1.35f, 1.45f, 2.25f, 2.40f, false },

			/* 14: QWQ SunStrike (Helios Sun Strike) */
			{ TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Circle"),
			  TEXT("/Game/_SplineVFX/NS/NS_Spline_Holy"),
			  TEXT("/Game/FreeStylizedLaserBeamVFX/VFX/NS_Laser_03"),
			  TEXT("/Game/FXVarietyPack/Particles/P_ky_explosion"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Shockwave"),
			  1.30f, 1.65f, 2.00f, 2.15f, true },

			/* 15: WEW EyeOfTheStorm (Eye of the Storm) */
			{ TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Air"),
			  TEXT("/Game/Vefects/Wind_VFX/VFX/Particles/NS_Wind_Flows_02_Fast"),
			  TEXT("/Game/FXVarietyPack/Particles/P_ky_storm"),
			  TEXT("/Game/FXVarietyPack/Particles/P_ky_lightning2"),
			  TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_02_Blue"),
			  1.25f, 1.85f, 1.55f, 1.45f, false },

			/* 16: EWE RequiemBlast (Requiem of Shattered Souls) */
			{ TEXT("/Game/ParagonSevarog/FX/Particles/Abilities/Ultimate/FX/P_UltActivate"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Aura_Soul"),
			  TEXT("/Game/ParagonSevarog/FX/Particles/Abilities/Ultimate/FX/P_UltMeshArc_Expanding"),
			  TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_07_Black"),
			  TEXT("/Game/Free_Spells/VFX_Niagara/NS_Free_Spells_Shockwave"),
			  1.35f, 1.85f, 2.10f, 2.25f, false }
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

bool UAetheriaPackVFX::PlayOne(UWorld* World, const TCHAR* Path, FVector Location, FRotator Rotation, FVector Scale, bool bAutoDestroy, float MaxLifeSeconds)
{
	if (!World || !Path || !Path[0]) return false;
	const FString Obj = AetheriaPackVFXPrivate::ObjectPath(Path);
	if (UNiagaraSystem* NS = LoadObject<UNiagaraSystem>(nullptr, *Obj, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		if (UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, NS, Location, Rotation, Scale, bAutoDestroy, true, ENCPoolMethod::None, true))
		{
			if (MaxLifeSeconds > 0.0f)
			{
				TWeakObjectPtr<UNiagaraComponent> WeakNC(NC);
				FTimerHandle Handle;
				World->GetTimerManager().SetTimer(Handle, [WeakNC]()
				{
					if (UNiagaraComponent* C = WeakNC.Get())
					{
						C->DeactivateImmediate();
						C->DestroyComponent();
					}
				}, MaxLifeSeconds, false);
			}
			return true;
		}
		return false;
	}
	if (UParticleSystem* PS = LoadObject<UParticleSystem>(nullptr, *Obj, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		if (UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAtLocation(World, PS, Location, Rotation, Scale, bAutoDestroy))
		{
			if (MaxLifeSeconds > 0.0f)
			{
				TWeakObjectPtr<UParticleSystemComponent> WeakPSC(PSC);
				FTimerHandle Handle;
				World->GetTimerManager().SetTimer(Handle, [WeakPSC]()
				{
					if (UParticleSystemComponent* C = WeakPSC.Get())
					{
						C->DeactivateSystem();
						C->DestroyComponent();
					}
				}, MaxLifeSeconds, false);
			}
			return true;
		}
		return false;
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

	// Layered Cast + Runic Accent at staff tip / caster origin
	PlayOne(World, FX.Cast, Origin, AimRot, FVector(FX.CastScale), true, 1.4f);
	if (FX.CastAccent)
	{
		PlayOne(World, FX.CastAccent, Origin, FRotator::ZeroRotator, FVector(FX.CastScale * 0.95f), true, 1.4f);
	}

	// Projectile spells spawn layered impact + shockwave in AAetheriaSpellProjectile::Detonate().
	if (SpellId == EAetheriaSpellId::None || SpellId == EAetheriaSpellId::Fireball)
	{
		return;
	}

	// ChaosMeteor zone spawns exact timed Phase 1 target circle -> Phase 2 impact slam -> Phase 3 rolling fire trail!
	if (SpellId == EAetheriaSpellId::ChaosMeteor)
	{
		return;
	}

	// BlackHole zone layers Gargantua core + NS_Distortion_Flat_05 + P_Gideon_Ultimate at target; add black-hole tether flash here
	if (SpellId == EAetheriaSpellId::BlackHole)
	{
		if (FX.Shockwave)
		{
			PlayOne(World, FX.Shockwave, Target + FVector(0.0f, 0.0f, 95.0f), AimRot, FVector(FX.ShockwaveScale), true, 2.2f);
		}
		return;
	}

	// Delayed-detonation zones play telegraph/travel now and invoke PlayHit() on exact detonation frame
	if (SpellId == EAetheriaSpellId::SunStrike || SpellId == EAetheriaSpellId::CataclysmNova || SpellId == EAetheriaSpellId::RequiemBlast)
	{
		PlayOne(World, FX.Cast, Target + FVector(0.0f, 0.0f, 10.0f), FRotator::ZeroRotator, FVector(FX.CastScale * 1.45f), true, 1.3f);
		PlayOne(World, FX.Travel, Target + FVector(0.0f, 0.0f, 20.0f), FX.bOrientTravel ? AimRot : FRotator::ZeroRotator, FVector(FX.TravelScale), true, 2.2f);
		return;
	}

	if (SpellId == EAetheriaSpellId::ChainLightning && ChainPoints.Num() > 1)
	{
		for (int32 i = 0; i < ChainPoints.Num(); ++i)
		{
			const FRotator HopRot = (i + 1 < ChainPoints.Num()) ? (ChainPoints[i + 1] - ChainPoints[i]).Rotation() : AimRot;
			PlayOne(World, FX.Travel, ChainPoints[i], HopRot, FVector(FX.TravelScale), true, 1.6f);
			if (i + 1 < ChainPoints.Num())
			{
				PlayOne(World, FX.Impact, ChainPoints[i + 1], FRotator::ZeroRotator, FVector(FX.ImpactScale), true, 1.6f);
				if (FX.Shockwave)
				{
					PlayOne(World, FX.Shockwave, ChainPoints[i + 1], FRotator::ZeroRotator, FVector(FX.ShockwaveScale), true, 1.2f);
				}
			}
		}
		return;
	}

	// Macropyre: blazing magma corridor stepped from origin forward along cast direction
	if (SpellId == EAetheriaSpellId::Macropyre)
	{
		const FVector Fwd2D = Delta.IsNearlyZero() ? FVector::ForwardVector : Delta.GetSafeNormal2D();
		for (int32 Step = 1; Step <= 6; ++Step)
		{
			const FVector StepLoc = Origin + Fwd2D * (Step * 260.0f);
			const TCHAR* FlamePath = ((Step % 2) == 0) ? FX.Impact : TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_1");
			PlayOne(World, FlamePath, StepLoc, AimRot, FVector(1.65f), true, 3.2f);
		}
		PlayOne(World, FX.Travel, Origin + Fwd2D * 140.0f, AimRot, FVector(FX.TravelScale), true, 3.0f);
		return;
	}

	// Standard layered Travel + Target Impact + Shockwave/Distortion
	const FVector TravelLoc = FX.bOrientTravel ? Origin : Target;
	PlayOne(World, FX.Travel, TravelLoc, FX.bOrientTravel ? AimRot : FRotator::ZeroRotator, FVector(FX.TravelScale), true, 2.6f);
	if (!Target.Equals(Origin, 8.0f) || !FX.bOrientTravel)
	{
		PlayOne(World, FX.Impact, Target, FRotator::ZeroRotator, FVector(FX.ImpactScale), true, 2.4f);
		if (FX.Shockwave)
		{
			PlayOne(World, FX.Shockwave, Target + FVector(0.0f, 0.0f, 12.0f), FRotator::ZeroRotator, FVector(FX.ShockwaveScale), true, 1.8f);
		}
	}
}

void UAetheriaPackVFX::PlayHit(UObject* WorldContext, FVector Location, EAetheriaSpellId SpellId)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return;
	const AetheriaPackVFXPrivate::FSpellFX& FX = AetheriaPackVFXPrivate::ForSpell(SpellId);
	PlayOne(World, FX.Impact, Location, FRotator::ZeroRotator, FVector(FX.ImpactScale), true, 2.2f);
	if (FX.Shockwave)
	{
		PlayOne(World, FX.Shockwave, Location + FVector(0.0f, 0.0f, 8.0f), FRotator::ZeroRotator, FVector(FX.ShockwaveScale), true, 1.7f);
	}
}

void UAetheriaPackVFX::PlayCastFlourish(UObject* WorldContext, FVector Location, EAetheriaSpellId SpellId)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return;
	const AetheriaPackVFXPrivate::FSpellFX& FX = AetheriaPackVFXPrivate::ForSpell(SpellId);
	PlayOne(World, FX.Cast, Location, FRotator::ZeroRotator, FVector(FX.CastScale), true, 1.3f);
	if (FX.CastAccent)
	{
		PlayOne(World, FX.CastAccent, Location, FRotator::ZeroRotator, FVector(FX.CastScale * 0.9f), true, 1.2f);
	}
}

void UAetheriaPackVFX::PlaySpawnTelegraph(UObject* WorldContext, EAetheriaMobKind Kind, FVector Location, bool bElite)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return;
	const FVector Mark = Location + FVector(0.f, 0.f, 4.f);
	if (bElite || Kind == EAetheriaMobKind::Boss)
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, Mark, 280.f, FLinearColor(0.88f, 0.14f, 0.24f, 1.f), 0.90f);
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, Mark + FVector(0.f, 0.f, 2.f), 190.f, FLinearColor(0.96f, 0.76f, 0.24f, 1.f), 0.90f);
		PlayOne(World, TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Portal/FX/P_Portal_Teleport_Enter"), Mark + FVector(0.f, 0.f, 20.f), FRotator::ZeroRotator, FVector(0.65f), true, 1.15f);
		return;
	}
	switch (Kind)
	{
	case EAetheriaMobKind::Swarm:
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, Mark, 160.f, FLinearColor(0.76f, 0.16f, 0.42f, 1.f), 0.90f);
		break;
	case EAetheriaMobKind::Ranged:
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, Mark, 170.f, FLinearColor(0.24f, 0.72f, 0.98f, 1.f), 0.90f);
		PlayOne(World, TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Portal/FX/P_Portal_Teleport_Enter"), Mark + FVector(0.f, 0.f, 20.f), FRotator::ZeroRotator, FVector(0.65f), true, 1.15f);
		break;
	case EAetheriaMobKind::Bruiser:
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, Mark, 220.f, FLinearColor(0.95f, 0.34f, 0.08f, 1.f), 0.90f);
		break;
	default:
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, Mark, 180.f, FLinearColor(0.55f, 0.22f, 0.92f, 1.f), 0.90f);
		break;
	}
}

void UAetheriaPackVFX::PlaySpawnEntrance(UObject* WorldContext, EAetheriaMobKind Kind, FVector Location, bool bElite, bool bIsBoss)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return;
	if (bIsBoss || Kind == EAetheriaMobKind::Boss || bElite)
	{
		PlayOne(World, TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Ultimate/FX/P_Gideon_Ultimate"), Location + FVector(0, 0, 40), FRotator::ZeroRotator, FVector(0.75f), true, 2.2f);
		return;
	}
	switch (Kind)
	{
	case EAetheriaMobKind::Swarm:
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, Location, 190.f, FLinearColor(0.85f, 0.18f, 0.25f, 1.f), 0.42f);
		PlayOne(World, TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Portal/FX/P_Portal_Teleport_Exit"), Location + FVector(0, 0, 20), FRotator::ZeroRotator, FVector(0.60f), true, 1.4f);
		break;
	case EAetheriaMobKind::Ranged:
		PlayOne(World, TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Portal/FX/P_Portal_Teleport_Exit"), Location + FVector(0, 0, 35), FRotator::ZeroRotator, FVector(0.75f), true, 1.5f);
		break;
	case EAetheriaMobKind::Bruiser:
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, Location, 280.f, FLinearColor(1.0f, 0.36f, 0.08f, 1.f), 0.55f);
		PlayOne(World, TEXT("/Game/ParagonGideon/FX/Particles/Gideon/Abilities/Burden/FX/P_Gideon_Burden_HitWorld"), Location, FRotator::ZeroRotator, FVector(0.85f), true, 1.8f);
		break;
	default:
		break;
	}
}
