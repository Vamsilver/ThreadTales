#include "Combat/ThreadTalesProjectile.h"
#include "ThreadTales.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemInterface.h"
#include "Abilities/ThreadTalesGameplayEffects.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "GameFramework/Character.h"

AThreadTalesProjectile::AThreadTalesProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Sphere collision setup with BlockAllDynamic profile
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(10.0f);
	CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComp->SetNotifyRigidBodyCollision(true);
	CollisionComp->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.f));
	CollisionComp->CanCharacterStepUpOn = ECB_No;
	RootComponent = CollisionComp;

	// Visual mesh
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(RootComponent);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Heavy solid stone ballistic physics
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 2200.f;
	ProjectileMovement->MaxSpeed = 4500.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.30f; // Solid rock: 1-2 heavy thuds, not rubbery
	ProjectileMovement->Friction = 0.65f;   // Realistic rough stone friction
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = 50.0f;
	ProjectileMovement->bBounceAngleAffectsFriction = true;
	ProjectileMovement->ProjectileGravityScale = 1.3f; // Heavy stone ballistic drop

	BaseDamage = 25.0f;
	InitialLifeSpan = 4.0f;
	MaxBounces = 3;
	CurrentBounceCount = 0;
	bHasDealtDamage = false;
	DamageEffectClass = UThreadTalesGE_Damage::StaticClass();
}

void AThreadTalesProjectile::BeginPlay()
{
	Super::BeginPlay();

	CollisionComp->OnComponentHit.AddDynamic(this, &AThreadTalesProjectile::OnHit);
}

void AThreadTalesProjectile::InitializeProjectile(float ChargeAlpha, float InBaseDamage)
{
	BaseDamage = InBaseDamage;

	// Speed scales with charge (1500 cm/s up to 4000 cm/s)
	float ScaledSpeed = FMath::Lerp(1500.0f, 4000.0f, ChargeAlpha);
	
	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = ScaledSpeed;
		ProjectileMovement->MaxSpeed = ScaledSpeed;
		ProjectileMovement->Velocity = GetActorForwardVector() * ScaledSpeed;
	}

	UE_LOG(LogThreadTalesCombat, Log, TEXT("[Projectile] Spawned with Charge: %.2f, Speed: %.1f, Damage: %.1f"), ChargeAlpha, ScaledSpeed, BaseDamage);
}

void AThreadTalesProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Spawn impact effects
	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, Hit.ImpactPoint);
	}

	// Apply damage to hit actor (if not already damaged and is a damageable entity / pawn)
	if (!bHasDealtDamage && OtherActor && OtherActor != this && OtherActor != GetOwner())
	{
		if (Cast<APawn>(OtherActor) || OtherActor->CanBeDamaged())
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
					SpecHandle.Data->SetSetByCallerMagnitude(ThreadTalesGameplayTags::TAG_SetByCaller_PoiseDamage, BaseDamage * 0.5f);

					TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

					UE_LOG(LogThreadTalesCombat, Display, TEXT("[Projectile] Hit %s applying GAS Damage=%.1f!"),
						*OtherActor->GetName(), BaseDamage);
				}
			}
			else
			{
				// Fallback: PointDamage for non-GAS actors / world destruction
				AController* InstigatorController = GetInstigatorController();
				UGameplayStatics::ApplyPointDamage(OtherActor, BaseDamage, Hit.ImpactNormal, Hit, InstigatorController, this, UDamageType::StaticClass());
				UE_LOG(LogThreadTalesCombat, Display, TEXT("[Projectile] Hit non-GAS %s dealing %.1f damage via ApplyPointDamage!"),
					*OtherActor->GetName(), BaseDamage);
			}

			// Destroy projectile immediately on direct character hit
			if (Cast<APawn>(OtherActor))
			{
				Destroy();
				return;
			}
		}
	}

	CurrentBounceCount++;
	if (CurrentBounceCount >= MaxBounces)
	{
		// Let it stay on the ground briefly before despawning
		SetLifeSpan(1.5f);
	}
}
