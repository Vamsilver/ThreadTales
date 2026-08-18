#include "Combat/ThreadTalesWeapon_Sword.h"
#include "ThreadTales.h"
#include "Core/ThreadTalesGameplayTags.h"
#include "Components/BoxComponent.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

AThreadTalesWeapon_Sword::AThreadTalesWeapon_Sword()
{
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
}

void AThreadTalesWeapon_Sword::BeginPlay()
{
	Super::BeginPlay();
}

void AThreadTalesWeapon_Sword::SetHitboxEnabled(bool bEnabled)
{
	if (bEnabled)
	{
		HitActorsThisSwing.Empty();
		Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		UE_LOG(LogThreadTalesCombat, Verbose, TEXT("[Sword] Hitbox ENABLED for attack swing"));
	}
	else
	{
		Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HitActorsThisSwing.Empty();
		UE_LOG(LogThreadTalesCombat, Verbose, TEXT("[Sword] Hitbox DISABLED"));
	}
}

void AThreadTalesWeapon_Sword::OnHitboxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	if (OtherActor && OtherActor != GetOwner() && !HitActorsThisSwing.Contains(OtherActor))
	{
		HitActorsThisSwing.Add(OtherActor);

		ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
		AController* InstigatorController = OwnerChar ? OwnerChar->GetController() : nullptr;

		UGameplayStatics::ApplyPointDamage(OtherActor, BaseDamage, SweepResult.ImpactNormal, SweepResult, InstigatorController, this, UDamageType::StaticClass());

		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Sword] Hit %s dealing %.1f damage!"), *OtherActor->GetName(), BaseDamage);
	}
}
