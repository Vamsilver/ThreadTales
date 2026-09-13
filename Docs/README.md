# ThreadTales — Архитектурная документация и база знаний проекта

> **Назначение:** Единый структурированный реестр подсистем проекта ThreadTales для разработчиков и AI-агентов.  
> Для соблюдения строгих правил разработки и запретов см. [AGENTS.md](file:///Y:/UnrealProjects/ThreadTales/AGENTS.md).

---

## 1. Карта подсистем и документации

| Подсистема | Папка документации | Ключевые классы C++ | Ключевые ассеты / Конфиги |
|---|---|---|---|
| **Вертикальный паркур и скалолазание** | [Docs/Climbing/](file:///Y:/UnrealProjects/ThreadTales/Docs/Climbing/README.md) | `UThreadTalesTraversalComponent`, `FThreadTalesLedgeDetector`, `FThreadTalesLedgeClimbingHandler`, `FThreadTalesClimbingKinematics` | `UThreadTalesClimbingConfig`, `DA_ClimbingConfig_Default` |
| **Боевая система и GAS** | [Docs/Combat/](file:///Y:/UnrealProjects/ThreadTales/Docs/Combat/README.md) | `UThreadTalesAbilitySystemComponent`, `UThreadTalesEquipmentComponent`, `AThreadTalesWeapon`, `UThreadTalesGA_Melee`, `UThreadTalesGA_Evade` | `BP_Weapon_Katana`, `BP_Weapon_Slingshot`, `GA_Melee`, `GA_Evade` |
| **Анимации и локомоция** | [Docs/Animation/](file:///Y:/UnrealProjects/ThreadTales/Docs/Animation/README.md) | `UThreadTalesMotionMatchingLibrary` | `ABP_SandboxCharacter`, `ABP_LayerBlending`, UEFN Mannequin |
| **Сеть и мультиплеер (Co-op 2P)** | [Docs/Network/](file:///Y:/UnrealProjects/ThreadTales/Docs/Network/README.md) | `AThreadTalesCharacter`, `AThreadTalesPlayerController`, RPCs | `GM_ThreadTalesGameMode`, `BP_HeroMale`, `BP_HeroFemale` |

---

## 2. Модули системы скалолазания (Climbing System Deep Dive)

Система вертикального перемещения декомпозирована на специализированные справочники:

1. [Climbing Overview & State Machine](file:///Y:/UnrealProjects/ThreadTales/Docs/Climbing/README.md) — Обзор архитектуры, стейт-машина подсостояний, управление и жизненный цикл.
2. [Spatial Detection Pipeline](file:///Y:/UnrealProjects/ThreadTales/Docs/Climbing/Detection.md) — 6-фазный детект уступов, веерные лучи, клиренс тела, спуск с обрыва (ClimbDown), встречный уступ (BackJump).
3. [Kinematics & Jump Tables](file:///Y:/UnrealProjects/ThreadTales/Docs/Climbing/Kinematics.md) — Матрица прыжков 8 направлений $O(1)$, диапазоны Short/Long/Leap, фазы полета, формулы отскока от стены.
4. [Traps, Invariants & Bugfixes](file:///Y:/UnrealProjects/ThreadTales/Docs/Climbing/Traps_And_Fixes.md) — **КРИТИЧЕСКИЙ РЕЕСТР:** решенные баги, грабли и правила, исключающие регрессии (скручивание CMC, слоты монтажей, маскирование клавиш).

---

## 3. Быстрая навигация по кодовой базе для AI-агентов

* **Персонаж (C++ ядро):** `Source/ThreadTales/Characters/ThreadTalesCharacter.h`
* **Конфиг паркура (Data Asset):** `Source/ThreadTales/Characters/ThreadTalesClimbingConfig.h`
* **Детектор коллизий уступов:** `Source/ThreadTales/Characters/ThreadTalesLedgeDetector.h`
* **Обработчик лазания:** `Source/ThreadTales/Characters/ThreadTalesLedgeClimbingHandler.h`
* **Вспомогательный ввод:** `Source/ThreadTales/Characters/ThreadTalesTraversalInputHelper.h`
* **Gameplay Tags:** `Source/ThreadTales/Core/ThreadTalesGameplayTags.h`
