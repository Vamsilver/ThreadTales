# Animation System — Стандарты анимаций и интеграция с GASPALS

> **Краткая суть:** Стандарты анимаций, правила интеграции с плагином GASPALS, слоты воспроизведения и обязательные требования к UEFN-скелету.

---

## 1. Главные правила и запреты

1. **ИСКЛЮЧИТЕЛЬНО UEFN скелет:**
   * В проекте ThreadTales разрешено использовать анимации и монтажи **ТОЛЬКО на UEFN скелете**.
   * Запрещены Mixamo, UE4 Mannequin или сторонние риги без ретаргета на целевой UEFN скелет.
2. **Только `DefaultGroup.DefaultSlot`:**
   * В AnimBP плагина GASPALS (`ABP_SandboxCharacter`) выходная поза подключена через слот `DefaultGroup.DefaultSlot`.
   * Монтажи со сторонними слотами (`Ledge_Climbing` и др.) **СТРОГО ЗАПРЕЩЕНЫ** — они приводят к застыванию меша в Т-позе при перемещении капсулы по Root Motion.
3. **Ручной ретаргет и импорт:**
   * Импорт и ретаргетинг выполняет исключительно пользователь вручную через Unreal Editor. AI-агент не запускает скрипты ретаргета.

---

## 2. Интеграция с GASPALS

* **Главный Anim Blueprint:** `/GASPALS/Blueprints/ABP_SandboxCharacter` (Read-only, не модифицировать).
* **Motion Matching:** использует базы поз `UPoseSearchDatabase` и таблицы выбора `UChooserTable`.
* **Боевые позы (Overlay System):** смена стойки с оружием реализуется через оверлей-систему GASPALS (`ABP_LayerBlending`) и библиотеку `UThreadTalesMotionMatchingLibrary::SetCombatStanceActive(bool)`.

---

## 3. Стандарты настройки AnimMontage

| Параметр | Стандарт для прыжков / паркура | Стандарт для атак / комбо |
|---|---|---|
| **Слот** | `DefaultGroup.DefaultSlot` | `DefaultGroup.DefaultSlot` |
| **Blend In Time** | `0.10 с` (`HERMITE_CUBIC`) | `0.10 .. 0.15 с` |
| **Blend Out Time** | `0.20 с` (`HERMITE_CUBIC`) | `0.20 .. 0.25 с` |
| **Blend Out Trigger Time** | **`-1.0`** (не выходить досрочно) | `-1.0` (или по окну комбо) |
| **Root Motion** | Включен (`EnableRootMotion`) | Включен (`EnableRootMotion`) |
| **Foot IK** | Кривая `foot_alpha = 0.0` | По умолчанию |
