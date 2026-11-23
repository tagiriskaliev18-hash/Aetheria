#include "AetheriaSlateHUD.h"
#include "AetheriaPlayerController.h"
#include "AetheriaGameMode.h"
#include "AetheriaWorldDirector.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "AetheriaWizardCharacter.h"
#include "AetheriaMobCharacter.h"
#include "AetheriaGameState.h"
#include "AetheriaSpellDatabase.h"
#include "AetheriaBiomeDatabase.h"
#include "AetheriaMaterialLibrary.h"
#include "AetheriaSaveGame.h"
#include "Rendering/DrawElements.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "EngineUtils.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Fonts/CompositeFont.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Notifications/SProgressBar.h"

namespace
{
	const FLinearColor MindBgGlass(0.038f, 0.036f, 0.048f, 0.95f);
	const FLinearColor MindVoidPanel(0.020f, 0.019f, 0.026f, 0.97f);
	const FLinearColor MindButtonBg(0.11f, 0.10f, 0.14f, 0.98f);
	const FLinearColor MindAccentBtnBg(0.46f, 0.11f, 0.09f, 0.98f);
	const FLinearColor BronzeFrameColor(0.76f, 0.61f, 0.35f, 0.92f);
	const FLinearColor DimBronzeBorder(0.42f, 0.32f, 0.18f, 0.72f);
	const FLinearColor GildedBronzeText(0.90f, 0.76f, 0.49f, 1.0f);
	const FLinearColor BoneParchmentText(0.92f, 0.88f, 0.80f, 1.0f);
	const FLinearColor AshMutedText(0.66f, 0.60f, 0.52f, 0.95f);
	const FLinearColor EmberCrimsonText(0.96f, 0.36f, 0.24f, 1.0f);
	const FLinearColor ParchmentInkDark(0.118f, 0.086f, 0.063f, 1.0f);
	const FLinearColor ParchmentInkMuted(0.28f, 0.21f, 0.15f, 1.0f);
	const FLinearColor ParchmentRuleColor(0.46f, 0.35f, 0.22f, 0.55f);
	const FLinearColor WaxSealCrimson(0.55f, 0.10f, 0.09f, 1.0f);

	FString GetCleanSpellTitle(const FAetheriaSpellDef& S)
	{
		switch (S.SpellId)
		{
		case EAetheriaSpellId::Fireball:         return TEXT("Шар Первородного Огня");
		case EAetheriaSpellId::BlackHole:        return TEXT("Разверстая Пасть Бездны");
		case EAetheriaSpellId::Chronosphere:     return TEXT("Купол Вечного Безвременья");
		case EAetheriaSpellId::ChaosMeteor:      return TEXT("Звезда Погибели");
		case EAetheriaSpellId::ChainLightning:   return TEXT("Цепная Молния Бури");
		case EAetheriaSpellId::SupernovaBeam:    return TEXT("Копьё Испепеляющего Света");
		case EAetheriaSpellId::ChronoBlink:      return TEXT("Астральный Шаг и Морок");
		case EAetheriaSpellId::GravityCataclysm: return TEXT("Разлом Земной Тверди");
		case EAetheriaSpellId::PrismWall:        return TEXT("Стена Морозных Граней");
		case EAetheriaSpellId::PrimalOverload:   return TEXT("Гнев Трёх Первоначал");
		default:                                 return S.DisplayName;
		}
	}

	FString GetSpellSchoolSubtitle(EAetheriaSpellId Id)
	{
		switch (Id)
		{
		case EAetheriaSpellId::Fireball:         return TEXT("Чары Первородного Пламени");
		case EAetheriaSpellId::BlackHole:        return TEXT("Печать Глубокой Пустоты");
		case EAetheriaSpellId::Chronosphere:     return TEXT("Завет Застывшего Времени");
		case EAetheriaSpellId::ChaosMeteor:      return TEXT("Обряд Небесного Огня");
		case EAetheriaSpellId::ChainLightning:   return TEXT("Заклятие Грозового Эфира");
		case EAetheriaSpellId::SupernovaBeam:    return TEXT("Чары Небесного Горнила");
		case EAetheriaSpellId::ChronoBlink:      return TEXT("Тайный Обряд Перехода");
		case EAetheriaSpellId::GravityCataclysm: return TEXT("Проклятие Каменных Недр");
		case EAetheriaSpellId::PrismWall:        return TEXT("Заслон Ледяной Пустоты");
		case EAetheriaSpellId::PrimalOverload:   return TEXT("Высший Обряд Триады");
		default:                                 return TEXT("Древнее Заклинание");
		}
	}

	void LoadUIBrush(TSharedPtr<FSlateDynamicImageBrush>& OutBrush, const TCHAR* AssetName, const TCHAR* RelPath,
		FVector2D ImageSize, ESlateBrushDrawType::Type DrawType = ESlateBrushDrawType::Image, FMargin Margin = FMargin(0.0f))
	{
		UTexture2D* Tex = nullptr;
		const FString FullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelPath);
		if (IFileManager::Get().FileExists(*FullPath))
		{
			Tex = FImageUtils::ImportFileAsTexture2D(FullPath);
			if (Tex)
			{
				Tex->LODGroup = TEXTUREGROUP_UI;
				Tex->NeverStream = true;
			}
		}
		if (!Tex)
		{
			Tex = UAetheriaMaterialLibrary::LoadOrImportTexture(AssetName, RelPath, false);
		}
		if (Tex)
		{
			Tex->AddToRoot();
			OutBrush = MakeShareable(new FSlateDynamicImageBrush(Tex, ImageSize, FName(AssetName)));
			OutBrush->DrawAs = DrawType;
			OutBrush->Margin = Margin;
		}
	}

	TWeakPtr<const FCompositeFont> GDisplayFontPtr;
	TWeakPtr<const FCompositeFont> GSerifFontPtr;

	FSlateFontInfo MindFont(int32 Size, bool bBold = false, bool bShadowOutline = true)
	{
		TSharedPtr<const FCompositeFont> FontData = (bBold || Size >= 15) ? GDisplayFontPtr.Pin() : GSerifFontPtr.Pin();
		if (!FontData.IsValid())
		{
			FontData = GSerifFontPtr.Pin();
		}
		if (FontData.IsValid())
		{
			const FFontOutlineSettings Outline = bShadowOutline
				? FFontOutlineSettings(1, FLinearColor(0.01f, 0.01f, 0.015f, 0.88f))
				: FFontOutlineSettings();
			return FSlateFontInfo(FontData, static_cast<float>(Size), NAME_None, Outline);
		}
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}
}

SAetheriaMainOverlay::~SAetheriaMainOverlay() = default;

void SAetheriaMainOverlay::Construct(const FArguments& InArgs)
{
	PC = InArgs._OwningController;
	InitVolumetricStyles();
	InitGrimoireBrushes();
	RefreshCachedProfileStats();

	// Auto-hide lobby when joining a ?listen server map directly
	if (PC.IsValid() && PC->GetWorld() && PC->GetWorld()->GetNetMode() != NM_Standalone)
	{
		bShowLobby = false;
	}

	ChildSlot
	[
		SNew(SOverlay)

		// 1. In-Game Diegetic HUD
		+ SOverlay::Slot()
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return (bShowLobby || bShowCharacterEditor || bShowMainMenu || bShowLoading) ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; })
			[
				BuildInGameHUD()
			]
		]

		// 2. Full-Screen Symmetrical Diablo IV Sanctuary Main Menu (Crown + Left Hero Altar + Center Archmage + Right Expedition Pact + Bottom Entry Pedestal)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return bShowLobby ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SOverlay)

				// Top Full-Width Sanctuary Heraldry Crown Strip
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.015f, 0.014f, 0.022f, 0.88f))
					.Padding(42.0f, 14.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
							[
								SNew(SBox).WidthOverride(36.0f).HeightOverride(36.0f)
								[
									SNew(SImage).Image_Lambda([this]() { return SkullCrestBrush.IsValid() ? SkullCrestBrush.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"); })
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("AETHERIA  ·  ОСКОЛКИ ХРОНО-ПЕЧАТИ")))
								.Font(MindFont(18, true))
								.ColorAndOpacity(GildedBronzeText)
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f) [ SNew(SSpacer) ]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("ОРДЕН ХРАНИТЕЛЕЙ  ·  СОВМЕСТНЫЙ ПОХОД 1–6 МАГОВ")))
								.Font(MindFont(10, true))
								.ColorAndOpacity(AshMutedText)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
						[
							SNew(SBox).HeightOverride(1.5f)
							[
								SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
								.BorderBackgroundColor(DimBronzeBorder)
							]
						]
					]
				]

				// Left Sanctuary Pillar: Hero Altar & Customization
				+ SOverlay::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(40.0f, 70.0f, 0.0f, 64.0f)
				[
					BuildLobbyPanel()
				]

				// Right Sanctuary Pillar: Expedition Pact & Arcane Controls Legend
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 70.0f, 40.0f, 64.0f)
				[
					SNew(SBorder)
					.BorderImage(PanelObsidianBrush.IsValid() ? PanelObsidianBrush.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(PanelObsidianBrush.IsValid() ? FLinearColor::White : MindVoidPanel)
					.Padding(22.0f, 20.0f)
					[
						SNew(SBox)
						.WidthOverride(368.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("ЗАВЕТ ЭКСПЕДИЦИИ")))
								.Font(MindFont(14, true))
								.ColorAndOpacity(GildedBronzeText)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("ВЫБОР МИРА И СПОСОБА ЧАР")))
								.Font(MindFont(9, true))
								.ColorAndOpacity(AshMutedText)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
							[
								SNew(SBox).HeightOverride(1.5f)
								[
									SNew(SBorder)
									.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
									.BorderBackgroundColor(BronzeFrameColor)
								]
							]
							// Start Location Selector
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
							[
								SNew(SButton)
								.ButtonStyle(&StoneBtnStyle)
								.OnClicked_Lambda([this]() {
									SelectedStartLocation = (SelectedStartLocation + 1) % 10;
									return FReply::Handled();
								})
								[
									SNew(STextBlock)
									.Justification(ETextJustify::Center)
									.Text_Lambda([this]() {
										const FAetheriaLocationDef Loc = UAetheriaBiomeDatabase::GetLocation(SelectedStartLocation);
										return FText::FromString(FString::Printf(TEXT("Мир: %s"), *Loc.DisplayName));
									})
									.Font(MindFont(10, true))
									.ColorAndOpacity(BoneParchmentText)
								]
							]
							// Cast Mode Row
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
								[
									SNew(SButton)
									.ButtonStyle(&StoneBtnStyle)
									.ButtonColorAndOpacity_Lambda([this]() {
										return (PC.IsValid() && PC->PreferredCastMode == EAetheriaCastMode::InvokeThenAim)
											? FLinearColor(1.0f, 0.78f, 0.42f, 1.0f)
											: FLinearColor(0.80f, 0.80f, 0.84f, 1.0f);
									})
									.OnClicked_Lambda([this]() {
										if (PC.IsValid()) PC->UI_SetCastMode(EAetheriaCastMode::InvokeThenAim);
										return FReply::Handled();
									})
									[
										SNew(STextBlock)
										.Justification(ETextJustify::Center)
										.Text(FText::FromString(TEXT("Печать D/F -> ЛКМ")))
										.Font(MindFont(9, true))
										.ColorAndOpacity_Lambda([this]() {
											return (PC.IsValid() && PC->PreferredCastMode == EAetheriaCastMode::InvokeThenAim)
												? FSlateColor(GildedBronzeText) : FSlateColor(BoneParchmentText);
										})
									]
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f)
								[
									SNew(SButton)
									.ButtonStyle(&StoneBtnStyle)
									.ButtonColorAndOpacity_Lambda([this]() {
										return (PC.IsValid() && PC->PreferredCastMode == EAetheriaCastMode::InstantOnCombo)
											? FLinearColor(1.0f, 0.78f, 0.42f, 1.0f)
											: FLinearColor(0.80f, 0.80f, 0.84f, 1.0f);
									})
									.OnClicked_Lambda([this]() {
										if (PC.IsValid()) PC->UI_SetCastMode(EAetheriaCastMode::InstantOnCombo);
										return FReply::Handled();
									})
									[
										SNew(STextBlock)
										.Justification(ETextJustify::Center)
										.Text(FText::FromString(TEXT("Мгновенные чары")))
										.Font(MindFont(9, true))
										.ColorAndOpacity_Lambda([this]() {
											return (PC.IsValid() && PC->PreferredCastMode == EAetheriaCastMode::InstantOnCombo)
												? FSlateColor(GildedBronzeText) : FSlateColor(BoneParchmentText);
										})
									]
								]
							]
							// Pacing & Allied Mages Count
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
								[
									SNew(SButton)
									.ButtonStyle(&StoneBtnStyle)
									.ContentPadding(FMargin(4.0f, 3.0f))
									.ButtonColorAndOpacity_Lambda([this]() {
										return SelectedPacing == EAetheriaPacingMode::Epic45Min
											? FLinearColor(1.35f, 1.02f, 0.38f, 1.0f)
											: FLinearColor(0.38f, 0.38f, 0.42f, 0.85f);
									})
									.OnClicked_Lambda([this]() { SelectedPacing = EAetheriaPacingMode::Epic45Min; return FReply::Handled(); })
									[
										SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Эпос (45м)"))).Font(MindFont(10, true))
										.ColorAndOpacity_Lambda([this]() {
											return SelectedPacing == EAetheriaPacingMode::Epic45Min
												? FSlateColor(FLinearColor(1.0f, 0.95f, 0.72f, 1.0f))
												: FSlateColor(FLinearColor(0.60f, 0.58f, 0.54f, 0.75f));
										})
									]
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
								[
									SNew(SButton)
									.ButtonStyle(&StoneBtnStyle)
									.ContentPadding(FMargin(4.0f, 3.0f))
									.ButtonColorAndOpacity_Lambda([this]() {
										return SelectedPacing == EAetheriaPacingMode::DemoBlitz
											? FLinearColor(1.35f, 1.02f, 0.38f, 1.0f)
											: FLinearColor(0.38f, 0.38f, 0.42f, 0.85f);
									})
									.OnClicked_Lambda([this]() { SelectedPacing = EAetheriaPacingMode::DemoBlitz; return FReply::Handled(); })
									[
										SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Блиц (6м)"))).Font(MindFont(10, true))
										.ColorAndOpacity_Lambda([this]() {
											return SelectedPacing == EAetheriaPacingMode::DemoBlitz
												? FSlateColor(FLinearColor(1.0f, 0.95f, 0.72f, 1.0f))
												: FSlateColor(FLinearColor(0.60f, 0.58f, 0.54f, 0.75f));
										})
									]
								]
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SButton)
									.ButtonStyle(&StoneBtnStyle)
									.ContentPadding(FMargin(6.0f, 3.0f))
									.OnClicked_Lambda([this]() {
										SelectedBotCount = (SelectedBotCount + 1) % 6;
										return FReply::Handled();
									})
									[
										SNew(STextBlock)
										.Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("Спутники: %d"), SelectedBotCount)); })
										.Font(MindFont(10, true))
										.ColorAndOpacity(GildedBronzeText)
									]
								]
							]
							// Join by IP
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 6, 0)
								[
									SAssignNew(JoinIpInput, SEditableTextBox)
									.Style(&DarkInputBoxStyle)
									.Text(FText::FromString(TEXT("127.0.0.1:7777")))
									.Font(MindFont(10, false, false))
								]
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(SButton)
									.ButtonStyle(&StoneBtnStyle)
									.OnClicked_Lambda([this]() {
										if (PC.IsValid())
										{
											const FString Ip = JoinIpInput.IsValid() ? JoinIpInput->GetText().ToString() : TEXT("127.0.0.1:7777");
											const FString Name = PlayerNameInput.IsValid() ? PlayerNameInput->GetText().ToString() : TEXT("Маг");
											PC->UI_JoinByAddress(Ip, Name);
										}
										return FReply::Handled();
									})
									[
										SNew(STextBlock).Text(FText::FromString(TEXT("К ОТРЯДУ"))).Font(MindFont(9, true)).ColorAndOpacity(BoneParchmentText)
									]
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
							[
								SNew(SBox).HeightOverride(1.0f)
								[
									SNew(SBorder)
									.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
									.BorderBackgroundColor(DimBronzeBorder)
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("КОДЕКС УПРАВЛЕНИЯ")))
								.Font(MindFont(11, true))
								.ColorAndOpacity(GildedBronzeText)
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("[ПКМ] Движение  ·  [ЛКМ] Атака / Чары\n[Q / W / E] Сферы  ·  [R] Сотворение\n[D / F] Слоты заклятий  ·  [Space] Рывок\n[Tab] Гримуар  ·  [M] Миры  ·  [C] Облик")))
								.Font(MindFont(10))
								.ColorAndOpacity(BoneParchmentText)
								.AutoWrapText(true)
							]
						]
					]
				]

				// Bottom-Center Grand Altar Entry Pedestal right under Hero
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Bottom)
				.Padding(0.0f, 0.0f, 0.0f, 26.0f)
				[
					SNew(SBorder)
					.BorderImage(PanelObsidianBrush.IsValid() ? PanelObsidianBrush.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(PanelObsidianBrush.IsValid() ? FLinearColor::White : MindVoidPanel)
					.Padding(18.0f, 12.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
						[
							SNew(SBox).WidthOverride(250.0f).HeightOverride(46.0f)
							[
								SNew(SButton)
								.ButtonStyle(&CrimsonBtnStyle)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.OnClicked_Lambda([this]() {
									if (PC.IsValid())
									{
										const FString Name = PlayerNameInput.IsValid() ? PlayerNameInput->GetText().ToString() : TEXT("Архимаг");
										PC->UI_StartGame(false, Name, SelectedStartLocation, SelectedPacing, SelectedBotCount);
									}
									return FReply::Handled();
								})
								[
									SNew(STextBlock)
									.Justification(ETextJustify::Center)
									.Text(FText::FromString(TEXT("ВОЙТИ В СВЯТИЛИЩЕ")))
									.Font(MindFont(13, true))
									.ColorAndOpacity(FLinearColor(0.98f, 0.90f, 0.72f, 1.0f))
								]
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
						[
							SNew(SBox).WidthOverride(245.0f).HeightOverride(46.0f)
							[
								SNew(SButton)
								.ButtonStyle(&StoneBtnStyle)
								.ButtonColorAndOpacity(FLinearColor(1.0f, 0.84f, 0.52f, 1.0f))
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.OnClicked_Lambda([this]() {
									if (PC.IsValid())
									{
										const FString Name = PlayerNameInput.IsValid() ? PlayerNameInput->GetText().ToString() : TEXT("Архимаг");
										PC->UI_StartGame(true, Name, SelectedStartLocation, SelectedPacing, SelectedBotCount);
									}
									return FReply::Handled();
								})
								[
									SNew(STextBlock)
									.Justification(ETextJustify::Center)
									.Text(FText::FromString(TEXT("СОВМЕСТНЫЙ ПОХОД")))
									.Font(MindFont(11, true))
									.ColorAndOpacity(GildedBronzeText)
								]
							]
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(110.0f).HeightOverride(46.0f)
							[
								SNew(SButton)
								.ButtonStyle(&StoneBtnStyle)
								.ButtonColorAndOpacity(FLinearColor(0.95f, 0.55f, 0.55f, 1.0f))
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.OnClicked_Lambda([this]() {
									if (PC.IsValid()) PC->UI_QuitToDesktop();
									return FReply::Handled();
								})
								[
									SNew(STextBlock)
									.Justification(ETextJustify::Center)
									.Text(FText::FromString(TEXT("ВЫХОД")))
									.Font(MindFont(10, true))
									.ColorAndOpacity(EmberCrimsonText)
								]
							]
						]
					]
				]
			]
		]

		// 3. Grimoire Modal (Tab)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return (!bShowLobby && !bShowCharacterEditor && bShowGrimoire) ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				BuildGrimoireModal()
			]
		]

		// 4. World Map Modal (M)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return (!bShowLobby && !bShowCharacterEditor && bShowWorldMap) ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				BuildWorldMapModal()
			]
		]

		// 5. Settings Modal (Esc)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return (!bShowLobby && !bShowCharacterEditor && bShowSettings) ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				BuildSettingsModal()
			]
		]

		// 6. Character Editor Modal (C)
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(28.0f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return bShowCharacterEditor ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				BuildCharacterEditorModal()
			]
		]

		// 7. Dark-fantasy cinematic main menu (3D cathedral shows through)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return (bShowMainMenu && !bShowLoading) ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				BuildCinematicMainMenu()
			]
		]

		// 8. Dark-fantasy loading screen (art backdrop + ornate bar + lore tip)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return bShowLoading ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				BuildCinematicLoadingScreen()
			]
		]
		];
}


