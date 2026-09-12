# Climbing System — Обзор и архитектура скалолазания

> **Краткая суть:** Модульная система вертикального паркура, позволяющая персонажу хвататься за любые уступы геометрии мира, висеть, перемещаться перебором рук (Shimmy), совершать направленные прыжки (8 направлений), залезать на платформы (Climb Up), спускаться в вис с обрыва (Climb Down) и отталкиваться от стены (Wall Eject / Back Jump).

---

## 1. Quick Reference (Быстрый паспорт)

| Параметр | Значение |
|---|---|
| **Координатор режимов** | `UThreadTalesTraversalComponent` (`Source/ThreadTales/Characters/ThreadTalesTraversalComponent.h`) |
| **Обработчик лазания** | `FThreadTalesLedgeClimbingHandler` (`Source/ThreadTales/Characters/ThreadTalesLedgeClimbingHandler.h`) |
| **Камера перемещения** | `FThreadTalesClimbingCameraHandler` (`Source/ThreadTales/Characters/ThreadTalesClimbingCameraHandler.h`) |
| **Пространственный детектор** | `FThreadTalesLedgeDetector` (`Source/ThreadTales/Characters/ThreadTalesLedgeDetector.h`) |
| **Кинематика и прыжки** | `FThreadTalesClimbingKinematics`, `FThreadTalesJumpConfigTable` |
| **Обработка ввода** | `FThreadTalesTraversalInputHelper` |
| **Data Asset конфигурации** | `UThreadTalesClimbingConfig` (`/Game/Blueprints/DA_ClimbingConfig_Default`) |
| **Ключевые Gameplay Tags** | `TAG_State_Movement_Climbing` (блокирует атаку, уклонение, стрельбу, двойной прыжок) |

---

## 2. Стейт-машина режимов и подсостояний

### 2.1 Глобальное состояние (`EThreadTalesTraversalState`)
* `None` (0) — обычное передвижение (бег, прыжки по земле, Mover/CMC).
* `LedgeClimbing` (1) — активный режим скалолазания по уступам.
* `Ladder` (2) — зарезервировано под лестницы.
* `Rope` (3) — зарезервировано под канаты.

### 2.2 Машина подсостояний уступов (`EThreadTalesClimbingSubState`)
Внутри режима `LedgeClimbing` действует строгая стейт-машина, исключающая конфликт действий:

```
           ┌──────────────────────┐
           │         None         │
           └──────────┬───────────┘
                      │ (Автозахват в воздухе / ClimbDown с крыши)
                      ▼
             ┌─────────────────┐
    ┌───────►│    IdleHang     │◄──────┐
    │        └─┬──────┬──────┬─┘       │
    │ (Blend)  │      │      │ (Blend) │
    │          ▼      ▼      ▼         │
┌───┴──────────┐ ┌─────────┐ ┌─────────┴─────────┐
│JumpTransition│ │Shimmying│ │    ClimbDown      │
└──────────────┘ └─────────┘ └───────────────────┘
       │               │
       │               │ (A/D у края стены без непрерывного уступа)
       │               ▼
       │      ┌──────────────────┐
       │      │ CornerTransition │ ──► (двухсегм. дуга Start→Pivot→Target)
       │      └──────────────────┘
       │               │ (W / Space у вершины)
       ▼               ▼
  ┌─────────┐  ┌─────────┐
  │MantleUp │  │MantleUp │ ──► (Выход на ноги в MOVE_Walking)
  └─────────┘  └─────────┘
```

| Подсостояние | Значение | Описание |
|---|---|---|
| `None` | 0 | Вне лазания или в момент отпускания уступа |
| `IdleHang` | 1 | Стабильный вис (Braced или Freehang). Готов к прыжкам, шимми или выходу |
| `Shimmying` | 2 | Горизонтальное смещение вдоль непрерывного уступа |
| `JumpTransition` | 3 | Кинематический направленный прыжок (Short, Long, Leap) |
| `MantleUp` | 4 | Залезание на платформу по Root Motion |
| `ClimbDown` | 5 | Спуск с платформы в вис по Root Motion |
| `CornerTransition` | 6 | Огибание внешнего/внутреннего угла стены: двухсегментная дуга `Start → CornerPivot → Target` |

---

## 3. Управление (Input Mapping)

| Действие игрока | Клавиша | Поведение системы |
|---|---|---|
| **Автозахват** | В воздухе при подлёте к стене | При `Velocity.Z <= 400 см/с` хватает найденный уступ |
| **Подъем на крышу (Mantle)** | `Space` / `W` (у края крыши) | Залезание на ноги через Root Motion (`MD_climb_up_ALS1_Montage`) |
| **Прыжок вверх (Hop Up)** | `Space` / `W` (есть уступ выше) | Прыжок на верхний ярус уступа |
| **Перебор рук (Shimmy)** | Удержание `A` или `D` | Шаги вдоль непрерывной полки с кулдауном 0.55с |
| **Огибание угла (Corner Wrap)** | `A` или `D` у края стены | Если `FindSideLedge=false` и `ProbeCornerLedge=true` — двухсегментный угловой переход |
| **Прыжок в сторону** | `A + Space` / `D + Space` | Прыжок на соседний блок/уступ через разрыв стены |
| **Прыжок назад / Отскок** | `S + Space` (или `Shift + S`) | Поиск встречного уступа сзади. Если найден — прыжок назад; если нет — отскок `WallEject` |
| **Спуск с края крыши** | `C` (стоя у кромки пола) | Плавный разворот на 180° и спуск в вис на руках |
| **Спрыгивание со стены** | **Строго `C` (Crouch)** | Сброс виса, переход в падение, кулдаун повторного зацепа 1.5с |

