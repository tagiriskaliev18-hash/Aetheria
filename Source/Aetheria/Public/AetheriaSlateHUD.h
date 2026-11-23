#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "AetheriaTypes.h"
#include "AetheriaSlateHUD.generated.h"

class AAetheriaPlayerController;
class SEditableTextBox;
struct FSlateDynamicImageBrush;
struct FCompositeFont;

class SAetheriaMinimapWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAetheriaMinimapWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AAetheriaPlayerController>, OwningController)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(210.0f, 210.0f); }

private:
	TWeakObjectPtr<AAetheriaPlayerController> PC;
};

class SAetheriaMainOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAetheriaMainOverlay) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AAetheriaPlayerController>, OwningController)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAetheriaMainOverlay();

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	void SetShowLobby(bool bVisible) { bShowLobby = bVisible; }
	bool IsLobbyVisible() const { return bShowLobby; }
	void SetShowMainMenu(bool bVisible);
	void SetShowWorldSelect(bool bVisible);
	void SetLoadingScreen(bool bVisible, float Progress, const FString& Title, const FString& Tip);
	bool IsMainMenuVisible() const { return bShowMainMenu; }


	void ToggleGrimoire();
	void ToggleWorldMap();
	void ToggleSettings();
	void ToggleCharacterEditor();
	bool IsAnyModalOpen() const { return bShowMainMenu || bShowWorldSelect || bShowLoading || bShowLobby || bShowGrimoire || bShowWorldMap || bShowSettings || bShowCharacterEditor; }

