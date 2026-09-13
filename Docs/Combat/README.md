# Combat System — Боевая система и Gameplay Ability System (GAS)

> **Краткая суть:** Co-op Souls-like боевая система ближнего и дальнего боя. Основана на связке Gameplay Ability System (GAS), компонента экипировки `EquipmentComponent` и анимационных нотификаций комбо-системы.

---

## 1. Quick Reference (Быстрый паспорт)

| Параметр | Значение |
|---|---|
| **C++ ASC** | `UThreadTalesAbilitySystemComponent` (`Source/ThreadTales/Abilities/ThreadTalesAbilitySystemComponent.h`) |
| **AttributeSet** | `UThreadTalesAttributeSet` (Health, MaxHealth, Damage meta-атрибут) |
| **Управление экипировкой** | `UThreadTalesEquipmentComponent` (`Source/ThreadTales/Characters/ThreadTalesEquipmentComponent.h`) |
| **Базовый класс оружия** | `AThreadTalesWeapon` (`Source/ThreadTales/Combat/ThreadTalesWeapon.h`) |
| **Оружие ближнего боя** | `AThreadTalesWeapon_Sword` (`/Game/Weapons/BP_Weapon_Katana`) |
| **Оружие дальнего боя** | `AThreadTalesWeapon_Slingshot` (`/Game/Weapons/BP_Weapon_Slingshot`) |
| **Способность атаки** | `UThreadTalesGA_Melee` (`/Game/Blueprints/GA_Melee`) |
| **Способность уклонения** | `UThreadTalesGA_Evade` (`/Game/Blueprints/GA_Evade`) |

---

## 2. Архитектура оружия и экипировки

* `AThreadTalesWeapon` — `Abstract` класс. Спавнится только через дочерние Blueprint-классы.
* Оружие управляется исключительно через `UThreadTalesEquipmentComponent`:
  * `EquipWeapon(TSubclassOf<AThreadTalesWeapon>)` — экипировка.
  * `ToggleCombatStance()` — достать / убрать оружие (Server RPC).
  * `CurrentWeapon` реплицируется через `OnRep_CurrentWeapon`.
* При обнажении оружия подключается `WeaponMappingContext` (`Priority = 1`), перехватывающий ввод для атак и парирования.

---

## 3. Combo-система ближнего боя (`UThreadTalesGA_Melee`)

* **Секционное комбо:** до 4 ударов в монтаже `ComboAttackMontage` через переход по секциям.
* **Синхронизация через AnimNotify:**
  * `AnimNotifyState_ComboWindow` — открывает окно ввода для следующего удара.
  * `AnimNotifyState_WeaponHitbox` — включает отслеживание коллизии лезвия для нанесения урона.
  * `AnimNotify_AttachWeapon` — переприкрепляет оружие между сокетами ножен и руки.
* **Input Buffering:** нажатие кнопки за `InputBufferTolerance = 0.4 с` до открытия окна засчитывается автоматически.

---

## 4. Уклонение и I-Frames (`UThreadTalesGA_Evade`)

* **8 направлений:** направление кувырка/отскока вычисляется относительно камеры игрока.
* **I-Frames (Неуязвимость):** тег `TAG_State_Combat_Evading` добавляется в ASC в момент старта и удаляется по завершении монтажа.
* **Ранняя отмена (Early Cancel):** `DefaultEarlyCancelPercentage = 0.75` — возможность прервать хвост анимации следующим действием.

---

## 5. Пайплайн нанесения урона (GE Migration Pipeline)

> [!IMPORTANT]
> **Текущий приоритет:** Полный переход на GAS-эффекты:
> 1. Оружие накладывает `UGameplayEffect` с meta-атрибутом `Damage`.
> 2. `UGameplayEffectExecutionCalculation` рассчитывает резисты и итоговый урон.
> 3. `PostGameplayEffectExecute` в `UThreadTalesAttributeSet` изменяет `Health` и уведомляет делегаты смерти/ранения.
> Временный метод `ReceiveDamage()` не должен дополняться новой логикой.

---

## 6. Стандарты написания Gameplay Abilities

* **Таймеры:** использовать исключительно `UAbilityTask_WaitDelay` (запрещено использовать `FTimerHandle`).
* **Монтажи:** воспроизводить только через `UAbilityTask_PlayMontageAndWait` (запрещено вызывать `PlayAnimMontage` напрямую из Ability).
* **Ожидание ввода:** использовать `UAbilityTask_WaitInputPress` / `WaitInputRelease`.
* **Cost и Cooldown:** только через `UGameplayEffect`, без ручного отсчёта времени в Ability.
* **Теги:** `AbilityTags`, `CancelAbilitiesWithTag`, `BlockAbilitiesWithTag` настраивать в Blueprint-дочернем классе.

