#include "AetheriaSaveGame.h"
#include "Kismet/GameplayStatics.h"

int32 UAetheriaSaveGame::GetXPForNextLevel(int32 CurrentLevel)
{
	const float L = static_cast<float>(FMath::Max(1, CurrentLevel));
	return FMath::RoundToInt(100.0f * FMath::Pow(L, 1.65f));
}

UAetheriaSaveGame* UAetheriaSaveGame::LoadOrCreateSave(const FString& SlotName)
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		if (UAetheriaSaveGame* Loaded = Cast<UAetheriaSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
		{
			if (Loaded->SaveVersion < 1)
			{
				Loaded->SaveVersion = 1;
			}
			return Loaded;
		}
	}

	return Cast<UAetheriaSaveGame>(UGameplayStatics::CreateSaveGameObject(UAetheriaSaveGame::StaticClass()));
}

bool UAetheriaSaveGame::PersistSave(UAetheriaSaveGame* SaveObj, const FString& SlotName)
{
	if (!SaveObj) return false;
	return UGameplayStatics::SaveGameToSlot(SaveObj, SlotName, 0);
}

bool UAetheriaSaveGame::UnlockCosmeticWithEssence(int32 BitIndex, int32 Cost)
{
	if (BitIndex < 0 || BitIndex >= 31) return false;
	const int32 BitFlag = (1 << BitIndex);
	if ((UnlockedCosmeticsMask & BitFlag) != 0)
	{
		return true;
	}
	if (ArcaneEssence < Cost)
	{
		return false;
	}
	ArcaneEssence -= Cost;
	UnlockedCosmeticsMask |= BitFlag;
	return PersistSave(this);
}