---

## 4. Сетевая синхронизация (Replication & RPCs)

1. `CurrentTraversalState` и `CurrentClimbingSubState` (`IdleHang`, `Shimmying`, `CornerTransition`, `JumpTransition`, `MantleUp`, `ClimbDown`) реплицируются через `DOREPLIFETIME` + RepNotify (`OnRep_TraversalState`, `OnRep_ClimbingSubState`) и делегаты.
2. Клиент отправляет Server RPC: `Server_StartLedgeClimb`, `Server_LedgeJump`, `Server_StartShimmy`, `Server_ClimbUp`, `Server_ClimbDown`, `Server_WallEject`, `Server_ReleaseLedge`, `Server_StartCornerTransition`.
3. Сервер транслирует Multicast RPC для воспроизведения анимаций и перемещений на всех клиентах.
4. **Синхронная камера (Camera Follow):** Во время активной фазы разворота `CornerTransition` (0..0.55с) выделенный модуль `FThreadTalesClimbingCameraHandler` синхронно в реальном времени вращает `ControlRotation.Yaw` локального контроллера в такт кинематике тела и Root Motion. Относительный угол обзора игрока к стене сохраняется. При любом активном движении мыши игроком автосопровождение мгновенно уступает приоритет ручному управлению.

---

## 5. Анимационная матрица: Braced vs Freehang (С упором в стену vs Свободный вис)

Система поддерживает раздельные наборы анимаций для виса с опорой ногами в стену (`bIsFreehang = false`) и виса на руках без опоры (`bIsFreehang = true` — на балках, перекладинах или тонких выступах):

| Действие | Braced свойство (`UThreadTalesClimbingConfig`) | Freehang свойство (`UThreadTalesClimbingConfig`) |
|---|---|---|
| **Idle Hang** | `HangMontage` | `BarHangMontage` |
| **Climb Up (Mantle)** | `ClimbOnTopMontage` | `BarClimbOnTopMontage` |
| **Climb Down** | `ClimbDownMontage` | `BarClimbDownMontage` |
| **Shimmy L / R** | `ShimmyLeftMontage` / `ShimmyRightMontage` | `BarShimmyLeftMontage` / `BarShimmyRightMontage` |
| **Corner Outer L / R** | `CornerOuterLeftMontage` / `CornerOuterRightMontage` | `BarCornerOuterLeftMontage` / `BarCornerOuterRightMontage` |
| **Corner Inner L / R** | `CornerInnerLeftMontage` / `CornerInnerRightMontage` | `BarCornerInnerLeftMontage` / `BarCornerInnerRightMontage` |
| **Back Jump / Opposing Leap** | `ClimbBackJumpMontage` | `BarClimbBackJumpMontage` |
| **Hop Up / Hop Down** | `JumpConfigTable` (Short Up/Down) | `BarClimbHopUpMontage` / `BarClimbHopDownMontage` |
| **Jump Left / Jump Right** | `JumpConfigTable` (Short Left/Right) | `BarClimbJumpLeftMontage` / `BarClimbJumpRightMontage` |

### Иерархический Fallback (Безопасность конфигурации)
Если для какого-либо действия Freehang-монтаж не назначен (`nullptr`) в Data Asset:
1. `GetActiveBackJumpMontage(bFreehang)` возвращает `ClimbBackJumpMontage`.
2. `GetActiveShimmyMontage(bLeft, bFreehang)` возвращает `ShimmyLeftMontage` / `ShimmyRightMontage`.
3. `GetActiveCornerMontage(bLeft, CornerType, bFreehang)` сначала проверяет Freehang Corner, затем Braced Corner, и в итоге откатывается на `GetActiveShimmyMontage(bLeft, bFreehang)`.
4. `GetJumpMontage(Direction, DistanceType, bFreehang)` для `Short` дистанций берет Freehang прыжок/подскок; если он не задан — берет монтаж из `FThreadTalesJumpConfigTable`.
Это гарантирует отсутствие сбоев и зависаний даже при частичной настройке ассетов в Data Asset.

---

## 6. Дочерние разделы

* [Spatial Detection Pipeline](file:///Y:/UnrealProjects/ThreadTales/Docs/Climbing/Detection.md) — Алгоритмы сканирования, 6 фаз детекта, ClimbDown, BackJump, ProbeCornerLedge.
* [Kinematics & Jump Tables](file:///Y:/UnrealProjects/ThreadTales/Docs/Climbing/Kinematics.md) — Таблица прыжков 8 направлений, формулы скоростей и IK, двухсегментная дуга Corner.
* [Traps, Invariants & Bugfixes](file:///Y:/UnrealProjects/ThreadTales/Docs/Climbing/Traps_And_Fixes.md) — Реестр критических правил и устранённых багов.
