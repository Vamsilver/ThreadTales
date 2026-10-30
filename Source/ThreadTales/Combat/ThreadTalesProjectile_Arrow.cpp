// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ThreadTalesProjectile_Arrow.h"
#include "ThreadTales.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemInterface.h"
#include "Abilities/ThreadTalesGameplayEffects.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"

AThreadTalesProjectile_Arrow::AThreadTalesProjectile_Arrow()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Slim precision collision proxy for arrow shaft and tip
	if (CollisionComp)
	{
		CollisionComp->InitSphereRadius(4.0f);
		CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CollisionComp->SetNotifyRigidBodyCollision(true);
		CollisionComp->SetUseCCD(true);
	}

	// Visual arrow mesh: authentic Archer pack mesh with tip positioned at collision origin
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ArrowMeshFinder(TEXT("/Game/Archer/Demo/Characters/Mannequins/Meshes/Arrow.Arrow"));
	if (ArrowMeshFinder.Succeeded() && ProjectileMesh)
	{
		ProjectileMesh->SetStaticMesh(ArrowMeshFinder.Object);
		ProjectileMesh->SetRelativeScale3D(FVector::OneVector);
		ProjectileMesh->SetRelativeLocation(FVector(-93.52f, 0.0f, 0.0f));
		ProjectileMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	// Arrow ballistics: high initial velocity, realistic drop arc, strictly NO bounce
	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = 5000.0f;
		ProjectileMovement->MaxSpeed = 8000.0f;
		ProjectileMovement->bRotationFollowsVelocity = true;
		ProjectileMovement->bInitialVelocityInLocalSpace = false;
		ProjectileMovement->bShouldBounce = false;
		ProjectileMovement->ProjectileGravityScale = 0.08f;
	}

	BaseDamage = 50.0f;
	PoiseDamage = 25.0f;
	PenetrationDepth = 12.0f;
	InitialLifeSpan = 15.0f;
	bHasStruck = false;
	DamageEffectClass = UThreadTalesGE_Damage::StaticClass();
}

void AThreadTalesProjectile_Arrow::BeginPlay()
{
	Super::BeginPlay();

	if (ProjectileMesh && !ProjectileMesh->GetStaticMesh())
	{
		if (UStaticMesh* ArrowMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Archer/Demo/Characters/Mannequins/Meshes/Arrow.Arrow")))
		{
			ProjectileMesh->SetStaticMesh(ArrowMesh);
			ProjectileMesh->SetRelativeScale3D(FVector::OneVector);
			ProjectileMesh->SetRelativeLocation(FVector(-93.52f, 0.0f, 0.0f));
			ProjectileMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		}
	}
}

void AThreadTalesProjectile_Arrow::InitializeArrow(float DrawRatio, float InBaseDamage, float InPoiseDamage, AActor* InShooter, const FVector& InLaunchDirection)
{
	SetOwner(InShooter);
	SetInstigator(Cast<APawn>(InShooter));

	const float ClampedRatio = FMath::Clamp(DrawRatio, 0.0f, 1.0f);
	const float SpeedAlpha = FMath::Pow(ClampedRatio, 1.2f);

	// Velocity scaling: 1400 cm/s (low speed arc) to 3600 cm/s (punchy full tension, never bullet-like)
	const float ScaledSpeed = FMath::Lerp(MinFlightSpeed, MaxFlightSpeed, SpeedAlpha);

	// Dynamic ballistic gravity: underdrawn arrows drop naturally in high parabolic arcs
	const float ScaledGravity = FMath::Lerp(MinDrawGravityScale, MaxDrawGravityScale, SpeedAlpha);

	// Damage scaling: scale base damage and poise damage by draw tension
	BaseDamage = InBaseDamage * FMath::Lerp(0.35f, 1.0f, SpeedAlpha);
	PoiseDamage = InPoiseDamage * FMath::Lerp(0.35f, 1.0f, SpeedAlpha);

	const FVector NormalizedDir = InLaunchDirection.IsNearlyZero() ? GetActorForwardVector() : InLaunchDirection.GetSafeNormal();
	SetActorRotation(NormalizedDir.Rotation());

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = ScaledSpeed;
		ProjectileMovement->MaxSpeed = ScaledSpeed;
		ProjectileMovement->bInitialVelocityInLocalSpace = false;
		ProjectileMovement->Velocity = NormalizedDir * ScaledSpeed;
		ProjectileMovement->ProjectileGravityScale = ScaledGravity;
		ProjectileMovement->bRotationFollowsVelocity = true;
	}

	// Ignore shooter pawn, skeletal meshes, and attached weapons
	if (InShooter && CollisionComp)
	{
		CollisionComp->IgnoreActorWhenMoving(InShooter, true);
		TArray<UPrimitiveComponent*> ShooterComps;
		InShooter->GetComponents<UPrimitiveComponent>(ShooterComps);
		for (UPrimitiveComponent* Comp : ShooterComps)
		{
			CollisionComp->IgnoreComponentWhenMoving(Comp, true);
		}

		TArray<AActor*> AttachedActors;
		InShooter->GetAttachedActors(AttachedActors, true, true);
		for (AActor* AttActor : AttachedActors)
		{
			CollisionComp->IgnoreActorWhenMoving(AttActor, true);
			TArray<UPrimitiveComponent*> AttComps;
			AttActor->GetComponents<UPrimitiveComponent>(AttComps);
			for (UPrimitiveComponent* Comp : AttComps)
			{
				CollisionComp->IgnoreComponentWhenMoving(Comp, true);
			}
		}
	}

	UE_LOG(LogThreadTalesCombat, Display, TEXT("[Arrow] Spawned by %s | Draw: %.2f | Speed: %.0f | Dmg: %.1f | Poise: %.1f | Dir: %s"),
		*GetNameSafe(InShooter), DrawRatio, ScaledSpeed, BaseDamage, PoiseDamage, *NormalizedDir.ToString());
}

