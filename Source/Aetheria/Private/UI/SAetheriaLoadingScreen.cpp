// Copyright Aetheria. All Rights Reserved.
#include "UI/SAetheriaLoadingScreen.h"
#include "AetheriaWorldDirector.h"

#include "MoviePlayer.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "Fonts/SlateFontInfo.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "AetheriaLoadingScreen"

DEFINE_LOG_CATEGORY_STATIC(LogAetheriaLoading, Log, All);

namespace AetheriaLoadingPrivate
{
	static UTexture2D* GCachedKeyartTex = nullptr;
	static bool bKeyartAttempted = false;
	static bool bDelegatesRegistered = false;
	static bool bTravelArmed = false;
	static TSharedPtr<SAetheriaLoadingScreen> GViewportWidget;
	static TWeakObjectPtr<UGameViewportClient> GAttachedViewport;
	static FTimerHandle GDismissTimerHandle;
}

void FAetheriaLoadingState::ResetForTravel(const FString& InTitle, const FString& InLore, float InInitialProgress)
{
	FScopeLock Lock(&Mutex);
	WorldTitle = InTitle;
	LoreLine = InLore;
	StatusLine = TEXT("Пробуждение хроно-печати и потоковая загрузка мира...");
	TargetProgress = FMath::Clamp(InInitialProgress, 0.05f, 0.95f);
	DisplayedProgress = TargetProgress;
	bStreamingComplete = false;
	bFadingOut = false;
	FadeAlpha = 1.0f;
}

void FAetheriaLoadingState::SetProgress(float InTargetProgress, const FString& InStatus)
{
	FScopeLock Lock(&Mutex);
	TargetProgress = FMath::Clamp(FMath::Max(TargetProgress, InTargetProgress), 0.0f, 1.0f);
	if (!InStatus.IsEmpty())
	{
		StatusLine = InStatus;
	}
}

void FAetheriaLoadingState::MarkComplete()
{
	FScopeLock Lock(&Mutex);
	TargetProgress = 1.0f;
	DisplayedProgress = 1.0f;
	bStreamingComplete = true;
	StatusLine = TEXT("Врата открыты. Добро пожаловать в Аэтерию.");
}

void FAetheriaLoadingState::StartFadeOut()
{
	FScopeLock Lock(&Mutex);
	bStreamingComplete = true;
	TargetProgress = 1.0f;
	bFadingOut = true;
}

UTexture2D* SAetheriaLoadingScreen::PreloadKeyartTexture()
{
	if (!IsInGameThread())
	{
		return AetheriaLoadingPrivate::GCachedKeyartTex;
	}
	if (AetheriaLoadingPrivate::GCachedKeyartTex && IsValid(AetheriaLoadingPrivate::GCachedKeyartTex))
	{
		return AetheriaLoadingPrivate::GCachedKeyartTex;
	}
	if (AetheriaLoadingPrivate::bKeyartAttempted)
	{
		return nullptr;
	}
	AetheriaLoadingPrivate::bKeyartAttempted = true;

	// 1. Prefer cooked uasset if imported into /Game/Cinematic/menu_keyart
	UTexture2D* Tex = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/Cinematic/menu_keyart.menu_keyart"),
		nullptr,
		LOAD_NoWarn | LOAD_Quiet);

	// 2. Fallback to staged RawAssets/Cinematic/menu_keyart.png
	if (!Tex)
	{
		const FString Candidates[] = {
			FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("RawAssets/Cinematic/menu_keyart.png")),
			FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Cinematic/menu_keyart.png"))
		};
		for (const FString& Path : Candidates)
		{
			if (IFileManager::Get().FileExists(*Path))
			{
				Tex = FImageUtils::ImportFileAsTexture2D(Path);
				if (Tex)
				{
					Tex->LODGroup = TEXTUREGROUP_UI;
					Tex->NeverStream = true;
					break;
				}
			}
		}
	}

	if (Tex)
	{
		Tex->AddToRoot();
		AetheriaLoadingPrivate::GCachedKeyartTex = Tex;
		UE_LOG(LogAetheriaLoading, Display, TEXT("[LoadingScreen] Loaded keyart texture (%dx%d)"), Tex->GetSizeX(), Tex->GetSizeY());
	}
	else
	{
		UE_LOG(LogAetheriaLoading, Warning, TEXT("[LoadingScreen] Keyart not found; using rich dark-fantasy fallback gradient"));
	}
	return AetheriaLoadingPrivate::GCachedKeyartTex;
}

