#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "ARPGDemo.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UTextRenderComponent;
class UTextBlock;
class UVerticalBox;
class UProgressBar;
class UARPGAttackAbility;

#define ARPG_ATTRIBUTE(Name) \
 GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UARPGAttributes, Name) \
 GAMEPLAYATTRIBUTE_VALUE_GETTER(Name) \
 GAMEPLAYATTRIBUTE_VALUE_SETTER(Name) \
 GAMEPLAYATTRIBUTE_VALUE_INITTER(Name)

UCLASS()
class MYCPPPROJECT_API UARPGAttributes : public UAttributeSet
{
 GENERATED_BODY()
public:
 UPROPERTY(BlueprintReadOnly, Category="ARPG") FGameplayAttributeData Health;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") FGameplayAttributeData MaxHealth;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") FGameplayAttributeData Energy;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") FGameplayAttributeData Guard;
 ARPG_ATTRIBUTE(Health)
 ARPG_ATTRIBUTE(MaxHealth)
 ARPG_ATTRIBUTE(Energy)
 ARPG_ATTRIBUTE(Guard)
 virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};

UCLASS()
class MYCPPPROJECT_API UARPGHealthEffect : public UGameplayEffect
{
 GENERATED_BODY()
public: UARPGHealthEffect();
};
UCLASS()
class MYCPPPROJECT_API UARPGEnergyEffect : public UGameplayEffect
{
 GENERATED_BODY()
public: UARPGEnergyEffect();
};
UCLASS()
class MYCPPPROJECT_API UARPGGuardEffect : public UGameplayEffect
{
 GENERATED_BODY()
public: UARPGGuardEffect();
};

/** Designers can tune each hero without modifying combat code. */
UCLASS(BlueprintType)
class MYCPPPROJECT_API UARPGHeroDefinition : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero") FText DisplayName;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero") bool bHeavyStyle = false;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero", meta=(ClampMin="1")) float MaxHealth = 220;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero", meta=(ClampMin="1")) float WalkSpeed = 620;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero", meta=(ClampMin="1")) float AttackDamage = 24;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero", meta=(ClampMin="0.1")) float AttackInterval = 0.38f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hero") FLinearColor Accent = FLinearColor(0.1f,0.7f,1.f);
};

UENUM(BlueprintType)
enum class EARPGAttack : uint8 { Light, Heavy, Burst, Dodge };

UCLASS()
class MYCPPPROJECT_API UARPGAttackAbility : public UGameplayAbility
{
 GENERATED_BODY()
public:
 UARPGAttackAbility();
 virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
  const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
 virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
  const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;
 void Finish();
};

UCLASS(Blueprintable)
class MYCPPPROJECT_API AARPGUnit : public ACharacter, public IAbilitySystemInterface
{
 GENERATED_BODY()
public:
 AARPGUnit();
 virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS") TObjectPtr<UAbilitySystemComponent> AbilitySystem;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS") TObjectPtr<UARPGAttributes> Attributes;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGHeroDefinition> Definition;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ARPG") bool bEnemy = false;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ARPG") bool bElite = false;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") bool bDead = false;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") bool bBenched = false;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") int32 Combo = 0;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") float ChargeStarted = -1;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") float BrokenUntil = 0;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") float BurstReadyAt = 0;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") float DodgeReadyAt = 0;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") EARPGAttack RequestedAttack = EARPGAttack::Light;
 UFUNCTION(BlueprintCallable, Category="ARPG") bool RequestAttack(EARPGAttack Attack);
 UFUNCTION(BlueprintCallable, Category="ARPG") void BeginCharge();
 UFUNCTION(BlueprintCallable, Category="ARPG") void ReleaseCharge();
 UFUNCTION(BlueprintCallable, Category="ARPG") void ReceiveCombatHit(AARPGUnit* Source, float Damage, float BreakPower);
 UFUNCTION(BlueprintPure, Category="ARPG") float HealthFraction() const;
 UFUNCTION(BlueprintPure, Category="ARPG") bool IsAttacking() const;
 UFUNCTION(BlueprintPure, Category="ARPG") bool IsHeavy() const;
 UFUNCTION(BlueprintPure, Category="ARPG") FText HeroName() const;
 void SetBenched(bool bValue);
 void StartAttack(UARPGAttackAbility* Ability);
 void StopAttack();
 void HandleHealthChanged();
 void ApplyAttributeDelta(TSubclassOf<UGameplayEffect> Effect, float Delta, AARPGUnit* Source = nullptr);
 void ResolveHit();
 void UpdateEnemy(float DeltaSeconds);
 void UpdateLabel();
 float GetChargeFraction() const;