TSharedRef<SWidget> SAetheriaMainOverlay::BuildCinematicMainMenu()
{
	auto TitleFont = [](int32 Size, float OutlinePx, const FLinearColor& OutlineCol) -> FSlateFontInfo
	{
		TSharedPtr<const FCompositeFont> FontData = GDisplayFontPtr.Pin();
		if (!FontData.IsValid())
		{
			FontData = GSerifFontPtr.Pin();
		}
		if (FontData.IsValid())
		{
			return FSlateFontInfo(FontData, static_cast<float>(Size), NAME_None, FFontOutlineSettings(OutlinePx, OutlineCol));
		}
		return FCoreStyle::GetDefaultFontStyle("Bold", Size);
	};

	auto PlayHover = [this]()
	{
		if (PC.IsValid())
		{
			UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_UI_Hover"), 0.45f);
		}
	};
	auto PlayClick = [this]()
	{
		if (PC.IsValid())
		{
			UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_UIClick"), 0.65f);
		}
	};

	auto MakePlate = [this, PlayHover, PlayClick, TitleFont](const FText& Label, bool bPrimary, TFunction<FReply()> OnClick) -> TSharedRef<SWidget>
	{
		const FButtonStyle* Style = bPrimary ? &CrimsonBtnStyle : &StoneBtnStyle;
		const FLinearColor LabelCol = bPrimary ? BoneParchmentText : GildedBronzeText;
		return SNew(SBox)
			.WidthOverride(440.0f)
			.HeightOverride(88.0f)
			.Padding(FMargin(0.0f, 8.0f))
			[
				SNew(SButton)
				.ButtonStyle(Style)
				.OnHovered_Lambda([PlayHover]() { PlayHover(); })
				.OnClicked_Lambda([PlayClick, OnClick]() { PlayClick(); return OnClick(); })
				.ContentPadding(FMargin(22.0f, 16.0f))
				[
					SNew(STextBlock)
					.Text(Label)
					.Font(TitleFont(18, 2.0f, FLinearColor(0.06f, 0.03f, 0.01f, 0.9f)))
					.ColorAndOpacity(LabelCol)
					.Justification(ETextJustify::Center)
				]
			];
	};

	const FLinearColor EmberGlow(1.0f, 0.42f, 0.10f, 0.40f);
	const FLinearColor IvoryGold(0.93f, 0.82f, 0.55f, 1.0f);
	const FLinearColor EngravedOutline(0.06f, 0.03f, 0.01f, 0.96f);
	const FLinearColor CyanHair(0.35f, 0.78f, 0.92f, 0.35f);

	return SNew(SOverlay)

		// Vignette
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image_Lambda([this]() -> const FSlateBrush*
			{
				return MenuVignetteBrush.IsValid() ? MenuVignetteBrush.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
			})
			.ColorAndOpacity_Lambda([this]()
			{
				return MenuVignetteBrush.IsValid() ? FLinearColor::White : FLinearColor(0, 0, 0, 0.45f);
			})
			.Visibility(EVisibility::HitTestInvisible)
		]

		// Film grain
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image_Lambda([this]() -> const FSlateBrush*
			{
				return MenuGrainBrush.IsValid() ? MenuGrainBrush.Get() : FCoreStyle::Get().GetBrush("NoBrush");
			})
			.ColorAndOpacity(FLinearColor(1, 1, 1, 0.22f))
			.Visibility(EVisibility::HitTestInvisible)
		]

		// Left readability fade (not a flat panel)
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Fill)
		[
			SNew(SBox)
			.WidthOverride(620.0f)
			[
				SNew(SImage)
				.Image_Lambda([this]() -> const FSlateBrush*
				{
					return MenuEdgeFadeBrush.IsValid() ? MenuEdgeFadeBrush.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
				})
				.ColorAndOpacity_Lambda([this]()
				{
					return MenuEdgeFadeBrush.IsValid() ? FLinearColor::White : FLinearColor(0.02f, 0.015f, 0.02f, 0.0f);
				})
				.Visibility(EVisibility::HitTestInvisible)
			]
		]

		// Title + plates, left
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(72.0f, 0.0f, 0.0f, 24.0f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
				[
					SNew(SBox).WidthOverride(42.0f).HeightOverride(42.0f)
					[
						SNew(SImage)
						.Image_Lambda([this]()
						{
							return SkullCrestBrush.IsValid() ? static_cast<const FSlateBrush*>(SkullCrestBrush.Get()) : FCoreStyle::Get().GetBrush("WhiteBrush");
						})
						.ColorAndOpacity(GildedBronzeText)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
				[
					SNew(SBox).HeightOverride(2.0f)
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(0.72f, 0.55f, 0.28f, 0.65f))
						[ SNew(SSpacer) ]
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SOverlay)
				+ SOverlay::Slot().Padding(2, 3, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("AETHERIA")))
					.Font(TitleFont(78, 8.0f, EmberGlow))
					.ColorAndOpacity(EmberGlow)
				]
				+ SOverlay::Slot()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("AETHERIA")))
					.Font(TitleFont(76, 3.5f, EngravedOutline))
					.ColorAndOpacity(IvoryGold)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(220.0f).HeightOverride(2.0f)
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(0.90f, 0.72f, 0.38f, 0.85f))
						[ SNew(SSpacer) ]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
				[
					SNew(SBox).WidthOverride(72.0f).HeightOverride(1.0f)
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(CyanHair)
						[ SNew(SSpacer) ]
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 28)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("ОСКОЛКИ ХРОНО-ПЕЧАТИ")))
				.Font(MindFont(16, false, true))
				.ColorAndOpacity(FLinearColor(0.72f, 0.62f, 0.42f, 0.92f))
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				MakePlate(FText::FromString(TEXT("ИГРАТЬ")), true, [this]()
				{
					if (PC.IsValid())
					{
						if (AAetheriaGameMode* GM = PC->GetWorld() ? PC->GetWorld()->GetAuthGameMode<AAetheriaGameMode>() : nullptr)
						{
							GM->StartWorld(static_cast<EAetheriaWorldId>(SelectedWorldIndex), true);
						}
					}
					return FReply::Handled();
				})
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakePlate(FText::FromString(TEXT("ВЫБОР МИРА")), false, [this]()
				{
					bShowWorldSelect = !bShowWorldSelect;
					return FReply::Handled();
				})
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakePlate(FText::FromString(TEXT("НАСТРОЙКИ")), false, [this]()
				{
					bShowSettings = true;
					return FReply::Handled();
				})
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakePlate(FText::FromString(TEXT("ВЫХОД")), false, [this]()
				{
					if (PC.IsValid())
					{
						UKismetSystemLibrary::QuitGame(PC->GetWorld(), PC.Get(), EQuitPreference::Quit, false);
					}
					return FReply::Handled();
				})
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)
			[
				SNew(SBox)
				.Visibility_Lambda([this]() { return bShowWorldSelect ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakePlate(FText::FromString(TEXT("Бастион Аотера")), false, [this]() { SelectedWorldIndex = 0; return FReply::Handled(); })
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakePlate(FText::FromString(TEXT("Руины Монолита")), false, [this]() { SelectedWorldIndex = 1; return FReply::Handled(); })
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakePlate(FText::FromString(TEXT("Пустотный Предел")), false, [this]() { SelectedWorldIndex = 2; return FReply::Handled(); })
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakePlate(FText::FromString(TEXT("Святилище Архонта")), false, [this]() { SelectedWorldIndex = 3; return FReply::Handled(); })
					]
				]
			]
		]

		// Footer
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(76.0f, 0.0f, 0.0f, 36.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Врата закрыты. Жаровни горят. Орден ждёт.")))
			.Font(MindFont(12, false, true))
			.ColorAndOpacity(AshMutedText)
		];
}

TSharedRef<SWidget> SAetheriaMainOverlay::BuildCinematicLoadingScreen()
{
	auto TitleFont = [](int32 Size) -> FSlateFontInfo
	{
		TSharedPtr<const FCompositeFont> FontData = GDisplayFontPtr.Pin();
		if (!FontData.IsValid()) { FontData = GSerifFontPtr.Pin(); }
		if (FontData.IsValid())
		{
			return FSlateFontInfo(FontData, static_cast<float>(Size), NAME_None,
				FFontOutlineSettings(3.0f, FLinearColor(0.06f, 0.03f, 0.01f, 0.95f)));
		}
		return FCoreStyle::GetDefaultFontStyle("Bold", Size);
	};

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image_Lambda([this]() -> const FSlateBrush*
			{
				return LoadingArtBrush.IsValid() ? LoadingArtBrush.Get() : FCoreStyle::Get().GetBrush("NoBrush");
			})
			.ColorAndOpacity_Lambda([this]()
			{
				// Slow ember pulse keeps the plate alive without stealing focus.
				const float Pulse = 0.86f + 0.08f * FMath::Sin(FPlatformTime::Seconds() * 0.9f);
				return LoadingArtBrush.IsValid() ? FLinearColor(Pulse, Pulse, Pulse, 1.0f) : FLinearColor::White;
			})
			.Visibility(EVisibility::HitTestInvisible)
		]
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image_Lambda([this]() -> const FSlateBrush*
			{
				return MenuVignetteBrush.IsValid() ? MenuVignetteBrush.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
			})
			.ColorAndOpacity_Lambda([this]()
			{
				return MenuVignetteBrush.IsValid() ? FLinearColor(1, 1, 1, 1) : FLinearColor(0.01f, 0.01f, 0.02f, 0.55f);
			})
			.Visibility(EVisibility::HitTestInvisible)
		]
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.01f, 0.01f, 0.015f, 0.42f))
			.Padding(0)
			[ SNew(SSpacer) ]
			.Visibility(EVisibility::HitTestInvisible)
		]
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image_Lambda([this]() -> const FSlateBrush*
			{
				return MenuGrainBrush.IsValid() ? MenuGrainBrush.Get() : FCoreStyle::Get().GetBrush("NoBrush");
			})
			.ColorAndOpacity(FLinearColor(1, 1, 1, 0.20f))
			.Visibility(EVisibility::HitTestInvisible)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 110.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 18)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return FText::FromString(LoadingTitle.IsEmpty() ? TEXT("ЗАГРУЗКА") : LoadingTitle); })
				.Font(TitleFont(42))
				.ColorAndOpacity(FLinearColor(0.93f, 0.82f, 0.55f, 1.0f))
				.Justification(ETextJustify::Center)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 16)
			[
				SNew(SBox)
				.WidthOverride(640.0f)
				.HeightOverride(28.0f)
				[
					SNew(SBorder)
					.BorderImage(BtnStoneNormBrush.IsValid() ? static_cast<const FSlateBrush*>(BtnStoneNormBrush.Get()) : FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor::White)
					.Padding(FMargin(10.0f, 7.0f))
					[
						SNew(SProgressBar)
						.Style(&XPBarStyle)
						.Percent_Lambda([this]() { return LoadingProgress; })
						.FillColorAndOpacity(FLinearColor(0.86f, 0.58f, 0.22f, 1.0f))
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(720.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return FText::FromString(LoadingTip); })
					.Font(MindFont(15, false, true))
					.ColorAndOpacity(FLinearColor(0.70f, 0.62f, 0.48f, 0.92f))
					.Justification(ETextJustify::Center)
					.AutoWrapText(true)
				]
			]
		];
}