void SAetheriaLoadingScreen::Construct(const FArguments& InArgs)
{
	{
		FAetheriaLoadingState& State = FAetheriaLoadingState::Get();
		FScopeLock Lock(&State.Mutex);
		if (!InArgs._WorldTitle.IsEmpty())
		{
			State.WorldTitle = InArgs._WorldTitle;
		}
		if (!InArgs._LoreLine.IsEmpty())
		{
			State.LoreLine = InArgs._LoreLine;
		}
	}

	UTexture2D* KeyartTex = IsInGameThread() ? PreloadKeyartTexture() : AetheriaLoadingPrivate::GCachedKeyartTex;
	KeyartBrush = FSlateBrush();
	if (KeyartTex && IsValid(KeyartTex))
	{
		KeyartBrush.SetResourceObject(KeyartTex);
		KeyartBrush.ImageSize = FVector2D(
			FMath::Max(1920.0f, static_cast<float>(KeyartTex->GetSizeX())),
			FMath::Max(1080.0f, static_cast<float>(KeyartTex->GetSizeY())));
		KeyartBrush.DrawAs = ESlateBrushDrawType::Image;
		KeyartBrush.Tiling = ESlateBrushTileType::NoTile;
		bHasKeyart = true;
	}
	else
	{
		KeyartBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		bHasKeyart = false;
	}

	PulseSequence = FCurveSequence(0.0f, 2.2f);
	PulseCurve = PulseSequence.AddCurve(0.0f, 2.2f, ECurveEaseFunction::QuadInOut);
	PulseSequence.Play(AsShared(), /*bPlayLooped=*/true);

	BirthTime = FPlatformTime::Seconds();

	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 42);
	const FSlateFontInfo LoreFont = FCoreStyle::GetDefaultFontStyle("Italic", 17);
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);

	ChildSlot
	[
		SNew(SOverlay)

		// Layer 0: Full-screen keyart (or transparent so OnPaint gradient shows cleanly)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SImage)
			.Image(&KeyartBrush)
			.Visibility(bHasKeyart ? EVisibility::Visible : EVisibility::Collapsed)
			.ColorAndOpacity_Lambda([this]() -> FSlateColor
			{
				const FLinearColor Tint = GetRootTint();
				return FSlateColor(FLinearColor(0.84f * Tint.R, 0.84f * Tint.G, 0.90f * Tint.B, Tint.A));
			})
		]

		// Layer 1: Dark atmospheric veil so gold/ivory typography is always crisp
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor_Lambda([this]() -> FSlateColor
			{
				const FLinearColor Tint = GetRootTint();
				return FSlateColor(FLinearColor(0.015f, 0.014f, 0.025f, (bHasKeyart ? 0.54f : 0.25f) * Tint.A));
			})
			.Padding(0)
		]

		// Layer 2: Bottom-anchored world title, lore, and gilded-violet progress bar
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(110.0f, 0.0f, 260.0f, 84.0f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SAssignNew(TitleText, STextBlock)
				.Text(this, &SAetheriaLoadingScreen::GetTitleText)
				.Font(TitleFont)
				.ColorAndOpacity_Lambda([this]() -> FSlateColor
				{
					const FLinearColor Tint = GetRootTint();
					return FSlateColor(FLinearColor(0.95f, 0.85f, 0.58f, Tint.A));
				})
				.ShadowOffset(FVector2D(2.0f, 2.0f))
				.ShadowColorAndOpacity_Lambda([this]() -> FLinearColor
				{
					return FLinearColor(0.0f, 0.0f, 0.0f, 0.92f * GetRootTint().A);
				})
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 22.0f)
			[
				SAssignNew(LoreText, STextBlock)
				.Text(this, &SAetheriaLoadingScreen::GetLoreText)
				.Font(LoreFont)
				.ColorAndOpacity_Lambda([this]() -> FSlateColor
				{
					const FLinearColor Tint = GetRootTint();
					return FSlateColor(FLinearColor(0.82f, 0.83f, 0.89f, 0.94f * Tint.A));
				})
				.AutoWrapText(true)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor_Lambda([this]() -> FSlateColor
				{
					return FSlateColor(FLinearColor(0.76f, 0.62f, 0.34f, 0.55f * GetRootTint().A));
				})
				.Padding(FMargin(1.5f))
				[
					SNew(SBox)
					.HeightOverride(10.0f)
					[
						SAssignNew(ProgressBar, SProgressBar)
						.Percent(this, &SAetheriaLoadingScreen::GetProgress)
						.FillColorAndOpacity_Lambda([this]() -> FSlateColor
						{
							return FSlateColor(FLinearColor(0.72f, 0.48f, 0.98f, GetRootTint().A));
						})
						.BackgroundImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.FillImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderPadding(FVector2D::ZeroVector)
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SAetheriaLoadingScreen::GetStatusText)
				.Font(SmallFont)
				.ColorAndOpacity_Lambda([this]() -> FSlateColor
				{
					return FSlateColor(FLinearColor(0.74f, 0.72f, 0.78f, 0.86f * GetRootTint().A));
				})
			]
		]
	];
}

