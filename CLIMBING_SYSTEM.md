# ThreadTales — Система вертикального паркура и скалолазания (Climbing System)

> **Статус документа:** Официальный архитектурный стандарт проекта ThreadTales.  
> **Расположение:** Корень проекта на ПК (`y:\UnrealProjects\ThreadTales\CLIMBING_SYSTEM.md`).  
> **Назначение:** Полная фиксация всех математических, физических, анимационных и сетевых правил скалолазания для разработчика и AI-агентов.  
> **Модульные версии для быстрого чтения:** См. специализированный каталог [Docs/Climbing/](file:///Y:/UnrealProjects/ThreadTales/Docs/Climbing/README.md) ([Detection.md](file:///Y:/UnrealProjects/ThreadTales/Docs/Climbing/Detection.md), [Kinematics.md](file:///Y:/UnrealProjects/ThreadTales/Docs/Climbing/Kinematics.md), [Traps_And_Fixes.md](file:///Y:/UnrealProjects/ThreadTales/Docs/Climbing/Traps_And_Fixes.md)).

---

## 1. Концепция и разделение ответственности

В игре совмещены две системы перемещения:
1. **GASPALS (Горизонтальный паркур):** Низкие препятствия (высота до 2.0–2.2 м). Персонаж преодолевает их на бегу без остановки (Vault, Hurdle, Mantle) через Motion Matching.
2. **Vertical Traversal / Climbing (Вертикальный паркур):** Высокие стены (> 2.2 м) или свободное падение в воздухе. Персонаж переходит в режим скалолазания: цепляется руками за уступ, зависает, может подтянуться наверх или спрыгнуть вниз.

### Базовый принцип: «Вариант А — где руки, там и хватает»
- `bRequireLedgeTag = false` по умолчанию.
- Персонаж может зацепиться за **любую** физическую геометрию мира (ящики, стены, парапеты, скалы), если она удовлетворяет геометрическим критериям доступности для рук.
- Тегирование уступов (`LedgeTag`) оставлено лишь как опциональный фильтр.

---

## 2. Математика и алгоритм детекта (Multi-Phase Edge Pipeline — FThreadTalesLedgeDetector)

Обычный одиночный луч не работает в динамичном прыжке. Детект уступов вынесен в выделенный класс `FThreadTalesLedgeDetector::DetectLedge` и построен по строгой 6-фазной схеме геометрического пересечения:

1. **Фаза 1 — Поиск стены (Forward Wall Sweep):**
   - Выполняется сферический свип радиусом `15 см` на расстояние `ForwardTraceDistance = 120 см` от высоты груди (`Z + 45 см`), дополненный многоуровневыми свипами по высоте.
   - Сфера гарантирует, что стена не потеряется из-за разницы высот в прыжке.
   - Проверяется угол подхода: скалярное произведение вектора взгляда и нормали стены должно быть в пределах конуса `MaxApproachAngle = 65°`.
   - Захват разрешён при вертикальной скорости `Velocity.Z <= MaxGrabVelocityZ (400 см/с)` — работает как на излёте прыжка, так и на его взлёте.
   - Дистанция до стены валидируется через `MaxGrabWallDistance = 55 см`, исключая телепортацию.

2. **Фаза 2 — Поиск верхней грани (Ledge Top Trace с Multi-Depth Inward Probing):**
   - Точки зондирования последовательно опускаются на глубинах `LedgeProbeDepths = { 5.0, 12.0, 20.0 } см` внутрь уступа от стены.
   - Трейс опускается строго вниз из наивысшей точки досягаемости рук:
     - Начало: `ActorLoc.Z + MaxGrabHeight` (`+155 см` относительно центра капсулы).
     - Конец: `ActorLoc.Z + MinGrabHeight - 10` (`0 см`).
   - Проверяется горизонтальность: `TopHit.ImpactNormal.Z >= 0.6` (нельзя повиснуть на крутом скате).
   - Фиксируется точная высота полки: `LedgeTopZ = TopHit.ImpactPoint.Z`.

3. **Фаза 3 — Засечка ребра стены (Pinpoint Edge Retrace):**
   - На высоте `LedgeTopZ - 10 см` (под пальцами) пускается короткий горизонтальный встречный луч в стену.
   - Это находит **истинную внешнюю плоскость стены** прямо под кромкой перегиба.

4. **Фаза 4 — Вычисление истинной 3D-кромки (`TrueLedgeEdge`):**
   - `TrueLedgeEdge = FVector(EdgeWallHit.ImpactPoint.X, EdgeWallHit.ImpactPoint.Y, LedgeTopZ)`
   - `X, Y` лежат **строго на внешней стене**.
   - `Z` лежит **строго на верхней плоскости**.
   - Это математически идеальная линия стыка вертикальной и горизонтальной граней.

5. **Фаза 5 — Проверка кромки и пространства для тела (Lip & Clearance Validation):**
   - Проверяется наличие сплошной стены под пальцами (`LipBelowWallTraceDepth = 6 см`).
   - Проверяется открытый воздух над уступом (`LipAboveAirTraceHeight = 6 см`) для отсечения узких щелей и модульных швов (`LipModularSeamRejectDistance = 15 см`).
   - Проверяется вместимость головы и торса персонажа сферой `BodyClearanceRadius = 20 см` на высоте `BodyClearanceHeight = 40 см` над уступом.

6. **Фаза 6 — Проверка ширины уступа и перекоса рук (Hands Placement & Slope):**
   - Вдоль касательной к стене проверяются точки левой и правой руки (`HandSpacing = 22 см`).
   - Если обе руки на уступе, проверяется перепад высот ладоней (`LedgeSlopeMaxZDifference = 25 см`).
   - Если одна рука свисает с угла узкого столба, захват разрешается по эвристике «зацепился одной рукой».

---

## 3. Позиционирование и анкоринг персонажа (Hang Offsets)

Когда персонаж находится в состоянии виса (`EThreadTalesTraversalState::LedgeClimbing`), его капсула жестко фиксируется в пространстве:

```cpp
CachedHangRotation = FRotator(0.0f, (-WallNormal2D).Rotation().Yaw, 0.0f);
CachedHangLocation = TrueLedgeEdge + (WallNormal2D * HangWallOffset) - FVector(0.0f, 0.0f, HangHeightOffset);
```

### Калибровочные значения оффсетов:
- `HangWallOffset = 24.0 см`: Расстояние от центра капсулы до внешней стены. При радиусе капсулы 50 см тело прижимается к стене, а предплечья и ладони естественно ложатся на верхнюю плоскость уступа.
- `HangHeightOffset = 73.0 см`: Расстояние от центра капсулы вниз от кромки уступа. В позе `MD_hang_idle1` ладони персонажа находятся ровно на высоте `+73 см` над центром капсулы, поэтому кисти ложатся точно на горизонтальную полку уступа.

### Физический замок (Hard Anchor):
- При захвате вызывается `MoveComp->DisableMovement()` (`MOVE_None`).
- В `TickComponent` позиция персонажа принудительно устанавливается в `CachedHangLocation` с типом телепортации `ETeleportType::TeleportPhysics`, а `Velocity` обнуляется.
- Это полностью исключает дрожание, физическое выталкивание капсулы из стены и сползание вниз под гравитацией.

---

## 4. Анимации и решение конфликта с IK ног (Foot Placement)

### 4.1 Используемые ассеты из пакета ATS:
- **Вис (Hang Idle):** `/Game/AdvancedTraversalSystem/Climbing_System/Animations/Idle/Braced/MD_hang_idle1_Montage`
- **Выход на уступ (Climb Up Mantle):** `/Game/AdvancedTraversalSystem/Climbing_System/Animations/Montages/ClimbOnTop/Braced/ALS/MD_climb_up_ALS1_Montage`

### 4.2 Отключение IK ног (`foot_alpha`) и плавный выход:
- **Проблема:** В анимационном блюпринте GASPALS (`ABP_SandboxCharacter`) работает нодовая система `Foot Placement IK`, которая пытается притянуть стопы к ближайшему полу под персонажем. Из-за этого при висе на стене ноги неестественно вытягивались вниз к земле.
- **Решение для виса:** В `MD_hang_idle1_Montage` задано `foot_alpha = 0.0`.
- **Плавный бленд при выходе (Climb Up):** В `MD_climb_up_ALS1_Montage` кривая `foot_alpha` держится `0.0` до 1.1 сек (пока ноги подтягиваются), а затем плавно интерполируется с 1.1 сек до 1.45 сек к `1.0` (когда персонаж встает на стопы). Это устраняет резкий «рывок» ног при переходе в локомоцию.
- **Привязка к полу (Floor Snapping):** В `OnClimbMontageEnded` перед включением `MOVE_Walking` выполняется вертикальный трейс вниз от центра капсулы, и капсула устанавливается точно на пол (`FloorHit.Z + HalfHeight`). Это исключает зависание на 12 см в воздухе и последующее оседание под гравитацией.

---

## 5. Взаимодействие и кнопки управления

| Действие игрока | Кнопка | Поведение системы |
|---|---|---|
| **Автозахват уступа** | В воздухе / прыжок | При падении или у вершины прыжка (`Velocity.Z <= 350`) система автоматически хватает найденную грань. |
| **Выход на уступ (Climb Up)** | **`Space` (Пробел)** | 1. Режим движения переключается в `MOVE_Flying`.<br>2. Запускается рут-моушен монтаж `MD_climb_up_ALS1_Montage`.<br>3. По завершении монтажа вызывается `OnClimbMontageEnded`, восстанавливается `MOVE_Walking`. |
| **Спрыгнуть с уступа (Drop)** | **`C` / `Ctrl` (Crouch)** | 1. Вызывается `RequestReleaseLedge()`.<br>2. Стейт сбрасывается в `None`, включается `MOVE_Walking`.<br>3. Запускается кулдаун `RegrabCooldownDuration = 0.5 сек`, чтобы персонаж не прилип обратно к той же стене. |

---

## 6. Интеграция с Gameplay Ability System (GAS)

- При переходе в вис на персонажа накладывается тег **`TAG_State_Movement_Climbing`**.
- Этот тег прописан в `BlockAbilitiesWithTag` у способностей:
  - `GA_Melee` (атаки оружием ближнего боя)
  - `GA_Ranged` (стрельба из рогатки)
  - `GA_Evade` (уклонения и кувырки)
  - `GA_DoubleJump` (двойной прыжок)
- При выходе на уступ или падении тег автоматически снимается через `SetTraversalState(EThreadTalesTraversalState::None)`.

---

## 7. Сетевая синхронизация (Co-op 2 игрока)

Все действия реплицируются по клиент-серверной модели:
- `CurrentTraversalState` реплицируется через `DOREPLIFETIME` + `OnRep_TraversalState`.
- Клиент запрашивает действия через Server RPC:
  - `Server_StartLedgeClimb(LedgeLocation, WallNormal)`
  - `Server_ClimbUp()`
  - `Server_ReleaseLedge()`
  - `Server_HopUp(TargetLedgeLoc, TargetWallNormal)`
  - `Server_SideJump(DirectionSign, TargetLedgeLoc, TargetWallNormal)`
  - `Server_StartShimmy(DirectionSign, TargetLedgeLoc, TargetWallNormal)`
- Сервер транслирует выполнение на всех через Multicast RPC:
  - `Multicast_StartLedgeClimb`
  - `Multicast_PlayClimbMontage`
  - `Multicast_ReleaseLedge`
  - `Multicast_HopUp`
  - `Multicast_SideJump`
  - `Multicast_StartShimmy`

---

## 8. Многонаправленное перемещение (Hop Up, Side Jump, Shimmy)

### 8.1 Матрица принятия решений при вводе:
1. **`Space` (без горизонтального ввода):**
   - Проверяется наличие следующего яруса уступа прямо над головой через `FindUpperLedge`:
     - Диапазон высот для короткой анимации: от `MinHopUpHeight = 35.0 см` до `MaxHopUpHeight = 85.0 см` (проверочные лучи на 45, 60, 75 см).
     - Проверка воздушного кармана (Air Clearance): перед гранью уступа на 15 см вперед обязательно должно быть открытое пространство (отсекает плоские крыши/полы).
     - Если найден -> выполняется **Hop Up** (`MD_climb_short_r_up1_Montage`).
     - Если сверху стена закончилась или выше нет уступа -> выполняется **Climb Up / Mantle** (`MD_climb_up_ALS1_Montage`) с выходом на ноги.
2. **`A` или `D` (удержание без Space):**
   - В `TickComponent` каждые `ShimmyCooldownDuration = 0.65 сек` зондируется продолжение уступа через `FindSideLedge` (`bForJump = false`):
     - Направление рассчитывается через `ActorRightVector` персонажа (D = вправо, A = влево).
     - Если полка продолжается непрерывно на том же уровне (`bOutIsContinuous == true`) и перед ней свободное пространство -> выполняется **Shimmy** шаг на `45 см` (`MD_Shimmy_L1_Montage` / `MD_Shimmy_R1_Montage`).
     - Если уступ оборвался -> персонаж безопасно остается висеть на краю.
3. **`A + Space` или `D + Space` (прыжок в сторону):**
   - Вызывается `HandleLedgeJumpInput()` с активным боковым вводом (прямой опрос `EKeys::A / EKeys::D` и вектора движения):
     - Зондируется уступ сбоку через `FindSideLedge` (`bForJump = true`) на дистанциях `65..105 см`.
     - При обнаружении уступа (как через разрыв между блоками, так и длинного прыжка вдоль непрерывной стены) совершается **Side Jump** (`MD_climb_short_l_left1_Montage` / `MD_climb_short_r_right1_Montage`).
     - Если уступа в выбранном направлении нет -> персонаж остается висеть и **НЕ** вылезает вверх на крышу.

### 8.2 Математика перехода и интерполяции (`bIsTransitioning`):
- Во время перехода персонаж плавно интерполируется между старой и новой точкой виса:
  `SmoothAlpha = FMath::SmoothStep(0.0f, 1.0f, Timer / Duration)`
  `NewLoc = FMath::Lerp(TransitionStartLocation, TransitionTargetLocation, SmoothAlpha)`
- При достижении `Alpha >= 1.0f` персонаж жестко перепривязывается к новому анкору (`CachedHangLocation = TargetHangLoc`, `CachedLedgeLocation = TargetLedgeLoc`), включается `HangMontage`.
- Все монтажи переходов содержат кривую `foot_alpha = 0.0`, блокирующую нежелательное срабатывание Foot IK.

---

## 9. Сводная таблица калибровочных констант

| Переменная | Дефолт | Назначение |
|---|---|---|
| `bRequireLedgeTag` | `false` | Игнорировать теги, хватать любую геометрию («Вариант А») |
| `ForwardTraceDistance`| `120.0` | Вылет трейса вперед от центра персонажа (см) |
| `ForwardTraceZOffset` | `45.0`  | Высота старта трейса над центром капсулы (грудь) |
| `MinGrabHeight`       | `10.0`  | Минимальная высота уступа над центром капсулы |
| `MaxGrabHeight`       | `155.0` | Максимальная высота уступа над центром капсулы (размах рук на взлете) |
| `MaxGrabVelocityZ`    | `400.0` | Максимальная скорость вверх для разрешения хвата на взлете прыжка |
| `MaxApproachAngle`    | `65.0`  | Допустимый угол отклонения от нормали стены (градусы) |
| `HandSpacing`         | `22.0`  | Полуширина размаха рук для проверки уступа |
| `HangWallOffset`      | `24.0`  | Горизонтальный отступ от стены при висе |
| `HangHeightOffset`    | `61.0`  | Вертикальный отступ от кромки вниз при висе |
| `MinHopUpHeight`      | `20.0`  | Мин. высота следующего уступа над головой (см) |
| `MaxHopUpHeight`      | `58.0`  | Макс. высота следующего уступа над головой (см) |
| `MinSideJumpDistance` | `25.0`  | Мин. ширина разрыва для бокового прыжка (см) |
| `MaxSideJumpDistance` | `58.0`  | Макс. ширина разрыва для бокового прыжка (см) |
| `ShimmyStepDistance`  | `67.0`  | Длина одного шага перемещения вдоль карниза (см) |
| `ShimmyCooldownDuration`| `0.55`| Задержка между шагами шимми (сек) |
| `RegrabCooldownDuration`| `1.5`  | Задержка перед повторным захватом после вылезания/сброса (сек) |
| `bDrawDebugTraces`    | `true`  | Отрисовка отладочных линий и точек в редакторе |

---

## 10. Чеклист для проверки в редакторе

1. **Детект:** При прыжке к любому блоку или стене появляется зеленая сфера `EDGE (Z:...)` и желтая линия между руками.
2. **Вис:** Персонаж мягко останавливается, руки лежат ровно на кромке уступа, ноги не тянутся к полу.
3. **Hop Up (Ярус сверху):** При наличии блока выше по нажатию `Space` персонаж подпрыгивает руками вверх и цепляется за следующий ярус.
4. **Mantle (Крыша/Конец стены):** Если уступа выше нет, по `Space` персонаж плавно выходит наверх и встает на ноги на пол.
5. **Shimmy (Перемещение вдоль стены):** При зажатии `A` или `D` персонаж перебирает руками вдоль непрерывного карниза.
6. **Side Jump (Прыжок через зазор):** При зажатии `A/D + Space` рядом с соседним блоком персонаж совершает прыжок вбок с перехватом.
7. **Спрыгивание (Drop):** По клавише `C`/`Ctrl` персонаж отпускает уступ и падает строго вниз без залипания.

---

## 11. Продвинутая кинематика, многоуровневый поиск и гибридная архитектура

### 11.1 Физическая калибровка и тайминги
- **Вис:** `HangWallOffset = 24.0 см`, `HangHeightOffset = 73.0 см` (кисти рук лежат точно на кромке уступа в позе `MD_hang_idle1`).
- **Shimmy (Боковое перемещение A/D):** `ShimmyStepDistance = 67.0 см`, `ShimmyCooldownDuration = 0.55с`. Поиск непрерывной кромки использует многоуровневое зондирование полок на глубинах `{ 4.0, 10.0 } см` с вертикальным окном 20 см и фильтром `!bStartPenetrating`, обеспечивая стабильное перемещение как по широким карнизам, так и по узким промежуточным полкам.
- **Боковые и диагональные прыжки:** `TopTraceOffsetZ = 28.0 см` для горизонтальных прыжков и `25.0 см` для диагональных вниз — исключает упирание луча в вышестоящие ярусы стены.
- **Захват уступа на взлёте:** `MaxGrabVelocityZ = 400.0 см/с`, `MaxGrabHeight = 155.0 см` — позволяет хватать как высокие, так и узкие промежуточные уступы прямо во время подъёма прыжка.
- **Многоуровневый поиск полок (Multi-Depth Inward):** зондирование полок на глубинах `{ 5.0, 12.0, 20.0 } см` обеспечивает детекцию узких полок и карнизов (от 5 см), исключая пролёт луча сквозь уступ в стену за ним.
- **Blend Out монтажей:** Для всех прыжковых монтажей и мантеля `Blend Out Trigger Time` = `-1.0` (полное исключение досрочного выхода в локомоцию на землю во время прыжка).

### 11.2 Диапазоны и тиры прыжков (Short, Long, Leap) с взаимным перекрытием (Overlap):
Для полного исключения слепых зон на стыках тиров диапазоны имеют гарантированный взаимный нахлёст:
- **Short (Короткие шаги):** 20 .. 58 см (номинал 48.1 см). Плейрейт `1.0x`, длительность `0.95с`.
- **Long (Широкие шаги):** 50 .. 110 см (номинал 96.2 см). Плейрейт `1.0x`, длительность `1.00с`. Перекрывает как 50..58 см (Short), так и 85..110 см (Leap).
- **Leap (Прыжки через пропасти):** 85 .. 200 см (номинал 200.0 см). Плейрейт `1.0x`, длительность `1.00с`. Начинается с 85 см, надёжно подхватывая любые прыжки от 1 метра.

### 11.3 Приоритеты ввода (W / Shift+W):
- **W (Обычное лазание):** `Short -> Long -> Leap -> Mantle` (CanClimbUp в конце).
- **Shift + W (Спринт):** `Leap -> Long -> Short -> Mantle`.
- **S (Вниз):** перебор уступов вниз, при отсутствии — безопасный сброс с уступа (Drop). Ограничение верхнего вылета луча (`DownTopOffset = 22.0 см`) предотвращает ложные зацепы за вышестоящие ярусы.

### 11.4 Гибридный физический пайплайн (Hybrid Sweep & Trace Pipeline)
Для исключения геометрических коллизий толстых фигур с острыми гранями утверждён стандарт:
1. **Объёмные свипы (Sphere & Capsule Sweeps):**
   - Поиск стены перед телом (`DetectLedge`): `Sphere(R=15)`.
   - Сканирование сектора в направлении прыжка (`ProbeAndValidateLedge`): `Sphere(R=14)`.
   - Вместимость туловища и головы при висе (`BodyClearance`): `Sphere(R=20, Z: -5..+40 см вокруг рук)` — проверяет свободное пространство плеч и подбородка, исключая ложные блокировки от близко расположенных верхних уступов (от 50 см).
   - Вместимость стоячей капсулы игрока перед залезанием (`CanClimbUp`): `Capsule(R=31, H=82)`.
   - Вместимость стоячей капсулы игрока перед залезанием (`CanClimbUp`): `Capsule(R=31, H=82)`.
2. **Математические чистые лучи (Line Trace):**
   - Засечка точной высоты $Z$ верхней грани уступа (`TopHit`).
   - Засечка вертикальной стены под кромкой ($XY$).
   - Проверка открытого воздуха в 6 см над уступом (`AboveLipCheck`).
   - Проверка пола площадки для залезания (`FloorHit`).

### 11.5 Кинематическая синхронизация рук
- `Duration`: Short = `0.95с`, Long = `1.00с`, Leap = `1.00с` (управляется через `ShortJumpDuration`, `LongJumpDuration`, `LeapJumpDuration`).
- `FlightEnd`: для Short и Long = `0.85` (`JumpFlightEndRatio`) — перемещение капсулы завершается ровно в момент касания уступа кистью, исключая взлёт рук в воздух.
- `ClimbAnimPlayRateMultiplier` = `1.0f` (натуральная скорость без искажения физики).
- Замер дистанции в дебаге ведётся строго между кромками: `FVector::Dist(CachedLedgeLocation, TargetLedgeLoc)`.

---

## 12. Справочник настраиваемых параметров (UThreadTalesClimbingConfig Data Asset Reference)

Все ключевые физические, геометрические, анимационные и временные параметры вынесены в Data Asset класса **`UThreadTalesClimbingConfig`** (`Source/ThreadTales/Characters/ThreadTalesClimbingConfig.h`). В панели **Details** компонента `UThreadTalesTraversalComponent` настраивается ссылка на активный Data Asset (`ClimbingConfig`) и флаг `DrawDebugTraces`.

Это позволяет настраивать физику и анимации независимыми ассетами (например, `/Game/Blueprints/DA_ClimbingConfig_Default`, `DA_ClimbingConfig_Male`, `DA_ClimbingConfig_Female`) без изменения кода и без риска конфликтов при слиянии веток.

### 12.1 Детекция уступов (`ThreadTales|Traversal|Detection`)
| Переменная | Тип | Дефолт | Назначение |
|---|---|---|---|
| `ForwardTraceDistance` | `float` | `120.0 см` | Дистанция переднего луча поиска стены от центра капсулы |
| `ForwardTraceZOffset` | `float` | `45.0 см` | Высота переднего луча над центром капсулы (уровень груди) |
| `MinGrabHeight` | `float` | `10.0 см` | Минимальная высота уступа относительно ног для захвата |
| `MaxGrabHeight` | `float` | `155.0 см` | Максимальная высота уступа относительно ног (досягаемость рук в прыжке) |
| `HandSpacing` | `float` | `22.0 см` | Полуширина плеч/рук для 3-точечной проверки ширины полки |
| `MaxApproachAngle` | `float` | `65.0°` | Максимальный угол подхода к нормали стены (конус захвата) |
| `MaxGrabVelocityZ` | `float` | `400.0 см/с` | Допустимая вертикальная скорость подъёма для захвата уступа |
| `LedgeProbeDepths` | `TArray<float>` | `{ 5.0, 12.0, 20.0 } см` | Многоуровневые глубины зондирования верхней плоскости полки |
| `MaxGrabWallDistance` | `float` | `55.0 см` | Максимальная дистанция до стены в момент хвата (защита от телепортации) |
| `LedgeSlopeMaxZDifference` | `float` | `25.0 см` | Максимально допустимый перепад высоты между левой и правой ладонью |
| `LedgeHandsProbeDepth` | `float` | `8.0 см` | Глубина зондирования поверхности под ладонями вглубь полки |
| `LedgeHandsVerticalRange` | `float` | `15.0 см` | Вертикальный полудиапазон лучей под левой и правой ладонью |

### 12.2 Оффсеты виса (`ThreadTales|Traversal|Offsets`)
| Переменная | Тип | Дефолт | Назначение |
|---|---|---|---|
| `HangWallOffset` | `float` | `24.0 см` | Расстояние от центра капсулы до внешней стены при висе |
| `HangHeightOffset` | `float` | `70.0 см` | Смещение капсулы вниз от кромки уступа (пальцы и ладони чисто на полке без врезания в геометрию) |

### 12.3 Валидация кромки и свободного пространства (`ThreadTales|Traversal|Validation`)
| Переменная | Тип | Дефолт | Назначение |
|---|---|---|---|
| `LipBelowWallTraceDepth` | `float` | `6.0 см` | Глубина луча вниз под кромку для проверки сплошной стены |
| `LipAboveAirTraceHeight` | `float` | `6.0 см` | Высота луча над кромкой для проверки открытого воздуха |
| `LipModularSeamRejectDistance`| `float` | `15.0 см` | Порог отсечения модульных швов и сплошных стен без кромки |
| `BodyClearanceRadius` | `float` | `20.0 см` | Радиус сферы проверки пространства для головы и торса |
| `BodyClearanceHeight` | `float` | `40.0 см` | Высота проверки пространства над точкой виса |

### 12.4 Свободный вис (`ThreadTales|Traversal|Freehang`)
| Переменная | Тип | Дефолт | Назначение |
|---|---|---|---|
| `bAllowFreehang` | `bool` | `true` | Разрешить захват узких балок и перил без стены под кромкой |
| `FreehangWallCheckDepth` | `float` | `70.0 см` | Глубина проверки стены под уступом (уровень упора ног/коленей) |
| `FreehangWallTraceForward` | `float` | `35.0 см` | Дистанция начала луча перед стеной |
| `FreehangWallTraceBack` | `float` | `25.0 см` | Дистанция проникновения луча вглубь стены |

### 12.5 Механика спуска (`ThreadTales|Traversal|ClimbDown`)
| Переменная | Тип | Дефолт | Назначение |
|---|---|---|---|
| `ClimbDownTraceDistance` | `float` | `70.0 см` | Дистанция поиска кромки перед/под ногами для спуска |
| `ClimbDownRootMotionForwardOffset` | `float` | `72.2 см` | Горизонтальное смещение Root Motion монтажа спуска |
| `ClimbDownRootMotionDownwardOffset` | `float` | `171.32 см` | Вертикальное смещение Root Motion монтажа спуска |
| `ClimbDownFloorTraceForwardDist` | `float` | `8.0 см` | Смещение луча пола вглубь стены при поиске кромки |
| `ClimbDownFloorTraceZRange` | `float` | `35.0 см` | Вертикальный полудиапазон луча поиска пола у края платформы |
| `ClimbDownEdgeLipDepth` | `float` | `8.0 см` | Глубина под верхней кромкой для засечки XY плоскости стены |
| `ClimbDownEdgeLipProbeRange` | `float` | `25.0 см` | Диапазон горизонтального встречного луча засечки кромки XY |
| `ClimbDownMinApproachDot` | `float` | `0.25` | Минимальный угол взгляда/движения к обрыву для детекции спуска |

### 12.6 Подъем на уступ / Mantle (`ThreadTales|Traversal|Mantle`)
| Переменная | Тип | Дефолт | Назначение |
|---|---|---|---|
| `MantleStandCapsuleRadius` | `float` | `31.0 см` | Радиус тестовой капсулы проверки стоячего места |
| `MantleStandCapsuleHalfHeight` | `float` | `82.0 см` | Полувысота тестовой капсулы стоячего места |
| `MantleStandForwardOffset` | `float` | `40.0 см` | Смещение точки приземления вглубь платформы от кромки |
| `MantleStandUpOffset` | `float` | `5.0 см` | Приподнятие над уровнем пола при проверке свободного места |
| `MantleWallContinuityProbeDepth` | `float` | `15.0 см` | Глубина луча проверки целостности пола под ногами |

### 12.7 Отскок от стены и прыжок назад (`ThreadTales|Traversal|WallEject`)
| Переменная | Тип | Дефолт | Назначение |
|---|---|---|---|
| `WallEjectSectorHalfAngle` | `float` | `45.0°` | Полусектор веерного сканирования встречных уступов за спиной |
| `WallEjectSectorRayCount` | `int32` | `7` | Количество веерных лучей сканирования сектора |
| `WallEjectMinOpposingHeight` | `float` | `-100.0 см` | Нижняя граница высоты встречного уступа |
| `WallEjectMaxOpposingHeight` | `float` | `150.0 см` | Верхняя граница высоты встречного уступа |
| `WallEjectNormalToleranceDot` | `float` | `-0.7` | Допустимая параллельность встречной стены |
| `WallEjectRegrabCooldown` | `float` | `0.55 с` | Защитный кулдаун повторного зацепа после отскока |
| `BackJumpOpposingLedgeDistance` | `float` | `450.0 см` | Максимальная дистанция зондирования встречного уступа сзади |
| `WallEjectHorizontalSpeed` | `float` | `450.0 см/с` | Горизонтальный импульс отталкивания от стены при отскоке |
| `WallEjectVerticalSpeed` | `float` | `350.0 см/с` | Вертикальный импульс подъема при отскоке от стены |

### 12.8 Боковое перемещение (Shimmy) (`ThreadTales|Traversal|MultiLedge|Shimmy`)
| Переменная | Тип | Дефолт | Назначение |
|---|---|---|---|
| `ShimmyStepDistance` | `float` | `67.0 см` | Дистанция одного шага вдоль кромки (рут-моушен `MD_Shimmy`) |
| `ShimmyCooldownDuration` | `float` | `0.55 с` | Кулдаун между последовательными шагами перебора рук |
| `ShimmyProbeDepths` | `TArray<float>` | `{ 4.0, 10.0 } см` | Многоуровневое зондирование полок при боковом шаге |
| `ShimmyVerticalTraceRange` | `float` | `20.0 см` | Вертикальное полуокно сканирования верхней грани полки |
| `ShimmyMaxZDifference` | `float` | `20.0 см` | Допустимый перепад высоты полки для продолжения шимми |

### 12.9 Зондирование направленных прыжков (`ThreadTales|Traversal|MultiLedge|Probing`)
| Переменная | Тип | Дефолт | Назначение |
|---|---|---|---|
| `DirectionalProbeDepths` | `TArray<float>` | `{ 4.0, 8.0, 16.0 } см`| Глубины проникновения в уступ при сканировании сектора |
| `UpJumpTopTraceOffset` | `float` | `55.0 см` | Высота начала верхнего опускающегося луча при прыжке вверх (Hop Up) |
| `DownJumpTopTraceOffset` | `float` | `22.0 см` | Высота начала опускающегося луча при прыжке вниз (Short / Long) |
| `DownJumpLeapTopTraceOffset` | `float` | `28.0 см` | Высота начала опускающегося луча при прыжке вниз (Leap) |
| `LateralJumpTopTraceOffset` | `float` | `28.0 см` | Высота начала опускающегося луча при прыжке вбок (Left / Right) |
| `DiagonalUpJumpTopTraceOffset` | `float` | `50.0 см` | Высота начала опускающегося луча при прыжке по диагонали вверх |
| `DiagonalDownJumpTopTraceOffset` | `float` | `25.0 см` | Высота начала опускающегося луча при прыжке по диагонали вниз |

### 12.10 Кинематика и задержки ввода (`ThreadTales|Traversal|Kinematics`)
| Переменная | Тип | Дефолт | Назначение |
|---|---|---|---|
| `ShortJumpDuration` | `float` | `0.95 с` | Полная длительность кинематического цикла короткого прыжка |
| `LongJumpDuration` | `float` | `1.00 с` | Полная длительность кинематического цикла длинного прыжка |
| `LeapJumpDuration` | `float` | `1.00 с` | Полная длительность кинематического цикла прыжка через пропасть |
| `JumpFlightEndRatio` | `float` | `0.85` | Доля цикла, в которой завершается полёт и руки берут уступ |
| `ShimmyInputDebounceDuration` | `float` | `0.15 с` | Защитный интервал ввода после шага шимми |
| `JumpInputDebounceDuration` | `float` | `0.35 с` | Защитный интервал ввода после направленного прыжка |
| `ClimbBackJumpPlayRate` | `float` | `1.35` | Множитель скорости проигрывания отскока от стены |
| `ClimbAnimPlayRateMultiplier` | `float` | `1.0` | Глобальный множитель скорости проигрывания монтажей |
| `RegrabCooldownDuration` | `float` | `1.5 с` | Минимальное время кулдауна до повторного захвата уступа после сброса |

### 12.11 Монтажи и режимы лазания (`ThreadTales|Traversal|Montages`)
| Переменная | Тип | Ассет по умолчанию | Назначение |
|---|---|---|---|
| `ClimbDownMontage` | `UAnimMontage*` | `MD_climb_down_Montage1` | Спуск с платформы в вис со стеной под ногами |
| `BarClimbDownMontage` | `UAnimMontage*` | `MD_bar_climb_down_Montage1` | Спуск с платформы на узкую перекладину (Freehang) |
| `BarHangMontage` | `UAnimMontage*` | `MD_bar_hang_idle1_Montage` | Свободный вис на руках без стены под ногами |
| `BarClimbOnTopMontage` | `UAnimMontage*` | `MD_bar_climb_up_ALS_Montage1` | Силовой подъем (Muscle-up) на перекладину/балку |
| `ClimbBackJumpMontage` | `UAnimMontage*` | `MD_leap_short_r_back1_Montage` | Прыжок назад / Отскок от стены с отталкиванием ногами |

---

## 13. Расширенные механики паркура

### 13.1 Climb Down (Mantle Down / Спуск с края крыши в вис)
- **Управление:** Клавиша приседания (`C`) стоя у края платформы (или `S + C`).
- **Алгоритм детекции:**
  1. Проверяется, что персонаж стоит на земле (`MOVE_Walking`).
  2. Лучи вперед и вниз от стоп фиксируют обрыв пола (`FloorCheck`).
  3. Горизонтальный обратный луч под уровнем пола находит внешнюю грань стены (`WallHit`).
  4. Опускающийся луч находит точную кромку уступа $Z$.
  5. Проверяется проходимость тела (`ValidateLedgeLipAndClearance`).
  6. Выполняется математическое предварительное позиционирование (Pre-Alignment): персонаж разворачивается лицом к обрыву (`+WallNormal2D`) и ставится в точку за 72.2 см от точки виса (строго под смещение рут-моушена анимации: `CachedHangLocation.XY - (WallNormal2D * 72.2f)` и `CachedHangLocation.Z + 171.32f`).
  7. Капсула временно игнорирует коллизию `ECC_WorldStatic` и `ECC_WorldDynamic` (`SetClimbDownCollision(true)`), что позволяет капсуле радиусом 50 см беспрепятственно пройти угол кромки пола и опуститься на 171.32 см вниз.
  8. Воспроизводится `ClimbDownMontage` (или `BarClimbDownMontage`). Персонаж делает шаг назад/переворот на 180°, опускается в вис, и рут-моушен приводит капсулу ровно в точку виса (`CachedHangLocation`) с нулевым смещением и без застревания в воздухе.
  9. В момент Blend Out / End коллизия немедленно восстанавливается, и управление мягко переходит в зацикленный `HangMontage`.

### 13.2 Back Jump & Wall Eject (Прыжок назад / Отскок от стены)
- **Управление:** При висе на уступе нажать `S + Space` (направление от стены + прыжок).
- **Двухфазное поведение:**
  1. **Прыжок на встречный уступ (Opposing Ledge Leap):**
     - Компонент зондирует сектор за спиной персонажа (`+CachedWallNormal`) на расстояниях до 4.5 метров.
     - Если сзади обнаружен параллельный встречный уступ с нормалью навстречу игроку, запускается кинематический прыжок назад с разворотом и захватом противоположного карниза.
  2. **Отскок от стены (Wall Eject Kick-off):**
     - Если сзади нет уступа, персонаж отталкивается обеими ногами от стены (`ClimbBackJumpMontage`).
     - Персонажу придаётся скорость `(WallNormal * 450) + (Up * 350)`, включается режим `MOVE_Falling` с полным контролем полета в воздухе.
     - Активируется кулдаун повторного зацепа (0.55с), чтобы персонаж чисто отлетел от стены.

### 13.3 Freehang / Bar Hanging (Автоматический режим перекладины)
- При захвате уступа луч `BelowHit` проверяет наличие вертикальной стены под кистями (глубина 70 см).
- Если стены нет (`BelowHit.bBlockingHit == false`), уступ классифицируется как **Freehang** (балка, перила, труба).
- В режиме Freehang:
  - Автоматически используется монтаж виса с болтающимися ногами `MD_bar_hang_idle1_Montage`.
  - При нажатии `W` / подъеме наверх воспроизводится силовой выход на две руки `MD_bar_climb_up_ALS_Montage1`.
  - При спуске с края на балку воспроизводится `MD_bar_climb_down_Montage1`.

### 13.4 Стандарты слотов и зацикливания монтажей (КРИТИЧЕСКИ ВАЖНО)
1. **Только `DefaultGroup.DefaultSlot` (и правило пересоздания):**
   - В плагине GASPALS главный Anim Blueprint (`ABP_SandboxCharacter`) подключен на выходную позу исключительно через слот **`DefaultGroup.DefaultSlot`** (`DefaultSlot`).
   - Исходные ассеты ATS настроены на кастомный слот `Ledge_Climbing`. Если существует монтаж с нужным именем, но в нем не стоит `DefaultGroup.DefaultSlot`, его **СТРОГО ЗАПРЕЩЕНО** использовать в таком виде.
   - **Правило пересоздания:** Если в монтаже указан не `DefaultGroup.DefaultSlot`, монтаж **ОБЯЗАТЕЛЬНО пересоздается заново** от целевого AnimSequence (`ПКМ по AnimSequence -> Create -> Create AnimMontage`). Созданный движком новый монтаж автоматически получает чистый слот `DefaultGroup.DefaultSlot`.
   - *Симптом при неверном слоте:* Персонаж перемещается в пространстве по Root Motion, но меш застывает в Idle/T-позе без воспроизведения анимации.
2. **Зацикливание в свойствах ассета (Montage Section Loop):**
   - Все монтажи постоянного виса (`HangMontage`, `BarHangMontage`) обязаны иметь включенное зацикливание секции (`Default -> Default`) непосредственно в самом ассете AnimMontage через окно Montage Sections в Unreal Editor.
   - Это гарантирует бесшовное зацикливание на уровне движка без необходимости runtime-вызовов из C++.
3. **Управление спрыгиванием (Drop ONLY on C):**
   - Нажатие `S` при висе на уступе никогда не сбрасывает персонажа со стены (позволяет надежно прожимать `S + Space` для Back Jump).
   - Спрыгивание со стены выполняется **строго клавишей приседания `C`** (Crouch).

---

## 14. Архитектура компонента: Машина подсостояний и Data-Driven Jump Table

### 14.1 Машина подсостояний (`EThreadTalesClimbingSubState`)
Внутри состояния `EThreadTalesTraversalState::LedgeClimbing` управление поведением персонажа передано строгой машине подсостояний (Single Source of Truth), полностью заменившей разрозненные булевы флаги:

| Подсостояние | Значение | Описание |
|---|---|---|
| `None` | 0 | Вне режима скалолазания или в процессе отпускания уступа |
| `IdleHang` | 1 | Стабильный вис на уступе (Braced или Freehang). Готов к приему ввода прыжков, шимми или залезания |
| `Shimmying` | 2 | Непрерывное горизонтальное смещение вдоль непрерывного карниза |
| `JumpTransition` | 3 | Кинематический 3-фазный прыжок к смежному уступу (Short, Long, Leap) |
| `MantleUp` | 4 | Залезание на платформу по Root Motion |
| `ClimbDown` | 5 | Спуск с платформы в вис по Root Motion |

**Преимущества архитектуры:**
- Исключены конфликты и рассинхроны: персонаж математически не может одновременно находиться в прыжке и залезать наверх.
- Все проверки гардов сведены к лаконичному `ClimbingSubState == EThreadTalesClimbingSubState::IdleHang`.

### 14.2 Data-Driven конфигурация прыжков (`FThreadTalesLedgeJumpConfig` & `JumpConfigTable`)
Все параметры 8 направлений прыжков по 3 категориям дистанций (Short, Long, Leap) сведены в единую внутреннюю структуру и матрицу быстрого доступа:

```cpp
USTRUCT(BlueprintType)
struct FThreadTalesLedgeJumpConfig
{
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    TObjectPtr<UAnimMontage> Montage = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    float MinDistance = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    float MaxDistance = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    float NominalDistance = 48.1f;
};
```

- **Быстрый доступ $O(1)$:** `JumpConfigTable[Direction][DistanceType]` компилируется при `BeginPlay()`, разрешая все цепочки fallback (диагональ $\to$ горизонталь $\to$ вертикаль; Leap $\to$ Long $\to$ Short) один раз на старте.
- **Устранение дублирования:** Запросы монтажей, диапазонов и номинальных дистанций сокращены с более чем 600 строк громоздких `switch-case` блоков до прямых запросов по индексу.
- **100% обратная совместимость:** Все индивидуальные свойства `UPROPERTY` в редакторе сохранены, гарантируя полную целостность сериализации в `BP_ThreadTalesCharacter`.

### 14.3 Полное разделение конфигурации (Архитектура `UThreadTalesClimbingConfig` Data Asset)
В соответствии со стандартом чистой модульной архитектуры выполнено 100% разделение ответственности между физическим детектом мира и параметрами персонажа:

- **C++ класс:** `UThreadTalesClimbingConfig : public UDataAsset` (`Source/ThreadTales/Characters/ThreadTalesClimbingConfig.h`).
- **Свойство в компоненте:** `UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Traversal|Config") TObjectPtr<UThreadTalesClimbingConfig> ClimbingConfig;`.
- **Созданный эталонный ассет:** `/Game/Blueprints/DA_ClimbingConfig_Default` (полностью заполнен всеми 33 монтажами и точными калибровочными значениями).

#### Разделение зон ответственности:
1. **В Data Asset (`UThreadTalesClimbingConfig`):** вынесены **все свойства, зависящие от анимаций и анатомии персонажа**:
   - **Все 33 монтажа:** базовые (Hang, BarHang, ClimbOnTop, BarClimbOnTop, ClimbDown, BarClimbDown, BackJump, ShimmyL/R) и направленные прыжки (Short, Long, Leap для 8 направлений).
   - **Offsets:** `HangWallOffset`, `HangHeightOffset`.
   - **Playback:** `ClimbAnimPlayRateMultiplier`, `ClimbBackJumpPlayRate`.
   - **Kinematics:** `ShortJumpDuration`, `LongJumpDuration`, `LeapJumpDuration`, `JumpFlightEndRatio`, `JumpArcPeakTime`, `JumpArcHeight`, `LeapJumpArcPeakTime`, `LeapJumpArcHeight`.
   - **Distance Bounds:** Все 24 диапазона для Short, Long и Leap прыжков.
   - **Shimmy:** `ShimmyStepDistance`, `ShimmyCooldownDuration`, `ShimmyProbeDepths`, `ShimmyVerticalTraceRange`, `ShimmyMaxZDifference`, `ShimmyInputDebounceDuration`.
   - **Climb Down:** `ClimbDownStepBackDistance`, `ClimbDownDownwardTraceDepth`, `ClimbDownRootMotionForwardOffset`, `ClimbDownRootMotionDownwardOffset`, `ClimbDownMinApproachDot`.
   - **Mantle:** `MantleStandCapsuleRadius`, `MantleStandCapsuleHalfHeight`, `MantleStandForwardOffset`, `MantleStandUpOffset`, `MantleWallContinuityProbeDepth`.
   - **Wall Eject & Back Jump:** `WallEjectHorizontalSpeed`, `WallEjectVerticalSpeed`, `WallEjectSectorHalfAngle`, `WallEjectSectorRayCount`, `WallEjectMinOpposingHeight`, `WallEjectMaxOpposingHeight`, `WallEjectNormalToleranceDot`, `WallEjectRegrabCooldown`, `BackJumpOpposingLedgeDistance`.

2. **В Data Asset также вынесены все параметры зондирования физического мира (Sensing & World Detection):**
   - Детекция: `ForwardTraceDistance`, `ForwardTraceZOffset`, `LedgeDownTraceHeight`, `LedgeDownTraceDepth`, `MinGrabHeight`, `MaxGrabHeight`, `HandSpacing`, `MaxApproachAngle`, `MaxGrabVelocityZ`, `LedgeProbeDepths`, `MaxGrabWallDistance`, `LedgeSlopeMaxZDifference`, `LedgeHandsProbeDepth`, `LedgeHandsVerticalRange`.
   - Валидация: `LipBelowWallTraceDepth`, `LipAboveAirTraceHeight`, `LipModularSeamRejectDistance`, `BodyClearanceRadius`, `BodyClearanceHeight`.
   - Freehang и ClimbDown: `RegrabCooldownDuration`, `bAllowFreehang`, `ClimbDownTraceDistance`, `FreehangWallCheckDepth`, `FreehangWallTraceForward`, `FreehangWallTraceBack`, `ClimbDownFloorTraceForwardDist`, `ClimbDownFloorTraceZRange`, `ClimbDownEdgeLipDepth`, `ClimbDownEdgeLipProbeRange`.
   - Multi-Ledge Probing: `DirectionalProbeDepths`, вертикальные оффсеты верхнего трейса (`UpJumpTopTraceOffset` и др.).

3. **В панели Details компонента (`ThreadTalesTraversalComponent`):**
   - Отображаются **ИСКЛЮЧИТЕЛЬНО два свойства**:
     - `Config -> Climbing Config` (слот для назначения Data Asset `DA_ClimbingConfig_Default`).
     - `Debug -> Draw Debug Traces` (флаг визуализации трейсов).
   - Все остальные свойства переведены во внутренние члены `UPROPERTY()` без `EditDefaultsOnly` для обеспечения 100% обратной совместимости и внутренних fallback-дефолтов.

4. **Resolver Getters и Fallback-механизм:**
   - Внутри `ThreadTalesTraversalComponent` созданы 38 типизированных геттеров-резольверов (`GetForwardTraceDistance()`, `GetMinGrabHeight()`, `GetHangWallOffset()`, `GetShimmyStepDistance()`, `GetJumpFlightEndRatio()` и т.д.).
   - При наличии назначенного `ClimbingConfig` данные берутся напрямую из Data Asset.
   - Если `ClimbingConfig == nullptr`, система прозрачно обращается к внутренним переменным компонента (`UPROPERTY()`), гарантируя 100% стабильность работы.

### 14.4 Модульная C++ архитектура Traversal системы (Паттерн Mode Handlers)
Для полного исключения проблемы «God Component» и подготовки к добавлению лестниц и канатов без разрастания кода, система разделена на 5 специализированных подсистем:

1. **`ThreadTalesTraversalTypes.h`:**
   - Изолированный заголовок типов, перечислений (`EThreadTalesTraversalState`, `EThreadTalesClimbingSubState`, `EThreadTalesLedgeJumpDirection`, `EThreadTalesLedgeJumpDistanceType`) и структуры `FThreadTalesLedgeJumpConfig`.

2. **`FThreadTalesTraversalInputHelper` (`ThreadTalesTraversalInputHelper.h / .cpp`):**
   - Математический классификатор 8 направлений прыжков по углам вектора (`VectorToJumpDirection`) с фильтром мертвых зон (`DeadZone = 0.25`).
   - Извлечение 2D вектора перемещения с поддержкой как аналоговых стиков геймпада, так и клавиш клавиатуры (`GetMovementInputVector`).
   - Определение зажатого спринта (`IsSprintHeld`).

3. **`FThreadTalesLedgeDetector` (`ThreadTalesLedgeDetector.h / .cpp`):**
   - Выделенный модуль пространственного зондирования и физических коллизий мира.
   - Содержит всю низкоуровневую логику: `DetectLedge`, `TraceForwardWall`, `ProbeLedgeTopSurface`, `ValidateLedgeLipAndClearance`, `ValidateLedgeHandsPlacement`, `CheckIsFreehangLedge`, `FindUpperLedge`, `FindSideLedge`, `FindLedgeInDirection`, `FindEdgeForClimbDown`, `CanClimbUp`.
   - Полностью отвязан от сетевой репликации и GAS.

4. **`FThreadTalesClimbingKinematics` & `FThreadTalesJumpConfigTable` (`ThreadTalesClimbingKinematics.h / .cpp`):**
   - Выделенный модуль кинематики переходов и прыжковых таблиц.
   - Содержит компиляцию fallback-цепочек прыжков `[9][3]`, расчет параболических траекторий и сглаживания смещений (`CalculateTransitionTransform`), расчет импульса отскока от стены (`CalculateWallEjectVelocity`) и начального позиционирования спуска (`CalculateClimbDownPreAlignment`).

5. **`FThreadTalesLedgeClimbingHandler` (`ThreadTalesLedgeClimbingHandler.h / .cpp`):**
   - Выделенный обработчик режима лазания по уступам.
   - Инкапсулирует анкоры виса, интерполяцию переходов, дебаунс-таймеры, обработку ввода, запуск монтажей и коллбэки завершения анимаций.
    - **Инварианты делегатов спуска и подъема:**
      * `OnClimbDownMontageBlendingOut`: обязан немедленно восстанавливать коллизию (`SetClimbDownCollision(false)`), сразу же фиксировать персонажа в точке виса (`SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation)`), переводить субстейт в `IdleHang`, подавлять разворот (`bUseControllerDesiredRotation = false; bOrientRotationToMovement = false;`) и запускать `CrossfadeToHangMontage()`. Это исключает паразитный поворот капсулы боком и устраняет провисание веса слота DefaultSlot.
      * `OnClimbDownMontageEnded`: БЕЗУСЛОВНО (независимо от `bInterrupted`) подтверждает фиксацию `CachedHangLocation/Rotation`, переводит стейт в `IdleHang`, устанавливает `MOVE_Flying + Velocity=Zero` (НИКОГДА не `DisableMovement()` — ломает Root Motion и вызывает бой с `CBP_SandboxCharacter`).
      * **Подавление разворота в `Tick`:** пока активно состояние `ClimbDown` или `IdleHang`, в `Tick` каждый кадр принудительно поддерживаются `bOrientRotationToMovement = false` и `bUseControllerDesiredRotation = false`, нейтрализуя принудительный разворот из `CBP_SandboxCharacter::UpdateRotation_PreCMC`. Восстановление флагов разрешено ТОЛЬКО при выходе из лазания (`ReleaseLedge`, `PerformBackJump`, `OnClimbUpMontageEnded`).
      * `OnClimbMontageBlendingOut` (Mantle): обязан выполнять трейс вниз от Root Motion позиции капсулы и выравнивать высоту ног по полу с переключением в `MOVE_Walking`.
      * `OnClimbMontageEnded` (Mantle): сбрасывает `MantleUp -> None`, восстанавливает флаги вращения и запускает `RegrabCooldownTimer`.

6. **`UThreadTalesTraversalComponent` (`ThreadTalesTraversalComponent.h / .cpp`):**
   - Сокращен с 3430 до **~240 строк в .h и ~450 строк в .cpp**.
   - Является чистым диспетчером режимов (None, LedgeClimbing, Ladder, Rope), фасадом для Blueprint API и координатором сетевых RPC (Server/Multicast) и GAS-тегов.
   - При переключении в `Ladder` или `Rope` управление просто делегируется в соответствующий обработчик (`LadderHandler` / `RopeHandler`), не затрагивая код уступов.

---

## 15. Мульти-персонажная Co-op конфигурация (HeroMale vs HeroFemale)

В игре ThreadTales предусмотрено 2 уникальных героя для совместной игры (Игрок 1 = `BP_HeroMale`, Игрок 2 = `BP_HeroFemale`). Благодаря разделению на Data Asset, разница в анимациях и пропорциях тела решается на 100% без изменения C++ кода:

### 15.1 Назначение конфигураций в Blueprint:
- В `BP_HeroMale` в компоненте `ThreadTalesTraversal` назначается `/Game/Blueprints/DA_ClimbingConfig_Male` (или эталонный `DA_ClimbingConfig_Default`).
- В `BP_HeroFemale` назначается `/Game/Blueprints/DA_ClimbingConfig_Female`.

### 15.2 Калибровка под женский скелет:
- Если у женского персонажа другой размах плеч или рост:
  - `HandSpacing`: может быть уменьшен (например, `18.0 см` вместо `22.0 см`).
  - `HangHeightOffset`: корректируется под точное положение ладоней в женском монтаже виса.
  - `HangWallOffset`: настраивается под толщину женского меша/доспехов.
- Если для женского персонажа используются более грациозные монтажи (например, другие анимации шимми или запрыгивания на стену) — они назначаются в соответствующие слоты `DA_ClimbingConfig_Female`.

---

## 16. Дорожная карта расширения системы (Future Expansion Roadmap)

Благодаря модульной декомпозиции кода, добавление новых механик перемещения в будущем не вызовет конфликтов и не раздует базовые классы:

### 16.1 Обход углов (Corner Wrapping — Внутренние и внешние углы)
- **Что потребуется добавить:**
  1. В `ThreadTalesTraversalTypes.h` в `EThreadTalesClimbingSubState` добавляется флаг `CornerTransition`.
  2. В `FThreadTalesLedgeDetector` добавляется статический метод `ProbeCornerLedge(...)`, пускающий луч за угол перпендикулярно текущей нормали стены.
  3. В `UThreadTalesClimbingConfig` добавляются 4 монтажа: `CornerInnerLeftMontage`, `CornerInnerRightMontage`, `CornerOuterLeftMontage`, `CornerOuterRightMontage`.
  4. В `UThreadTalesTraversalComponent` метод `ProcessClimbingInput` при нажатии `A/D` на краю стены переключает персонажа в `CornerTransition`.

### 16.2 Вертикальные лестницы (Ladders)
- В `EThreadTalesTraversalState` уже зарезервировано состояние **`Ladder`**.
- Создается выделенный статический класс детекции `FThreadTalesLadderDetector` (по аналогии с `FThreadTalesLedgeDetector`).
- Персонаж при захвате лестницы входит в `SetTraversalState(EThreadTalesTraversalState::Ladder)`, блокируя все способности уступов и переключая управление на подъем/спуск по ступеням.

### 16.3 Канаты, лианы и раскачивание (Ropes & Swing)
- В `EThreadTalesTraversalState` уже зарезервировано состояние **`Rope`**.
- Логика физического раскачивания и зацепа за трос будет полностью изолирована в отдельном модуле, не влияя на стабильность скалолазания по стенам.





