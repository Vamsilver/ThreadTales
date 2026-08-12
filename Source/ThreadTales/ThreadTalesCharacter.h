// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "ThreadTalesCharacter.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(abstract)
class AThreadTalesCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AThreadTalesCharacter();

	/** Example variable to prove C++ connection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ThreadTales|Stats")
	float MaxHealth;

protected:
	virtual void BeginPlay() override;
};