TOptional<float> SAetheriaLoadingScreen::GetProgress() const
{
	const FAetheriaLoadingState& State = FAetheriaLoadingState::Get();
	FScopeLock Lock(&State.Mutex);
	return FMath::Clamp(State.DisplayedProgress, 0.0f, 1.0f);
}

FText SAetheriaLoadingScreen::GetTitleText() const
{
	const FAetheriaLoadingState& State = FAetheriaLoadingState::Get();
	FScopeLock Lock(&State.Mutex);
	return FText::FromString(State.WorldTitle.IsEmpty() ? TEXT("AETHERIA") : State.WorldTitle.ToUpper());
}

FText SAetheriaLoadingScreen::GetLoreText() const
{
	const FAetheriaLoadingState& State = FAetheriaLoadingState::Get();
	FScopeLock Lock(&State.Mutex);
	return FText::FromString(State.LoreLine);
}

FText SAetheriaLoadingScreen::GetStatusText() const
{
	const FAetheriaLoadingState& State = FAetheriaLoadingState::Get();
	FScopeLock Lock(&State.Mutex);
	return State.StatusLine.IsEmpty()
		? LOCTEXT("Channeling", "Пробуждение Хроно-Печати...")
		: FText::FromString(State.StatusLine);
}

FLinearColor SAetheriaLoadingScreen::GetRootTint() const
{
	const FAetheriaLoadingState& State = FAetheriaLoadingState::Get();
	FScopeLock Lock(&State.Mutex);
	return FLinearColor(1.0f, 1.0f, 1.0f, FMath::Clamp(State.FadeAlpha, 0.0f, 1.0f));
}

void SAetheriaLoadingScreen::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const float ClampedDt = FMath::Clamp(InDeltaTime, 0.001f, 0.1f);
	RingAngle = FMath::Fmod(RingAngle + ClampedDt * 88.0f, 360.0f);

	FAetheriaLoadingState& State = FAetheriaLoadingState::Get();
	FScopeLock Lock(&State.Mutex);

	// Gently creep progress while map/streaming loads so the bar always feels alive
	if (!State.bStreamingComplete && State.TargetProgress < 0.88f)
	{
		State.TargetProgress = FMath::Min(0.88f, State.TargetProgress + ClampedDt * 0.045f);
	}
	const float Target = State.bStreamingComplete ? 1.0f : State.TargetProgress;
	State.DisplayedProgress = FMath::FInterpTo(State.DisplayedProgress, Target, ClampedDt, 4.5f);

	if (State.bFadingOut)
	{
		State.FadeAlpha = FMath::Max(0.0f, State.FadeAlpha - ClampedDt * 3.0f);
	}
}