void SAetheriaMainOverlay::InitVolumetricStyles()
{
	FString DisplayFontPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("RawAssets/Fonts/AetheriaDisplay-Bold.ttf"));
	if (!IFileManager::Get().FileExists(*DisplayFontPath))
	{
		DisplayFontPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Fonts/AetheriaDisplay-Bold.ttf"));
	}
	if (IFileManager::Get().FileExists(*DisplayFontPath))
	{
		DisplayFontData = MakeShared<FStandaloneCompositeFont>(
			FName("AetheriaDisplayBold"), DisplayFontPath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		GDisplayFontPtr = DisplayFontData;
	}

	FString SerifFontPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("RawAssets/Fonts/AetheriaSerif-SemiBold.ttf"));
	if (!IFileManager::Get().FileExists(*SerifFontPath))
	{
		SerifFontPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Fonts/AetheriaSerif-SemiBold.ttf"));
	}
	if (IFileManager::Get().FileExists(*SerifFontPath))
	{
		SerifFontData = MakeShared<FStandaloneCompositeFont>(
			FName("AetheriaSerifSemiBold"), SerifFontPath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		GSerifFontPtr = SerifFontData;
	}

	LoadUIBrush(PanelObsidianBrush,   TEXT("T_UI_PanelObsidian"),      TEXT("RawAssets/UI/T_UI_PanelObsidian.png"),      FVector2D(512, 512), ESlateBrushDrawType::Box, FMargin(0.11f));
	LoadUIBrush(ActionDockBrush,      TEXT("T_UI_ActionDockPedestal"), TEXT("RawAssets/UI/T_UI_ActionDockPedestal.png"), FVector2D(1160, 268), ESlateBrushDrawType::Image);
	LoadUIBrush(GlobeBackBrush,       TEXT("T_UI_GlobeBack"),          TEXT("RawAssets/UI/T_UI_GlobeBack.png"),          FVector2D(256, 256));
	LoadUIBrush(GlobeRimBrush,        TEXT("T_UI_GlobeGlassRim"),      TEXT("RawAssets/UI/T_UI_GlobeGlassRim.png"),      FVector2D(256, 256));
	LoadUIBrush(GlobeHPFillBrush,     TEXT("T_UI_GlobeFill_HP"),       TEXT("RawAssets/UI/T_UI_GlobeFill_HP.png"),       FVector2D(256, 256));
	LoadUIBrush(GlobeMPFillBrush,     TEXT("T_UI_GlobeFill_MP"),       TEXT("RawAssets/UI/T_UI_GlobeFill_MP.png"),       FVector2D(256, 256));
	LoadUIBrush(SocketNormalBrush,    TEXT("T_UI_SkillSocket_Normal"), TEXT("RawAssets/UI/T_UI_SkillSocket_Normal.png"), FVector2D(128, 128));
	LoadUIBrush(SocketArmedBrush,     TEXT("T_UI_SkillSocket_Armed"),  TEXT("RawAssets/UI/T_UI_SkillSocket_Armed.png"),  FVector2D(128, 128));
	LoadUIBrush(BtnCrimsonNormBrush,  TEXT("T_UI_BtnCrimson_Normal"),  TEXT("RawAssets/UI/T_UI_BtnCrimson_Normal.png"),  FVector2D(384, 88), ESlateBrushDrawType::Box, FMargin(0.14f, 0.24f));
	LoadUIBrush(BtnCrimsonHovBrush,   TEXT("T_UI_BtnCrimson_Hovered"), TEXT("RawAssets/UI/T_UI_BtnCrimson_Hovered.png"), FVector2D(384, 88), ESlateBrushDrawType::Box, FMargin(0.14f, 0.24f));
	LoadUIBrush(BtnCrimsonPressBrush, TEXT("T_UI_BtnCrimson_Pressed"), TEXT("RawAssets/UI/T_UI_BtnCrimson_Pressed.png"), FVector2D(384, 88), ESlateBrushDrawType::Box, FMargin(0.14f, 0.24f));
	LoadUIBrush(BtnStoneNormBrush,    TEXT("T_UI_BtnStone_Normal"),    TEXT("RawAssets/UI/T_UI_BtnStone_Normal.png"),    FVector2D(384, 88), ESlateBrushDrawType::Box, FMargin(0.14f, 0.24f));
	LoadUIBrush(BtnStoneHovBrush,     TEXT("T_UI_BtnStone_Hovered"),   TEXT("RawAssets/UI/T_UI_BtnStone_Hovered.png"),   FVector2D(384, 88), ESlateBrushDrawType::Box, FMargin(0.14f, 0.24f));
	LoadUIBrush(BtnStonePressBrush,   TEXT("T_UI_BtnStone_Pressed"),   TEXT("RawAssets/UI/T_UI_BtnStone_Pressed.png"),   FVector2D(384, 88), ESlateBrushDrawType::Box, FMargin(0.14f, 0.24f));
	LoadUIBrush(SplashBannerBrush,    TEXT("Splash_Aetheria_Diablo_Master"), TEXT("Design/Splash_Aetheria_Diablo_Master.png"), FVector2D(430, 126));
	LoadUIBrush(MenuVignetteBrush,    TEXT("T_UI_MenuVignette"), TEXT("RawAssets/UI/T_UI_MenuVignette.png"), FVector2D(1024, 1024));
	// Cinematic key art (gpt-image-2.6, RawAssets/Cinematic) behind the loading screen.
	LoadUIBrush(LoadingArtBrush,      TEXT("T_Cine_MenuKeyArt"),  TEXT("RawAssets/Cinematic/menu_keyart.png"),  FVector2D(1536, 1024));
	LoadUIBrush(MenuGrainBrush,       TEXT("T_UI_FilmGrain"),    TEXT("RawAssets/UI/T_UI_FilmGrain.png"),    FVector2D(256, 256), ESlateBrushDrawType::Image);
	LoadUIBrush(MenuEdgeFadeBrush,    TEXT("T_UI_MenuEdgeFade"), TEXT("RawAssets/UI/T_UI_MenuEdgeFade.png"), FVector2D(768, 8));
	if (MenuGrainBrush.IsValid())
	{
		MenuGrainBrush->Tiling = ESlateBrushTileType::Both;
	}

	LoadUIBrush(IconIgnisBrush,       TEXT("T_UI_Icon_Ignis"),         TEXT("RawAssets/UI/Icons/T_UI_Icon_Ignis.png"),    FVector2D(128, 128));
	LoadUIBrush(IconTempestBrush,     TEXT("T_UI_Icon_Tempest"),       TEXT("RawAssets/UI/Icons/T_UI_Icon_Tempest.png"),  FVector2D(128, 128));
	LoadUIBrush(IconGravitonBrush,    TEXT("T_UI_Icon_Graviton"),      TEXT("RawAssets/UI/Icons/T_UI_Icon_Graviton.png"), FVector2D(128, 128));
	LoadUIBrush(IconInvokeBrush,      TEXT("T_UI_Icon_Invoke"),        TEXT("RawAssets/UI/Icons/T_UI_Icon_Invoke.png"),   FVector2D(128, 128));
	LoadUIBrush(IconDashBrush,        TEXT("T_UI_Icon_Dash"),          TEXT("RawAssets/UI/Icons/T_UI_Icon_Dash.png"),     FVector2D(128, 128));
	LoadUIBrush(SkullCrestBrush,      TEXT("T_UI_Crest_Skull"),        TEXT("RawAssets/UI/Icons/T_UI_Crest_Skull.png"),   FVector2D(64, 64));

	HeroPortraitBrushes.SetNum(3);
	LoadUIBrush(HeroPortraitBrushes[0], TEXT("T_UI_Portrait_Hero0"), TEXT("RawAssets/UI/Icons/T_UI_Portrait_Hero0.png"), FVector2D(64, 64));
	LoadUIBrush(HeroPortraitBrushes[1], TEXT("T_UI_Portrait_Hero1"), TEXT("RawAssets/UI/Icons/T_UI_Portrait_Hero1.png"), FVector2D(64, 64));
	LoadUIBrush(HeroPortraitBrushes[2], TEXT("T_UI_Portrait_Hero2"), TEXT("RawAssets/UI/Icons/T_UI_Portrait_Hero2.png"), FVector2D(64, 64));

	CrimsonBtnStyle = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
	if (BtnCrimsonNormBrush.IsValid())  CrimsonBtnStyle.SetNormal(*BtnCrimsonNormBrush);
	if (BtnCrimsonHovBrush.IsValid())   CrimsonBtnStyle.SetHovered(*BtnCrimsonHovBrush);
	if (BtnCrimsonPressBrush.IsValid()) CrimsonBtnStyle.SetPressed(*BtnCrimsonPressBrush);
	CrimsonBtnStyle.SetNormalForeground(FSlateColor(FLinearColor(0.98f, 0.90f, 0.74f, 1.0f)));
	CrimsonBtnStyle.SetHoveredForeground(FSlateColor(FLinearColor(1.0f, 0.96f, 0.86f, 1.0f)));
	CrimsonBtnStyle.SetPressedForeground(FSlateColor(GildedBronzeText));
	CrimsonBtnStyle.SetNormalPadding(FMargin(14.0f, 8.0f));
	CrimsonBtnStyle.SetPressedPadding(FMargin(14.0f, 9.5f, 14.0f, 6.5f));

	StoneBtnStyle = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
	if (BtnStoneNormBrush.IsValid())  StoneBtnStyle.SetNormal(*BtnStoneNormBrush);
	if (BtnStoneHovBrush.IsValid())   StoneBtnStyle.SetHovered(*BtnStoneHovBrush);
	if (BtnStonePressBrush.IsValid()) StoneBtnStyle.SetPressed(*BtnStonePressBrush);
	StoneBtnStyle.SetNormalForeground(FSlateColor(BoneParchmentText));
	StoneBtnStyle.SetHoveredForeground(FSlateColor(GildedBronzeText));
	StoneBtnStyle.SetPressedForeground(FSlateColor(BronzeFrameColor));
	StoneBtnStyle.SetNormalPadding(FMargin(10.0f, 6.0f));
	StoneBtnStyle.SetPressedPadding(FMargin(10.0f, 7.5f, 10.0f, 4.5f));

	const FSlateBrush* WhiteB = FCoreStyle::Get().GetBrush("WhiteBrush");

	DarkInputBoxStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	if (BtnStoneNormBrush.IsValid() && BtnStoneHovBrush.IsValid())
	{
		DarkInputBoxStyle
			.SetBackgroundImageNormal(*BtnStoneNormBrush)
			.SetBackgroundImageHovered(*BtnStoneHovBrush)
			.SetBackgroundImageFocused(*BtnStoneHovBrush)
			.SetBackgroundImageReadOnly(*BtnStoneNormBrush)
			.SetBackgroundColor(FSlateColor(FLinearColor::White));
	}
	else
	{
		FSlateBrush DarkBox = *WhiteB;
		DarkBox.TintColor = FSlateColor(FLinearColor(0.025f, 0.024f, 0.032f, 0.98f));
		FSlateBrush FocusBox = *WhiteB;
		FocusBox.TintColor = FSlateColor(FLinearColor(0.055f, 0.048f, 0.040f, 0.98f));
		DarkInputBoxStyle
			.SetBackgroundImageNormal(DarkBox)
			.SetBackgroundImageHovered(FocusBox)
			.SetBackgroundImageFocused(FocusBox)
			.SetBackgroundImageReadOnly(DarkBox)
			.SetBackgroundColor(FSlateColor(FLinearColor::White));
	}
	DarkInputBoxStyle
		.SetForegroundColor(FSlateColor(BoneParchmentText))
		.SetFont(MindFont(11, false, false))
		.SetPadding(FMargin(10.0f, 6.0f));

	FSlateBrush SliderBarNorm = *WhiteB;
	SliderBarNorm.TintColor = FSlateColor(FLinearColor(0.015f, 0.012f, 0.020f, 0.98f));
	FSlateBrush SliderBarHov = *WhiteB;
	SliderBarHov.TintColor = FSlateColor(FLinearColor(0.14f, 0.11f, 0.07f, 0.98f));
	FSlateBrush SliderThumbNorm = *WhiteB;
	SliderThumbNorm.TintColor = FSlateColor(FLinearColor(0.92f, 0.74f, 0.35f, 1.0f));
	SliderThumbNorm.ImageSize = FVector2D(12.0f, 20.0f);
	FSlateBrush SliderThumbHov = *WhiteB;
	SliderThumbHov.TintColor = FSlateColor(FLinearColor(1.15f, 0.95f, 0.55f, 1.0f));
	SliderThumbHov.ImageSize = FVector2D(14.0f, 22.0f);
	BronzeSliderStyle = FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider");
	BronzeSliderStyle
		.SetNormalBarImage(SliderBarNorm)
		.SetHoveredBarImage(SliderBarHov)
		.SetDisabledBarImage(SliderBarNorm)
		.SetNormalThumbImage(SliderThumbNorm)
		.SetHoveredThumbImage(SliderThumbHov)
		.SetDisabledThumbImage(SliderBarNorm)
		.SetBarThickness(8.0f);

	FSlateBrush TransparentBrush;
	TransparentBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	SocketBtnStyle = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
	SocketBtnStyle.SetNormal(TransparentBrush).SetHovered(TransparentBrush).SetPressed(TransparentBrush).SetDisabled(TransparentBrush);
	SocketBtnStyle.SetNormalPadding(FMargin(0.0f));
	SocketBtnStyle.SetPressedPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));

	HPGlobeBarStyle = FCoreStyle::Get().GetWidgetStyle<FProgressBarStyle>("ProgressBar");
	HPGlobeBarStyle.SetBackgroundImage(TransparentBrush).SetEnableFillAnimation(false);
	if (GlobeHPFillBrush.IsValid()) HPGlobeBarStyle.SetFillImage(*GlobeHPFillBrush);

	MPGlobeBarStyle = FCoreStyle::Get().GetWidgetStyle<FProgressBarStyle>("ProgressBar");
	MPGlobeBarStyle.SetBackgroundImage(TransparentBrush).SetEnableFillAnimation(false);
	if (GlobeMPFillBrush.IsValid()) MPGlobeBarStyle.SetFillImage(*GlobeMPFillBrush);

	FSlateBrush DarkTrack = *WhiteB;
	DarkTrack.TintColor = FSlateColor(FLinearColor(0.015f, 0.016f, 0.022f, 0.96f));
	FSlateBrush GoldFill = *WhiteB;
	GoldFill.TintColor = FSlateColor(FLinearColor(0.86f, 0.68f, 0.25f, 1.0f));
	FSlateBrush CrimsonFill = *WhiteB;
	CrimsonFill.TintColor = FSlateColor(FLinearColor(0.80f, 0.14f, 0.10f, 1.0f));

	XPBarStyle = FCoreStyle::Get().GetWidgetStyle<FProgressBarStyle>("ProgressBar");
	XPBarStyle.SetBackgroundImage(DarkTrack).SetFillImage(GoldFill).SetEnableFillAnimation(false);

	BossBarStyle = FCoreStyle::Get().GetWidgetStyle<FProgressBarStyle>("ProgressBar");
	BossBarStyle.SetBackgroundImage(DarkTrack).SetFillImage(CrimsonFill).SetEnableFillAnimation(false);
}

FSlateFontInfo SAetheriaMainOverlay::GetDisplayFont(int32 Size) const
{
	return MindFont(Size, true, true);
}

FSlateFontInfo SAetheriaMainOverlay::GetSerifFont(int32 Size) const
{
	return MindFont(Size, false, true);
}

void SAetheriaMainOverlay::InitGrimoireBrushes()
{
	LoadUIBrush(BookSpreadBrush, TEXT("T_GrimoireBookSpread"), TEXT("RawAssets/Grimoire/T_GrimoireBookSpread.png"), FVector2D(1440.0f, 920.0f));

	SpellPlateBrushes.Reset();
	SpellPlateBrushes.SetNum(10);
	for (int32 i = 0; i < 10; ++i)
	{
		const FString AssetName = FString::Printf(TEXT("SpellPlate_%02d"), i + 1);
		const FString RelPath = FString::Printf(TEXT("RawAssets/Grimoire/SpellPlate_%02d.png"), i + 1);
		LoadUIBrush(SpellPlateBrushes[i], *AssetName, *RelPath, FVector2D(512.0f, 512.0f));
	}
}

void SAetheriaMainOverlay::ToggleGrimoire()
{
	if (bShowCharacterEditor)
	{
		bShowCharacterEditor = false;
		EditorTurntableYaw = 0.0f;
		if (PC.IsValid())
		{
			PC->UI_SetEditorPreviewCamera(false, 0.0f);
		}
		if (bReturnToLobbyAfterEditor)
		{
			bShowLobby = true;
			bReturnToLobbyAfterEditor = false;
		}
	}

	bShowGrimoire = !bShowGrimoire;
	bShowWorldMap = false;
	bShowSettings = false;

	if (PC.IsValid())
	{
		UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_PageTurn"), 0.60f);
	}
	if (bShowGrimoire && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(AsShared());
	}
}

void SAetheriaMainOverlay::ToggleWorldMap()
{
	if (bShowCharacterEditor)
	{
		bShowCharacterEditor = false;
		EditorTurntableYaw = 0.0f;
		if (PC.IsValid())
		{
			PC->UI_SetEditorPreviewCamera(false, 0.0f);
		}
		if (bReturnToLobbyAfterEditor)
		{
			bShowLobby = true;
			bReturnToLobbyAfterEditor = false;
		}
	}

	bShowWorldMap = !bShowWorldMap;
	bShowGrimoire = false;
	bShowSettings = false;
}

void SAetheriaMainOverlay::ToggleSettings()
{
	if (bShowCharacterEditor)
	{
		bShowCharacterEditor = false;
		EditorTurntableYaw = 0.0f;
		if (PC.IsValid())
		{
			PC->UI_SetEditorPreviewCamera(false, 0.0f);
			UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_UIClick"), 0.60f);
		}
		if (bReturnToLobbyAfterEditor)
		{
			bShowLobby = true;
			bReturnToLobbyAfterEditor = false;
		}
		return;
	}

	bShowSettings = !bShowSettings;
	bShowGrimoire = false;
	bShowWorldMap = false;
}

void SAetheriaMainOverlay::ToggleCharacterEditor()
{
	if (!bShowCharacterEditor)
	{
		bShowCharacterEditor = true;
		bShowGrimoire = false;
		bShowWorldMap = false;
		bShowSettings = false;

		if (bShowLobby)
		{
			bReturnToLobbyAfterEditor = true;
			bShowLobby = false;
		}

		if (PC.IsValid())
		{
			if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
			{
				WorkingCustomization = Wiz->Customization;
			}
			else if (UAetheriaSaveGame* Save = UAetheriaSaveGame::LoadOrCreateSave())
			{
				WorkingCustomization = Save->SavedCustomization;
			}
			EditorTurntableYaw = 0.0f;
			PC->UI_SetEditorPreviewCamera(true, EditorTurntableYaw);
			UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_UIClick"), 0.65f);
		}

		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetKeyboardFocus(AsShared());
		}
	}
	else
	{
		bShowCharacterEditor = false;
		EditorTurntableYaw = 0.0f;
		if (PC.IsValid())
		{
			PC->UI_SetEditorPreviewCamera(false, 0.0f);
			UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_UIClick"), 0.65f);
		}

		if (bReturnToLobbyAfterEditor)
		{
			RefreshCachedProfileStats();
			bShowLobby = true;
			bReturnToLobbyAfterEditor = false;
		}
	}
}

void SAetheriaMainOverlay::RefreshCachedProfileStats()
{
	UAetheriaSaveGame* Save = UAetheriaSaveGame::LoadOrCreateSave();
	const int32 Lvl = Save ? Save->WizardLevel : 1;
	const int32 Ess = Save ? Save->ArcaneEssence : 0;
	const int32 Unlocked = Save ? FMath::Clamp(Save->UnlockedHighestLocation + 1, 1, 10) : 1;
	CachedProfileStatusText = FString::Printf(TEXT("Уровень: %d  ·  Арканная Эссенция: %d  ·  Локации: %d/10"), Lvl, Ess, Unlocked);
}

void SAetheriaMainOverlay::SetGrimoirePage(int32 NewPage)
{
	const int32 Wrapped = ((NewPage % 10) + 10) % 10;
	if (Wrapped != CurrentGrimoirePage)
	{
		CurrentGrimoirePage = Wrapped;
		if (PC.IsValid())
		{
			UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_PageTurn"), 0.65f);
		}
	}
}

FReply SAetheriaMainOverlay::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (bShowCharacterEditor)
	{
		if (Key == EKeys::Escape || Key == EKeys::C)
		{
			ToggleCharacterEditor();
			return FReply::Handled();
		}
		if (Key == EKeys::Left)
		{
			EditorTurntableYaw -= 35.0f;
			if (EditorTurntableYaw < -180.0f) EditorTurntableYaw += 360.0f;
			if (PC.IsValid())
			{
				PC->UI_SetEditorPreviewCamera(true, EditorTurntableYaw);
				UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_UIClick"), 0.5f);
			}
			return FReply::Handled();
		}
		if (Key == EKeys::Right)
		{
			EditorTurntableYaw += 35.0f;
			if (EditorTurntableYaw > 180.0f) EditorTurntableYaw -= 360.0f;
			if (PC.IsValid())
			{
				PC->UI_SetEditorPreviewCamera(true, EditorTurntableYaw);
				UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_UIClick"), 0.5f);
			}
			return FReply::Handled();
		}
	}

	if (!bShowLobby && bShowGrimoire)
	{
		if (Key == EKeys::Left || Key == EKeys::PageUp)
		{
			SetGrimoirePage(CurrentGrimoirePage - 1);
			return FReply::Handled();
		}
		if (Key == EKeys::Right || Key == EKeys::PageDown)
		{
			SetGrimoirePage(CurrentGrimoirePage + 1);
			return FReply::Handled();
		}
		if (Key == EKeys::Tab || Key == EKeys::Escape)
		{
			bShowGrimoire = false;
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

FReply SAetheriaMainOverlay::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bShowLobby && bShowGrimoire)
	{
		const float Delta = MouseEvent.GetWheelDelta();
		if (Delta > 0.0f)
		{
			SetGrimoirePage(CurrentGrimoirePage - 1);
			return FReply::Handled();
		}
		if (Delta < 0.0f)
		{
			SetGrimoirePage(CurrentGrimoirePage + 1);
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

const FSlateBrush* SAetheriaMainOverlay::GetBookSpreadBrush() const
{
	if (BookSpreadBrush.IsValid())
	{
		return BookSpreadBrush.Get();
	}
	return FCoreStyle::Get().GetBrush("WhiteBrush");
}

const FSlateBrush* SAetheriaMainOverlay::GetCurrentGrimoirePlateBrush() const
{
	const int32 Idx = FMath::Clamp(CurrentGrimoirePage, 0, 9);
	if (SpellPlateBrushes.IsValidIndex(Idx) && SpellPlateBrushes[Idx].IsValid())
	{
		return SpellPlateBrushes[Idx].Get();
	}
	return FCoreStyle::Get().GetBrush("WhiteBrush");
}

const FSlateBrush* SAetheriaMainOverlay::GetSlotSpellBrush(int32 SlotIdx) const
{
	if (PC.IsValid())
	{
		if (const AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
		{
			const EAetheriaSpellId SpellId = (SlotIdx == 0) ? Wiz->SlotD : Wiz->SlotF;
			if (SpellId != EAetheriaSpellId::None)
			{
				const TArray<FAetheriaSpellDef>& AllSpells = UAetheriaSpellDatabase::GetAllSpells();
				for (int32 i = 0; i < AllSpells.Num(); ++i)
				{
					if (AllSpells[i].SpellId == SpellId && SpellPlateBrushes.IsValidIndex(i) && SpellPlateBrushes[i].IsValid())
					{
						return SpellPlateBrushes[i].Get();
					}
				}
			}
		}
	}
	return FCoreStyle::Get().GetBrush("WhiteBrush");
}

TSharedRef<SWidget> SAetheriaMainOverlay::BuildLobbyPanel()
{
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FSlateBrush* PanelBrush = PanelObsidianBrush.IsValid() ? PanelObsidianBrush.Get() : WhiteBrush;

	TSharedRef<SHorizontalBox> ArchetypeRow = SNew(SHorizontalBox);
	static const TCHAR* ArchNumerals[6] = { TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V"), TEXT("VI") };
	for (int32 ArchIdx = 0; ArchIdx < 6; ++ArchIdx)
	{
		ArchetypeRow->AddSlot().FillWidth(1.0f).Padding(ArchIdx == 0 ? 0.0f : 3.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.ButtonStyle(&StoneBtnStyle)
			.ButtonColorAndOpacity_Lambda([this, ArchIdx]() {
				return (SelectedHeroArchetype == ArchIdx)
					? FLinearColor(1.0f, 0.78f, 0.42f, 1.0f)
					: FLinearColor(0.85f, 0.85f, 0.88f, 1.0f);
			})
			.OnClicked_Lambda([this, ArchIdx]() {
				SelectedHeroArchetype = ArchIdx;
				if (PC.IsValid()) PC->UI_SetHeroArchetype(ArchIdx);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Text(FText::FromString(ArchNumerals[ArchIdx]))
				.Font(MindFont(11, true))
				.ColorAndOpacity_Lambda([this, ArchIdx]() {
					return (SelectedHeroArchetype == ArchIdx)
						? FSlateColor(FLinearColor(1.0f, 0.88f, 0.55f, 1.0f))
						: FSlateColor(AetheriaMindColors::CrispAlabaster);
				})
			]
		];
	}

	return SNew(SBorder)
		.BorderImage(PanelBrush)
		.BorderBackgroundColor(PanelObsidianBrush.IsValid() ? FLinearColor::White : MindVoidPanel)
		.Padding(22.0f, 20.0f)
		[
			SNew(SBox)
			.WidthOverride(420.0f)
			[
				SNew(SVerticalBox)

				// Left Pillar Header: Hero Altar
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("ЛИК АРХИМАГА")))
					.Font(MindFont(14, true))
					.ColorAndOpacity(GildedBronzeText)
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("ВЫБОР ХРАНИТЕЛЯ И ОБЛИКА")))
					.Font(MindFont(9, true))
					.ColorAndOpacity(AshMutedText)
				]

				// Engraved bronze divider line
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
				[
					SNew(SBox).HeightOverride(1.5f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush)
						.BorderBackgroundColor(BronzeFrameColor)
					]
				]

				// Profile Status Strip
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush)
					.BorderBackgroundColor(DimBronzeBorder)
					.Padding(1.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush)
						.BorderBackgroundColor(FLinearColor(0.015f, 0.015f, 0.020f, 0.94f))
						.Padding(10.0f, 6.0f)
						[
							SNew(STextBlock)
							.Text_Lambda([this]() { return FText::FromString(CachedProfileStatusText); })
							.Font(MindFont(10, true))
							.ColorAndOpacity(GildedBronzeText)
						]
					]
				]

				// Hero Archetype Selector + Live Turntable & Editor
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("АРХЕТИП ХРАНИТЕЛЯ")))
					.Font(MindFont(9, true))
					.ColorAndOpacity(AshMutedText)
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[
					ArchetypeRow
				]

				// Row 1: Turntable Rotation Controls
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
					[
						SNew(SButton)
						.ButtonStyle(&StoneBtnStyle)
						.HAlign(HAlign_Center)
						.OnClicked_Lambda([this]() {
							EditorTurntableYaw -= 35.0f;
							if (PC.IsValid()) PC->UI_SetEditorPreviewCamera(false, EditorTurntableYaw);
							return FReply::Handled();
						})
						[ SNew(STextBlock).Text(FText::FromString(TEXT("< Повернуть"))).Font(MindFont(9, true)).ColorAndOpacity(BoneParchmentText) ]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SButton)
						.ButtonStyle(&StoneBtnStyle)
						.HAlign(HAlign_Center)
						.OnClicked_Lambda([this]() {
							EditorTurntableYaw += 35.0f;
							if (PC.IsValid()) PC->UI_SetEditorPreviewCamera(false, EditorTurntableYaw);
							return FReply::Handled();
						})
						[ SNew(STextBlock).Text(FText::FromString(TEXT("Повернуть >"))).Font(MindFont(9, true)).ColorAndOpacity(BoneParchmentText) ]
					]
				]

				// Row 2: Full-width Forge Appearance Button
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
				[
					SNew(SButton)
					.ButtonStyle(&StoneBtnStyle)
					.HAlign(HAlign_Center)
					.OnClicked_Lambda([this]() {
						bReturnToLobbyAfterEditor = true;
						ToggleCharacterEditor();
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Text(FText::FromString(TEXT("КУЗНИЦА ОБЛИКА [C]")))
						.Font(MindFont(10, true))
						.ColorAndOpacity(GildedBronzeText)
					]
				]

				// Wizard Name
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 10)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Имя мага:"))).Font(MindFont(11, true)).ColorAndOpacity(AshMutedText)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SAssignNew(PlayerNameInput, SEditableTextBox)
						.Style(&DarkInputBoxStyle)
						.Text(FText::FromString(TEXT("Архимаг")))
						.Font(MindFont(11, false, false))
					]
				]

				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Архимаг вращает взором за указателем и ждёт приказа у Алтаря Стихий.")))
					.Font(MindFont(9))
					.ColorAndOpacity(AshMutedText)
					.AutoWrapText(true)
				]
			]
		];
}