void AThreadTalesProjectile_Arrow::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHasStruck)
	{
		return;
	}

	// Co-op friendly fire guard: do not hit shooter or friendly player in 2P co-op
	if (OtherActor && OtherActor == GetOwner())
	{
		return;
	}

	bHasStruck = true;

	// 1. Play impact effects (SFX & VFX)
	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, Hit.ImpactPoint);
	}

	if (ImpactNiagara)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactNiagara, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
	}

	// 2. Apply Combat Damage & Poise to Hit Target via GAS
	if (OtherActor && OtherActor != this && !bHasDealtDamage)
	{
		bHasDealtDamage = true;

		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor);
		if (!TargetASC)
		{
			if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OtherActor))
			{
				TargetASC = ASI->GetAbilitySystemComponent();
			}
		}

		AActor* Shooter = GetOwner();
		ACharacter* ShooterChar = Cast<ACharacter>(Shooter);
		UAbilitySystemComponent* SourceASC = ShooterChar ? ShooterChar->FindComponentByClass<UAbilitySystemComponent>() : nullptr;

		if (TargetASC && DamageEffectClass)
		{
			FGameplayEffectContextHandle Context = SourceASC ? SourceASC->MakeEffectContext() : TargetASC->MakeEffectContext();
			Context.AddInstigator(ShooterChar, this);
			Context.AddHitResult(Hit);

			FGameplayEffectSpecHandle SpecHandle = SourceASC ?
				SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, Context) :
				FGameplayEffectSpecHandle(new FGameplayEffectSpec(DamageEffectClass->GetDefaultObject<UGameplayEffect>(), Context, 1.0f));

			if (SpecHandle.IsValid())
			{
				SpecHandle.Data->SetSetByCallerMagnitude(ThreadTalesGameplayTags::TAG_SetByCaller_Damage, BaseDamage);
				SpecHandle.Data->SetSetByCallerMagnitude(ThreadTalesGameplayTags::TAG_SetByCaller_PoiseDamage, PoiseDamage);

				TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

				UE_LOG(LogThreadTalesCombat, Display, TEXT("[Arrow] Struck %s applying GAS Damage=%.1f, PoiseDamage=%.1f!"),
					*OtherActor->GetName(), BaseDamage, PoiseDamage);
			}
		}
		else
		{
			// Fallback: PointDamage for non-GAS actors / world destruction
			AController* InstigatorController = GetInstigatorController();
			UGameplayStatics::ApplyPointDamage(OtherActor, BaseDamage, Hit.ImpactNormal, Hit, InstigatorController, this, UDamageType::StaticClass());
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Arrow] Struck non-GAS actor %s dealing %.1f damage via ApplyPointDamage!"),
				*OtherActor->GetName(), BaseDamage);
		}
	}

	// 3. Arrow Sticking / Embedding Mechanic
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Velocity = FVector::ZeroVector;
		ProjectileMovement->Deactivate();
	}

	if (CollisionComp)
	{
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionComp->SetSimulatePhysics(false);
	}

	// Advance slightly into surface along flight direction to embed the arrowhead securely
	const FVector EmbedLocation = Hit.ImpactPoint + (GetActorForwardVector() * PenetrationDepth);
	SetActorLocation(EmbedLocation);

	// Attach to dynamic character mesh or physics actor (keeps arrows stuck to moving enemies)
	if (ACharacter* HitChar = Cast<ACharacter>(OtherActor))
	{
		if (USkeletalMeshComponent* Mesh = HitChar->GetMesh())
		{
			AttachToComponent(Mesh, FAttachmentTransformRules::KeepWorldTransform, Hit.BoneName != NAME_None ? Hit.BoneName : NAME_None);
		}
		else
		{
			AttachToActor(HitChar, FAttachmentTransformRules::KeepWorldTransform);
		}
	}
	else if (OtherComp && OtherComp->IsSimulatingPhysics())
	{
		AttachToComponent(OtherComp, FAttachmentTransformRules::KeepWorldTransform, Hit.BoneName);
	}

	// Disable auto-despawn timer so arrows remain embedded until queue limit is reached
	SetLifeSpan(0.0f);

	// 4. FIFO Embedded Arrows Queue: retain up to MaxEmbeddedArrows (10), recycling the oldest
	static TArray<TWeakObjectPtr<AThreadTalesProjectile_Arrow>> EmbeddedArrowsQueue;
	EmbeddedArrowsQueue.RemoveAll([](const TWeakObjectPtr<AThreadTalesProjectile_Arrow>& WeakArrow) { return !WeakArrow.IsValid(); });
	while (EmbeddedArrowsQueue.Num() >= MaxEmbeddedArrows)
	{
		if (AThreadTalesProjectile_Arrow* OldestArrow = EmbeddedArrowsQueue[0].Get())
		{
			OldestArrow->Destroy();
		}
		EmbeddedArrowsQueue.RemoveAt(0);
	}
	EmbeddedArrowsQueue.Add(this);
}

void AThreadTalesProjectile_Arrow::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}