private:
 UPROPERTY() TObjectPtr<USpringArmComponent> CameraBoom;
 UPROPERTY() TObjectPtr<UCameraComponent> Camera;
 UPROPERTY() TObjectPtr<UTextRenderComponent> Label;
 UPROPERTY() TObjectPtr<UARPGAttackAbility> CurrentAbility;
 UPROPERTY() TArray<TObjectPtr<UAnimSequence>> AttackAnimations;
 UPROPERTY() TObjectPtr<UAnimSequence> ChargedAnimation;
 UPROPERTY() TObjectPtr<UAnimMontage> AttackMontage;
 UPROPERTY() TObjectPtr<UAnimMontage> ChargedMontage;
 FTimerHandle HitTimer, FinishTimer;
 float AttackDamage = 0, AttackRange = 0, AttackBreak = 0;
 float ChargePower = 0, LastLightTime = -10, EnemyReadyAt = 0, StaggerUntil = 0;
 float LabelUpdateAt = 0;
 bool bQueuedLight = false;
 bool bRadialAttack = false;
};

UCLASS()
class MYCPPPROJECT_API AARPGGameMode : public AGameModeBase
{
 GENERATED_BODY()
public:
 AARPGGameMode();
 virtual void BeginPlay() override;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") TArray<TObjectPtr<AARPGUnit>> Heroes;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") TArray<TObjectPtr<AARPGUnit>> Enemies;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") bool bMenu = true;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") bool bFinished = false;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") bool bWon = false;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") int32 ActiveHero = 0;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") int32 EliteTotal = 0;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") int32 EliteKilled = 0;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") int32 Kills = 0;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") int32 Stage = 1;
 UPROPERTY(BlueprintReadOnly, Category="ARPG") float MissionStartTime = 0;
 UFUNCTION(BlueprintCallable, Category="ARPG") bool SwitchHero();
 UFUNCTION(BlueprintCallable, Category="ARPG") void UnitDied(AARPGUnit* Unit);
 UFUNCTION(BlueprintCallable, Category="ARPG") void FinishMission(bool bVictory);
 AARPGUnit* PlayerUnit() const;
private:
 void StartMission();
 float SwitchReadyAt = 0;
};

UCLASS()
class MYCPPPROJECT_API AARPGPlayerController : public APlayerController
{
 GENERATED_BODY()
public:
 virtual void BeginPlay() override;
 virtual void SetupInputComponent() override;
 virtual void PlayerTick(float DeltaSeconds) override;
 UPROPERTY() TObjectPtr<class UARPGScreen> Screen;
 UFUNCTION(BlueprintCallable) void StartStageOne();
 UFUNCTION(BlueprintCallable) void StartStageTwo();
 UFUNCTION(BlueprintCallable) void ReturnToMenu();
 UFUNCTION(BlueprintCallable) void Retry();
 void SetMenuInput(bool bEnabled);
private:
 void Light(); void Charge(); void Release(); void Burst(); void Dodge(); void Switch(); void JumpPressed();
};

UCLASS()
class MYCPPPROJECT_API UARPGScreen : public UUserWidget
{
 GENERATED_BODY()
public:
 virtual void NativeOnInitialized() override;
 virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
 UPROPERTY() TObjectPtr<UTextBlock> Status;
 UPROPERTY() TObjectPtr<UTextBlock> HeroStatus;
 UPROPERTY() TObjectPtr<UTextBlock> Objective;
 UPROPERTY() TObjectPtr<UVerticalBox> ResultPanel;
 UPROPERTY() TObjectPtr<UProgressBar> HealthBar;
 UPROPERTY() TObjectPtr<UProgressBar> EnergyBar;
 bool bShowingResult = false;
 void AddText(UVerticalBox* Box, const FString& Text, int32 Size, FLinearColor Color);
 void AddButton(UVerticalBox* Box, const FString& Caption, FName Function);
 UFUNCTION() void PlayOne();
 UFUNCTION() void PlayTwo();
 UFUNCTION() void Menu();
 UFUNCTION() void Restart();
};