const AAetheriaWizardCharacter* SAetheriaMainOverlay::FindPartyWizardBySlot(int32 SlotIdx) const
{
	if (!PC.IsValid() || !PC->GetWorld()) return nullptr;
	int32 Index = 0;
	for (TActorIterator<AAetheriaWizardCharacter> It(PC->GetWorld()); It; ++It)
	{
		if (const AAetheriaWizardCharacter* Wiz = *It)
		{
			if (Index == SlotIdx) return Wiz;
			++Index;
		}
	}
	return nullptr;
}

TSharedRef<SWidget> SAetheriaMainOverlay::BuildPartyMemberRow(int32 SlotIdx)
{
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FSlateBrush* PanelBrush = PanelObsidianBrush.IsValid() ? PanelObsidianBrush.Get() : WhiteBrush;

	return SNew(SBorder)
		.Visibility_Lambda([this, SlotIdx]() { return FindPartyWizardBySlot(SlotIdx) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		.BorderImage(PanelBrush)
		.BorderBackgroundColor(PanelObsidianBrush.IsValid() ? FLinearColor::White : MindBgGlass)
		.Padding(8.0f, 6.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush)
				.BorderBackgroundColor(BronzeFrameColor)
				.Padding(1.5f)
				[
					SNew(SBox).WidthOverride(36.0f).HeightOverride(36.0f)
					[
						SNew(SImage)
						.Image_Lambda([this, SlotIdx]() -> const FSlateBrush* {
							const AAetheriaWizardCharacter* W = FindPartyWizardBySlot(SlotIdx);
							const int32 PIdx = W ? (FMath::Abs(W->PartyColorIndex) % 3) : (SlotIdx % 3);
							if (HeroPortraitBrushes.IsValidIndex(PIdx) && HeroPortraitBrushes[PIdx].IsValid())
							{
								return HeroPortraitBrushes[PIdx].Get();
							}
							return FCoreStyle::Get().GetBrush("WhiteBrush");
						})
					]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 3)
				[
					SNew(STextBlock)
					.Text_Lambda([this, SlotIdx]() {
						if (const AAetheriaWizardCharacter* W = FindPartyWizardBySlot(SlotIdx))
						{
							FString OrbsStr;
							for (EAetheriaOrb O : W->ActiveOrbs)
							{
								OrbsStr += AetheriaMindColors::GetOrbLetter(O);
							}
							return FText::FromString(FString::Printf(TEXT("%s  [%s]"), *W->WizardName, *OrbsStr));
						}
						return FText::GetEmpty();
					})
					.Font(MindFont(10, true))
					.ColorAndOpacity(GildedBronzeText)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).WidthOverride(150.0f).HeightOverride(6.0f)
					[
						SNew(SProgressBar)
						.Style(&BossBarStyle)
						.Percent_Lambda([this, SlotIdx]() {
							if (const AAetheriaWizardCharacter* W = FindPartyWizardBySlot(SlotIdx))
							{
								return W->MaxHP > 0.0f ? FMath::Clamp(W->HP / W->MaxHP, 0.0f, 1.0f) : 0.0f;
							}
							return 0.0f;
						})
					]
				]
			]
		];
}

void SAetheriaMinimapWidget::Construct(const FArguments& InArgs)
{
	PC = InArgs._OwningController;
}

int32 SAetheriaMinimapWidget::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
	const FVector2D Center(WidgetSize.X * 0.5f, WidgetSize.Y * 0.5f);
	const float MapRadius = FMath::Min(WidgetSize.X, WidgetSize.Y) * 0.46f;

	// 1. Background dark obsidian panel
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2D::ZeroVector, WidgetSize),
		WhiteBrush,
		ESlateDrawEffect::None,
		FLinearColor(0.014f, 0.012f, 0.020f, 0.94f)
	);
	LayerId++;

	// 2. Arena perimeter contour (36 segments)
	TArray<FVector2D> CirclePts;
	const int32 NumSegs = 36;
	for (int32 i = 0; i <= NumSegs; ++i)
	{
		const float Ang = (static_cast<float>(i) / static_cast<float>(NumSegs)) * TWO_PI;
		CirclePts.Add(Center + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * MapRadius);
	}
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		CirclePts,
		ESlateDrawEffect::None,
		FLinearColor(0.74f, 0.60f, 0.35f, 0.90f),
		false,
		2.0f
	);

	// Inner range ring (0.5 Radius)
	TArray<FVector2D> InnerPts;
	for (int32 i = 0; i <= NumSegs; ++i)
	{
		const float Ang = (static_cast<float>(i) / static_cast<float>(NumSegs)) * TWO_PI;
		InnerPts.Add(Center + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * (MapRadius * 0.5f));
	}
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		InnerPts,
		ESlateDrawEffect::None,
		FLinearColor(0.35f, 0.40f, 0.55f, 0.28f),
		false,
		1.0f
	);

	// Crosshairs
	TArray<FVector2D> HorizLine = { Center - FVector2D(MapRadius, 0.0f), Center + FVector2D(MapRadius, 0.0f) };
	TArray<FVector2D> VertLine = { Center - FVector2D(0.0f, MapRadius), Center + FVector2D(0.0f, MapRadius) };
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), HorizLine, ESlateDrawEffect::None, FLinearColor(0.35f, 0.40f, 0.55f, 0.20f), false, 1.0f);
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), VertLine, ESlateDrawEffect::None, FLinearColor(0.35f, 0.40f, 0.55f, 0.20f), false, 1.0f);
	LayerId++;

	UWorld* World = PC.IsValid() ? PC->GetWorld() : nullptr;
	if (!World)
	{
		return LayerId;
	}

	const AAetheriaGameState* GS = World->GetGameState<AAetheriaGameState>();
	const int32 LocIdx = GS ? GS->CurrentLocationIndex : 0;
	const FAetheriaLocationDef LocDef = UAetheriaBiomeDatabase::GetLocation(LocIdx);
	const float ArenaRadius = (LocDef.ArenaRadiusUU > 500.0f) ? LocDef.ArenaRadiusUU : 4200.0f;

	auto WorldToMap = [&](const FVector& WorldLoc) -> FVector2D
	{
		const float NormX = FMath::Clamp(WorldLoc.X / ArenaRadius, -1.0f, 1.0f);
		const float NormY = FMath::Clamp(-WorldLoc.Y / ArenaRadius, -1.0f, 1.0f);
		return Center + FVector2D(NormX, NormY) * (MapRadius * 0.94f);
	};

	// 3. Relief Zones (Elevated Bastions / Sunken Trenches)
	for (const FAetheriaTerrainZone& Zone : LocDef.TerrainZones)
	{
		if (Zone.Kind != EAetheriaTerrainZoneKind::ElevatedPlateau)
		{
			continue;
		}
		const FVector2D ZonePt = WorldToMap(FVector(Zone.Center.X, Zone.Center.Y, 0.0f));
		const float ZoneR = FMath::Clamp((Zone.RadiusUU / ArenaRadius) * MapRadius, 4.0f, 26.0f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(ZonePt - FVector2D(ZoneR, ZoneR), FVector2D(ZoneR * 2.0f, ZoneR * 2.0f)),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.18f, 0.72f, 0.95f, 0.16f)
		);
	}
	for (const FAetheriaTerrainZone& Zone : LocDef.TerrainZones)
	{
		if (Zone.Kind != EAetheriaTerrainZoneKind::SunkenTrench)
		{
			continue;
		}
		const FVector2D ZonePt = WorldToMap(FVector(Zone.Center.X, Zone.Center.Y, 0.0f));
		const float ZoneR = FMath::Clamp((Zone.RadiusUU / ArenaRadius) * MapRadius, 4.0f, 26.0f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(ZonePt - FVector2D(ZoneR, ZoneR), FVector2D(ZoneR * 2.0f, ZoneR * 2.0f)),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.72f, 0.22f, 0.95f, 0.16f)
		);
	}
	LayerId++;

	// 4. Objectives & Beacons
	if (GS)
	{
		for (const FAetheriaObjectiveInfo& Obj : GS->Objectives)
		{
			const FVector2D BeaconPos = WorldToMap(Obj.Location);
			const FLinearColor BeaconColor = Obj.bCompleted
				? FLinearColor(0.20f, 0.95f, 0.40f, 0.95f)
				: (Obj.Progress > 0.0f ? FLinearColor(1.0f, 0.85f, 0.18f, 0.95f) : FLinearColor(0.20f, 0.85f, 1.0f, 0.95f));
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(BeaconPos - FVector2D(5.0f, 5.0f), FVector2D(10.0f, 10.0f)),
				WhiteBrush,
				ESlateDrawEffect::None,
				BeaconColor
			);
		}
	}
	LayerId++;

	// 5. Enemies (Red dots / Boss icon)
	for (TActorIterator<AAetheriaMobCharacter> MobIt(World); MobIt; ++MobIt)
	{
		const AAetheriaMobCharacter* Mob = *MobIt;
		if (!Mob || Mob->HP <= 0.0f) continue;

		const FVector2D MobPos = WorldToMap(Mob->GetActorLocation());
		if (Mob->MobKind == EAetheriaMobKind::Boss)
		{
			// Boss: large crimson skull marker
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(MobPos - FVector2D(7.0f, 7.0f), FVector2D(14.0f, 14.0f)),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(1.0f, 0.15f, 0.12f, 1.0f)
			);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(MobPos - FVector2D(3.0f, 3.0f), FVector2D(6.0f, 6.0f)),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(1.0f, 0.95f, 0.50f, 1.0f)
			);
		}
		else if (Mob->MobKind == EAetheriaMobKind::Bruiser)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(MobPos - FVector2D(4.0f, 4.0f), FVector2D(8.0f, 8.0f)),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(1.0f, 0.38f, 0.15f, 0.95f)
			);
		}
		else
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(MobPos - FVector2D(2.5f, 2.5f), FVector2D(5.0f, 5.0f)),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.92f, 0.20f, 0.20f, 0.90f)
			);
		}
	}
	LayerId += 2;

	// 6. Wizards / Allies (Living allies, Fallen golden crosses, Local player)
	const AActor* LocalPawn = PC.IsValid() ? PC->GetPawn() : nullptr;
	for (TActorIterator<AAetheriaWizardCharacter> WizIt(World); WizIt; ++WizIt)
	{
		const AAetheriaWizardCharacter* Wiz = *WizIt;
		if (!Wiz) continue;

		const FVector2D WizPos = WorldToMap(Wiz->GetActorLocation());
		if (!Wiz->bIsAlive)
		{
			// Fallen ally: Golden cross marker!
			const float CrossHalf = 6.0f;
			TArray<FVector2D> CrossH = { WizPos - FVector2D(CrossHalf, 0.0f), WizPos + FVector2D(CrossHalf, 0.0f) };
			TArray<FVector2D> CrossV = { WizPos - FVector2D(0.0f, CrossHalf), WizPos + FVector2D(0.0f, CrossHalf) };
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), CrossH, ESlateDrawEffect::None, FLinearColor(1.0f, 0.86f, 0.24f, 1.0f), false, 2.5f);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), CrossV, ESlateDrawEffect::None, FLinearColor(1.0f, 0.86f, 0.24f, 1.0f), false, 2.5f);
		}
		else if (Wiz == LocalPawn)
		{
			// Local player: Cyan/White icon with forward heading line
			const float HeadingDeg = Wiz->GetActorRotation().Yaw;
			const float HeadingRad = FMath::DegreesToRadians(HeadingDeg);
			const FVector2D FwdDir(FMath::Sin(HeadingRad), -FMath::Cos(HeadingRad));
			TArray<FVector2D> HeadingPts = { WizPos, WizPos + FwdDir * 12.0f };

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(WizPos - FVector2D(4.5f, 4.5f), FVector2D(9.0f, 9.0f)),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.20f, 0.95f, 1.0f, 1.0f)
			);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), HeadingPts, ESlateDrawEffect::None, FLinearColor::White, false, 2.0f);
		}
		else
		{
			// Living ally wizard: Bright emerald/teal dot
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(WizPos - FVector2D(3.5f, 3.5f), FVector2D(7.0f, 7.0f)),
				WhiteBrush,
				ESlateDrawEffect::None,
				FLinearColor(0.25f, 0.90f, 0.50f, 0.95f)
			);
		}
	}
	LayerId += 3;

	return LayerId;
}

TSharedRef<SWidget> SAetheriaMainOverlay::BuildMinimapWidget()
{
	return SNew(SBox)
		.WidthOverride(210.0f)
		.HeightOverride(210.0f)
		[
			SNew(SAetheriaMinimapWidget)
			.OwningController(PC)
		];
}

