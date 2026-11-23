	if (FParse::Param(FCommandLine::Get(), TEXT("GrokSmoke")))
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