private:
	TWeakObjectPtr<AAetheriaPlayerController> PC;

	bool bShowLobby = false;
	bool bShowMainMenu = true;
	bool bShowWorldSelect = false;
	bool bShowLoading = false;
	float LoadingProgress = 0.0f;
	FString LoadingTitle;
	FString LoadingTip;
	int32 SelectedWorldIndex = 0;
	bool bShowGrimoire = false;
	bool bShowWorldMap = false;
	bool bShowSettings = false;
	bool bShowCharacterEditor = false;
	bool bReturnToLobbyAfterEditor = false;
	float EditorTurntableYaw = 0.0f;
	FAetheriaCharacterCustomization WorkingCustomization;
	FString CachedProfileStatusText;
	void RefreshCachedProfileStats();

	int32 CurrentGrimoirePage = 0;
	int32 SelectedStartLocation = 0;
	int32 SelectedBotCount = 2;
	EAetheriaPacingMode SelectedPacing = EAetheriaPacingMode::DemoBlitz;

	TSharedPtr<FSlateDynamicImageBrush> BookSpreadBrush;
	TArray<TSharedPtr<FSlateDynamicImageBrush>> SpellPlateBrushes;

	TSharedPtr<FSlateDynamicImageBrush> PanelObsidianBrush;
	TSharedPtr<FSlateDynamicImageBrush> ActionDockBrush;
	TSharedPtr<FSlateDynamicImageBrush> GlobeBackBrush;
	TSharedPtr<FSlateDynamicImageBrush> GlobeRimBrush;
	TSharedPtr<FSlateDynamicImageBrush> GlobeHPFillBrush;
	TSharedPtr<FSlateDynamicImageBrush> GlobeMPFillBrush;
	TSharedPtr<FSlateDynamicImageBrush> SocketNormalBrush;
	TSharedPtr<FSlateDynamicImageBrush> SocketArmedBrush;
	TSharedPtr<FSlateDynamicImageBrush> BtnCrimsonNormBrush;
	TSharedPtr<FSlateDynamicImageBrush> BtnCrimsonHovBrush;
	TSharedPtr<FSlateDynamicImageBrush> BtnCrimsonPressBrush;
	TSharedPtr<FSlateDynamicImageBrush> BtnStoneNormBrush;
	TSharedPtr<FSlateDynamicImageBrush> BtnStoneHovBrush;
	TSharedPtr<FSlateDynamicImageBrush> BtnStonePressBrush;
	TSharedPtr<FSlateDynamicImageBrush> SplashBannerBrush;
	TSharedPtr<FSlateDynamicImageBrush> IconIgnisBrush;
	TSharedPtr<FSlateDynamicImageBrush> IconTempestBrush;
	TSharedPtr<FSlateDynamicImageBrush> IconGravitonBrush;
	TSharedPtr<FSlateDynamicImageBrush> IconInvokeBrush;
	TSharedPtr<FSlateDynamicImageBrush> IconDashBrush;
	TSharedPtr<FSlateDynamicImageBrush> SkullCrestBrush;
	TSharedPtr<FSlateDynamicImageBrush> MenuVignetteBrush;
	TSharedPtr<FSlateDynamicImageBrush> LoadingArtBrush;
	TSharedPtr<FSlateDynamicImageBrush> MenuGrainBrush;
	TSharedPtr<FSlateDynamicImageBrush> MenuEdgeFadeBrush;
	TArray<TSharedPtr<FSlateDynamicImageBrush>> HeroPortraitBrushes;

	FButtonStyle CrimsonBtnStyle;
	FButtonStyle StoneBtnStyle;
	FButtonStyle SocketBtnStyle;
	FEditableTextBoxStyle DarkInputBoxStyle;
	FSliderStyle BronzeSliderStyle;
	FProgressBarStyle HPGlobeBarStyle;
	FProgressBarStyle MPGlobeBarStyle;
	FProgressBarStyle XPBarStyle;
	FProgressBarStyle BossBarStyle;

	TSharedPtr<const FCompositeFont> DisplayFontData;
	TSharedPtr<const FCompositeFont> SerifFontData;
	int32 SelectedHeroArchetype = 0;

	void InitGrimoireBrushes();
	void InitVolumetricStyles();
	FSlateFontInfo GetDisplayFont(int32 Size) const;
	FSlateFontInfo GetSerifFont(int32 Size) const;
	void SetGrimoirePage(int32 NewPage);
	const FSlateBrush* GetSlotSpellBrush(int32 SlotIdx) const;
	const FSlateBrush* GetSocketFrameBrush(int32 SlotIdx) const;
	FSlateColor GetSocketTint(int32 SlotIdx) const;
	const FSlateBrush* GetCurrentGrimoirePlateBrush() const;
	const FSlateBrush* GetBookSpreadBrush() const;
	const class AAetheriaWizardCharacter* FindPartyWizardBySlot(int32 SlotIdx) const;
	TSharedRef<SWidget> BuildPartyMemberRow(int32 SlotIdx);

	TSharedPtr<SEditableTextBox> PlayerNameInput;
	TSharedPtr<SEditableTextBox> JoinIpInput;

	TSharedRef<SWidget> BuildLobbyPanel();
	TSharedRef<SWidget> BuildInGameHUD();
	TSharedRef<SWidget> BuildMinimapWidget();
	TSharedRef<SWidget> BuildGrimoireModal();
	TSharedRef<SWidget> BuildWorldMapModal();
	TSharedRef<SWidget> BuildSettingsModal();
	TSharedRef<SWidget> BuildCharacterEditorModal();
	TSharedRef<SWidget> BuildCinematicMainMenu();
	TSharedRef<SWidget> BuildCinematicLoadingScreen();

	FText GetLocationHeaderText() const;
	FText GetQuestObjectiveText() const;
	TOptional<float> GetQuestProgressPercent() const;
	EVisibility GetBossBarVisibility() const;
	FText GetBossTitleText() const;
	TOptional<float> GetBossHPPercent() const;
	EVisibility GetPortalBannerVisibility() const;
	FText GetPartyRosterText() const;

	FText GetOrbTriadText() const;
	FText GetNextSpellPreviewText() const;
	FText GetSlotLabelText(int32 SlotIdx) const;
	FText GetVitalsText() const;
	FText GetXPText() const;
	TOptional<float> GetXPPercent() const;
	FText GetSlotCooldownText(int32 SlotIdx) const;
	TOptional<float> GetHPPercent() const;
	TOptional<float> GetManaPercent() const;
	FText GetCastModePillText() const;
};

UCLASS()
class AETHERIA_API AAetheriaHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ShowLobby();
	void ShowMainMenu();
	void SetShowMainMenu(bool bVisible);
	void SetLoadingScreen(bool bVisible, float Progress, const FString& Title, const FString& Tip);
	void HideLobby();
	void ToggleGrimoire();
	void ToggleWorldMap();
	void ToggleSettings();
	void ToggleCharacterEditor();
	bool IsAnyModalOpen() const;

private:
	TSharedPtr<SAetheriaMainOverlay> MainOverlay;
};
