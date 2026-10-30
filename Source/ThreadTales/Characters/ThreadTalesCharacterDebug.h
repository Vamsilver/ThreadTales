// Copyright ThreadTales. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ThreadTalesCharacterDebug.generated.h"

class AThreadTalesCharacter;

/**
 * Static debug utility library for AThreadTalesCharacter diagnostics and runtime hot-switching.
 */
UCLASS()
class THREADTALES_API UThreadTalesCharacterDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Switches current character to the specified class */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Debug")
	static void SwitchCharacter(AThreadTalesCharacter* SourceCharacter, TSubclassOf<AThreadTalesCharacter> NewCharacterClass);

	/** Switch to Male character */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Debug")
	static void SwitchToMale(AThreadTalesCharacter* SourceCharacter);

	/** Switch to Female character */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Debug")
	static void SwitchToFemale(AThreadTalesCharacter* SourceCharacter);

	/** Toggles between Male and Female characters */
	UFUNCTION(BlueprintCallable, Category = "ThreadTales|Debug")
	static void ToggleGender(AThreadTalesCharacter* SourceCharacter);

	/** Resolves Male character class (/Game/Blueprints/BP_HeroMale.BP_HeroMale_C) */
	static TSubclassOf<AThreadTalesCharacter> GetMaleClass();

	/** Resolves Female character class (/Game/Blueprints/BP_HeroFemale.BP_HeroFemale_C) */
	static TSubclassOf<AThreadTalesCharacter> GetFemaleClass();

	/** Sets explicit debug classes */
	static void SetDebugClasses(TSubclassOf<AThreadTalesCharacter> InMaleClass, TSubclassOf<AThreadTalesCharacter> InFemaleClass);

private:
	static TSubclassOf<AThreadTalesCharacter> CachedDebugMaleClass;
	static TSubclassOf<AThreadTalesCharacter> CachedDebugFemaleClass;
};