TSharedRef<SWidget> SAetheriaMainOverlay::BuildInGameHUD()
{
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FSlateBrush* PanelBrush = PanelObsidianBrush.IsValid() ? PanelObsidianBrush.Get() : WhiteBrush;
	const FSlateBrush* DockBrush = ActionDockBrush.IsValid() ? ActionDockBrush.Get() : PanelBrush;

	auto MakeIconSocket = [this, WhiteBrush](const TCHAR* KeyBadge, TSharedPtr<FSlateDynamicImageBrush> IconBrush, FLinearColor AccentTint, TFunction<void()> OnClick) -> TSharedRef<SWidget>
	{
		return SNew(SBox).WidthOverride(76.0f).HeightOverride(76.0f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot().Padding(7.0f)
			[
				SNew(SImage)
				.Image_Lambda([IconBrush, WhiteBrush]() { return IconBrush.IsValid() ? IconBrush.Get() : WhiteBrush; })
				.ColorAndOpacity(IconBrush.IsValid() ? FLinearColor::White : FLinearColor(AccentTint.R * 0.45f, AccentTint.G * 0.45f, AccentTint.B * 0.45f, 0.9f))
			]
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image_Lambda([this]() { return SocketNormalBrush.IsValid() ? SocketNormalBrush.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"); })
				.ColorAndOpacity(FLinearColor(0.78f, 0.72f, 0.65f, 1.0f))
			]
			+ SOverlay::Slot()
			[
				SNew(SButton)
				.ButtonStyle(&SocketBtnStyle)
				.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
				[
					SNew(SOverlay)
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(4.0f, 4.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush)
						.BorderBackgroundColor(FLinearColor(0.015f, 0.012f, 0.020f, 0.90f))
						.Padding(FMargin(5.0f, 1.5f))
						[
							SNew(STextBlock)
							.Text(FText::FromString(KeyBadge))
							.Font(MindFont(10, true))
							.ColorAndOpacity(FLinearColor(0.96f, 0.88f, 0.72f, 0.98f))
						]
					]
				]
			]
		];
	};

	auto MakeSpellSlotSocket = [this, WhiteBrush](int32 SlotIdx, const TCHAR* KeyBadge) -> TSharedRef<SWidget>
	{
		return SNew(SBox).WidthOverride(76.0f).HeightOverride(76.0f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot().Padding(7.0f)
			[
				SNew(SImage)
				.Image_Lambda([this, SlotIdx]() { return GetSlotSpellBrush(SlotIdx); })
				.ColorAndOpacity_Lambda([this, SlotIdx]() { return GetSocketTint(SlotIdx); })
			]
			+ SOverlay::Slot().Padding(7.0f)
			[
				SNew(SBorder)
				.Visibility_Lambda([this, SlotIdx]() {
					if (PC.IsValid())
					{
						if (const AAetheriaWizardCharacter* W = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
						{
							return (W->GetSlotCooldown(SlotIdx) > 0.1f) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
						}
					}
					return EVisibility::Collapsed;
				})
				.BorderImage(WhiteBrush)
				.BorderBackgroundColor(FLinearColor(0.005f, 0.005f, 0.010f, 0.92f))
			]
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image_Lambda([this, SlotIdx]() { return GetSocketFrameBrush(SlotIdx); })
				.ColorAndOpacity(FLinearColor(0.78f, 0.72f, 0.65f, 1.0f))
			]
			+ SOverlay::Slot()
			[
				SNew(SButton)
				.ButtonStyle(&SocketBtnStyle)
				.OnClicked_Lambda([this, SlotIdx]() { if (PC.IsValid()) PC->UI_SelectOrCastSlot(SlotIdx); return FReply::Handled(); })
				[
					SNew(SOverlay)
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this, SlotIdx]() { return GetSlotCooldownText(SlotIdx); })
						.Font(MindFont(16, true))
						.ColorAndOpacity(FLinearColor(1.0f, 0.90f, 0.55f, 1.0f))
					]
					+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(4.0f, 4.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush)
						.BorderBackgroundColor(FLinearColor(0.015f, 0.012f, 0.020f, 0.90f))
						.Padding(FMargin(5.0f, 1.5f))
						[
							SNew(STextBlock)
							.Text(FText::FromString(KeyBadge))
							.Font(MindFont(10, true))
							.ColorAndOpacity(FLinearColor(0.96f, 0.88f, 0.72f, 0.98f))
						]
					]
				]
			]
		];
	};

	TSharedRef<SVerticalBox> PartyListBox = SNew(SVerticalBox);
	for (int32 i = 0; i < 6; ++i)
	{
		PartyListBox->AddSlot().AutoHeight().Padding(0, 0, 0, 5)
		[
			BuildPartyMemberRow(i)
		];
	}

	return SNew(SOverlay)

		// Top-Left: Sculpted Party Portrait Medallions (1-6)
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(16.0f)
		[
			PartyListBox
		]

		// Top-Center: Sanctuary Boss Dread-Bar & Astral Portal Banner
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(16.0f)
		[
			SNew(SBox)
			.WidthOverride(520.0f)
			[
				SNew(SVerticalBox)

				// Boss Bar (Act V only)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder)
					.Visibility(this, &SAetheriaMainOverlay::GetBossBarVisibility)
					.BorderImage(PanelBrush)
					.BorderBackgroundColor(PanelObsidianBrush.IsValid() ? FLinearColor::White : MindVoidPanel)
					.Padding(14.0f, 10.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 5)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
							[
								SNew(SBox).WidthOverride(24.0f).HeightOverride(24.0f)
								[
									SNew(SImage).Image_Lambda([this, WhiteBrush]() { return SkullCrestBrush.IsValid() ? SkullCrestBrush.Get() : WhiteBrush; })
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(this, &SAetheriaMainOverlay::GetBossTitleText)
								.Font(MindFont(13, true))
								.ColorAndOpacity(FLinearColor(1.0f, 0.72f, 0.36f, 1.0f))
							]
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBox).HeightOverride(11.0f)
							[
								SNew(SProgressBar)
								.Style(&BossBarStyle)
								.Percent(this, &SAetheriaMainOverlay::GetBossHPPercent)
							]
						]
					]
				]

				// Portal Open Banner
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0).HAlign(HAlign_Center)
				[
					SNew(SButton)
					.Visibility(this, &SAetheriaMainOverlay::GetPortalBannerVisibility)
					.ButtonStyle(&CrimsonBtnStyle)
					.OnClicked_Lambda([this]() {
						if (PC.IsValid()) PC->ServerEnterPortal();
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("АСТРАЛЬНЫЙ ПОРТАЛ ОТКРЫТ  [ENTER]")))
						.Font(MindFont(12, true))
						.ColorAndOpacity(FLinearColor(0.98f, 0.90f, 0.72f, 1.0f))
					]
				]
			]
		]

		// Top-Right: Minimap + Sanctuary Quest Plaque + Forged Stone Buttons
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(16.0f)
		[
			SNew(SVerticalBox)

			// Live Minimap (210x210)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 0, 0, 10)
			[
				BuildMinimapWidget()
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 0, 0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
				[
					SNew(SButton).ButtonStyle(&StoneBtnStyle)
					.OnClicked_Lambda([this]() { ToggleCharacterEditor(); return FReply::Handled(); })
					[ SNew(STextBlock).Text(FText::FromString(TEXT("Облик [C]"))).Font(MindFont(9, true)).ColorAndOpacity(BoneParchmentText) ]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
				[
					SNew(SButton).ButtonStyle(&StoneBtnStyle)
					.OnClicked_Lambda([this]() { ToggleGrimoire(); return FReply::Handled(); })
					[ SNew(STextBlock).Text(FText::FromString(TEXT("Гримуар [Tab]"))).Font(MindFont(9, true)).ColorAndOpacity(BoneParchmentText) ]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
				[
					SNew(SButton).ButtonStyle(&StoneBtnStyle)
					.OnClicked_Lambda([this]() { ToggleWorldMap(); return FReply::Handled(); })
					[ SNew(STextBlock).Text(FText::FromString(TEXT("Миры [M]"))).Font(MindFont(9, true)).ColorAndOpacity(BoneParchmentText) ]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
				[
					SNew(SButton).ButtonStyle(&StoneBtnStyle)
					.OnClicked_Lambda([this]() { if (PC.IsValid()) PC->ServerAddBot(); return FReply::Handled(); })
					[ SNew(STextBlock).Text(FText::FromString(TEXT("+Маг"))).Font(MindFont(9, true)).ColorAndOpacity(GildedBronzeText) ]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).ButtonStyle(&StoneBtnStyle)
					.OnClicked_Lambda([this]() { ToggleSettings(); return FReply::Handled(); })
					[ SNew(STextBlock).Text(FText::FromString(TEXT("[Esc]"))).Font(MindFont(9, true)).ColorAndOpacity(BoneParchmentText) ]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
			[
				SNew(SBorder)
				.BorderImage(PanelBrush)
				.BorderBackgroundColor(PanelObsidianBrush.IsValid() ? FLinearColor::White : MindBgGlass)
				.Padding(14.0f, 10.0f)
				[
					SNew(SBox).WidthOverride(424.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
							[
								SNew(SBox).WidthOverride(22.0f).HeightOverride(22.0f)
								[
									SNew(SImage).Image_Lambda([this, WhiteBrush]() { return SkullCrestBrush.IsValid() ? SkullCrestBrush.Get() : WhiteBrush; })
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(this, &SAetheriaMainOverlay::GetLocationHeaderText)
								.Font(MindFont(11, true))
								.ColorAndOpacity(GildedBronzeText)
								.AutoWrapText(true)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 6)
						[
							SNew(STextBlock)
							.Text(this, &SAetheriaMainOverlay::GetQuestObjectiveText)
							.Font(MindFont(10))
							.ColorAndOpacity(BoneParchmentText)
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBox).HeightOverride(6.0f)
							[
								SNew(SProgressBar)
								.Style(&XPBarStyle)
								.Percent(this, &SAetheriaMainOverlay::GetQuestProgressPercent)
							]
						]
					]
				]
			]
		]

		// Bottom-Center: Sculpted Gargoyle Sanctuary Reliquary + Twin Elixir Globes
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// Floating Triad & Mode Preview Crown Ribbon above Dock Horned Crest
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 4)
			[
				SNew(SBorder)
				.BorderImage(PanelBrush)
				.BorderBackgroundColor(PanelObsidianBrush.IsValid() ? FLinearColor::White : MindVoidPanel)
				.Padding(12.0f, 5.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
					[
						SNew(STextBlock)
						.Text(this, &SAetheriaMainOverlay::GetOrbTriadText)
						.Font(MindFont(10, true))
						.ColorAndOpacity(BoneParchmentText)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
					[
						SNew(STextBlock)
						.Text(this, &SAetheriaMainOverlay::GetNextSpellPreviewText)
						.Font(MindFont(10, true))
						.ColorAndOpacity(GildedBronzeText)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SAetheriaMainOverlay::GetCastModePillText)
						.Font(MindFont(9, true))
						.ColorAndOpacity(AshMutedText)
					]
				]
			]

			// Exact 1160x268 Sculpted Dock Frame with pixel-aligned Globes & Center Sockets
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(1160.0f).HeightOverride(268.0f)
				[
					SNew(SOverlay)

					// Layer 0: Sculpted Gargoyle Basalt & Bronze Reliquary Frame
					+ SOverlay::Slot()
					[
						SNew(SImage).Image(DockBrush)
					]

					// Layer 1 Left: 224x224 Ornate Blood HP Globe centered in Left Cradle
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(48.0f, 22.0f, 0.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(224.0f).HeightOverride(224.0f)
						[
							SNew(SOverlay)
							+ SOverlay::Slot().Padding(46.0f)
							[
								SNew(SImage).Image_Lambda([this]() { return GlobeBackBrush.IsValid() ? GlobeBackBrush.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"); })
							]
							+ SOverlay::Slot().Padding(46.0f)
							[
								SNew(SProgressBar)
								.Style(&HPGlobeBarStyle)
								.BarFillType(EProgressBarFillType::BottomToTop)
								.Percent(this, &SAetheriaMainOverlay::GetHPPercent)
							]
							// Dynamic glowing arterial liquid surface meniscus tracking HP fill level
							+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Top)
							.Padding(TAttribute<FMargin>::CreateLambda([this]() {
								const float Pct = FMath::Clamp(GetHPPercent().Get(1.0f), 0.04f, 0.98f);
								const float TopY = 46.0f + (1.0f - Pct) * 132.0f - 1.5f;
								return FMargin(0.0f, TopY, 0.0f, 0.0f);
							}))
							[
								SNew(SBox)
								.Visibility_Lambda([this]() {
									const float Pct = GetHPPercent().Get(1.0f);
									return (Pct >= 0.985f || Pct <= 0.025f) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
								})
								.WidthOverride_Lambda([this]() {
									const float Pct = FMath::Clamp(GetHPPercent().Get(1.0f), 0.04f, 0.98f);
									const float Dy = ((1.0f - Pct) - 0.5f) * 2.0f;
									const float Chord = 2.0f * FMath::Sqrt(FMath::Max(0.0f, 1.0f - Dy * Dy)) * 62.0f;
									return FMath::Max(14.0f, Chord);
								})
								.HeightOverride(3.5f)
								[
									SNew(SBorder)
									.BorderImage(WhiteBrush)
									.BorderBackgroundColor(FLinearColor(1.0f, 0.45f, 0.35f, 0.58f))
									.Padding(0.0f)
								]
							]
							+ SOverlay::Slot()
							[
								SNew(SImage).Image_Lambda([this]() { return GlobeRimBrush.IsValid() ? GlobeRimBrush.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"); })
							]
							+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 14)
							[
								SNew(STextBlock)
								.Text_Lambda([this]() {
									if (PC.IsValid())
									{
										if (const AAetheriaWizardCharacter* W = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
										{
											return FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), W->HP, W->MaxHP));
										}
									}
									return FText::FromString(TEXT("450 / 450"));
								})
								.Font(MindFont(11, true))
								.ColorAndOpacity(FLinearColor(0.98f, 0.90f, 0.78f, 1.0f))
							]
						]
					]

					// Layer 1 Right: 224x224 Ornate Abyssal Mana Globe centered in Right Cradle (1160 - 48 - 224 = 888)
					+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(888.0f, 22.0f, 0.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(224.0f).HeightOverride(224.0f)
						[
							SNew(SOverlay)
							+ SOverlay::Slot().Padding(46.0f)
							[
								SNew(SImage).Image_Lambda([this]() { return GlobeBackBrush.IsValid() ? GlobeBackBrush.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"); })
							]
							+ SOverlay::Slot().Padding(46.0f)
							[
								SNew(SProgressBar)
								.Style(&MPGlobeBarStyle)
								.BarFillType(EProgressBarFillType::BottomToTop)
								.Percent(this, &SAetheriaMainOverlay::GetManaPercent)
							]
							// Dynamic glowing arcane liquid surface meniscus tracking MP fill level
							+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Top)
							.Padding(TAttribute<FMargin>::CreateLambda([this]() {
								const float Pct = FMath::Clamp(GetManaPercent().Get(1.0f), 0.04f, 0.98f);
								const float TopY = 46.0f + (1.0f - Pct) * 132.0f - 1.5f;
								return FMargin(0.0f, TopY, 0.0f, 0.0f);
							}))
							[
								SNew(SBox)
								.Visibility_Lambda([this]() {
									const float Pct = GetManaPercent().Get(1.0f);
									return (Pct >= 0.985f || Pct <= 0.025f) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
								})
								.WidthOverride_Lambda([this]() {
									const float Pct = FMath::Clamp(GetManaPercent().Get(1.0f), 0.04f, 0.98f);
									const float Dy = ((1.0f - Pct) - 0.5f) * 2.0f;
									const float Chord = 2.0f * FMath::Sqrt(FMath::Max(0.0f, 1.0f - Dy * Dy)) * 62.0f;
									return FMath::Max(14.0f, Chord);
								})
								.HeightOverride(3.5f)
								[
									SNew(SBorder)
									.BorderImage(WhiteBrush)
									.BorderBackgroundColor(FLinearColor(0.35f, 0.80f, 1.0f, 0.58f))
									.Padding(0.0f)
								]
							]
							+ SOverlay::Slot()
							[
								SNew(SImage).Image_Lambda([this]() { return GlobeRimBrush.IsValid() ? GlobeRimBrush.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"); })
							]
							+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 14)
							[
								SNew(STextBlock)
								.Justification(ETextJustify::Center)
								.Text_Lambda([this]() {
									if (PC.IsValid())
									{
										if (const AAetheriaWizardCharacter* W = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
										{
											if (W->Shield > 1.0f)
											{
												return FText::FromString(FString::Printf(TEXT("%.0f +%.0f"), W->Mana, W->Shield));
											}
											return FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), W->Mana, W->MaxMana));
										}
									}
									return FText::FromString(TEXT("250 / 250"));
								})
								.Font(MindFont(11, true))
								.ColorAndOpacity(FLinearColor(0.88f, 0.96f, 1.0f, 1.0f))
							]
						]
					]

					// Layer 2 Center: 7 Painted Bronze Sockets (76px) + Gold XP Bar centered in Dock Trough (top=96)
					+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.0f, 96.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)
							[ MakeIconSocket(TEXT("Q"), IconIgnisBrush, FLinearColor(1.0f, 0.42f, 0.16f, 1.0f), [this]() { if (PC.IsValid()) PC->UI_PushOrb(EAetheriaOrb::Ignis); }) ]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)
							[ MakeIconSocket(TEXT("W"), IconTempestBrush, FLinearColor(0.22f, 0.92f, 1.0f, 1.0f), [this]() { if (PC.IsValid()) PC->UI_PushOrb(EAetheriaOrb::Tempest); }) ]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)
							[ MakeIconSocket(TEXT("E"), IconGravitonBrush, FLinearColor(0.70f, 0.44f, 1.0f, 1.0f), [this]() { if (PC.IsValid()) PC->UI_PushOrb(EAetheriaOrb::Graviton); }) ]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
							[ MakeIconSocket(TEXT("R"), IconInvokeBrush, BronzeFrameColor, [this]() { if (PC.IsValid()) PC->UI_InvokeOrCast(); }) ]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)
							[ MakeSpellSlotSocket(0, TEXT("D")) ]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
							[ MakeSpellSlotSocket(1, TEXT("F")) ]
							+ SHorizontalBox::Slot().AutoWidth()
							[ MakeIconSocket(TEXT("SPC"), IconDashBrush, FLinearColor(0.75f, 0.88f, 1.0f, 1.0f), [this]() { if (PC.IsValid()) PC->UI_Dash(); }) ]
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 8, 0, 0)
						[
							SNew(SBox).WidthOverride(560.0f).HeightOverride(6.0f)
							[
								SNew(SProgressBar)
								.Style(&XPBarStyle)
								.Percent(this, &SAetheriaMainOverlay::GetXPPercent)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 2, 0, 0)
						[
							SNew(STextBlock)
							.Text(this, &SAetheriaMainOverlay::GetXPText)
							.Font(MindFont(9, true))
							.ColorAndOpacity(GildedBronzeText)
						]
					]
				]
			]
		];
}

