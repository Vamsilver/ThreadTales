# Network Architecture — Сеть, репликация и Co-op 2 игрока

> **Краткая суть:** Сетевая модель проекта ThreadTales, правила синхронизации персонажей в Co-op (Listen/Dedicated Server) и ограничения для будущей совместимости со split-screen.

---

## 1. Модель игроков

* **Игрок 1:** `BP_HeroMale` (`/Game/Blueprints/BP_HeroMale`).
* **Игрок 2:** `BP_HeroFemale` (`/Game/Blueprints/BP_HeroFemale`).
* Оба персонажа наследуются от `BP_ThreadTalesCharacter` $\to$ `CBP_SandboxCharacter` $\to$ `AThreadTalesCharacter` (C++).
* Архитектура C++ полностью симметрична и рассчитана на 2 игроков.

---

## 2. Стандарты репликации

* **Серверная валидация:** спавн акторов, нанесение урона, выдача абилок (GiveAbility) — строго под `if (HasAuthority())`.
* **RPC цепочки действий клиента:**
  1. Локальный клиент вызывает действие и отправляет `UFUNCTION(Server, Reliable)` RPC на сервер.
  2. Сервер валидирует состояние и вызывает `UFUNCTION(NetMulticast, Reliable)` RPC для синхронизации на удалённых клиентах.
* **Свойства и OnRep:**
  * `CurrentWeapon` $\to$ `OnRep_CurrentWeapon`.
  * `WeaponState` $\to$ `OnRep_WeaponState`.
  * `CurrentTraversalState` $\to$ `OnRep_TraversalState`.
  * `bIsAlive` $\to$ `OnRep_IsAlive`.

---

## 3. Архитектурные ограничения для split-screen (Future-Proofing)

Чтобы в будущем можно было добавить локальный split-screen на одном экране без переписывания архитектуры, действуют жесткие правила:

1. **Запрет `GetPlayerController(0)`:** никогда не запрашивать контроллер по нулевому индексу. Всегда использовать `GetController()` на персонаже или `GetOwningPlayer()` в UI.
2. **Запрет глобальных синглтонов камеры/HUD:** камера и интерфейс строго привязаны к конкретному `APlayerController`.
3. **Создание UI:** только через `CreateWidget(PlayerController, ...)`, никогда через `CreateWidget(GetWorld(), ...)`.
4. **Чтение ввода:** не обращаться напрямую к `GEngine->GameViewport`.