int32 SAetheriaLoadingScreen::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                                      const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                                      int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");

	float FadeAlpha = 1.0f;
	{
		const FAetheriaLoadingState& State = FAetheriaLoadingState::Get();
		FScopeLock Lock(&State.Mutex);
		FadeAlpha = FMath::Clamp(State.FadeAlpha, 0.0f, 1.0f);
	}

	// Rich dark-fantasy multi-stop gradient backdrop (always drawn behind keyart so edges/fallback are never white or flat black)
	if (LocalSize.X > 1.0f && LocalSize.Y > 1.0f)
	{
		constexpr int32 BandCount = 12;
		const float BandH = LocalSize.Y / static_cast<float>(BandCount);
		const FLinearColor TopCol(0.022f, 0.020f, 0.045f, FadeAlpha);
		const FLinearColor MidCol(0.055f, 0.036f, 0.088f, FadeAlpha);
		const FLinearColor BotCol(0.095f, 0.058f, 0.032f, FadeAlpha);

		for (int32 i = 0; i < BandCount; ++i)
		{
			const float T = static_cast<float>(i) / static_cast<float>(BandCount - 1);
			const FLinearColor BandCol = (T < 0.62f)
				? FMath::Lerp(TopCol, MidCol, T / 0.62f)
				: FMath::Lerp(MidCol, BotCol, (T - 0.62f) / 0.38f);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2f(static_cast<float>(LocalSize.X), BandH + 1.5f), FSlateLayoutTransform(FVector2f(0.0f, i * BandH))),
				WhiteBrush,
				ESlateDrawEffect::None,
				BandCol);
		}
		LayerId++;
	}

	int32 MaxLayer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
	                                          LayerId, InWidgetStyle, bParentEnabled);

	if (FadeAlpha <= 0.01f || LocalSize.X < 64.0f || LocalSize.Y < 64.0f)
	{
		return MaxLayer;
	}

	// Subtle bottom dark-amber vignette strip behind title/progress
	const float VignetteH = FMath::Min(280.0f, static_cast<float>(LocalSize.Y) * 0.36f);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		MaxLayer,
		AllottedGeometry.ToPaintGeometry(FVector2f(static_cast<float>(LocalSize.X), VignetteH), FSlateLayoutTransform(FVector2f(0.0f, static_cast<float>(LocalSize.Y) - VignetteH))),
		WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.012f, 0.010f, 0.018f, 0.46f * FadeAlpha));

	// Dual counter-rotating segmented arcane rune rings in lower-right corner
	const FVector2D Center(LocalSize.X - 148.0f, LocalSize.Y - 134.0f);
	const float Pulse = 0.5f + 0.5f * FMath::Sin(static_cast<float>(FPlatformTime::Seconds() - BirthTime) * 3.2f);
	const float Alpha = (0.68f + 0.32f * Pulse) * FadeAlpha;

	auto DrawArc = [&](float Radius, float StartDeg, float SweepDeg, int32 Segments, float Thickness, const FLinearColor& Color)
	{
		TArray<FVector2D> Pts;
		Pts.Reserve(Segments + 1);
		for (int32 s = 0; s <= Segments; ++s)
		{
			const float Deg = StartDeg + (SweepDeg * static_cast<float>(s) / static_cast<float>(Segments));
			const float Rad = FMath::DegreesToRadians(Deg);
			Pts.Add(Center + FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)) * Radius);
		}
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			MaxLayer + 1,
			AllottedGeometry.ToPaintGeometry(),
			Pts,
			ESlateDrawEffect::None,
			Color,
			true,
			Thickness);
	};

	const FLinearColor GoldArc(0.95f, 0.78f, 0.42f, Alpha);
	const FLinearColor VioletArc(0.62f, 0.36f, 1.0f, Alpha * 0.92f);
	const FLinearColor CyanCore(0.28f, 0.90f, 1.0f, Alpha * 0.85f);

	// Outer gilded ring: 3 rotating rune arcs
	for (int32 i = 0; i < 3; ++i)
	{
		DrawArc(54.0f, RingAngle + i * 120.0f, 84.0f, 18, 2.8f, GoldArc);
	}
	// Middle counter-rotating arcane violet ring: 4 arcs
	for (int32 i = 0; i < 4; ++i)
	{
		DrawArc(38.0f, -RingAngle * 1.45f + i * 90.0f, 56.0f, 14, 2.2f, VioletArc);
	}
	// Inner pulsing cyan sigil ring
	DrawArc(20.0f + 2.5f * Pulse, RingAngle * 2.1f, 250.0f, 24, 1.8f, CyanCore);

	// Glowing orbiting keystone diamond on outer ring
	const float SparkRad = FMath::DegreesToRadians(RingAngle);
	const FVector2D SparkPos = Center + FVector2D(FMath::Cos(SparkRad), FMath::Sin(SparkRad)) * 54.0f;
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		MaxLayer + 2,
		AllottedGeometry.ToPaintGeometry(FVector2f(8.0f, 8.0f), FSlateLayoutTransform(FVector2f(static_cast<float>(SparkPos.X) - 4.0f, static_cast<float>(SparkPos.Y) - 4.0f))),
		WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(1.0f, 0.92f, 0.68f, FadeAlpha));

	return MaxLayer + 3;
}