TSharedRef<SWidget> SAetheriaMainOverlay::BuildGrimoireModal()
{
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FSlateBrush* PanelBrush = PanelObsidianBrush.IsValid() ? PanelObsidianBrush.Get() : WhiteBrush;
	static const TCHAR* RomanChapters[10] = {
		TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV"), TEXT("V"),
		TEXT("VI"), TEXT("VII"), TEXT("VIII"), TEXT("IX"), TEXT("X")
	};

	TSharedRef<SHorizontalBox> BookmarkTabsRow = SNew(SHorizontalBox);
	for (int32 TabIdx = 0; TabIdx < 10; ++TabIdx)
	{
		BookmarkTabsRow->AddSlot().AutoWidth().Padding(0, 0, 4, 0)
		[
			SNew(SButton)
			.ButtonStyle(&StoneBtnStyle)
			.ButtonColorAndOpacity_Lambda([this, TabIdx]() {
				return (CurrentGrimoirePage == TabIdx)
					? FLinearColor(1.0f, 0.78f, 0.42f, 1.0f)
					: FLinearColor(0.84f, 0.84f, 0.88f, 1.0f);
			})
			.OnClicked_Lambda([this, TabIdx]() {
				SetGrimoirePage(TabIdx);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromString(RomanChapters[TabIdx]))
				.Font(MindFont(10, true))
				.ColorAndOpacity_Lambda([this, TabIdx]() {
					return (CurrentGrimoirePage == TabIdx)
						? FSlateColor(GildedBronzeText)
						: FSlateColor(BoneParchmentText);
				})
			]
		];
	}

	BookmarkTabsRow->AddSlot().FillWidth(1.0f) [ SNew(SSpacer) ];
	BookmarkTabsRow->AddSlot().AutoWidth()
	[
		SNew(SButton)
		.ButtonStyle(&StoneBtnStyle)
		.OnClicked_Lambda([this]() { bShowGrimoire = false; return FReply::Handled(); })
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Закрыть [Tab]")))
			.Font(MindFont(10, true))
			.ColorAndOpacity(BoneParchmentText)
		]
	];

	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
		.BorderBackgroundColor(FLinearColor::Transparent)
		.Padding(0.0f)
		[
			SNew(SBox)
			.WidthOverride(1080.0f)
			.HeightOverride(690.0f)
			[
				SNew(SOverlay)

				// Layer 0: Open Grimoire Book Spread Artwork
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image_Lambda([this]() { return GetBookSpreadBrush(); })
					.ColorAndOpacity_Lambda([this]() {
						return BookSpreadBrush.IsValid()
							? FLinearColor::White
							: FLinearColor(0.84f, 0.78f, 0.66f, 1.0f);
					})
				]

				// Layer 1: Illustrated Two-Page Content + Roman Bookmarks + Flipping Controls
				+ SOverlay::Slot()
				.Padding(56.0f, 36.0f, 56.0f, 34.0f)
				[
					SNew(SVerticalBox)

					// Top Roman Bookmark Tabs I..X + Close
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
					[
						BookmarkTabsRow
					]

					// Center Spread: Left Illustration Page + Spine Gutter + Right Lore/Stats Page
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SNew(SHorizontalBox)

						// LEFT PAGE: Spell Engraving Plate + Roman Index + Elemental Formula
						+ SHorizontalBox::Slot().FillWidth(0.495f).Padding(18.0f, 10.0f, 16.0f, 10.0f)
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 12)
							[
								SNew(STextBlock)
								.Text_Lambda([this]() {
									static const TCHAR* Romans[] = {TEXT("I"),TEXT("II"),TEXT("III"),TEXT("IV"),TEXT("V"),TEXT("VI"),TEXT("VII"),TEXT("VIII"),TEXT("IX"),TEXT("X")};
									const int32 Idx = FMath::Clamp(CurrentGrimoirePage, 0, 9);
									return FText::FromString(FString::Printf(TEXT("ЛИСТ %s  ·  АРКАНУМ СТИХИЙ"), Romans[Idx]));
								})
								.Font(MindFont(11, true, false))
								.ColorAndOpacity(ParchmentInkMuted)
							]

							+ SVerticalBox::Slot().FillHeight(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
							[
								SNew(SBorder)
								.BorderImage(WhiteBrush)
								.BorderBackgroundColor(BronzeFrameColor)
								.Padding(2.0f)
								[
									SNew(SBox).WidthOverride(336.0f).HeightOverride(336.0f)
									[
										SNew(SImage)
										.Image_Lambda([this]() { return GetCurrentGrimoirePlateBrush(); })
									]
								]
							]

							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 14, 0, 0)
							[
								SNew(STextBlock)
								.Text_Lambda([this]() {
									const TArray<FAetheriaSpellDef>& Spells = UAetheriaSpellDatabase::GetAllSpells();
									if (Spells.IsEmpty()) return FText::GetEmpty();
									const FAetheriaSpellDef& S = Spells[FMath::Clamp(CurrentGrimoirePage, 0, Spells.Num() - 1)];
									return FText::FromString(FString::Printf(TEXT("Формула сфер: [ %s ]  ·  %s"), *S.ComboKey, *GetSpellSchoolSubtitle(S.SpellId)));
								})
								.Font(MindFont(11, true, false))
								.ColorAndOpacity(ParchmentInkDark)
							]
						]

						// CENTER SPINE GUTTER (96px clear margin around physical book crease)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(96.0f)
						]

						// RIGHT PAGE: Title, Description, Combat Metrics, Synergy, Wax-Seal Cast Action
						+ SHorizontalBox::Slot().FillWidth(0.505f).Padding(18.0f, 12.0f, 30.0f, 10.0f)
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
							[
								SNew(STextBlock)
								.Text_Lambda([this]() {
									const TArray<FAetheriaSpellDef>& Spells = UAetheriaSpellDatabase::GetAllSpells();
									if (Spells.IsEmpty()) return FText::GetEmpty();
									return FText::FromString(GetCleanSpellTitle(Spells[FMath::Clamp(CurrentGrimoirePage, 0, Spells.Num() - 1)]));
								})
								.Font(MindFont(17, true, false))
								.ColorAndOpacity(ParchmentInkDark)
								.AutoWrapText(true)
							]

							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
							[
								SNew(STextBlock)
								.Text_Lambda([this]() {
									const TArray<FAetheriaSpellDef>& Spells = UAetheriaSpellDatabase::GetAllSpells();
									if (Spells.IsEmpty()) return FText::GetEmpty();
									const FAetheriaSpellDef& S = Spells[FMath::Clamp(CurrentGrimoirePage, 0, Spells.Num() - 1)];
									return FText::FromString(FString::Printf(TEXT("Тайная формула: [%s]  ·  %s"), *S.ComboKey, *GetSpellSchoolSubtitle(S.SpellId)));
								})
								.Font(MindFont(10, true, false))
								.ColorAndOpacity(ParchmentInkMuted)
							]

							+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 12)
							[
								SNew(SBox).HeightOverride(1.5f)
								[
									SNew(SBorder)
									.BorderImage(WhiteBrush)
									.BorderBackgroundColor(ParchmentRuleColor)
								]
							]

							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
							[
								SNew(STextBlock)
								.Text_Lambda([this]() {
									const TArray<FAetheriaSpellDef>& Spells = UAetheriaSpellDatabase::GetAllSpells();
									if (Spells.IsEmpty()) return FText::GetEmpty();
									return FText::FromString(Spells[FMath::Clamp(CurrentGrimoirePage, 0, Spells.Num() - 1)].Description);
								})
								.Font(MindFont(11, false, false))
								.ColorAndOpacity(ParchmentInkDark)
								.AutoWrapText(true)
							]

							// Combat Parameters Box
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
							[
								SNew(SBorder)
								.BorderImage(WhiteBrush)
								.BorderBackgroundColor(ParchmentRuleColor)
								.Padding(1.0f)
								[
									SNew(SBorder)
									.BorderImage(WhiteBrush)
									.BorderBackgroundColor(FLinearColor(0.84f, 0.78f, 0.65f, 0.45f))
									.Padding(10.0f)
									[
										SNew(STextBlock)
										.Text_Lambda([this]() {
											const TArray<FAetheriaSpellDef>& Spells = UAetheriaSpellDatabase::GetAllSpells();
											if (Spells.IsEmpty()) return FText::GetEmpty();
											const FAetheriaSpellDef& S = Spells[FMath::Clamp(CurrentGrimoirePage, 0, Spells.Num() - 1)];
											return FText::FromString(FString::Printf(
												TEXT("Урон: %.0f   ·   Кулдаун: %.1f сек\nМана: %.0f   ·   Радиус / Дальность: %.1f м / %.0f м"),
												S.BaseDamage, S.Cooldown, S.ManaCost, S.RadiusUU / 100.0f, S.RangeUU / 100.0f));
										})
										.Font(MindFont(10, true, false))
										.ColorAndOpacity(ParchmentInkDark)
										.AutoWrapText(true)
									]
								]
							]

							// Synergy Note
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
							[
								SNew(SBorder)
								.BorderImage(WhiteBrush)
								.BorderBackgroundColor(ParchmentRuleColor)
								.Padding(1.0f)
								[
									SNew(SBorder)
									.BorderImage(WhiteBrush)
									.BorderBackgroundColor(FLinearColor(0.80f, 0.73f, 0.58f, 0.35f))
									.Padding(10.0f)
									[
										SNew(STextBlock)
										.Text_Lambda([this]() {
											const TArray<FAetheriaSpellDef>& Spells = UAetheriaSpellDatabase::GetAllSpells();
											if (Spells.IsEmpty()) return FText::GetEmpty();
											return FText::FromString(Spells[FMath::Clamp(CurrentGrimoirePage, 0, Spells.Num() - 1)].SynergyHint);
										})
										.Font(MindFont(10, false, false))
										.ColorAndOpacity(ParchmentInkDark)
										.AutoWrapText(true)
									]
								]
							]

							+ SVerticalBox::Slot().FillHeight(1.0f) [ SNew(SSpacer) ]

							// Wax Seal Invoke Button
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
							[
								SNew(SButton)
								.ButtonStyle(&CrimsonBtnStyle)
								.OnClicked_Lambda([this]() {
									const TArray<FAetheriaSpellDef>& Spells = UAetheriaSpellDatabase::GetAllSpells();
									if (PC.IsValid() && Spells.Num() > 0)
									{
										const FAetheriaSpellDef& S = Spells[FMath::Clamp(CurrentGrimoirePage, 0, Spells.Num() - 1)];
										for (EAetheriaOrb O : S.RequiredOrbs)
										{
											PC->UI_PushOrb(O);
										}
										if (PC->PreferredCastMode == EAetheriaCastMode::InvokeThenAim && !PC->bAutoInvokeOnThree)
										{
											PC->UI_InvokeOrCast();
										}
									}
									return FReply::Handled();
								})
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("СОТВОРИТЬ ПЕЧАТЬ СФЕР [R]")))
									.Font(MindFont(10, true))
									.ColorAndOpacity(FLinearColor(0.98f, 0.90f, 0.74f, 1.0f))
								]
							]
						]
					]

					// Bottom Page Flipping Footer
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(&StoneBtnStyle)
							.OnClicked_Lambda([this]() { SetGrimoirePage(CurrentGrimoirePage - 1); return FReply::Handled(); })
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("< Предыдущая [<-]")))
								.Font(MindFont(10, true))
								.ColorAndOpacity(BoneParchmentText)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text_Lambda([this]() {
								return FText::FromString(FString::Printf(
									TEXT("Страница %d / 10   ·   Листайте [<-] / [->] или колесом мыши"),
									FMath::Clamp(CurrentGrimoirePage, 0, 9) + 1));
							})
							.Font(MindFont(10, true, false))
							.ColorAndOpacity(FLinearColor(FColor::FromHex(TEXT("C8B99B"))))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(&StoneBtnStyle)
							.OnClicked_Lambda([this]() { SetGrimoirePage(CurrentGrimoirePage + 1); return FReply::Handled(); })
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("Следующая [->] >")))
								.Font(MindFont(10, true))
								.ColorAndOpacity(BoneParchmentText)
							]
						]
					]
				]
			]
		];
}

TSharedRef<SWidget> SAetheriaMainOverlay::BuildWorldMapModal()
{
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FSlateBrush* PanelBrush = PanelObsidianBrush.IsValid() ? PanelObsidianBrush.Get() : WhiteBrush;
	TSharedRef<SVerticalBox> ListBox = SNew(SVerticalBox);

	const TArray<FAetheriaLocationDef>& Locs = UAetheriaBiomeDatabase::GetAllLocations();
	for (const FAetheriaLocationDef& Loc : Locs)
	{
		ListBox->AddSlot().AutoHeight().Padding(0, 4)
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush)
			.BorderBackgroundColor(DimBronzeBorder)
			.Padding(1.0f)
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush)
				.BorderBackgroundColor(FLinearColor(0.022f, 0.020f, 0.028f, 0.96f))
				.Padding(12.0f, 10.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 12, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
						[
							SNew(STextBlock)
							.Text(FText::FromString(Loc.DisplayName))
							.Font(MindFont(12, true))
							.ColorAndOpacity(GildedBronzeText)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("Угроза: %s  ·  Босс: %s"), *Loc.HazardName, *Loc.Boss.DisplayName)))
							.Font(MindFont(9))
							.ColorAndOpacity(AshMutedText)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0).VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(&StoneBtnStyle)
						.OnClicked_Lambda([this, Idx = Loc.Index]() {
							if (PC.IsValid()) PC->ServerWarpLocation(Idx, 1);
							bShowWorldMap = false;
							return FReply::Handled();
						})
						[ SNew(STextBlock).Text(FText::FromString(TEXT("В локацию"))).Font(MindFont(10, true)).ColorAndOpacity(BoneParchmentText) ]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(&CrimsonBtnStyle)
						.OnClicked_Lambda([this, Idx = Loc.Index]() {
							if (PC.IsValid()) PC->ServerWarpLocation(Idx, 5);
							bShowWorldMap = false;
							return FReply::Handled();
						})
						[ SNew(STextBlock).Text(FText::FromString(TEXT("К Боссу (Акт V)"))).Font(MindFont(10, true)).ColorAndOpacity(FLinearColor(0.98f, 0.90f, 0.74f, 1.0f)) ]
					]
				]
			]
		];
	}

	return SNew(SBorder)
		.BorderImage(PanelBrush)
		.BorderBackgroundColor(PanelObsidianBrush.IsValid() ? FLinearColor::White : MindVoidPanel)
		.Padding(26.0f, 22.0f)
		[
			SNew(SBox).WidthOverride(760.0f).HeightOverride(540.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[ SNew(STextBlock).Text(FText::FromString(TEXT("АТЛАС ДЕСЯТИ МИРОВ"))).Font(MindFont(18, true)).ColorAndOpacity(GildedBronzeText) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SButton).ButtonStyle(&StoneBtnStyle)
						.OnClicked_Lambda([this]() { bShowWorldMap = false; return FReply::Handled(); })
						[ SNew(STextBlock).Text(FText::FromString(TEXT("Закрыть [M]"))).Font(MindFont(10, true)).ColorAndOpacity(BoneParchmentText) ]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
				[
					SNew(SBox).HeightOverride(1.5f)
					[
						SNew(SBorder).BorderImage(WhiteBrush).BorderBackgroundColor(BronzeFrameColor)
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot() [ ListBox ]
				]
			]
		];
}

TSharedRef<SWidget> SAetheriaMainOverlay::BuildSettingsModal()
{
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	const FSlateBrush* PanelBrush = PanelObsidianBrush.IsValid() ? PanelObsidianBrush.Get() : WhiteBrush;

	return SNew(SBorder)
		.BorderImage(PanelBrush)
		.BorderBackgroundColor(PanelObsidianBrush.IsValid() ? FLinearColor::White : MindVoidPanel)
		.Padding(26.0f, 22.0f)
		[
			SNew(SBox).WidthOverride(560.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[ SNew(STextBlock).Text(FText::FromString(TEXT("СКРИЖАЛЬ НАСТРОЕК"))).Font(MindFont(18, true)).ColorAndOpacity(GildedBronzeText) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SButton).ButtonStyle(&StoneBtnStyle)
						.OnClicked_Lambda([this]() { bShowSettings = false; return FReply::Handled(); })
						[ SNew(STextBlock).Text(FText::FromString(TEXT("Закрыть [Esc]"))).Font(MindFont(10, true)).ColorAndOpacity(BoneParchmentText) ]
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
				[
					SNew(SBox).HeightOverride(1.5f)
					[
						SNew(SBorder).BorderImage(WhiteBrush).BorderBackgroundColor(BronzeFrameColor)
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 6, 0)
					[
						SNew(SButton)
						.ButtonStyle(&StoneBtnStyle)
						.ButtonColorAndOpacity_Lambda([this]() {
							return (PC.IsValid() && PC->PreferredCastMode == EAetheriaCastMode::InvokeThenAim)
								? FLinearColor(1.0f, 0.78f, 0.42f, 1.0f)
								: FLinearColor(0.80f, 0.80f, 0.84f, 1.0f);
						})
						.OnClicked_Lambda([this]() {
							if (PC.IsValid()) PC->UI_SetCastMode(EAetheriaCastMode::InvokeThenAim);
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Классика: Слот -> ЛКМ"))).Font(MindFont(10, true))
							.ColorAndOpacity_Lambda([this]() {
								return (PC.IsValid() && PC->PreferredCastMode == EAetheriaCastMode::InvokeThenAim)
									? FSlateColor(GildedBronzeText) : FSlateColor(BoneParchmentText);
							})
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SButton)
						.ButtonStyle(&StoneBtnStyle)
						.ButtonColorAndOpacity_Lambda([this]() {
							return (PC.IsValid() && PC->PreferredCastMode == EAetheriaCastMode::InstantOnCombo)
								? FLinearColor(1.0f, 0.78f, 0.42f, 1.0f)
								: FLinearColor(0.80f, 0.80f, 0.84f, 1.0f);
						})
						.OnClicked_Lambda([this]() {
							if (PC.IsValid()) PC->UI_SetCastMode(EAetheriaCastMode::InstantOnCombo);
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Прямое сотворение (Q/W/E)"))).Font(MindFont(10, true))
							.ColorAndOpacity_Lambda([this]() {
								return (PC.IsValid() && PC->PreferredCastMode == EAetheriaCastMode::InstantOnCombo)
									? FSlateColor(GildedBronzeText) : FSlateColor(BoneParchmentText);
							})
						]
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 6, 0)
					[
						SNew(SButton).ButtonStyle(&StoneBtnStyle)
						.ButtonColorAndOpacity_Lambda([this]() {
							return (PC.IsValid() && PC->bAutoInvokeOnThree)
								? FLinearColor(1.0f, 0.78f, 0.42f, 1.0f)
								: FLinearColor(0.84f, 0.84f, 0.88f, 1.0f);
						})
						.OnClicked_Lambda([this]() {
							if (PC.IsValid()) PC->UI_SetAutoInvoke(!PC->bAutoInvokeOnThree);
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Justification(ETextJustify::Center)
							.Text_Lambda([this]() {
								const bool bOn = PC.IsValid() && PC->bAutoInvokeOnThree;
								return FText::FromString(bOn ? TEXT("Авто-призыв (3 сферы): ВКЛ") : TEXT("Авто-призыв (3 сферы): ВЫКЛ"));
							})
							.Font(MindFont(10, true))
							.ColorAndOpacity_Lambda([this]() {
								return (PC.IsValid() && PC->bAutoInvokeOnThree)
									? FSlateColor(GildedBronzeText) : FSlateColor(BoneParchmentText);
							})
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SButton).ButtonStyle(&StoneBtnStyle)
						.OnClicked_Lambda([this]() {
							if (PC.IsValid()) PC->UI_ToggleFullscreen();
							return FReply::Handled();
						})
						[ SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Полный экран / Окно"))).Font(MindFont(10, true)).ColorAndOpacity(BoneParchmentText) ]
					]
				]

				// Audio Master Volume Slider
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush)
					.BorderBackgroundColor(DimBronzeBorder)
					.Padding(1.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush)
						.BorderBackgroundColor(FLinearColor(0.022f, 0.020f, 0.028f, 0.96f))
						.Padding(10.0f, 8.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
							[
								SNew(STextBlock)
								.Text_Lambda([this]() {
									const int32 Pct = PC.IsValid() ? FMath::RoundToInt(PC->MasterVolume * 100.0f) : 75;
									return FText::FromString(FString::Printf(TEXT("Громкость звука: %d%%"), Pct));
								})
								.Font(MindFont(10, true))
								.ColorAndOpacity(GildedBronzeText)
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SSlider)
								.Style(&BronzeSliderStyle)
								.Value_Lambda([this]() { return PC.IsValid() ? PC->MasterVolume : 0.75f; })
								.OnValueChanged_Lambda([this](float NewVal) {
									if (PC.IsValid()) PC->UI_SetVolume(NewVal);
								})
							]
						]
					]
				]

				// Graphics Preset
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush)
					.BorderBackgroundColor(DimBronzeBorder)
					.Padding(1.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush)
						.BorderBackgroundColor(FLinearColor(0.022f, 0.020f, 0.028f, 0.96f))
						.Padding(10.0f, 8.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("КАЧЕСТВО ИЗОБРАЖЕНИЯ:")))
								.Font(MindFont(10, true))
								.ColorAndOpacity(GildedBronzeText)
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
								[
									SNew(SButton)
									.ButtonStyle(&StoneBtnStyle)
									.ButtonColorAndOpacity_Lambda([this]() {
										return (PC.IsValid() && PC->GraphicsPresetIndex == 0)
											? FLinearColor(1.35f, 1.02f, 0.38f, 1.0f)
											: FLinearColor(0.38f, 0.38f, 0.42f, 0.85f);
									})
									.OnClicked_Lambda([this]() {
										if (PC.IsValid()) PC->UI_ApplyGraphicsPreset(0);
										return FReply::Handled();
									})
									[
										SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Баланс"))).Font(MindFont(9, true))
										.ColorAndOpacity_Lambda([this]() {
											return (PC.IsValid() && PC->GraphicsPresetIndex == 0)
												? FSlateColor(FLinearColor(1.0f, 0.95f, 0.72f, 1.0f))
												: FSlateColor(FLinearColor(0.60f, 0.58f, 0.54f, 0.75f));
										})
									]
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
								[
									SNew(SButton)
									.ButtonStyle(&StoneBtnStyle)
									.ButtonColorAndOpacity_Lambda([this]() {
										return (PC.IsValid() && PC->GraphicsPresetIndex == 1)
											? FLinearColor(1.35f, 1.02f, 0.38f, 1.0f)
											: FLinearColor(0.38f, 0.38f, 0.42f, 0.85f);
									})
									.OnClicked_Lambda([this]() {
										if (PC.IsValid()) PC->UI_ApplyGraphicsPreset(1);
										return FReply::Handled();
									})
									[
										SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Ультра"))).Font(MindFont(9, true))
										.ColorAndOpacity_Lambda([this]() {
											return (PC.IsValid() && PC->GraphicsPresetIndex == 1)
												? FSlateColor(FLinearColor(1.0f, 0.95f, 0.72f, 1.0f))
												: FSlateColor(FLinearColor(0.60f, 0.58f, 0.54f, 0.75f));
										})
									]
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f)
								[
									SNew(SButton)
									.ButtonStyle(&StoneBtnStyle)
									.ButtonColorAndOpacity_Lambda([this]() {
										return (PC.IsValid() && PC->GraphicsPresetIndex == 2)
											? FLinearColor(1.35f, 1.02f, 0.38f, 1.0f)
											: FLinearColor(0.38f, 0.38f, 0.42f, 0.85f);
									})
									.OnClicked_Lambda([this]() {
										if (PC.IsValid()) PC->UI_ApplyGraphicsPreset(2);
										return FReply::Handled();
									})
									[
										SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Плавность"))).Font(MindFont(9, true))
										.ColorAndOpacity_Lambda([this]() {
											return (PC.IsValid() && PC->GraphicsPresetIndex == 2)
												? FSlateColor(FLinearColor(1.0f, 0.95f, 0.72f, 1.0f))
												: FSlateColor(FLinearColor(0.60f, 0.58f, 0.54f, 0.75f));
										})
									]
								]
							]
						]
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 6, 0)
					[
						SNew(SButton).ButtonStyle(&StoneBtnStyle)
						.ButtonColorAndOpacity_Lambda([this]() {
							EAetheriaPacingMode CurrentPacing = SelectedPacing;
							if (PC.IsValid() && PC->GetWorld())
							{
								if (const AAetheriaGameState* GS = PC->GetWorld()->GetGameState<AAetheriaGameState>())
								{
									CurrentPacing = GS->PacingMode;
								}
							}
							return (CurrentPacing == EAetheriaPacingMode::Epic45Min)
								? FLinearColor(1.35f, 1.02f, 0.38f, 1.0f)
								: FLinearColor(0.38f, 0.38f, 0.42f, 0.85f);
						})
						.OnClicked_Lambda([this]() { if (PC.IsValid()) PC->ServerSetPacing(EAetheriaPacingMode::Epic45Min); return FReply::Handled(); })
						[
							SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Темп: 40-60 мин"))).Font(MindFont(10, true))
							.ColorAndOpacity_Lambda([this]() {
								EAetheriaPacingMode CurrentPacing = SelectedPacing;
								if (PC.IsValid() && PC->GetWorld())
								{
									if (const AAetheriaGameState* GS = PC->GetWorld()->GetGameState<AAetheriaGameState>())
									{
										CurrentPacing = GS->PacingMode;
									}
								}
								return (CurrentPacing == EAetheriaPacingMode::Epic45Min)
									? FSlateColor(FLinearColor(1.0f, 0.95f, 0.72f, 1.0f))
									: FSlateColor(FLinearColor(0.60f, 0.58f, 0.54f, 0.75f));
							})
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SButton).ButtonStyle(&StoneBtnStyle)
						.ButtonColorAndOpacity_Lambda([this]() {
							EAetheriaPacingMode CurrentPacing = SelectedPacing;
							if (PC.IsValid() && PC->GetWorld())
							{
								if (const AAetheriaGameState* GS = PC->GetWorld()->GetGameState<AAetheriaGameState>())
								{
									CurrentPacing = GS->PacingMode;
								}
							}
							return (CurrentPacing == EAetheriaPacingMode::DemoBlitz)
								? FLinearColor(1.35f, 1.02f, 0.38f, 1.0f)
								: FLinearColor(0.38f, 0.38f, 0.42f, 0.85f);
						})
						.OnClicked_Lambda([this]() { if (PC.IsValid()) PC->ServerSetPacing(EAetheriaPacingMode::DemoBlitz); return FReply::Handled(); })
						[
							SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Темп: Демо-Блиц"))).Font(MindFont(10, true))
							.ColorAndOpacity_Lambda([this]() {
								EAetheriaPacingMode CurrentPacing = SelectedPacing;
								if (PC.IsValid() && PC->GetWorld())
								{
									if (const AAetheriaGameState* GS = PC->GetWorld()->GetGameState<AAetheriaGameState>())
									{
										CurrentPacing = GS->PacingMode;
									}
								}
								return (CurrentPacing == EAetheriaPacingMode::DemoBlitz)
									? FSlateColor(FLinearColor(1.0f, 0.95f, 0.72f, 1.0f))
									: FSlateColor(FLinearColor(0.60f, 0.58f, 0.54f, 0.75f));
							})
						]
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 6, 0)
					[
						SNew(SButton).ButtonStyle(&StoneBtnStyle)
						.OnClicked_Lambda([this]() { if (PC.IsValid()) PC->ServerAddBot(); return FReply::Handled(); })
						[ SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("+ Призвать спутника"))).Font(MindFont(10, true)).ColorAndOpacity(GildedBronzeText) ]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SButton).ButtonStyle(&StoneBtnStyle)
						.OnClicked_Lambda([this]() { if (PC.IsValid()) PC->ServerRemoveBot(); return FReply::Handled(); })
						[ SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("- Отпустить спутника"))).Font(MindFont(10, true)).ColorAndOpacity(BoneParchmentText) ]
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 6, 0)
					[
						SNew(SButton).ButtonStyle(&StoneBtnStyle)
						.OnClicked_Lambda([this]() {
							bShowSettings = false;
							bShowLobby = true;
							if (PC.IsValid())
							{
								if (AAetheriaWizardCharacter* W = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
								{
									W->SetLobbyCameraMode(true);
								}
							}
							return FReply::Handled();
						})
						[ SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("В Лобби"))).Font(MindFont(10, true)).ColorAndOpacity(BoneParchmentText) ]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SButton).ButtonStyle(&CrimsonBtnStyle)
						.OnClicked_Lambda([this]() { if (PC.IsValid()) PC->UI_QuitToDesktop(); return FReply::Handled(); })
						[ SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Выйти на рабочий стол"))).Font(MindFont(10, true)).ColorAndOpacity(FLinearColor(0.98f, 0.90f, 0.74f, 1.0f)) ]
					]
				]
			]
		];
}

