#include "Animation/AnimNotifyState_WeaponHitbox.h"
#include "ThreadTales.h"
#include "Characters/ThreadTalesEquipmentComponent.h"
#include "Characters/ThreadTalesDummy.h"
#include "Characters/ThreadTalesEnemyCharacter.h"
#include "Combat/ThreadTalesWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UAnimNotifyState_WeaponHitbox::UAnimNotifyState_WeaponHitbox()
{
}

void UAnimNotifyState_WeaponHitbox::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld()) return;

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	// Support sparring training dummy (activates both dummy logic and its equipped weapon hitbox)
	if (AThreadTalesDummy* Dummy = Cast<AThreadTalesDummy>(OwnerActor))
	{
		Dummy->SetHitboxActive(true);
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Hitbox Notify] Dummy '%s': BEGIN Hitbox on %s (Duration: %.3fs)"),
			*GetNameSafe(OwnerActor), *GetNameSafe(Animation), TotalDuration);
	}

	// Support natural body hitbox for melee enemies without weapons (e.g. Ladybug)
	if (AThreadTalesEnemyCharacter* Enemy = Cast<AThreadTalesEnemyCharacter>(OwnerActor))
	{
		if (bUnarmedStrike)
		{
			Enemy->SetHitboxActive(true, HitboxSocketName);
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Hitbox Notify] Enemy '%s': BEGIN Unarmed Limb Hitbox on %s (Socket: %s, Duration: %.3fs)"),
				*GetNameSafe(OwnerActor), *GetNameSafe(Animation), *HitboxSocketName.ToString(), TotalDuration);
			return;
		}

		UThreadTalesEquipmentComponent* Equip = Enemy->FindComponentByClass<UThreadTalesEquipmentComponent>();
		if (!Equip || !Equip->GetCurrentWeapon())
		{
			Enemy->SetHitboxActive(true, HitboxSocketName);
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Hitbox Notify] Enemy '%s': BEGIN Body Hitbox fallback on %s (Socket: %s, Duration: %.3fs)"),
				*GetNameSafe(OwnerActor), *GetNameSafe(Animation), *HitboxSocketName.ToString(), TotalDuration);
			return;
		}
	}

	UThreadTalesEquipmentComponent* EquipComp = OwnerActor->FindComponentByClass<UThreadTalesEquipmentComponent>();
	if (!EquipComp) return;

	if (AThreadTalesWeapon* CurrentWeapon = EquipComp->GetCurrentWeapon())
	{
		CurrentWeapon->SetHitboxEnabled(true);
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Hitbox Notify] %s: BEGIN Weapon Hitbox (%s) on %s (Duration: %.3fs)"),
			*GetNameSafe(OwnerActor), *GetNameSafe(CurrentWeapon), *GetNameSafe(Animation), TotalDuration);
	}
}

void UAnimNotifyState_WeaponHitbox::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld()) return;

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	// Support sparring training dummy
	if (AThreadTalesDummy* Dummy = Cast<AThreadTalesDummy>(OwnerActor))
	{
		Dummy->SetHitboxActive(false);
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Hitbox Notify] Dummy '%s': END Hitbox on %s"),
			*GetNameSafe(OwnerActor), *GetNameSafe(Animation));
	}

	// Support natural body hitbox for melee enemies without weapons
	if (AThreadTalesEnemyCharacter* Enemy = Cast<AThreadTalesEnemyCharacter>(OwnerActor))
	{
		if (bUnarmedStrike)
		{
			Enemy->SetHitboxActive(false, HitboxSocketName);
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Hitbox Notify] Enemy '%s': END Unarmed Limb Hitbox on %s"),
				*GetNameSafe(OwnerActor), *GetNameSafe(Animation));
			return;
		}

		UThreadTalesEquipmentComponent* Equip = Enemy->FindComponentByClass<UThreadTalesEquipmentComponent>();
		if (!Equip || !Equip->GetCurrentWeapon())
		{
			Enemy->SetHitboxActive(false, HitboxSocketName);
			UE_LOG(LogThreadTalesCombat, Display, TEXT("[Hitbox Notify] Enemy '%s': END Body Hitbox fallback on %s"),
				*GetNameSafe(OwnerActor), *GetNameSafe(Animation));
			return;
		}
	}

	UThreadTalesEquipmentComponent* EquipComp = OwnerActor->FindComponentByClass<UThreadTalesEquipmentComponent>();
	if (!EquipComp) return;

	if (AThreadTalesWeapon* CurrentWeapon = EquipComp->GetCurrentWeapon())
	{
		CurrentWeapon->SetHitboxEnabled(false);
		UE_LOG(LogThreadTalesCombat, Display, TEXT("[Hitbox Notify] %s: END Weapon Hitbox (%s) on %s"),
			*GetNameSafe(OwnerActor), *GetNameSafe(CurrentWeapon), *GetNameSafe(Animation));
	}
}

FString UAnimNotifyState_WeaponHitbox::GetNotifyName_Implementation() const
{
	if (bUnarmedStrike)
	{
		if (HitboxSocketName != NAME_None)
		{
			return FString::Printf(TEXT("Hitbox: %s"), *HitboxSocketName.ToString());
		}
		return TEXT("Hitbox (Unarmed)");
	}
	return TEXT("Weapon Hitbox");
}