void FAetheriaLoadingScreenController::EnsureInitialized()
{
	if (AetheriaLoadingPrivate::bDelegatesRegistered)
	{
		return;
	}
	AetheriaLoadingPrivate::bDelegatesRegistered = true;

	FCoreUObjectDelegates::PreLoadMap.AddStatic(&FAetheriaLoadingScreenController::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddStatic(&FAetheriaLoadingScreenController::HandlePostLoadMap);
	UE_LOG(LogAetheriaLoading, Display, TEXT("[LoadingScreen] Registered PreLoadMap / PostLoadMapWithWorld delegates"));
}

void FAetheriaLoadingScreenController::BeginWorldTravel(UWorld* CurrentWorld, const FString& WorldTitle, const FString& LoreLine)
{
	EnsureInitialized();
	SAetheriaLoadingScreen::PreloadKeyartTexture();

	FAetheriaLoadingState::Get().ResetForTravel(WorldTitle, LoreLine, 0.14f);
	AetheriaLoadingPrivate::bTravelArmed = true;

	// Put the persistent Slate overlay up BEFORE OpenLevel so the transition has zero black cut frames.
	AttachViewportOverlay(CurrentWorld);

	UE_LOG(LogAetheriaLoading, Display, TEXT("[LoadingScreen] BeginWorldTravel -> %s"), *WorldTitle);
}

void FAetheriaLoadingScreenController::UpdateProgress(float TargetProgress, const FString& StatusLine)
{
	FAetheriaLoadingState::Get().SetProgress(TargetProgress, StatusLine);
}

bool FAetheriaLoadingScreenController::IsOverlayActive()
{
	return AetheriaLoadingPrivate::GViewportWidget.IsValid();
}

void FAetheriaLoadingScreenController::AttachViewportOverlay(UWorld* World)
{
	if (!IsInGameThread() || IsRunningDedicatedServer())
	{
		return;
	}

	UGameViewportClient* Viewport = (World && World->GetGameViewport())
		? World->GetGameViewport()
		: (GEngine ? GEngine->GameViewport.Get() : nullptr);
	if (!Viewport)
	{
		return;
	}

	// If already attached to this exact viewport, keep the live widget seamlessly
	if (AetheriaLoadingPrivate::GViewportWidget.IsValid() && AetheriaLoadingPrivate::GAttachedViewport.Get() == Viewport)
	{
		return;
	}

	DetachViewportOverlay();

	FString Title, Lore;
	{
		const FAetheriaLoadingState& State = FAetheriaLoadingState::Get();
		FScopeLock Lock(&State.Mutex);
		Title = State.WorldTitle;
		Lore = State.LoreLine;
	}

	AetheriaLoadingPrivate::GViewportWidget = SNew(SAetheriaLoadingScreen)
		.WorldTitle(Title)
		.LoreLine(Lore);
	AetheriaLoadingPrivate::GAttachedViewport = Viewport;
	Viewport->AddViewportWidgetContent(AetheriaLoadingPrivate::GViewportWidget.ToSharedRef(), 10000);
}

void FAetheriaLoadingScreenController::DetachViewportOverlay()
{
	if (AetheriaLoadingPrivate::GViewportWidget.IsValid())
	{
		if (UGameViewportClient* Viewport = AetheriaLoadingPrivate::GAttachedViewport.Get())
		{
			Viewport->RemoveViewportWidgetContent(AetheriaLoadingPrivate::GViewportWidget.ToSharedRef());
		}
		else if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(AetheriaLoadingPrivate::GViewportWidget.ToSharedRef());
		}
	}
	AetheriaLoadingPrivate::GViewportWidget.Reset();
	AetheriaLoadingPrivate::GAttachedViewport.Reset();
}

