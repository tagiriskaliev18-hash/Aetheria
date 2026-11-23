#include "AetheriaSpellDatabase.h"

namespace
{
	TArray<FAetheriaSpellDef> BuildAllSpells()
	{
		TArray<FAetheriaSpellDef> List;
		List.Reserve(16);

		auto MakeOrbs = [](EAetheriaOrb A, EAetheriaOrb B, EAetheriaOrb C) -> TArray<EAetheriaOrb>
		{
			return { A, B, C };
		};

		// 1. QQQ - Solar Cataclysm Fireball
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::Fireball;
			S.ComboKey = TEXT("QQQ");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Ignis, EAetheriaOrb::Ignis, EAetheriaOrb::Ignis);
			S.DisplayName = TEXT("Шар Солнечного Пламени");
			S.EnglishName = TEXT("Заклинание Огня и Пепла");
			S.Description = TEXT("Сгусток первородного солнечного пламени в бушующей короне пепла. Разрывается ударной волной, отбрасывает порождения Бездны и обращает их плоть в угли.");
			S.SynergyHint = TEXT("Проходя сквозь Заслон Преломления (WEE), сгусток дробится на три огненных снаряда. Поглощённый Пастью Бездны (EEE), он воспламеняет Солнечную Буру.");
			S.TargetShape = EAetheriaTargetShape::Direction;
			S.Cooldown = 2.8f;
			S.ManaCost = 18.0f;
			S.RangeUU = 3400.0f;
			S.RadiusUU = 480.0f;
			S.Duration = 2.4f;
			S.BaseDamage = 150.0f;
			S.CastDelay = 0.0f;
			S.PrimaryColor = AetheriaMindColors::IgnisPlasma;
			S.AccentColor = FLinearColor(1.0f, 0.45f, 0.08f, 1.0f);
			List.Add(S);
		}

		// 2. EEE - Abyssal Maw Singularity
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::BlackHole;
			S.ComboKey = TEXT("EEE");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Graviton, EAetheriaOrb::Graviton, EAetheriaOrb::Graviton);
			S.DisplayName = TEXT("Разверстая Пасть Бездны");
			S.EnglishName = TEXT("Таинство Вечной Пустоты");
			S.Description = TEXT("Разрывает ткань мироздания, пробуждая прожорливую воронку Бездны в пылающем кольце пепла. Стягивает врагов и обломки камня по спирали, затем рушится сокрушительным взрывом.");
			S.SynergyHint = TEXT("Удерживает скопище врагов под удар Падающей Звезды (QQE) или воспламеняется Шаром Солнечного Пламени (QQQ).");
			S.TargetShape = EAetheriaTargetShape::PointAoE;
			S.Cooldown = 10.5f;
			S.ManaCost = 40.0f;
			S.RangeUU = 2600.0f;
			S.RadiusUU = 460.0f;
			S.Duration = 4.5f;
			S.BaseDamage = 48.0f;
			S.CastDelay = 0.0f;
			S.PrimaryColor = AetheriaMindColors::MindViolet;
			S.AccentColor = AetheriaMindColors::MindCyan;
			List.Add(S);
		}

		// 3. WWW - Dome of Timelessness
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::Chronosphere;
			S.ComboKey = TEXT("WWW");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Tempest, EAetheriaOrb::Tempest, EAetheriaOrb::Tempest);
			S.DisplayName = TEXT("Купол Вечного Безвременья");
			S.EnglishName = TEXT("Печать Остановки Мига");
			S.Description = TEXT("Запечатывает участок битвы прозрачным завесом безвременья: тварей и их чары сковывает вечный миг, а союзные маги обретают стремительность порыва.");
			S.SynergyHint = TEXT("Застывшие во времени враги уязвимы ко всем стихиям (+25% урона), а Ветвь Небесной Грозы (QWW) поражает их с резонансом +40%.");
			S.TargetShape = EAetheriaTargetShape::PointAoE;
			S.Cooldown = 11.5f;
			S.ManaCost = 42.0f;
			S.RangeUU = 2600.0f;
			S.RadiusUU = 550.0f;
			S.Duration = 4.5f;
			S.BaseDamage = 35.0f;
			S.CastDelay = 0.0f;
			S.PrimaryColor = AetheriaMindColors::MindCyan;
			S.AccentColor = AetheriaMindColors::MindIndigo;
			List.Add(S);
		}

		// 4. QQE - Fallen Star
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::ChaosMeteor;
			S.ComboKey = TEXT("QQE");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Ignis, EAetheriaOrb::Ignis, EAetheriaOrb::Graviton);
			S.DisplayName = TEXT("Кара Падающей Звезды");
			S.EnglishName = TEXT("Обряд Небесного Гнева");
			S.Description = TEXT("Рассекает небесную твердь и обрушивает раскалённую каменную глыбу. С грохотом падения она катится вперёд сквозь строй врагов, выжигая землю текучей магмой.");
			S.SynergyHint = TEXT("Направьте горящую глыбу в пленников Купола Безвременья (WWW) или в Пасть Бездны (EEE).");
			S.TargetShape = EAetheriaTargetShape::Direction;
			S.Cooldown = 7.0f;
			S.ManaCost = 32.0f;
			S.RangeUU = 2600.0f;
			S.RadiusUU = 420.0f;
			S.Duration = 3.0f;
			S.BaseDamage = 185.0f;
			S.CastDelay = 0.4f;
			S.PrimaryColor = AetheriaMindColors::IgnisPlasma;
			S.AccentColor = AetheriaMindColors::MindViolet;
			List.Add(S);
		}

		// 5. QWW - Sky Storm Branch
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::ChainLightning;
			S.ComboKey = TEXT("QWW");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Ignis, EAetheriaOrb::Tempest, EAetheriaOrb::Tempest);
			S.DisplayName = TEXT("Ветвь Небесной Грозы");
			S.EnglishName = TEXT("Чары Бури и Молний");
			S.Description = TEXT("Срывает с кончиков пальцев белую ветвь небесного гнева: молния перескакивает по восьми ближайшим целям, сжигая кровь и оглушая в миг удара.");
			S.SynergyHint = TEXT("Поражает застывших в Куполе Безвременья (WWW) врагов с ослепляющим резонансом (+40% урона).");
			S.TargetShape = EAetheriaTargetShape::Direction;
			S.Cooldown = 3.0f;
			S.ManaCost = 20.0f;
			S.RangeUU = 2800.0f;
			S.RadiusUU = 1000.0f;
			S.Duration = 0.6f;
			S.BaseDamage = 135.0f;
			S.CastDelay = 0.0f;
			S.PrimaryColor = AetheriaMindColors::MindCyan;
			S.AccentColor = AetheriaMindColors::CrispAlabaster;
			List.Add(S);
		}

		// 6. QQW - Spear of Sear Light
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::SupernovaBeam;
			S.ComboKey = TEXT("QQW");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Ignis, EAetheriaOrb::Ignis, EAetheriaOrb::Tempest);
			S.DisplayName = TEXT("Копьё Испепеляющего Света");
			S.EnglishName = TEXT("Лучевой Поток Солнца");
			S.Description = TEXT("Пронзает мрак арены испепеляющим потоком витого солнечного огня, прожигая насквозь плоть и броню всех врагов на линии взгляда.");
			S.SynergyHint = TEXT("Идеально сметает толпу в Пасти Бездны (EEE). Скрестите два потока в битве ради ослепляющего взрыва!");
			S.TargetShape = EAetheriaTargetShape::Direction;
			S.Cooldown = 4.8f;
			S.ManaCost = 26.0f;
			S.RangeUU = 2600.0f;
			S.RadiusUU = 240.0f;
			S.Duration = 0.95f;
			S.BaseDamage = 220.0f;
			S.CastDelay = 0.0f;
			S.PrimaryColor = FLinearColor(1.0f, 0.25f, 0.04f, 1.0f);
			S.AccentColor = AetheriaMindColors::MindCyan;
			List.Add(S);
		}

		// 7. WWE - Astral Stride
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::ChronoBlink;
			S.ComboKey = TEXT("WWE");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Tempest, EAetheriaOrb::Tempest, EAetheriaOrb::Graviton);
			S.DisplayName = TEXT("Астральный Шаг и Морок");
			S.EnglishName = TEXT("Покров Призрачного Сдвига");
			S.Description = TEXT("Рассеивает тело мага в вихре ветра и мгновенно переносит к цели. На старом месте остаётся морок, приманивающий и разрывающий преследователей.");
			S.SynergyHint = TEXT("Уводит из-под смертоносного удара хранителей разлома и открывает путь к дальним маякам.");
			S.TargetShape = EAetheriaTargetShape::PointAoE;
			S.Cooldown = 4.2f;
			S.ManaCost = 16.0f;
			S.RangeUU = 1600.0f;
			S.RadiusUU = 550.0f;
			S.Duration = 0.7f;
			S.BaseDamage = 125.0f;
			S.CastDelay = 0.0f;
			S.PrimaryColor = AetheriaMindColors::MindCobalt;
			S.AccentColor = AetheriaMindColors::MindCyan;
			List.Add(S);
		}

		// 8. QEE - Shattering Earth Spires
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::GravityCataclysm;
			S.ComboKey = TEXT("QEE");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Ignis, EAetheriaOrb::Graviton, EAetheriaOrb::Graviton);
			S.DisplayName = TEXT("Разлом Земной Тверди");
			S.EnglishName = TEXT("Гнев Каменных Игл");
			S.Description = TEXT("Разламывает землю под ногами неприятеля, выбрасывая вверх зубчатые обсидиановые иглы. Подброшенные враги падают замертво или оглушаются.");
			S.SynergyHint = TEXT("Обездвиженные враги теряют строй и становятся лёгкой добычей для Копья Света (QQW).");
			S.TargetShape = EAetheriaTargetShape::PointAoE;
			S.Cooldown = 6.2f;
			S.ManaCost = 28.0f;
			S.RangeUU = 2500.0f;
			S.RadiusUU = 850.0f;
			S.Duration = 1.4f;
			S.BaseDamage = 170.0f;
			S.CastDelay = 0.2f;
			S.PrimaryColor = AetheriaMindColors::MindViolet;
			S.AccentColor = AetheriaMindColors::IgnisPlasma;
			List.Add(S);
		}

		// 9. WEE - Refraction Crystal Barrier
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::PrismWall;
			S.ComboKey = TEXT("WEE");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Tempest, EAetheriaOrb::Graviton, EAetheriaOrb::Graviton);
			S.DisplayName = TEXT("Заслон Хрустального Преломления");
			S.EnglishName = TEXT("Грань Льда и Пустоты");
			S.Description = TEXT("Воздвигает заслон из остроконечных кристаллов Бездны: ледяная преграда сковывает шаг врагов, гасит их выстрелы и преломляет заклинания союзников на три залпа.");
			S.SynergyHint = TEXT("Пошлите Шар Солнечного Пламени (QQQ) или чару посоха сквозь грани заслона, чтобы утроить залп!");
			S.TargetShape = EAetheriaTargetShape::LineWall;
			S.Cooldown = 5.8f;
			S.ManaCost = 24.0f;
			S.RangeUU = 2000.0f;
			S.RadiusUU = 450.0f;
			S.Duration = 6.5f;
			S.BaseDamage = 55.0f;
			S.CastDelay = 0.0f;
			S.PrimaryColor = AetheriaMindColors::MindIndigo;
			S.AccentColor = AetheriaMindColors::MindCyan;
			List.Add(S);
		}

		// 10. QWE - Triad Convergence
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::PrimalOverload;
			S.ComboKey = TEXT("QWE");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Ignis, EAetheriaOrb::Tempest, EAetheriaOrb::Graviton);
			S.DisplayName = TEXT("Вспышка Трёх Стихий");
			S.EnglishName = TEXT("Великий Обряд Триады");
			S.Description = TEXT("На миг затаив дыхание, маг сжимает пламя, бурю и пустоту в единое сердце и обрушивает вихрь вокруг себя, даруя союзникам щит и выпуская шесть самонаводящихся копий эфира.");
			S.SynergyHint = TEXT("Высший обряд защиты: спасает орден в плотном окружении и разметает волну нежити.");
			S.TargetShape = EAetheriaTargetShape::SelfNova;
			S.Cooldown = 8.5f;
			S.ManaCost = 36.0f;
			S.RangeUU = 1100.0f;
			S.RadiusUU = 1100.0f;
			S.Duration = 1.2f;
			S.BaseDamage = 250.0f;
			S.CastDelay = 0.35f;
			S.PrimaryColor = AetheriaMindColors::CrispAlabaster;
			S.AccentColor = AetheriaMindColors::MindViolet;
			List.Add(S);
		}

		// 11. QEQ - Macropyre
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::Macropyre;
			S.ComboKey = TEXT("QEQ");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Ignis, EAetheriaOrb::Graviton, EAetheriaOrb::Ignis);
			S.DisplayName = TEXT("Макропира: Река Пламени");
			S.EnglishName = TEXT("Запретный Катаклизм Недр");
			S.Description = TEXT("Прорезает каменный пол пылающей рекой жидкой магмы длиной 20м. Разрушает укрытия и наносит сокрушительный периодический ожог.");
			S.SynergyHint = TEXT("Сжигает застывших в Хроносфере (WWW) или втянутых в Пасть Бездны (EEE).");
			S.TargetShape = EAetheriaTargetShape::Direction;
			S.Cooldown = 9.5f;
			S.ManaCost = 40.0f;
			S.RangeUU = 2000.0f;
			S.RadiusUU = 360.0f;
			S.Duration = 6.5f;
			S.BaseDamage = 34.0f;
			S.CastDelay = 0.0f;
			S.PrimaryColor = FLinearColor(1.0f, 0.34f, 0.04f, 1.0f);
			S.AccentColor = FLinearColor(1.0f, 0.72f, 0.16f, 1.0f);
			List.Add(S);
		}

		// 12. WQW - Plasma Field
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::PlasmaField;
			S.ComboKey = TEXT("WQW");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Tempest, EAetheriaOrb::Ignis, EAetheriaOrb::Tempest);
			S.DisplayName = TEXT("Плазменное Кольцо Бури");
			S.EnglishName = TEXT("Зов Штормового Разряда");
			S.Description = TEXT("Кольцо грозовой плазмы расширяется до 11.5м и сжимается обратно к магу, дважды поражая и замедляя врагов (урон выше на краю!).");
			S.SynergyHint = TEXT("Наносит +40% резонансного урона по пленникам Хроносферы (WWW).");
			S.TargetShape = EAetheriaTargetShape::SelfNova;
			S.Cooldown = 6.8f;
			S.ManaCost = 32.0f;
			S.RangeUU = 1150.0f;
			S.RadiusUU = 1150.0f;
			S.Duration = 1.62f;
			S.BaseDamage = 195.0f;
			S.CastDelay = 0.0f;
			S.PrimaryColor = FLinearColor(0.14f, 0.90f, 1.0f, 1.0f);
			S.AccentColor = FLinearColor(0.45f, 0.36f, 1.0f, 1.0f);
			List.Add(S);
		}

		// 13. EQE - Cataclysmic Nova Explosion
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::CataclysmNova;
			S.ComboKey = TEXT("EQE");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Graviton, EAetheriaOrb::Ignis, EAetheriaOrb::Graviton);
			S.DisplayName = TEXT("Апокалиптический Сверхвзрыв");
			S.EnglishName = TEXT("Печать Термобарической Бездны");
			S.Description = TEXT("Гравитационная стяжка сменяется колоссальным взрывом в радиусе 9м: разрывает нежить, бросает обсидиановые осколки и отбрасывает толпу.");
			S.SynergyHint = TEXT("Ударная волна швыряет врагов в стены (Удар о Стену!) и сносит броню босса.");
			S.TargetShape = EAetheriaTargetShape::PointAoE;
			S.Cooldown = 12.0f;
			S.ManaCost = 48.0f;
			S.RangeUU = 2000.0f;
			S.RadiusUU = 900.0f;
			S.Duration = 1.4f;
			S.BaseDamage = 345.0f;
			S.CastDelay = 0.56f;
			S.PrimaryColor = FLinearColor(1.0f, 0.28f, 0.08f, 1.0f);
			S.AccentColor = FLinearColor(0.68f, 0.20f, 1.0f, 1.0f);
			List.Add(S);
		}

		// 14. QWQ - Sun Strike
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::SunStrike;
			S.ComboKey = TEXT("QWQ");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Ignis, EAetheriaOrb::Tempest, EAetheriaOrb::Ignis);
			S.DisplayName = TEXT("Небесный Удар Гелиоса");
			S.EnglishName = TEXT("Солнечный Орбитальный Завет");
			S.Description = TEXT("Через 0.95с в любую точку арены обрушивается ослепляющий столб небесного огня (+25% урона по одиночной цели).");
			S.SynergyHint = TEXT("Гарантированно попадает по врагам в Хроносфере (WWW).");
			S.TargetShape = EAetheriaTargetShape::PointAoE;
			S.Cooldown = 8.0f;
			S.ManaCost = 34.0f;
			S.RangeUU = 3200.0f;
			S.RadiusUU = 360.0f;
			S.Duration = 1.2f;
			S.BaseDamage = 310.0f;
			S.CastDelay = 0.95f;
			S.PrimaryColor = FLinearColor(1.0f, 0.78f, 0.20f, 1.0f);
			S.AccentColor = FLinearColor(1.0f, 0.42f, 0.08f, 1.0f);
			List.Add(S);
		}

		// 15. WEW - Eye of the Storm
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::EyeOfTheStorm;
			S.ComboKey = TEXT("WEW");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Tempest, EAetheriaOrb::Graviton, EAetheriaOrb::Tempest);
			S.DisplayName = TEXT("Око Грозового Шторма");
			S.EnglishName = TEXT("Венец Разъярённой Бури");
			S.Description = TEXT("Призывает над магом грозовую тучу на 6с, каждые 0.45с поражающую ближайших врагов вертикальными молниями.");
			S.SynergyHint = TEXT("Сочетается с мобильностью Рывка и Плазменным Кольцом (WQW).");
			S.TargetShape = EAetheriaTargetShape::SelfNova;
			S.Cooldown = 10.0f;
			S.ManaCost = 38.0f;
			S.RangeUU = 750.0f;
			S.RadiusUU = 750.0f;
			S.Duration = 6.0f;
			S.BaseDamage = 74.0f;
			S.CastDelay = 0.0f;
			S.PrimaryColor = FLinearColor(0.24f, 0.78f, 1.0f, 1.0f);
			S.AccentColor = FLinearColor(0.55f, 0.28f, 1.0f, 1.0f);
			List.Add(S);
		}

		// 16. EWE - Requiem of Shattered Souls
		{
			FAetheriaSpellDef S;
			S.SpellId = EAetheriaSpellId::RequiemBlast;
			S.ComboKey = TEXT("EWE");
			S.RequiredOrbs = MakeOrbs(EAetheriaOrb::Graviton, EAetheriaOrb::Tempest, EAetheriaOrb::Graviton);
			S.DisplayName = TEXT("Реквием Расколотых Душ");
			S.EnglishName = TEXT("Голос Глубокого Разлома");
			S.Description = TEXT("Выпускает 12 сокрушительных багрово-пустотных волн во все стороны на 13.5м, сметая окружение.");
			S.SynergyHint = TEXT("Максимальный урон в упор крупным боссам.");
			S.TargetShape = EAetheriaTargetShape::SelfNova;
			S.Cooldown = 11.0f;
			S.ManaCost = 44.0f;
			S.RangeUU = 1350.0f;
			S.RadiusUU = 1350.0f;
			S.Duration = 1.4f;
			S.BaseDamage = 255.0f;
			S.CastDelay = 0.60f;
			S.PrimaryColor = FLinearColor(0.86f, 0.18f, 0.48f, 1.0f);
			S.AccentColor = FLinearColor(0.58f, 0.22f, 1.0f, 1.0f);
			List.Add(S);
		}

		return List;
	}
}