TSharedRef<SWidget> SAetheriaMainOverlay::BuildCharacterEditorModal()
{
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");

	static const TCHAR* ArchetypeNames[10] = {
		TEXT("I · Маг Пустоты"),
		TEXT("II · Рыцарь Ужаса"),
		TEXT("III · Мрачный Некромант"),
		TEXT("IV · Костяной Лич-Король"),
		TEXT("V · Рогатый Авангард"),
		TEXT("VI · Могильный Заклинатель"),
		TEXT("VII · Звёздный Скиталец"),
		TEXT("VIII · Ткач Сумерек"),
		TEXT("IX · Чумной Алхимик"),
		TEXT("X · Позолоченный Жнец")
	};

	static const TCHAR* HeadgearNames[6] = {
		TEXT("По умолчанию"),
		TEXT("Костяная Корона"),
		TEXT("Рога Ужаса"),
		TEXT("Железный Нимб"),
		TEXT("Маска Надежды"),
		TEXT("Капюшон Бездны")
	};

	static const TCHAR* StaffNames[5] = {
		TEXT("Встроенный"),
		TEXT("Коса Жнеца"),
		TEXT("Посох Проклятия"),
		TEXT("Хрустальный Скипетр"),
		TEXT("Боевой Молот")
	};

	static const TCHAR* FamiliarNames[4] = {
		TEXT("Череп + Гримуар"),
		TEXT("Открытый Гримуар"),
		TEXT("Астролябий"),
		TEXT("Нет")
	};

	static const TCHAR* BackNames[4] = {
		TEXT("Нет"),
		TEXT("Крылья Демона"),
		TEXT("Нимб Архонта"),
		TEXT("Рваный Плащ")
	};

	static const TCHAR* RobeDyeNames[8] = {
		TEXT("Первозданный цвет"),
		TEXT("Слейт Бездны"),
		TEXT("Обсидиан"),
		TEXT("Кровавый Собор"),
		TEXT("Лич Изумруд"),
		TEXT("Пепельная Кость"),
		TEXT("Индиго"),
		TEXT("Янтарь")
	};

	static const TCHAR* TrimMetalNames[4] = {
		TEXT("Закалённая Сталь"),
		TEXT("Античная Бронза"),
		TEXT("Чёрное Железо"),
		TEXT("Бледное Золото")
	};

	static const TCHAR* RuneGlowNames[4] = {
		TEXT("Золотое Пламя"),
		TEXT("Арканный Циан"),
		TEXT("Пламя Игнис"),
		TEXT("Эфирный Фиолет")
	};

	auto MakeCycleRow = [this, WhiteBrush](
		const FText& CategoryTitle,
		TFunction<int32()> GetVal,
		TFunction<void(int32)> SetVal,
		const TCHAR* const Names[],
		int32 Count) -> TSharedRef<SWidget>
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 2)
			[
				SNew(STextBlock)
				.Text(CategoryTitle)
				.Font(MindFont(10, true))
				.ColorAndOpacity(GildedBronzeText)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush)
				.BorderBackgroundColor(DimBronzeBorder)
				.Padding(1.0f)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush)
					.BorderBackgroundColor(FLinearColor(0.022f, 0.020f, 0.028f, 0.96f))
					.Padding(4.0f, 3.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(&StoneBtnStyle)
							.OnClicked_Lambda([this, GetVal, SetVal, Count]() {
								const int32 Cur = GetVal();
								const int32 Prev = (Cur - 1 + Count) % Count;
								SetVal(Prev);
								if (PC.IsValid())
								{
									PC->UI_UpdateCustomization(WorkingCustomization);
									UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_UIClick"), 0.55f);
								}
								return FReply::Handled();
							})
							[
								SNew(STextBlock).Text(FText::FromString(TEXT("<"))).Font(MindFont(10, true)).ColorAndOpacity(GildedBronzeText)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(6.0f, 0)
						[
							SNew(STextBlock)
							.Text_Lambda([GetVal, Names, Count]() {
								const int32 Val = FMath::Clamp(GetVal(), 0, Count - 1);
								return FText::FromString(Names[Val]);
							})
							.Font(MindFont(10))
							.ColorAndOpacity(BoneParchmentText)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(&StoneBtnStyle)
							.OnClicked_Lambda([this, GetVal, SetVal, Count]() {
								const int32 Cur = GetVal();
								const int32 Next = (Cur + 1) % Count;
								SetVal(Next);
								if (PC.IsValid())
								{
									PC->UI_UpdateCustomization(WorkingCustomization);
									UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_UIClick"), 0.55f);
								}
								return FReply::Handled();
							})
							[
								SNew(STextBlock).Text(FText::FromString(TEXT(">"))).Font(MindFont(10, true)).ColorAndOpacity(GildedBronzeText)
							]
						]
					]
				]
			];
	};

	const FSlateBrush* PanelBrush = PanelObsidianBrush.IsValid() ? PanelObsidianBrush.Get() : WhiteBrush;

	return SNew(SBorder)
		.BorderImage(PanelBrush)
		.BorderBackgroundColor(PanelObsidianBrush.IsValid() ? FLinearColor::White : MindVoidPanel)
		.Padding(22.0f, 18.0f)
		[
			SNew(SBox)
			.WidthOverride(460.0f)
			.HeightOverride(700.0f)
			[
				SNew(SVerticalBox)

				// Header: Title + Close [X]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("КУЗНИЦА ОБЛИКА")))
						.Font(MindFont(17, true))
						.ColorAndOpacity(GildedBronzeText)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(&StoneBtnStyle)
						.OnClicked_Lambda([this]() {
							ToggleCharacterEditor();
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("X [Esc]"))).Font(MindFont(10, true)).ColorAndOpacity(BoneParchmentText)
						]
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[
					SNew(SBox).HeightOverride(1.5f)
					[
						SNew(SBorder).BorderImage(WhiteBrush).BorderBackgroundColor(BronzeFrameColor)
					]
				]

				// Camera 3D Turntable Controls
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[
					SNew(SBorder)
					.BorderImage(WhiteBrush)
					.BorderBackgroundColor(DimBronzeBorder)
					.Padding(1.0f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush)
						.BorderBackgroundColor(FLinearColor(0.022f, 0.020f, 0.028f, 0.96f))
						.Padding(6.0f, 4.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
							[
								SNew(SButton)
								.ButtonStyle(&StoneBtnStyle)
								.OnClicked_Lambda([this]() {
									EditorTurntableYaw -= 35.0f;
									if (EditorTurntableYaw < -180.0f) EditorTurntableYaw += 360.0f;
									if (PC.IsValid())
									{
										PC->UI_SetEditorPreviewCamera(true, EditorTurntableYaw);
										UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_UIClick"), 0.5f);
									}
									return FReply::Handled();
								})
								[
									SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("< Вращать (-35°)"))).Font(MindFont(9, true)).ColorAndOpacity(BoneParchmentText)
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
							[
								SNew(SButton)
								.ButtonStyle(&StoneBtnStyle)
								.OnClicked_Lambda([this]() {
									EditorTurntableYaw = 0.0f;
									if (PC.IsValid())
									{
										PC->UI_SetEditorPreviewCamera(true, EditorTurntableYaw);
										UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_UIClick"), 0.5f);
									}
									return FReply::Handled();
								})
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("Сброс ракурса"))).Font(MindFont(9, true)).ColorAndOpacity(GildedBronzeText)
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[
								SNew(SButton)
								.ButtonStyle(&StoneBtnStyle)
								.OnClicked_Lambda([this]() {
									EditorTurntableYaw += 35.0f;
									if (EditorTurntableYaw > 180.0f) EditorTurntableYaw -= 360.0f;
									if (PC.IsValid())
									{
										PC->UI_SetEditorPreviewCamera(true, EditorTurntableYaw);
										UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_UIClick"), 0.5f);
									}
									return FReply::Handled();
								})
								[
									SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Вращать (+35°) >"))).Font(MindFont(9, true)).ColorAndOpacity(BoneParchmentText)
								]
							]
						]
					]
				]

					// Scrollable Customization Options
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SNew(SScrollBox)

						// 1. Body Archetype
						+ SScrollBox::Slot()
						[
							MakeCycleRow(
								FText::FromString(TEXT("АРХЕТИП ГЕРОЯ")),
								[this]() { return WorkingCustomization.BodyArchetypeIndex; },
								[this](int32 V) { WorkingCustomization.BodyArchetypeIndex = V; },
								ArchetypeNames, 10)
						]

						// 2. Headgear / Crown
						+ SScrollBox::Slot()
						[
							MakeCycleRow(
								FText::FromString(TEXT("ШЛЕМ / КОРОНА")),
								[this]() { return WorkingCustomization.HeadgearIndex; },
								[this](int32 V) { WorkingCustomization.HeadgearIndex = V; },
								HeadgearNames, 6)
						]

						// 3. Staff / Weapon
						+ SScrollBox::Slot()
						[
							MakeCycleRow(
								FText::FromString(TEXT("ПОСОХ / ОРУЖИЕ")),
								[this]() { return WorkingCustomization.StaffIndex; },
								[this](int32 V) { WorkingCustomization.StaffIndex = V; },
								StaffNames, 5)
						]

						// 4. Familiar
						+ SScrollBox::Slot()
						[
							MakeCycleRow(
								FText::FromString(TEXT("ФАМИЛЬЯР")),
								[this]() { return WorkingCustomization.FamiliarIndex; },
								[this](int32 V) { WorkingCustomization.FamiliarIndex = V; },
								FamiliarNames, 4)
						]

						// 5. Back / Cloak
						+ SScrollBox::Slot()
						[
							MakeCycleRow(
								FText::FromString(TEXT("СПИНА / ПЛАЩ")),
								[this]() { return WorkingCustomization.BackIndex; },
								[this](int32 V) { WorkingCustomization.BackIndex = V; },
								BackNames, 4)
						]

						// 6. Robe Dye
						+ SScrollBox::Slot()
						[
							MakeCycleRow(
								FText::FromString(TEXT("КРАСКА МАНТИИ")),
								[this]() { return WorkingCustomization.RobeDyeIndex; },
								[this](int32 V) { WorkingCustomization.RobeDyeIndex = V; },
								RobeDyeNames, 8)
						]

						// 7. Trim Metal
						+ SScrollBox::Slot()
						[
							MakeCycleRow(
								FText::FromString(TEXT("МЕТАЛЛ ОТДЕЛКИ")),
								[this]() { return WorkingCustomization.TrimMetalIndex; },
								[this](int32 V) { WorkingCustomization.TrimMetalIndex = V; },
								TrimMetalNames, 4)
						]

						// 8. Rune Glow
						+ SScrollBox::Slot()
						[
							MakeCycleRow(
								FText::FromString(TEXT("СВЕЧЕНИЕ РУН")),
								[this]() { return WorkingCustomization.RuneGlowIndex; },
								[this](int32 V) { WorkingCustomization.RuneGlowIndex = V; },
								RuneGlowNames, 4)
						]

						// 9. Body Scale (0.85 .. 1.20)
						+ SScrollBox::Slot().Padding(0, 6, 0, 2)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock)
								.Text_Lambda([this]() {
									return FText::FromString(FString::Printf(TEXT("МАСШТАБ ТЕЛА: %.2fx"), WorkingCustomization.BodyScale));
								})
								.Font(MindFont(10, true))
								.ColorAndOpacity(GildedBronzeText)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
							[
								SNew(SSlider)
								.Style(&BronzeSliderStyle)
								.Value_Lambda([this]() {
									return FMath::Clamp((WorkingCustomization.BodyScale - 0.85f) / 0.35f, 0.0f, 1.0f);
								})
								.OnValueChanged_Lambda([this](float NormalizedVal) {
									WorkingCustomization.BodyScale = 0.85f + NormalizedVal * 0.35f;
									if (PC.IsValid()) PC->UI_UpdateCustomization(WorkingCustomization, false);
								})
								.OnMouseCaptureEnd_Lambda([this]() {
									if (PC.IsValid()) PC->UI_UpdateCustomization(WorkingCustomization, true);
								})
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 2, 0)
								[
									SNew(SButton).ButtonStyle(&StoneBtnStyle)
									.OnClicked_Lambda([this]() {
										WorkingCustomization.BodyScale = 0.90f;
										if (PC.IsValid()) PC->UI_UpdateCustomization(WorkingCustomization);
										return FReply::Handled();
									})
									[ SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("0.90x"))).Font(MindFont(9, true)).ColorAndOpacity(BoneParchmentText) ]
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 2, 0)
								[
									SNew(SButton).ButtonStyle(&StoneBtnStyle)
									.OnClicked_Lambda([this]() {
										WorkingCustomization.BodyScale = 1.00f;
										if (PC.IsValid()) PC->UI_UpdateCustomization(WorkingCustomization);
										return FReply::Handled();
									})
									[ SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("1.00x"))).Font(MindFont(9, true)).ColorAndOpacity(BoneParchmentText) ]
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 2, 0)
								[
									SNew(SButton).ButtonStyle(&StoneBtnStyle)
									.OnClicked_Lambda([this]() {
										WorkingCustomization.BodyScale = 1.10f;
										if (PC.IsValid()) PC->UI_UpdateCustomization(WorkingCustomization);
										return FReply::Handled();
									})
									[ SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("1.10x"))).Font(MindFont(9, true)).ColorAndOpacity(BoneParchmentText) ]
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f)
								[
									SNew(SButton).ButtonStyle(&StoneBtnStyle)
									.OnClicked_Lambda([this]() {
										WorkingCustomization.BodyScale = 1.20f;
										if (PC.IsValid()) PC->UI_UpdateCustomization(WorkingCustomization);
										return FReply::Handled();
									})
									[ SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("1.20x"))).Font(MindFont(9, true)).ColorAndOpacity(BoneParchmentText) ]
								]
							]
						]
						+ SScrollBox::Slot().Padding(0, 0, 0, 32.0f)
						[
							SNew(SSpacer)
						]
					]

					// Bottom Action Buttons
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.2f).Padding(0, 0, 4, 0)
						[
							SNew(SButton)
							.ButtonStyle(&CrimsonBtnStyle)
							.OnClicked_Lambda([this]() {
								if (PC.IsValid())
								{
									PC->UI_UpdateCustomization(WorkingCustomization);
									UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_UIClick"), 0.8f);
								}
								return FReply::Handled();
							})
							[
								SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("СОХРАНИТЬ ОБЛИК"))).Font(MindFont(10, true)).ColorAndOpacity(FLinearColor(0.98f, 0.90f, 0.74f, 1.0f))
							]
						]
						+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(0, 0, 4, 0)
						[
							SNew(SButton)
							.ButtonStyle(&StoneBtnStyle)
							.OnClicked_Lambda([this]() {
								WorkingCustomization = FAetheriaCharacterCustomization();
								if (PC.IsValid())
								{
									PC->UI_UpdateCustomization(WorkingCustomization);
									UAetheriaMaterialLibrary::PlayGameSound2D(PC.Get(), TEXT("SFX_UIClick"), 0.6f);
								}
								return FReply::Handled();
							})
							[
								SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("СБРОС"))).Font(MindFont(10, true)).ColorAndOpacity(BoneParchmentText)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(SButton)
							.ButtonStyle(&StoneBtnStyle)
							.ButtonColorAndOpacity(FLinearColor(0.95f, 0.55f, 0.55f, 1.0f))
							.OnClicked_Lambda([this]() {
								ToggleCharacterEditor();
								return FReply::Handled();
							})
							[
								SNew(STextBlock).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("ЗАКРЫТЬ [C]"))).Font(MindFont(10, true)).ColorAndOpacity(EmberCrimsonText)
							]
					]
				]
			]
		];
}

