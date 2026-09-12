# Climbing System — Кинематика и таблицы прыжков (Kinematics & Jump Tables)

> **Краткая суть:** Описание 3-тирового пространства направленных прыжков (Short, Long, Leap), таблицы конфигураций `JumpConfigTable`, фаз кинематической интерполяции, формул отскока от стены и математики начального позиционирования спуска.

---

## 1. Тиры прыжков и взаимное перекрытие (Distance Tiers & Overlap)

Для исключения слепых зон на стыках дистанций диапазоны имеют гарантированный взаимный нахлёст:

| Тир прыжка | Диапазон расстояний | Номинал анимации | Длительность | Фаза касания (`FlightEnd`) |
|---|---|---|---|---|
| **Short (Короткие шаги)** | `20 .. 58 см` | `48.1 см` | `0.95 с` | `0.85` |
| **Long (Широкие шаги)** | `50 .. 110 см` | `96.2 см` | `1.00 с` | `0.85` |
| **Leap (Прыжки через пропасть)** | `85 .. 200 см` | `200.0 см` | `1.00 с` | `0.85` |

---

## 2. Структура быстрой таблицы `FThreadTalesJumpConfigTable`

Таблица `JumpConfigTable[9][3]` компилируется один раз на старте в `FThreadTalesJumpConfigTable::Build` и обеспечивает доступ за $O(1)$:

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

### Автоматические Fallback-цепочки:
* **По направлению:** Диагональ $\to$ Горизонталь $\to$ Вертикаль.
* **По дистанции:** Leap $\to$ Long $\to$ Short.
* **Защитный фоллбек виса:** если отсутствует монтаж движения вниз, используется `FallbackHangMontage`.

---

## 3. Фазы кинематического прыжка (`CalculateTransitionTransform`)

Перемещение персонажа между уступами во время монтажа делится на 3 фазы:
1. **Anticipation (Замах):** `0.0 .. AnticipationEnd (0.10)`. Персонаж подготавливается к толчку, смещение капсулы минимально.
2. **Flight (Полёт):** `AnticipationEnd .. FlightEnd (0.85)`. Капсула перемещается по гладкой кривой (с параболической дугой высоты для длинных прыжков) к целевой точке виса. В точке `0.85` кисти касаются целевого уступа.
3. **Catch & Settle (Захват):** `FlightEnd .. 1.0`. Доводка положения и гашение инерции.

В момент `BlendingOut` вызывается жесткая привязка (`SetActorLocationAndRotation(CachedHangLocation, CachedHangRotation)`), исключая сползание или рывок.

---

## 4. Математика отскока от стены (`WallEject`)

При отталкивании от стены без встречного уступа капсуле придаётся стартовая скорость:

$$\vec{V} = (\vec{N}_{wall} \times 450.0) + (\vec{Up} \times 350.0)$$

* Режим движения переключается в `MOVE_Falling` с полным сохранением воздушного контроля.
* Активируется кулдаун `WallEjectRegrabCooldown = 0.55 с`, предотвращающий мгновенный повторный захват той же стены.

---

## 5. Математика предварительного позиционирования спуска (`ClimbDown`)

Монтаж `MD_climb_down1_Montage` имеет фиксированное Root Motion смещение:
* Вперед от исходной точки: `72.2 см`
* Вниз от исходной точки: `171.32 см`

Чтобы после завершения анимации персонаж оказался **строго в целевой точке виса `CachedHangLocation`** без рывка и телепортации:

$$\vec{Start}_{XY} = \vec{Hang}_{XY} - (\vec{N}_{wall} \times 72.2)$$
$$Start_Z = Hang_Z + 171.32$$
$$Rotation = \vec{N}_{wall}.Rotation() \quad (\text{лицом к обрыву})$$

Перед запуском монтажа капсула перемещается в эти стартовые координаты, коллизия со статикой временно маскируется (`SetClimbDownCollision(true)`), и Root Motion опускает персонажа точно в вис на руках.