FString UAetheriaSpellDatabase::NormalizeComboKey(const TArray<EAetheriaOrb>& Orbs)
{
	if (Orbs.Num() < 3)
	{
		return TEXT("QWE");
	}

	TArray<EAetheriaOrb> Sorted;
	Sorted.Add(Orbs[0]);
	Sorted.Add(Orbs[1]);
	Sorted.Add(Orbs[2]);
	Sorted.Sort([](EAetheriaOrb A, EAetheriaOrb B)
	{
		return static_cast<uint8>(A) < static_cast<uint8>(B);
	});

	return AetheriaMindColors::GetOrbLetter(Sorted[0])
		+ AetheriaMindColors::GetOrbLetter(Sorted[1])
		+ AetheriaMindColors::GetOrbLetter(Sorted[2]);
}

FAetheriaSpellDef UAetheriaSpellDatabase::ResolveSpellFromOrbs(const TArray<EAetheriaOrb>& Orbs)
{
	const TArray<FAetheriaSpellDef>& All = GetAllSpells();
	if (Orbs.Num() >= 3)
	{
		const FString ExactKey = AetheriaMindColors::GetOrbLetter(Orbs[0])
			+ AetheriaMindColors::GetOrbLetter(Orbs[1])
			+ AetheriaMindColors::GetOrbLetter(Orbs[2]);
		if (ExactKey == TEXT("QEQ") || ExactKey == TEXT("WQW") || ExactKey == TEXT("EQE") ||
			ExactKey == TEXT("QWQ") || ExactKey == TEXT("WEW") || ExactKey == TEXT("EWE"))
		{
			for (const FAetheriaSpellDef& Def : All)
			{
				if (Def.ComboKey == ExactKey)
				{
					return Def;
				}
			}
		}
	}
	const FString Key = NormalizeComboKey(Orbs);
	for (const FAetheriaSpellDef& Def : All)
	{
		if (Def.ComboKey == Key)
		{
			return Def;
		}
	}
	return GetSpellDef(EAetheriaSpellId::PrimalOverload);
}

FAetheriaSpellDef UAetheriaSpellDatabase::GetSpellDef(EAetheriaSpellId SpellId)
{
	const TArray<FAetheriaSpellDef>& All = GetAllSpells();
	for (const FAetheriaSpellDef& Def : All)
	{
		if (Def.SpellId == SpellId)
		{
			return Def;
		}
	}
	return All.Num() > 0 ? All[0] : FAetheriaSpellDef();
}

const TArray<FAetheriaSpellDef>& UAetheriaSpellDatabase::GetAllSpells()
{
	static const TArray<FAetheriaSpellDef> CachedSpells = BuildAllSpells();
	return CachedSpells;
}