FText SAetheriaMainOverlay::GetLocationHeaderText() const
{
	if (PC.IsValid() && PC->GetWorld())
	{
		if (const AAetheriaGameState* GS = PC->GetWorld()->GetGameState<AAetheriaGameState>())
		{
			FString PhaseLabel;
			switch (GS->CurrentWavePhase)
			{
			case EAetheriaWavePhase::SanctuaryGrace:
				PhaseLabel = TEXT("[Покой]");
				break;
			case EAetheriaWavePhase::ScoutSkirmish:
				PhaseLabel = TEXT("[Разведка]");
				break;
			case EAetheriaWavePhase::TacticalEscalation:
				PhaseLabel = TEXT("[Натиск]");
				break;
			case EAetheriaWavePhase::ClimaxSiege:
				PhaseLabel = TEXT("[Осада]");
				break;
			case EAetheriaWavePhase::Breather:
				PhaseLabel = TEXT("[Передышка]");
				break;
			default:
				PhaseLabel = TEXT("[Покой]");
				break;
			}

			FString HighGroundTag;
			if (const AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
			{
				if (Wiz->bOnHighGround)
				{
					HighGroundTag = TEXT("  [ВЫСОТА +15%]");
				}
			}

			EAetheriaWorldId Wid = EAetheriaWorldId::Bastion;
			if (const AAetheriaGameMode* GM = PC->GetWorld()->GetAuthGameMode<AAetheriaGameMode>())
			{
				Wid = GM->GetActiveWorld();
			}
			const TCHAR* Place = AetheriaWorldDirector::WorldDisplayName(Wid);
			const TCHAR* Boss = AetheriaWorldDirector::BossHeroForWorld(Wid);
			return FText::FromString(FString::Printf(TEXT("%s  ·  %s  ·  Акт %d / 5  ·  %s%s"),
				Place, Boss, GS->CurrentAct, *PhaseLabel, *HighGroundTag));
		}
	}
	return FText::FromString(TEXT("Бастион Аотера  ·  Aurora"));
}

FText SAetheriaMainOverlay::GetQuestObjectiveText() const
{
	if (PC.IsValid() && PC->GetWorld())
	{
		if (const AAetheriaGameState* GS = PC->GetWorld()->GetGameState<AAetheriaGameState>())
		{
			if (!GS->ActiveAmbushAlert.IsEmpty())
			{
				return FText::FromString(GS->ActiveAmbushAlert);
			}
			if (!GS->ActiveSynergyBanner.IsEmpty())
			{
				return FText::FromString(GS->ActiveSynergyBanner);
			}
			return FText::FromString(GS->ActDescription);
		}
	}
	return FText::GetEmpty();
}

TOptional<float> SAetheriaMainOverlay::GetQuestProgressPercent() const
{
	if (PC.IsValid() && PC->GetWorld())
	{
		if (const AAetheriaGameState* GS = PC->GetWorld()->GetGameState<AAetheriaGameState>())
		{
			return FMath::Clamp(GS->ActProgress / 100.0f, 0.0f, 1.0f);
		}
	}
	return 0.0f;
}

EVisibility SAetheriaMainOverlay::GetBossBarVisibility() const
{
	if (PC.IsValid() && PC->GetWorld())
	{
		if (const AAetheriaGameState* GS = PC->GetWorld()->GetGameState<AAetheriaGameState>())
		{
			return (GS->bBossSpawned && !GS->bBossDefeated) ? EVisibility::Visible : EVisibility::Collapsed;
		}
	}
	return EVisibility::Collapsed;
}

FText SAetheriaMainOverlay::GetBossTitleText() const
{
	if (PC.IsValid() && PC->GetWorld())
	{
		if (const AAetheriaGameState* GS = PC->GetWorld()->GetGameState<AAetheriaGameState>())
		{
			FString OrbsStr;
			for (EAetheriaOrb O : GS->BossOrbs)
			{
				OrbsStr += AetheriaMindColors::GetOrbLetter(O);
			}
			if (!OrbsStr.IsEmpty())
			{
				return FText::FromString(FString::Printf(TEXT("%s  [%s]  (%.0f / %.0f)"), *GS->BossName, *OrbsStr, GS->BossHP, GS->BossMaxHP));
			}
			return FText::FromString(FString::Printf(TEXT("%s  (%.0f / %.0f)"), *GS->BossName, GS->BossHP, GS->BossMaxHP));
		}
	}
	return FText::GetEmpty();
}

TOptional<float> SAetheriaMainOverlay::GetBossHPPercent() const
{
	if (PC.IsValid() && PC->GetWorld())
	{
		if (const AAetheriaGameState* GS = PC->GetWorld()->GetGameState<AAetheriaGameState>())
		{
			return GS->GetBossHPPercent();
		}
	}
	return 0.0f;
}

EVisibility SAetheriaMainOverlay::GetPortalBannerVisibility() const
{
	if (PC.IsValid() && PC->GetWorld())
	{
		if (const AAetheriaGameState* GS = PC->GetWorld()->GetGameState<AAetheriaGameState>())
		{
			return GS->bPortalOpen ? EVisibility::Visible : EVisibility::Collapsed;
		}
	}
	return EVisibility::Collapsed;
}

FText SAetheriaMainOverlay::GetPartyRosterText() const
{
	if (!PC.IsValid() || !PC->GetWorld()) return FText::GetEmpty();

	FString Lines;
	for (TActorIterator<AAetheriaWizardCharacter> It(PC->GetWorld()); It; ++It)
	{
		const AAetheriaWizardCharacter* Wiz = *It;
		if (!Wiz) continue;

		FString OrbsStr;
		for (EAetheriaOrb O : Wiz->ActiveOrbs)
		{
			OrbsStr += AetheriaMindColors::GetOrbLetter(O);
		}
		Lines += FString::Printf(TEXT("%s  [%s]  HP %.0f/%.0f\n"), *Wiz->WizardName, *OrbsStr, Wiz->HP, Wiz->MaxHP);
	}
	return FText::FromString(Lines.TrimEnd());
}

FText SAetheriaMainOverlay::GetOrbTriadText() const
{
	if (PC.IsValid())
	{
		if (const AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
		{
			FString Str = TEXT("Сферы: ");
			for (EAetheriaOrb O : Wiz->ActiveOrbs)
			{
				Str += FString::Printf(TEXT("[%s] "), *AetheriaMindColors::GetOrbLetter(O));
			}
			return FText::FromString(Str.TrimEnd());
		}
	}
	return FText::FromString(TEXT("Сферы: [Q] [W] [E]"));
}

FText SAetheriaMainOverlay::GetNextSpellPreviewText() const
{
	if (PC.IsValid())
	{
		if (const AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
		{
			const FAetheriaSpellDef Def = UAetheriaSpellDatabase::ResolveSpellFromOrbs(Wiz->ActiveOrbs);
			return FText::FromString(FString::Printf(TEXT("-> %s [%s]"), *Def.DisplayName, *Def.ComboKey));
		}
	}
	return FText::GetEmpty();
}

const FSlateBrush* SAetheriaMainOverlay::GetSocketFrameBrush(int32 SlotIdx) const
{
	if (PC.IsValid() && PC->PreferredCastMode == EAetheriaCastMode::InvokeThenAim && PC->ArmedSlotIndex == SlotIdx)
	{
		if (SocketArmedBrush.IsValid())
		{
			return SocketArmedBrush.Get();
		}
	}
	if (SocketNormalBrush.IsValid())
	{
		return SocketNormalBrush.Get();
	}
	return FCoreStyle::Get().GetBrush("WhiteBrush");
}

FSlateColor SAetheriaMainOverlay::GetSocketTint(int32 SlotIdx) const
{
	if (PC.IsValid())
	{
		if (const AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
		{
			const EAetheriaSpellId Id = (SlotIdx == 0) ? Wiz->SlotD : Wiz->SlotF;
			if (Id == EAetheriaSpellId::None)
			{
				return FSlateColor(FLinearColor(0.08f, 0.08f, 0.11f, 0.45f));
			}
			if (Wiz->GetSlotCooldown(SlotIdx) > 0.1f)
			{
				return FSlateColor(FLinearColor(0.42f, 0.42f, 0.48f, 1.0f));
			}
			return FSlateColor(FLinearColor::White);
		}
	}
	return FSlateColor(FLinearColor(0.08f, 0.08f, 0.11f, 0.45f));
}

FText SAetheriaMainOverlay::GetSlotCooldownText(int32 SlotIdx) const
{
	if (PC.IsValid())
	{
		if (const AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
		{
			const float Cd = Wiz->GetSlotCooldown(SlotIdx);
			if (Cd > 0.1f)
			{
				return FText::FromString(FString::Printf(TEXT("%.1f"), Cd));
			}
		}
	}
	return FText::GetEmpty();
}

FText SAetheriaMainOverlay::GetXPText() const
{
	if (PC.IsValid())
	{
		if (const AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
		{
			const int32 NextXP = UAetheriaSaveGame::GetXPForNextLevel(Wiz->WizardLevel);
			return FText::FromString(FString::Printf(
				TEXT("Ур. %d  ·  XP %d/%d  ·  Эссенция %d"),
				Wiz->WizardLevel, Wiz->CurrentXP, NextXP, Wiz->ArcaneEssence));
		}
	}
	return FText::GetEmpty();
}

TOptional<float> SAetheriaMainOverlay::GetXPPercent() const
{
	if (PC.IsValid())
	{
		if (const AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
		{
			const int32 NextXP = UAetheriaSaveGame::GetXPForNextLevel(Wiz->WizardLevel);
			return FMath::Clamp(static_cast<float>(Wiz->CurrentXP) / static_cast<float>(FMath::Max(1, NextXP)), 0.0f, 1.0f);
		}
	}
	return 0.0f;
}

FText SAetheriaMainOverlay::GetSlotLabelText(int32 SlotIdx) const
{
	const TCHAR* KeyName = (SlotIdx == 0) ? TEXT("D") : TEXT("F");
	if (PC.IsValid())
	{
		if (const AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
		{
			const EAetheriaSpellId Id = (SlotIdx == 0) ? Wiz->SlotD : Wiz->SlotF;
			const float Cd = Wiz->GetSlotCooldown(SlotIdx);
			if (Id == EAetheriaSpellId::None)
			{
				return FText::FromString(FString::Printf(TEXT("[%s] Пусто"), KeyName));
			}
			const FAetheriaSpellDef Def = UAetheriaSpellDatabase::GetSpellDef(Id);
			const TCHAR* ArmedMark = (PC->ArmedSlotIndex == SlotIdx) ? TEXT(" [ПРИЦЕЛ]") : TEXT("");
			if (Cd > 0.1f)
			{
				return FText::FromString(FString::Printf(TEXT("[%s] %s (%.1fs)"), KeyName, *Def.DisplayName, Cd));
			}
			return FText::FromString(FString::Printf(TEXT("[%s] %s%s"), KeyName, *Def.DisplayName, ArmedMark));
		}
	}
	return FText::FromString(FString::Printf(TEXT("[%s]"), KeyName));
}

FText SAetheriaMainOverlay::GetVitalsText() const
{
	if (PC.IsValid())
	{
		if (const AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
		{
			const int32 NextXP = UAetheriaSaveGame::GetXPForNextLevel(Wiz->WizardLevel);
			return FText::FromString(FString::Printf(
				TEXT("Ур. %d (XP %d/%d) · Эссенция %d  |  HP %.0f/%.0f  |  MP %.0f/%.0f  |  Щит %.0f"),
				Wiz->WizardLevel, Wiz->CurrentXP, NextXP, Wiz->ArcaneEssence,
				Wiz->HP, Wiz->MaxHP, Wiz->Mana, Wiz->MaxMana, Wiz->Shield));
		}
	}
	return FText::GetEmpty();
}

TOptional<float> SAetheriaMainOverlay::GetHPPercent() const
{
	if (PC.IsValid())
	{
		if (const AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
		{
			return Wiz->MaxHP > 0.0f ? FMath::Clamp(Wiz->HP / Wiz->MaxHP, 0.0f, 1.0f) : 0.0f;
		}
	}
	return 1.0f;
}

TOptional<float> SAetheriaMainOverlay::GetManaPercent() const
{
	if (PC.IsValid())
	{
		if (const AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))
		{
			return Wiz->MaxMana > 0.0f ? FMath::Clamp(Wiz->Mana / Wiz->MaxMana, 0.0f, 1.0f) : 0.0f;
		}
	}
	return 1.0f;
}

FText SAetheriaMainOverlay::GetCastModePillText() const
{
	if (PC.IsValid())
	{
		return PC->PreferredCastMode == EAetheriaCastMode::InstantOnCombo
			? FText::FromString(TEXT("Режим: ПРЯМОЕ СОТВОРЕНИЕ"))
			: FText::FromString(TEXT("Режим: СЛОТЫ ЧАР (D/F -> ЛКМ)"));
	}
	return FText::GetEmpty();
}

void AAetheriaHUD::BeginPlay()
{
	Super::BeginPlay();

	if (GEngine)
	{
		if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
		{
			if (Settings->GetFullscreenMode() == EWindowMode::Windowed)
			{
				const FIntPoint DeskRes = Settings->GetDesktopResolution();
				if (DeskRes.X > 0 && DeskRes.Y > 0)
				{
					Settings->SetScreenResolution(DeskRes);
				}
				Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
				Settings->ApplySettings(false);
			}
		}
	}

	AAetheriaPlayerController* PC = Cast<AAetheriaPlayerController>(GetOwningPlayerController());
	if (PC && PC->IsLocalController() && GEngine && GEngine->GameViewport)
	{
		MainOverlay = SNew(SAetheriaMainOverlay).OwningController(PC);
		GEngine->GameViewport->AddViewportWidgetContent(MainOverlay.ToSharedRef(), 10);
		// Overlay defaults to the main menu. Sync with GameMode after travel;
		// GameMode::BeginPlay may already have tried to hide a not-yet-created HUD.
		if (const AAetheriaGameMode* GM = Cast<AAetheriaGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr))
		{
			MainOverlay->SetShowMainMenu(GM->IsInMainMenu());
			if (!GM->IsInMainMenu())
			{
				MainOverlay->SetShowLobby(false);
				MainOverlay->SetLoadingScreen(false, 1.0f, TEXT(""), TEXT(""));
			}
		}
	}
}

void AAetheriaHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MainOverlay.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(MainOverlay.ToSharedRef());
		MainOverlay.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void AAetheriaHUD::ShowLobby()
{
	if (MainOverlay.IsValid()) MainOverlay->SetShowLobby(true);
	if (AAetheriaWizardCharacter* W = Cast<AAetheriaWizardCharacter>(GetOwningPawn()))
	{
		W->SetLobbyCameraMode(true);
	}
}

void AAetheriaHUD::HideLobby()
{
	if (MainOverlay.IsValid()) MainOverlay->SetShowLobby(false);
	if (AAetheriaWizardCharacter* W = Cast<AAetheriaWizardCharacter>(GetOwningPawn()))
	{
		W->SetLobbyCameraMode(false);
	}
}

void AAetheriaHUD::ToggleGrimoire()
{
	if (MainOverlay.IsValid()) MainOverlay->ToggleGrimoire();
}

void AAetheriaHUD::ToggleWorldMap()
{
	if (MainOverlay.IsValid()) MainOverlay->ToggleWorldMap();
}

void AAetheriaHUD::ToggleSettings()
{
	if (MainOverlay.IsValid()) MainOverlay->ToggleSettings();
}

void AAetheriaHUD::ToggleCharacterEditor()
{
	if (MainOverlay.IsValid()) MainOverlay->ToggleCharacterEditor();
}

bool AAetheriaHUD::IsAnyModalOpen() const
{
	return MainOverlay.IsValid() && MainOverlay->IsAnyModalOpen();
}


void SAetheriaMainOverlay::SetShowMainMenu(bool bVisible)
{
	bShowMainMenu = bVisible;
	if (bVisible)
	{
		bShowLobby = false;
		bShowLoading = false;
	}
}

void SAetheriaMainOverlay::SetShowWorldSelect(bool bVisible)
{
	bShowWorldSelect = bVisible;
}

void SAetheriaMainOverlay::SetLoadingScreen(bool bVisible, float Progress, const FString& Title, const FString& Tip)
{
	bShowLoading = bVisible;
	LoadingProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
	LoadingTitle = Title;
	LoadingTip = Tip;
}


void AAetheriaHUD::ShowMainMenu()
{
	if (MainOverlay.IsValid())
	{
		MainOverlay->SetShowMainMenu(true);
		MainOverlay->SetLoadingScreen(false, 0.0f, TEXT(""), TEXT(""));
		MainOverlay->SetShowLobby(false);
	}
}

void AAetheriaHUD::SetShowMainMenu(bool bVisible)
{
	if (MainOverlay.IsValid()) MainOverlay->SetShowMainMenu(bVisible);
}

void AAetheriaHUD::SetLoadingScreen(bool bVisible, float Progress, const FString& Title, const FString& Tip)
{
	if (MainOverlay.IsValid()) MainOverlay->SetLoadingScreen(bVisible, Progress, Title, Tip);
}