void FAetheriaLoadingScreenController::HandlePreLoadMap(const FString& MapName)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	SAetheriaLoadingScreen::PreloadKeyartTexture();

	// If travel was triggered via direct map load (commandline / OpenLevel), derive title & lore cleanly
	if (!AetheriaLoadingPrivate::bTravelArmed && AetheriaWorldDirector::IsBakedWorldMap(MapName))
	{
		const EAetheriaWorldId WId = AetheriaWorldDirector::WorldIdFromMapName(MapName);
		FAetheriaLoadingState::Get().ResetForTravel(
			AetheriaWorldDirector::WorldDisplayName(WId),
			AetheriaWorldDirector::WorldSubtitle(WId),
			0.18f);
		AetheriaLoadingPrivate::bTravelArmed = true;
	}

	if (!AetheriaLoadingPrivate::bTravelArmed)
	{
		return;
	}

	FAetheriaLoadingState::Get().SetProgress(0.32f, TEXT("Переход через врата миров..."));

	if (IsMoviePlayerEnabled())
	{
		if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
		{
			FString Title, Lore;
			{
				const FAetheriaLoadingState& State = FAetheriaLoadingState::Get();
				FScopeLock Lock(&State.Mutex);
				Title = State.WorldTitle;
				Lore = State.LoreLine;
			}

			FLoadingScreenAttributes LoadingScreen;
			LoadingScreen.bAutoCompleteWhenLoadingCompletes = false;
			LoadingScreen.bWaitForManualStop = false;
			LoadingScreen.bMoviesAreSkippable = false;
			LoadingScreen.MinimumLoadingScreenDisplayTime = 1.2f;
			LoadingScreen.bAllowInEarlyStartup = false;
			LoadingScreen.WidgetLoadingScreen = SNew(SAetheriaLoadingScreen)
				.WorldTitle(Title)
				.LoreLine(Lore);

			MoviePlayer->SetupLoadingScreen(LoadingScreen);
			UE_LOG(LogAetheriaLoading, Display, TEXT("[LoadingScreen] Armed IGameMoviePlayer for map %s"), *MapName);
		}
	}
}

void FAetheriaLoadingScreenController::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (IsRunningDedicatedServer() || !LoadedWorld)
	{
		return;
	}
	if (!AetheriaLoadingPrivate::bTravelArmed)
	{
		return;
	}

	FAetheriaLoadingState::Get().SetProgress(0.68f, TEXT("Потоковая загрузка архитектуры и освещения..."));
	// Hold the persistent Slate overlay over the new viewport until FlushLevelStreaming + ground-snap finish
	AttachViewportOverlay(LoadedWorld);
}

void FAetheriaLoadingScreenController::DismissWhenReady(UWorld* World, bool bImmediate)
{
	AetheriaLoadingPrivate::bTravelArmed = false;

	if (IsMoviePlayerEnabled())
	{
		if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
		{
			if (MoviePlayer->IsLoadingFinished() || MoviePlayer->IsMovieCurrentlyPlaying())
			{
				MoviePlayer->StopMovie();
			}
		}
	}

	FAetheriaLoadingState::Get().MarkComplete();

	if (bImmediate || !World)
	{
		DetachViewportOverlay();
		return;
	}

	if (!AetheriaLoadingPrivate::GViewportWidget.IsValid())
	{
		return;
	}

	FAetheriaLoadingState::Get().StartFadeOut();

	World->GetTimerManager().ClearTimer(AetheriaLoadingPrivate::GDismissTimerHandle);
	World->GetTimerManager().SetTimer(
		AetheriaLoadingPrivate::GDismissTimerHandle,
		FTimerDelegate::CreateStatic(&FAetheriaLoadingScreenController::DetachViewportOverlay),
		0.38f,
		false);
}

#undef LOCTEXT_NAMESPACE
