// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ThreadTalesUIContext.h"
#include "Framework/Application/IInputProcessor.h"
#include "ThreadTalesUIInputSubsystem.generated.h"

class FThreadTalesCentralInputProcessor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnThreadTalesInputDeviceChanged, int32, PlayerSlotIndex, EThreadTalesInputDeviceType, DeviceType);

/**
 * Centralized GameInstance Subsystem for UI Input Pipeline and Device Routing in ThreadTales.
 * Eliminates Windows Slate User 0 monopoly conflicts by routing physical inputs
 * (KBM, Gamepad 0, Gamepad 1) directly to deterministic player slots (Slot 0, Slot 1)
 * and forwarding high-level semantic actions to the top-most active IThreadTalesUIContext.
 */
UCLASS()
class THREADTALES_API UThreadTalesUIInputSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UThreadTalesUIInputSubsystem();

	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

	/** Pushes an active UI context to the top of the stack */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UIInput")
	void PushContext(TScriptInterface<IThreadTalesUIContext> Context);

	/** Pops a specific UI context from the stack */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UIInput")
	void PopContext(TScriptInterface<IThreadTalesUIContext> Context);

	/** Returns the currently active top-most UI context */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UIInput")
	TScriptInterface<IThreadTalesUIContext> GetCurrentContext() const;

	/** Clears all registered contexts */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UIInput")
	void ClearContextStack();

	/** Resolves which player slot index (0 for P1, 1 for P2) owns the given physical input event */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UIInput")
	int32 ResolvePlayerSlotForInput(int32 UserIndex, const FKey& Key, bool bIsGamepad) const;

	/** Dispatches a discrete navigation direction to the active context */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UIInput")
	bool DispatchNavigation(int32 PlayerSlotIndex, EThreadTalesNavDirection Direction);

	/** Dispatches a semantic action (Accept, Cancel, etc.) to the active context */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UIInput")
	bool DispatchAction(int32 PlayerSlotIndex, EThreadTalesUIAction Action);

	/** Dispatches analog stick movement to the active context */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UIInput")
	bool DispatchAnalog(int32 PlayerSlotIndex, const FVector2D& AxisValue);

	/** Returns the current continuous left analog stick vector (-1..1) for the given player slot */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UIInput")
	FVector2D GetPlayerLeftStickAxis(int32 PlayerSlotIndex) const;

	/** Updates stored continuous stick axis for the slot */
	void UpdatePlayerLeftStickAxis(int32 PlayerSlotIndex, const FVector2D& NewAxis);

	/** Returns the currently detected input device type for the given player slot */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UIInput")
	EThreadTalesInputDeviceType GetActiveDeviceForPlayer(int32 PlayerSlotIndex) const;

	/** Manually overrides device assignment for a slot */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|UIInput")
	void SetPlayerDevice(int32 PlayerSlotIndex, EThreadTalesInputDeviceType DeviceType);

	/** Returns true if the central input processor is currently registered and active */
	UFUNCTION(BlueprintPure, Category = "ThreadTales|UIInput")
	bool IsInputProcessorActive() const;

	/** Fired when a player's active input device changes (e.g. KBM <-> Gamepad) */
	UPROPERTY(BlueprintAssignable, Category = "ThreadTales|UIInput")
	FOnThreadTalesInputDeviceChanged OnInputDeviceChanged;

protected:
	/** Stack of active UI contexts (last element is top) */
	UPROPERTY()
	TArray<TScriptInterface<IThreadTalesUIContext>> ContextStack;

	/** Detected devices per player slot */
	EThreadTalesInputDeviceType Slot0Device = EThreadTalesInputDeviceType::KeyboardMouse;
	EThreadTalesInputDeviceType Slot1Device = EThreadTalesInputDeviceType::Gamepad;

	/** Continuous left analog stick values per slot */
	FVector2D LeftStickAxes[2] = { FVector2D::ZeroVector, FVector2D::ZeroVector };

	/** Global Slate input pre-processor */
	TSharedPtr<FThreadTalesCentralInputProcessor> CentralInputProcessor;

	friend class FThreadTalesCentralInputProcessor;
};

/**
 * Slate Input Pre-Processor owned by UThreadTalesUIInputSubsystem.
 * Intercepts low-level hardware inputs and delegates them to the subsystem.
 */
class THREADTALES_API FThreadTalesCentralInputProcessor : public IInputProcessor
{
public:
	explicit FThreadTalesCentralInputProcessor(UThreadTalesUIInputSubsystem* InSubsystem);
	virtual ~FThreadTalesCentralInputProcessor() override = default;

	//~ Begin IInputProcessor Interface
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return false; }
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return false; }
	//~ End IInputProcessor Interface

private:
	TWeakObjectPtr<UThreadTalesUIInputSubsystem> Subsystem;
	double LastAnalogNavTime[2] = { 0.0, 0.0 };
	FVector2D CurrentLeftStick[2] = { FVector2D::ZeroVector, FVector2D::ZeroVector };
};
