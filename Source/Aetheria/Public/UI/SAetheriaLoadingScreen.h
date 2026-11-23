// Copyright Aetheria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Animation/CurveSequence.h"
#include "HAL/CriticalSection.h"

class STextBlock;
class SProgressBar;
class SImage;
class UTexture2D;
class UWorld;

/** Thread-safe shared loading progress state updated by GameMode / WorldDirector
 *  and read by both the MoviePlayer Slate loading thread and the viewport overlay. */
struct AETHERIA_API FAetheriaLoadingState
{
	mutable FCriticalSection Mutex;
	FString WorldTitle;
	FString LoreLine;
	FString StatusLine;
	float   TargetProgress = 0.0f;
	float   DisplayedProgress = 0.0f;
	bool    bStreamingComplete = false;
	bool    bFadingOut = false;
	float   FadeAlpha = 1.0f;

	static FAetheriaLoadingState& Get()
	{
		static FAetheriaLoadingState Instance;
		return Instance;
	}

	void ResetForTravel(const FString& InTitle, const FString& InLore, float InInitialProgress = 0.12f);
	void SetProgress(float InTargetProgress, const FString& InStatus = TEXT(""));
	void MarkComplete();
	void StartFadeOut();
};

/**
 * Full-screen dark-fantasy loading screen widget.
 * Keyart backdrop (with rich dark-fantasy gradient fallback) + world title & lore
 * + dual counter-rotating arcane rune rings in OnPaint + smooth progress bar.
 */
class AETHERIA_API SAetheriaLoadingScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAetheriaLoadingScreen) {}
		SLATE_ARGUMENT(FString, WorldTitle)
		SLATE_ARGUMENT(FString, LoreLine)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	                      const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	                      int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** Must be called on the GameThread before OpenLevel so keyart is cached and GC-rooted. */
	static UTexture2D* PreloadKeyartTexture();

private:
	TOptional<float> GetProgress() const;
	FText GetTitleText() const;
	FText GetLoreText() const;
	FText GetStatusText() const;
	FLinearColor GetRootTint() const;

	FSlateBrush KeyartBrush;
	bool bHasKeyart = false;
	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> LoreText;
	TSharedPtr<SProgressBar> ProgressBar;

	FCurveSequence PulseSequence;
	FCurveHandle PulseCurve;
	float RingAngle = 0.0f;
	double BirthTime = 0.0;
};

/**
 * Coordinates IGameMoviePlayer (FCoreUObjectDelegates::PreLoadMap / PostLoadMapWithWorld)
 * together with a persistent UGameViewportClient Slate overlay so there is ZERO black cut
 * before OpenLevel, during map travel, during FlushLevelStreaming, and until hero ground-snap.
 */
class AETHERIA_API FAetheriaLoadingScreenController
{
public:
	static void EnsureInitialized();
	static void BeginWorldTravel(UWorld* CurrentWorld, const FString& WorldTitle, const FString& LoreLine);
	static void UpdateProgress(float TargetProgress, const FString& StatusLine = TEXT(""));
	static void DismissWhenReady(UWorld* World, bool bImmediate = false);
	static bool IsOverlayActive();

private:
	static void HandlePreLoadMap(const FString& MapName);
	static void HandlePostLoadMap(UWorld* LoadedWorld);
	static void AttachViewportOverlay(UWorld* World);
	static void DetachViewportOverlay();
};
