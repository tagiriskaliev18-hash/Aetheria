#pragma once

#include "CoreMinimal.h"
#include "AetheriaTypes.h"

class UWorld;
class AAetheriaSanctumGate;

namespace AetheriaWorldDirector
{
	void ClearDressing(UWorld* World);
	void ApplyLighting(UWorld* World, EAetheriaWorldId WorldId);
	void PatchBrokenMaterials(UWorld* World, EAetheriaWorldId WorldId);
	void Dress(UWorld* World, EAetheriaWorldId WorldId);
	AAetheriaSanctumGate* SpawnGate(UWorld* World, EAetheriaWorldId WorldId);
	const TCHAR* WorldDisplayName(EAetheriaWorldId WorldId);
	const TCHAR* WorldSubtitle(EAetheriaWorldId WorldId);
	const TCHAR* BossHeroForWorld(EAetheriaWorldId WorldId);
	const TCHAR* BossPackageForWorld(EAetheriaWorldId WorldId);
	FVector GateLocationForWorld(EAetheriaWorldId WorldId);
	FVector BossArenaLocationForWorld(EAetheriaWorldId WorldId);
	const TCHAR* PersistentMapForWorld(EAetheriaWorldId WorldId);
	const TCHAR* LayoutMapForWorld(EAetheriaWorldId WorldId);
	FVector PlayerStartForWorld(UWorld* World, EAetheriaWorldId WorldId);
	bool MapBelongsToWorld(const FString& MapName, EAetheriaWorldId WorldId);
	EAetheriaWorldId WorldIdFromMapName(const FString& MapName);
	bool IsBakedWorldMap(const FString& MapName);
}
