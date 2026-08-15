// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThreadTalesDummy.h"
#include "ThreadTales.h"
#include "ThreadTalesAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

AThreadTalesDummy::AThreadTalesDummy()
{
	bIsImmortal = false;
	bAutoRespawn = true;
	RespawnDelay = 5.0f;

	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;
	}
}

void AThreadTalesDummy::BeginPlay()
{
	Super::BeginPlay();

	InitialSpawnTransform = GetActorTransform();
}

float AThreadTalesDummy::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsImmortal)
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] Immortal Dummy '%s' took %.1f damage! (Immortal - Cannot Die)"), *GetName(), DamageAmount);
		HandleHealthChanged(GetMaxHealth(), GetMaxHealth());
		return DamageAmount;
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AThreadTalesDummy::Die(AActor* KillerActor)
{
	Super::Die(KillerActor);

	if (bAutoRespawn)
	{
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] '%s' will respawn in %.1f seconds..."), *GetName(), RespawnDelay);
		GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AThreadTalesDummy::Respawn, RespawnDelay, false);
	}
}

void AThreadTalesDummy::Respawn()
{
	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);

	bIsAlive = true;

	// Reset Transform
	SetActorTransform(InitialSpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);

	// Reset Health in AttributeSet
	if (AttributeSet)
	{
		const_cast<UThreadTalesAttributeSet*>(AttributeSet)->SetHealth(AttributeSet->GetMaxHealth());
		HandleHealthChanged(AttributeSet->GetMaxHealth(), AttributeSet->GetMaxHealth());
	}

	// Restore Mesh physics and animation
	if (GetMesh())
	{
		GetMesh()->SetSimulatePhysics(false);
		GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);
		GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
		GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
	}

	// Restore Capsule and Movement
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	}

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Dummy] '%s' has RESPAWNED with full health (%.1f / %.1f)!"),
		*GetName(), GetHealth(), GetMaxHealth());
}
