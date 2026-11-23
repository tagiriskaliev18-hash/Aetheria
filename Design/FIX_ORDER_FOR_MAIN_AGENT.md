# ТЗ на исправление замечаний аудита AETHERIA (P0 → P2)

**Источник:** Агент-оценщик (`agy-evaluator`), аудит от 16.11.2026  
**Полный отчёт с разбором:** [AUDIT_Diablo_Magicka_2026-11-16.md](file:///c:/Users/user/Desktop/WizardGame/Design/AUDIT_Diablo_Magicka_2026-11-16.md)  
**Важное предупреждение перед началом правок:**
1. Сначала завершите/выдайте запрошенную Тагиром пробную сборку и сделайте бэкап текущего состояния перед правками C++ кода.
2. Параллельный агент игровой логики (`32b16ce5-cce9-48eb-9f7c-d7bba29e21be`) недавно вносил правки в `Source/Aetheria/` и `_backup_web_prototype/`. **Обязательно перечитайте актуальные файлы с диска и синхронизируйтесь с ним перед редактированием.**

---

## P0 — Критические баги, эксплойты и сломанный цикл игры (исправить в первую очередь)

### [P0-1] Полный обход кулдаунов ультимейтов игроками и ботами
- **Где в коде:**
  - [AetheriaWizardCharacter.h](file:///c:/Users/user/Desktop/WizardGame/Source/Aetheria/Public/AetheriaWizardCharacter.h)
  - [AetheriaWizardCharacter.cpp](file:///c:/Users/user/Desktop/WizardGame/Source/Aetheria/Private/AetheriaWizardCharacter.cpp#L919-L980)
  - [AetheriaBotAIController.cpp](file:///c:/Users/user/Desktop/WizardGame/Source/Aetheria/Private/AetheriaBotAIController.cpp#L267-L283)
- **Суть проблемы:**
  1. `ServerInvoke_Implementation` (`L926-933`) хранит КД только на слотах `SlotD`/`SlotF` и обнуляет `SlotDCooldownRemaining = 0.0f` при смене заклинания в слоте. Ротация `WWW -> R -> D -> QQQ -> R -> WWW -> R -> D` позволяет спамить `Chronosphere` каждые `0.5 с`.
  2. `ServerInstantCastCombo_Implementation` (`L965-980`) вообще не проверяет кулдаун спелла — каст ограничен только маной.
  3. `AAetheriaBotAIController::SynthesizeAndCastSpell` (`L281-282`) вызывает `ExecuteSpellServer(SpellId, TargetLocation)` напрямую, минуя списание маны и установку КД.
- **Что сделать:**
  - Хранить кулдауны на уровне `EAetheriaSpellId` (реплицируемый `TArray<float> SpellCooldowns` размера `11`, индексируемый по `static_cast<int32>(SpellId)`, с `DOREPLIFETIME`), тикать все элементы в `Tick`.
  - В `ServerInvoke_Implementation` при перемещении спелла в слот `D`/`F` читать его реальный остаток КД из `SpellCooldowns`, а не сбрасывать в `0.0f`.
  - В `ServerInstantCastCombo_Implementation` проверять `SpellCooldowns[Idx] <= 0.0f` до списания маны и выставлять `SpellCooldowns[Idx] = Resolved.Cooldown`.
  - В `BotAIController::SynthesizeAndCastSpell` использовать стандартный серверный каст со списанием маны и постановкой КД на `SpellId`.

### [P0-2] Ваншот боссов (`35% MaxHP`) и цепная рекурсия в `Thermal Shock`
- **Где в коде:**
  - [AetheriaMobCharacter.cpp](file:///c:/Users/user/Desktop/WizardGame/Source/Aetheria/Private/AetheriaMobCharacter.cpp#L243-L267)
  - [AetheriaMobCharacter.cpp](file:///c:/Users/user/Desktop/WizardGame/Source/Aetheria/Private/AetheriaMobCharacter.cpp#L496-L520)
  - [AetheriaMobCharacter.cpp](file:///c:/Users/user/Desktop/WizardGame/Source/Aetheria/Private/AetheriaMobCharacter.cpp#L632-L656)
- **Суть проблемы:**
  1. В `SetChronoFrozen(true)` (`L635`) при заморозке горящего врага вычисляется `const float ThermalShockDmg = MaxHP * 0.35f;` — финальному боссу на `22 000 HP` сносится **7 700 HP** одним ударом.
  2. Во всех трёх ветках `Thermal Shock` (`L264`, `L517`, `L653`) в `OtherMob->ApplySpellDamage(...)` передаётся `bIgnite = true`, что запускает цепную реакцию взрывов по соседним замороженным мобам.
- **Что сделать:**
  - Заменить `MaxHP * 0.35f` в `SetChronoFrozen(true)` на урон от базовой силы заклинания (например, `180.0f * 0.35f` или скалирование от `LastIgniteDamage` с жёстким капом для `EAetheriaMobKind::Boss`).
  - Передавать `bIgnite = false` во вторичных `OverlapMultiByChannel`-ударах `Thermal Shock`, чтобы не вызывать каскадную рекурсию.

### [P0-3] Настоящий ARPG-лут, скейл уровней и экономика `ArcaneEssence` (Diablo-слой)
- **Где в коде:**
  - [LootTables.json](file:///c:/Users/user/Desktop/WizardGame/Data/LootTables.json)
  - [AetheriaGameMode.cpp](file:///c:/Users/user/Desktop/WizardGame/Source/Aetheria/Private/AetheriaGameMode.cpp#L1000-L1060)
  - [AetheriaSaveGame.h](file:///c:/Users/user/Desktop/WizardGame/Source/Aetheria/Public/AetheriaSaveGame.h#L33)
  - [AetheriaWizardCharacter.cpp](file:///c:/Users/user/Desktop/WizardGame/Source/Aetheria/Private/AetheriaWizardCharacter.cpp#L1478-L1514)
- **Суть проблемы:**
  - `Data/LootTables.json` в C++ не парсится (используется захардкоженный массив строк в `AetheriaGameMode.cpp:1019-1025`).
  - `EquippedRelicNames` в `AetheriaSaveGame.h:33` нигде не заполняется и не читается; бонусы реликвий (`+12% Игнис`, `-10% кд Хроносферы` и т.д.) никак не влияют на статы мага.
  - `AwardProgression` даёт только `+18 MaxHP` за уровень без скейла урона; за 10 локаций набирается ~10-й уровень из 30; для `ArcaneEssence` нет геймплейной точки траты (Sink).
  - `static int32 LootPityCounter` (`AetheriaGameMode.cpp:1016`) и `static int32 SpellCycle` (`AetheriaMobCharacter.cpp:1468`) объявлены как глобальные `static` переменные.
- **Что сделать:**
  - Подключить парсинг `Data/LootTables.json` через `FJsonSerializer` (с fallback и копированием в `Content/Data/` для упакованного билда).
  - Сохранять полученные реликвии в `EquippedRelicNames` / активных слотах мага и **реально применять их модификаторы** в расчётах урона `Ignis`, КД `Chronosphere`, радиуса `BlackHole`, урона расщепления `PrismWall`, `MaxMana` и общего урона стихий.
  - Добавить скейл урона/маны от `WizardLevel` (либо пассивные ранги за уровень) и Святилище/Кузню в игре для траты `ArcaneEssence` на улучшение заклинаний/реликвий.
  - Перенести `LootPityCounter` в поле `AAetheriaGameMode` и `SpellCycle` в поле `AAetheriaMobCharacter`.

### [P0-4] Цена смерти, фляги HP и кооп-воскрешение (Diablo + Magicka)
- **Где в коде:**
  - [AetheriaGameMode.cpp](file:///c:/Users/user/Desktop/WizardGame/Source/Aetheria/Private/AetheriaGameMode.cpp#L433-L466)
- **Суть проблемы:**
  - Мёртвые маги автоматически воскресают каждые `3.0 с` с полными HP и маной. Невозможно проиграть ни один бой; нет нужды поднимать союзников.
  - Нет фляг здоровья или выпадающих сфер исцеления (`Health Globes`).
- **Что сделать:**
  - Убрать безусловный бесплатный авто-респаун в бою. Добавить условие `Party Wipe` (если все игроки/боты мертвы — откат/рестарт текущего акта или босса).
  - Добавить ручное воскрешение павшего союзника (удержание рядом или специальный ритуал/каст в духе *Magicka*).
  - Добавить заряжаемую флягу исцеления (например, клавиша `H` / `1`, заряжается от убийства элитников) или выпадающие сферы здоровья.

---

## P1 — Геймплейное сближение с *Magicka* и *Diablo* + сетевой полишинг

1. **Механики *Magicka* для коопа:**
   - Опциональная настройка лобби **Friendly Fire (`0% / 35% / 100%`)** (включая урон/отбрасывание союзников и самоподрывы, когда этот режим выбран).
   - **Скрещивание лучей (`Beam Crossing`):** при пересечении двух `SupernovaBeam` (`QQW`) разных магов в коопе — создание нестабильного резонансного ядра со взрывом (ключевая фишка Magicka).
   - Модификатор **Self-Cast (`Alt` / `СКМ`)** для применения заклинания на себя / вокруг себя и стихийные статусы на самих магах (`Wet` / `Burning` / `Chilled`).
2. **Механики *Diablo* в уровнях и боссах:**
   - Вместо одной плоской круглой арены с вращением на `Index * 37°` (`AetheriaBiomeDatabase.cpp:48-133`) — разбить локацию на 2–3 связных зала/коридора с воротами и мини-исследованием.
   - Дать каждому из 10 боссов хотя бы 1 уникальную фазовую механику помимо общего `ExecuteBossSlam` + `ExecuteBossOrbSpell` (`AetheriaMobCharacter.cpp:1434-1520`).
3. **Сеть и звук:**
   - Озвучить `SFX_Invoke` через `Multicast` / `OnRep` (сейчас звук вызывается только под `IsLocallyControlled()` на сервере в `AetheriaWizardCharacter.cpp:935-938`, поэтому удалённые клиенты его не слышат).
   - Синхронизировать изменения логики с `_instant_exe_preview` (или переключить лаунчер на нативный `Build\Windows\Aetheria.exe`, когда он будет готов).

---

## P2 — Визуал и импакт уровня *Diablo IV* / *Mind Critic*

1. **Заполнение 36 целевых меш-слотов (`0/36 ready` по `verify_balance_and_loot.py`) и физика смертей:**
   - Импортировать все модели героев (`SM_Hero_00..09`), боссов (`SM_Boss_Loc00..09`) и мобов (`SM_Mob_*`) со скелетом/анимациями.
   - Заменить мгновенное исчезновение моба через `Destroy()` (`AetheriaMobCharacter.cpp:1549`) на задержку смерти с шейдером распада в пепел (`Dissolve`) / отлетом осколков.
   - Добавить `Camera Shake` и короткий `Hitstop` (`30–50 мс`) на тяжёлых кастах (`ChaosMeteor`, `PrimalOverload`, `Boss Stagger Break`).
2. **Устранение блокаут-примитивов (по рендерам LookDev):**
   - В [LookDev_Diablo_Check.png](file:///c:/Users/user/Desktop/WizardGame/RawAssets/LookDev_Diablo_Check.png): заменить кубо-сферический алтарь на полноценную скульптурную геометрию и поставить Niagara-огонь в жаровнях вместо белых сфер.
   - В [LookDev_HollowMagus_3070.png](file:///c:/Users/user/Desktop/WizardGame/RawAssets/LookDev_HollowMagus_3070.png): заменить примитивную геометрию фамильяра/окружения и поднять читаемость силуэтов контровым светом.
   - В [SpellPlate_01.png](file:///c:/Users/user/Desktop/WizardGame/RawAssets/Grimoire/SpellPlate_01.png)–`10`: перерисовать иллюстрации Гримуара из простых кругов Pillow в офортные гравюры.
