void AAetheriaGameMode::StartMainMenu()
{
	bInMainMenu = true;
	bCombatReleased = false;
	// Courtyard is baked into AetheriaWorld. Do not stream a dungeon under the menu.
	if (UWorld* World = GetWorld())
	{
		AetheriaWorldDirector::ApplyLighting(World, EAetheriaWorldId::Bastion);
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
				if (Floor.Size2D() < 2500.f)
				{
					Wiz->SetActorLocation(Floor);
				}
				else
				{
					Wiz->SetActorLocation(FVector(0.f, 0.f, 110.f));
				}
			}
		}
	}
}

void AAetheriaGameMode::StartWorld(EAetheriaWorldId WorldId, bool bFromMenu)
{
	bInMainMenu = false;
	bCombatReleased = false;
	ActiveWorld = WorldId;
	UWorld* World = GetWorld();
	if (!World) return;

	const FString MapName = World->GetMapName();
	if (!AetheriaWorldDirector::MapBelongsToWorld(MapName, WorldId))
	{
		GAetheriaSkipMainMenu = true;
		GAetheriaPendingWorld = WorldId;
		const TCHAR* Path = AetheriaWorldDirector::PersistentMapForWorld(WorldId);
		UE_LOG(LogTemp, Display, TEXT("[Aetheria] OpenLevel baked world %s (from %s)"), Path, *MapName);
		UGameplayStatics::OpenLevel(World, FName(Path));
		return;
	}

	ApplyWorldPresentation();
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
			Wiz->SetActorLocation(Loc);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[Aetheria] StartWorld %s at %s"), AetheriaWorldDirector::WorldDisplayName(WorldId), *Loc.ToCompactString());
}
