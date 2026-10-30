#include "Combat/ThreadTalesWeapon_Sword.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Abilities/ThreadTalesGameplayEffects.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"

static TAutoConsoleVariable<int32> CVarCombatDebugTrace(
	TEXT("tt.Combat.DebugTrace"),
	0,
	TEXT("Visualize weapon hitbox and impact traces in real-time. 0 = off, 1 = on."),
	ECVF_Cheat);

AThreadTalesWeapon_Sword::AThreadTalesWeapon_Sword()
{
	PrimaryActorTick.bCanEverTick = true; // [TICK-ALWAYS-OK]
	PrimaryActorTick.bStartWithTickEnabled = true;

	bShowDebugTrace = false;
	bHitboxActive = false;

	PrimaryAttackTag = ThreadTalesGameplayTags::TAG_Ability_Melee;

	Hitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("Hitbox"));
	Hitbox->SetupAttachment(WeaponMesh);
	Hitbox->InitBoxExtent(FVector(15.0f, 15.0f, 60.0f)); // Covers length of sword blade
	Hitbox->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f)); // Offset along the blade
	Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Disabled by default, enabled during attack anim
	Hitbox->SetGenerateOverlapEvents(true);
	Hitbox->SetCollisionObjectType(ECC_WorldDynamic);
	Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hitbox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Hitbox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);

	Hitbox->OnComponentBeginOverlap.AddDynamic(this, &AThreadTalesWeapon_Sword::OnHitboxOverlap);

	BaseDamage = 35.0f;
	BasePoiseDamage = 30.0f;
	DamageEffectClass = UThreadTalesGE_Damage::StaticClass();
}

void AThreadTalesWeapon_Sword::BeginPlay()
{
	Super::BeginPlay();
}

bool AThreadTalesWeapon_Sword::ShouldDrawDebug() const
{
	return bShowDebugTrace || (CVarCombatDebugTrace.GetValueOnGameThread() > 0);
}

void AThreadTalesWeapon_Sword::SetHitboxEnabled(bool bEnabled)
{
	bHitboxActive = bEnabled;

	if (bEnabled)
	{
		HitActorsThisSwing.Empty();
		Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Hitbox->UpdateOverlaps();

		TArray<AActor*> OverlappingActors;
		Hitbox->GetOverlappingActors(OverlappingActors, ACharacter::StaticClass());
		for (AActor* OverlappedActor : OverlappingActors)
		{
			if (OverlappedActor && OverlappedActor != GetOwner())
			{
				FHitResult DummyHit;
				OnHitboxOverlap(Hitbox, OverlappedActor, OverlappedActor->FindComponentByClass<UPrimitiveComponent>(), INDEX_NONE, false, DummyHit);
			}
		}

		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Sword] Hitbox ENABLED for attack swing (Owner: %s)"), *GetNameSafe(GetOwner()));
	}
	else
	{
		Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HitActorsThisSwing.Empty();
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Sword] Hitbox DISABLED (Owner: %s)"), *GetNameSafe(GetOwner()));
	}
}

void AThreadTalesWeapon_Sword::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bHitboxActive && ShouldDrawDebug() && GetWorld())
	{
		// 1. Draw Hitbox volume (Cyan during swing)
		DrawDebugBox(
			GetWorld(),
			Hitbox->GetComponentLocation(),
			Hitbox->GetScaledBoxExtent(),
			Hitbox->GetComponentQuat(),
			FColor::Cyan,
			false,
			-1.0f,
			0,
			1.5f
		);

		// 2. Draw blade center trace line (Yellow from base to tip)
		if (WeaponMesh)
		{
			FVector BladeBase = WeaponMesh->GetComponentLocation();
			FVector BladeTip = BladeBase + WeaponMesh->GetUpVector() * (Hitbox->GetScaledBoxExtent().Z * 2.0f);
			DrawDebugLine(GetWorld(), BladeBase, BladeTip, FColor::Yellow, false, -1.0f, 0, 2.0f);
		}
	}
}

void AThreadTalesWeapon_Sword::OnHitboxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	if (OtherActor && OtherActor != GetOwner() && !HitActorsThisSwing.Contains(OtherActor))
	{
		HitActorsThisSwing.Add(OtherActor);

		if (ShouldDrawDebug() && GetWorld())
		{
			FVector HitLoc = SweepResult.bBlockingHit ? FVector(SweepResult.ImpactPoint) : (OtherComp ? OtherComp->GetComponentLocation() : OtherActor->GetActorLocation());
			DrawDebugSphere(GetWorld(), HitLoc, 16.0f, 16, FColor::Red, false, 2.5f, 0, 2.0f);
			DrawDebugLine(GetWorld(), Hitbox->GetComponentLocation(), HitLoc, FColor::Red, false, 2.5f, 0, 2.5f);
			DrawDebugString(GetWorld(), HitLoc + FVector(0, 0, 30.0f), FString::Printf(TEXT("HIT: %s\n-%.0f HP | Poise -%.0f"), *OtherActor->GetName(), BaseDamage, BasePoiseDamage), nullptr, FColor::Green, 2.5f, true, 1.2f);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Orange, FString::Printf(TEXT("[Combat Hit] '%s' hit '%s' for %.0f dmg!"), *GetNameSafe(GetOwner()), *OtherActor->GetName(), BaseDamage));
			}
		}

		ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
		AController* InstigatorController = OwnerChar ? OwnerChar->GetController() : nullptr;

		// 1. Check if Target has an AbilitySystemComponent
		UAbilitySystemComponent* TargetASC = nullptr;
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OtherActor))
		{
			TargetASC = ASI->GetAbilitySystemComponent();
		}
		if (!TargetASC)
		{
			TargetASC = OtherActor->FindComponentByClass<UAbilitySystemComponent>();
		}

		UAbilitySystemComponent* SourceASC = OwnerChar ? OwnerChar->FindComponentByClass<UAbilitySystemComponent>() : nullptr;

		if (TargetASC && DamageEffectClass)
		{
			FGameplayEffectContextHandle Context = SourceASC ? SourceASC->MakeEffectContext() : TargetASC->MakeEffectContext();
			Context.AddInstigator(OwnerChar, this);
			Context.AddHitResult(SweepResult);

			FGameplayEffectSpecHandle SpecHandle = SourceASC ?
				SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, Context) :
				FGameplayEffectSpecHandle(new FGameplayEffectSpec(DamageEffectClass->GetDefaultObject<UGameplayEffect>(), Context, 1.0f));

			if (SpecHandle.IsValid())
			{
				SpecHandle.Data->SetSetByCallerMagnitude(ThreadTalesGameplayTags::TAG_SetByCaller_Damage, BaseDamage);
				SpecHandle.Data->SetSetByCallerMagnitude(ThreadTalesGameplayTags::TAG_SetByCaller_PoiseDamage, BasePoiseDamage);

				TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

				UE_LOG(LogThreadTalesCombat, Display, TEXT("[Sword] Hit %s applying GAS Damage=%.1f, PoiseDamage=%.1f!"),
					*OtherActor->GetName(), BaseDamage, BasePoiseDamage);
			}
		}
		else
		{
			// Fallback: PointDamage for non-GAS actors
			UGameplayStatics::ApplyPointDamage(OtherActor, BaseDamage, SweepResult.ImpactNormal, SweepResult, InstigatorController, this, UDamageType::StaticClass());
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Sword] Hit %s dealing %.1f damage via ApplyPointDamage!"), *OtherActor->GetName(), BaseDamage);
		}
	}
}
