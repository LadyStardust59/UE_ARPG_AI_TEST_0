#include "ARPGDemo.h"
#include "GameplayEffectExtension.h"
#include "NativeGameplayTags.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if WITH_DEV_AUTOMATION_TESTS
void StartARPGSmokeTest(AARPGGameMode* Mode);
#endif

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ARPGDelta, "ARPG.Data.Delta");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ARPGAttacking, "ARPG.State.Attacking");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ARPGDead, "ARPG.State.Dead");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ARPGInvulnerable, "ARPG.State.Invulnerable");

static void ConfigureEffect(UGameplayEffect* Effect, FGameplayAttribute Attribute)
{
 Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
 FGameplayModifierInfo Modifier;
 Modifier.Attribute = Attribute;
 Modifier.ModifierOp = EGameplayModOp::Additive;
 FSetByCallerFloat Amount;
 Amount.DataTag = TAG_ARPGDelta;
 Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Amount);
 Effect->Modifiers.Add(Modifier);
}

UARPGHealthEffect::UARPGHealthEffect() { ConfigureEffect(this, UARPGAttributes::GetHealthAttribute()); }
UARPGEnergyEffect::UARPGEnergyEffect() { ConfigureEffect(this, UARPGAttributes::GetEnergyAttribute()); }
UARPGGuardEffect::UARPGGuardEffect() { ConfigureEffect(this, UARPGAttributes::GetGuardAttribute()); }

void UARPGAttributes::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
 Super::PostGameplayEffectExecute(Data);
 SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
 SetEnergy(FMath::Clamp(GetEnergy(), 0.f, 100.f));
 SetGuard(FMath::Clamp(GetGuard(), 0.f, 100.f));
 if (AARPGUnit* Unit = Cast<AARPGUnit>(GetOwningActor())) { Unit->HandleHealthChanged(); }
}

UARPGAttackAbility::UARPGAttackAbility()
{
 InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
 NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UARPGAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
 const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
 AARPGUnit* Unit = Cast<AARPGUnit>(ActorInfo->AvatarActor.Get());
 if (!Unit || Unit->bDead || Unit->bBenched || !CommitAbility(Handle, ActorInfo, ActivationInfo))
 {
  EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
  return;
 }
 Unit->StartAttack(this);
}

void UARPGAttackAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
 const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
 if (AARPGUnit* Unit = Cast<AARPGUnit>(ActorInfo->AvatarActor.Get())) { Unit->StopAttack(); }
 Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UARPGAttackAbility::Finish()
{
 if (AARPGUnit* Unit = Cast<AARPGUnit>(GetAvatarActorFromActorInfo())) { Unit->StopAttack(); }
 EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

AARPGUnit::AARPGUnit()
{
 PrimaryActorTick.bCanEverTick = true;
 GetCapsuleComponent()->InitCapsuleSize(34, 90);
 GetCharacterMovement()->bOrientRotationToMovement = true;
 GetCharacterMovement()->RotationRate = FRotator(0,720,0);
 GetCharacterMovement()->MaxWalkSpeed = 620;
 GetCharacterMovement()->JumpZVelocity = 560;
 GetCharacterMovement()->AirControl = 0.35f;
 GetCharacterMovement()->bRunPhysicsWithNoController = true;
 bUseControllerRotationYaw = false;
 GetMesh()->SetRelativeLocationAndRotation(FVector(0,0,-90), FRotator(0,-90,0));
 GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
 if (MeshAsset.Succeeded()) GetMesh()->SetSkeletalMesh(MeshAsset.Object);
 static ConstructorHelpers::FClassFinder<UAnimInstance> AnimClass(TEXT("/Game/Variant_Combat/Anims/ABP_Manny_Combat"));
 if (AnimClass.Succeeded()) GetMesh()->SetAnimInstanceClass(AnimClass.Class);
 static ConstructorHelpers::FObjectFinder<UAnimMontage> ComboAsset(TEXT("/Game/Variant_Combat/Anims/AM_ComboAttack"));
 static ConstructorHelpers::FObjectFinder<UAnimMontage> ChargeAsset(TEXT("/Game/Variant_Combat/Anims/AM_ChargedAttack"));
 AttackMontage = ComboAsset.Object;
 ChargedMontage = ChargeAsset.Object;
 AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
 Attributes = CreateDefaultSubobject<UARPGAttributes>(TEXT("Attributes"));
 CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
 CameraBoom->SetupAttachment(RootComponent);
 CameraBoom->TargetArmLength = 620;
 CameraBoom->bUsePawnControlRotation = true;
 CameraBoom->bEnableCameraLag = true;
 CameraBoom->CameraLagSpeed = 12;
 Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
 Camera->SetupAttachment(CameraBoom);
 Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("UnitLabel"));
 Label->SetupAttachment(RootComponent);
 Label->SetRelativeLocation(FVector(0,0,125));
 Label->SetHorizontalAlignment(EHTA_Center);
 Label->SetWorldSize(22);
}

void AARPGUnit::BeginPlay()
{
 Super::BeginPlay();
 AbilitySystem->InitAbilityActorInfo(this, this);
 const float HP = bEnemy ? (bElite ? 420.f : 65.f) : (Definition ? Definition->MaxHealth : 220.f);
 Attributes->InitMaxHealth(HP);
 Attributes->InitHealth(HP);
 Attributes->InitEnergy(0);
 Attributes->InitGuard(bElite ? 100.f : 0.f);
 AbilitySystem->GiveAbility(FGameplayAbilitySpec(UARPGAttackAbility::StaticClass(), 1));
 GetCharacterMovement()->MaxWalkSpeed = bEnemy ? (bElite ? 260.f : 310.f) : (Definition ? Definition->WalkSpeed : 620.f);
 if (bElite) SetActorScale3D(FVector(1.28f));
 if (!bEnemy && IsHeavy())
 {
  if (USkeletalMesh* Quinn=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"))) GetMesh()->SetSkeletalMesh(Quinn);
 }
 if (bEnemy) PrimaryActorTick.TickInterval = 0.05f;
 const FLinearColor Color = bEnemy ? (bElite ? FLinearColor(1,.48f,.06f) : FLinearColor(.8f,.1f,.12f)) : (Definition ? Definition->Accent : FLinearColor(.1f,.7f,1));
 Label->SetTextRenderColor(Color.ToFColor(true));
 GetMesh()->SetVectorParameterValueOnMaterials(TEXT("Tint"), FVector(Color.R,Color.G,Color.B));
 EnemyReadyAt = GetWorld()->GetTimeSeconds() + FMath::FRandRange(1.f,3.f);
 UpdateLabel();
}

bool AARPGUnit::IsHeavy() const { return Definition && Definition->bHeavyStyle; }
FText AARPGUnit::HeroName() const { return Definition ? Definition->DisplayName : FText::FromString(TEXT("Hero")); }
float AARPGUnit::HealthFraction() const { return Attributes->GetHealth() / FMath::Max(1.f,Attributes->GetMaxHealth()); }
bool AARPGUnit::IsAttacking() const { return AbilitySystem->HasMatchingGameplayTag(TAG_ARPGAttacking); }
float AARPGUnit::GetChargeFraction() const { return ChargeStarted < 0 ? 0 : FMath::Clamp((GetWorld()->GetTimeSeconds()-ChargeStarted)/1.2f,0.f,1.f); }

void AARPGUnit::ApplyAttributeDelta(TSubclassOf<UGameplayEffect> Effect, float Delta, AARPGUnit* Source)
{
 FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
 Context.AddSourceObject(Source ? Source : this);
 FGameplayEffectSpec Spec(Effect->GetDefaultObject<UGameplayEffect>(), Context, 1);
 Spec.SetSetByCallerMagnitude(TAG_ARPGDelta, Delta);
 AbilitySystem->ApplyGameplayEffectSpecToSelf(Spec);
}

bool AARPGUnit::RequestAttack(EARPGAttack Attack)
{
 const AARPGGameMode* GM = GetWorld()->GetAuthGameMode<AARPGGameMode>();
 const float Now = GetWorld()->GetTimeSeconds();
 if (bDead || bBenched || (GM && GM->bFinished) || Now < StaggerUntil || Now < BrokenUntil) return false;
 if (IsAttacking())
 {
  if (Attack == EARPGAttack::Light && RequestedAttack == EARPGAttack::Light) bQueuedLight = true;
  return false;
 }
 if (Attack == EARPGAttack::Burst && (Attributes->GetEnergy() < 100 || Now < BurstReadyAt)) return false;
 if (Attack == EARPGAttack::Dodge && Now < DodgeReadyAt) return false;
 if (Attack == EARPGAttack::Light)
 {
  Combo = Now - LastLightTime > 1.05f ? 1 : (Combo % 3) + 1;
  LastLightTime = Now;
 }
 RequestedAttack = Attack;
 return AbilitySystem->TryActivateAbilityByClass(UARPGAttackAbility::StaticClass());
}

void AARPGUnit::BeginCharge()
{
 if (const AARPGGameMode* GM=GetWorld()->GetAuthGameMode<AARPGGameMode>()) if (GM->bFinished) return;
 if (!bDead && !bBenched && !IsAttacking() && GetWorld()->GetTimeSeconds() >= StaggerUntil)
  ChargeStarted = GetWorld()->GetTimeSeconds();
}

void AARPGUnit::ReleaseCharge()
{
 if (ChargeStarted < 0) return;
 ChargePower = GetChargeFraction();
 ChargeStarted = -1;
 RequestAttack(EARPGAttack::Heavy);
}

void AARPGUnit::StartAttack(UARPGAttackAbility* Ability)
{
 CurrentAbility = Ability;
 bQueuedLight = false;
 AbilitySystem->AddLooseGameplayTag(TAG_ARPGAttacking);
 const float Now = GetWorld()->GetTimeSeconds();
 const float BaseDamage = bEnemy ? (bElite ? 26.f : 10.f) : (Definition ? Definition->AttackDamage : 24.f);
 float Duration = bEnemy ? 1.0f : (Definition ? Definition->AttackInterval : .38f);
 AttackDamage = BaseDamage * (Combo == 3 ? 1.6f : 1.f);
 AttackRange = bEnemy ? (bElite ? 240.f : 170.f) : (IsHeavy() ? 300.f : 260.f);
 AttackBreak = Combo == 3 ? 18.f : 8.f;
 bRadialAttack = false;
 if (RequestedAttack == EARPGAttack::Heavy)
 {
  Duration = IsHeavy() ? .9f : .65f;
  AttackDamage = BaseDamage * (1.4f + ChargePower * (IsHeavy() ? 3.3f : 1.8f));
  AttackBreak = (IsHeavy() ? 40.f : 20.f) + ChargePower * (IsHeavy() ? 65.f : 30.f);
  AttackRange += 100 * ChargePower;
 }
 else if (RequestedAttack == EARPGAttack::Burst)
 {
  Duration = 1.1f;
  bRadialAttack = true;
  AttackRange = 650;
  AttackDamage = IsHeavy() ? 155 : 120;
  AttackBreak = 100;
  ApplyAttributeDelta(UARPGEnergyEffect::StaticClass(), -100);
  BurstReadyAt = Now + 8;
 }
 else if (RequestedAttack == EARPGAttack::Dodge)
 {
  Duration = .32f;
  DodgeReadyAt = Now + .9f;
  AbilitySystem->AddLooseGameplayTag(TAG_ARPGInvulnerable);
  FVector Direction = GetLastMovementInputVector().GetSafeNormal2D();
  if (Direction.IsNearlyZero()) Direction = GetActorForwardVector();
  LaunchCharacter(Direction * 1500 + FVector(0,0,80), true, true);
 }
 if (RequestedAttack != EARPGAttack::Dodge)
 {
  if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
  {
   UAnimMontage* Montage = RequestedAttack == EARPGAttack::Light ? AttackMontage : ChargedMontage;
   if (Montage)
   {
    Anim->Montage_Play(Montage, IsHeavy() ? .85f : 1.3f);
    const int32 Section = RequestedAttack == EARPGAttack::Light ? FMath::Clamp(Combo-1,0,Montage->GetNumSections()-1) : Montage->GetNumSections()-1;
    if (Section >= 0) Anim->Montage_JumpToSection(Montage->GetSectionName(Section), Montage);
   }
  }
  GetWorldTimerManager().SetTimer(HitTimer, this, &AARPGUnit::ResolveHit, Duration * (bEnemy ? .68f : .45f), false);
 }
 GetWorldTimerManager().SetTimer(FinishTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
 {
  const bool bContinue = bQueuedLight;
  if (CurrentAbility) CurrentAbility->Finish();
  if (bContinue && !bDead && !bBenched) RequestAttack(EARPGAttack::Light);
 }), Duration, false);
}

void AARPGUnit::StopAttack()
{
 GetWorldTimerManager().ClearTimer(HitTimer);
 GetWorldTimerManager().ClearTimer(FinishTimer);
 AbilitySystem->RemoveLooseGameplayTag(TAG_ARPGAttacking);
 AbilitySystem->RemoveLooseGameplayTag(TAG_ARPGInvulnerable);
 CurrentAbility = nullptr;
 bQueuedLight = false;
 ChargeStarted = -1;
 if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) Anim->Montage_Stop(.12f);
}

void AARPGUnit::ResolveHit()
{
 if (bDead || bBenched) return;
 TArray<FOverlapResult> Hits;
 FCollisionObjectQueryParams Objects(ECC_Pawn);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(ARPGHit), false, this);
 GetWorld()->OverlapMultiByObjectType(Hits, GetActorLocation(), FQuat::Identity, Objects, FCollisionShape::MakeSphere(AttackRange), Query);
 TSet<AARPGUnit*> UniqueTargets;
 int32 HitCount = 0;
 for (const FOverlapResult& Hit : Hits)
 {
  AARPGUnit* Target = Cast<AARPGUnit>(Hit.GetActor());
  if (!Target || Target == this || Target->bEnemy == bEnemy || Target->bDead || Target->bBenched || UniqueTargets.Contains(Target)) continue;
  UniqueTargets.Add(Target);
  const FVector ToTarget = (Target->GetActorLocation()-GetActorLocation()).GetSafeNormal2D();
  if (!bRadialAttack && FVector::DotProduct(GetActorForwardVector(),ToTarget) < -.1f) continue;
  FHitResult Wall;
  FCollisionQueryParams WallQuery(SCENE_QUERY_STAT(ARPGVisibility), false, this);
  WallQuery.AddIgnoredActor(Target);
  if (GetWorld()->LineTraceSingleByChannel(Wall, GetActorLocation(),Target->GetActorLocation(), ECC_Visibility, WallQuery)) continue;
  Target->ReceiveCombatHit(this, AttackDamage, AttackBreak);
  ++HitCount;
 }
 if (!bEnemy && HitCount && RequestedAttack != EARPGAttack::Burst)
  ApplyAttributeDelta(UARPGEnergyEffect::StaticClass(), FMath::Min(HitCount,5) * (IsHeavy() ? 6.f : 9.f));
 const FColor Color = bEnemy ? FColor::Red : (IsHeavy() ? FColor::Orange : FColor::Cyan);
 DrawDebugCircle(GetWorld(), GetActorLocation()-FVector(0,0,70), AttackRange, 40, Color, false,.22f,0,5,FVector(1,0,0),FVector(0,1,0),false);
}

void AARPGUnit::ReceiveCombatHit(AARPGUnit* Source, float Damage, float BreakPower)
{
 if (bDead || bBenched || AbilitySystem->HasMatchingGameplayTag(TAG_ARPGInvulnerable)) return;
 const float Now = GetWorld()->GetTimeSeconds();
 if (bElite && Now >= BrokenUntil && Attributes->GetGuard() > 0)
 {
  ApplyAttributeDelta(UARPGGuardEffect::StaticClass(), -BreakPower, Source);
  if (Attributes->GetGuard() <= 0)
  {
   BrokenUntil = Now + 3;
   AbilitySystem->CancelAllAbilities();
  }
  else Damage *= .45f;
 }
 else if (bElite && Now < BrokenUntil) Damage *= 1.5f;
 ApplyAttributeDelta(UARPGHealthEffect::StaticClass(), -Damage, Source);
 if (bDead) return;
 if (!bElite || Now < BrokenUntil)
 {
  StaggerUntil = Now + .22f;
  AbilitySystem->CancelAllAbilities();
  if (Source) LaunchCharacter((GetActorLocation()-Source->GetActorLocation()).GetSafeNormal2D()*260 + FVector(0,0,90),true,true);
 }
 UpdateLabel();
}

void AARPGUnit::HandleHealthChanged()
{
 if (bDead || Attributes->GetHealth() > 0) return;
 bDead = true;
 AbilitySystem->CancelAllAbilities();
 AbilitySystem->AddLooseGameplayTag(TAG_ARPGDead);
 GetCharacterMovement()->DisableMovement();
 GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Label->SetText(FText::FromString(TEXT("DEFEATED")));
 GetMesh()->SetRelativeRotation(FRotator(0,-90,80));
 if (AARPGGameMode* GM = GetWorld()->GetAuthGameMode<AARPGGameMode>()) GM->UnitDied(this);
 if (bEnemy) SetLifeSpan(3);
}

void AARPGUnit::SetBenched(bool bValue)
{
 AbilitySystem->CancelAllAbilities();
 ChargeStarted = -1;
 bBenched = bValue;
 SetActorHiddenInGame(bValue);
 SetActorEnableCollision(!bValue && !bDead);
 SetActorTickEnabled(!bValue && !bDead);
 GetCharacterMovement()->StopMovementImmediately();
 if (bValue) GetCharacterMovement()->DisableMovement();
 else if (!bDead) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void AARPGUnit::UpdateLabel()
{
 if (bDead) return;
 if (bEnemy)
 {
  const FString State = GetWorld()->GetTimeSeconds() < BrokenUntil ? TEXT("BROKEN! ") : (IsAttacking() ? TEXT("! ATTACK ! ") : TEXT(""));
  Label->SetText(FText::FromString(FString::Printf(TEXT("%s%s  %.0f%s"), *State,bElite ? TEXT("ELITE") : TEXT("SOLDIER"),Attributes->GetHealth(),bElite ? *FString::Printf(TEXT(" | Guard %.0f"),Attributes->GetGuard()) : TEXT(""))));
 }
 else Label->SetText(FText::FromString(IsHeavy() ? TEXT("B / BREAKER") : TEXT("A / STRIKER")));
}

void AARPGUnit::UpdateEnemy(float DeltaSeconds)
{
 AARPGGameMode* GM = GetWorld()->GetAuthGameMode<AARPGGameMode>();
 if (!GM || GM->bFinished) return;
 AARPGUnit* Target = GM->PlayerUnit();
 if (!Target || Target->bDead) return;
 const float Now = GetWorld()->GetTimeSeconds();
 if (IsAttacking() || Now < StaggerUntil || Now < BrokenUntil) return;
 if (bElite && Attributes->GetGuard() <= 0) ApplyAttributeDelta(UARPGGuardEffect::StaticClass(),100);
 FVector Delta = Target->GetActorLocation()-GetActorLocation();
 const float Distance = Delta.Size2D();
 SetActorRotation(FRotator(0,Delta.Rotation().Yaw,0));
 if (Distance > (bElite ? 190.f : 125.f))
 {
  FVector Separation = FVector::ZeroVector;
  for (const AARPGUnit* Other : GM->Enemies)
  {
   if (!IsValid(Other) || Other == this || Other->bDead) continue;
   const FVector Away = GetActorLocation()-Other->GetActorLocation();
   const float Dist = Away.Size2D();
   if (Dist > 1 && Dist < 100) Separation += Away.GetSafeNormal2D() * (1-Dist/100);
  }
  AddMovementInput((Delta.GetSafeNormal2D()+Separation*1.5f).GetSafeNormal2D(), 1, true);
 }
 else if (Now >= EnemyReadyAt)
 {
  Combo = 1;
  if (RequestAttack(EARPGAttack::Light)) EnemyReadyAt = Now + (bElite ? 2.2f : FMath::FRandRange(2.5f,4.5f));
 }
}

void AARPGUnit::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 if (bDead || bBenched) return;
 if (GetActorLocation().Z < -600) { ApplyAttributeDelta(UARPGHealthEffect::StaticClass(),-Attributes->GetMaxHealth()); return; }
 if (bEnemy) UpdateEnemy(DeltaSeconds);
 if (bEnemy && IsAttacking()) DrawDebugCircle(GetWorld(),GetActorLocation()-FVector(0,0,75),AttackRange,24,FColor::Red,false,.07f,0,2,FVector(1,0,0),FVector(0,1,0),false);
 if (ChargeStarted >= 0) DrawDebugCircle(GetWorld(),GetActorLocation()-FVector(0,0,80),80+GetChargeFraction()*100,24,FColor::Yellow,false,.03f,0,3,FVector(1,0,0),FVector(0,1,0),false);
 const float Now = GetWorld()->GetTimeSeconds();
 if (Now > LabelUpdateAt)
 {
  UpdateLabel();
  LabelUpdateAt = Now + .15f;
  if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
  {
   FVector View; FRotator Rotation;
   PC->GetPlayerViewPoint(View,Rotation);
   Label->SetWorldRotation((View-Label->GetComponentLocation()).Rotation());
  }
 }
}

AARPGGameMode::AARPGGameMode()
{
 DefaultPawnClass = nullptr;
 PlayerControllerClass = AARPGPlayerController::StaticClass();
}

void AARPGGameMode::BeginPlay()
{
 Super::BeginPlay();
 const FString Map = UGameplayStatics::GetCurrentLevelName(this,true);
 bMenu = Map.Contains(TEXT("MainMenu"));
 Stage = Map.Contains(TEXT("Fortress")) ? 2 : 1;
 if (!bMenu) GetWorldTimerManager().SetTimerForNextTick(this,&AARPGGameMode::StartMission);
}

void AARPGGameMode::StartMission()
{
 MissionStartTime = GetWorld()->GetTimeSeconds();
 for (int32 Index=0; Index<2; ++Index)
 {
  AARPGUnit* Hero = GetWorld()->SpawnActorDeferred<AARPGUnit>(AARPGUnit::StaticClass(),FTransform(FRotator::ZeroRotator,FVector(-1200,0,110)),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  Hero->Definition = LoadObject<UARPGHeroDefinition>(nullptr,Index == 0 ? TEXT("/Game/ARPG/Data/DA_Striker.DA_Striker") : TEXT("/Game/ARPG/Data/DA_Breaker.DA_Breaker"));
  if (!Hero->Definition)
  {
   UE_LOG(LogTemp,Error,TEXT("ARPG: missing hero definition %d"),Index);
  }
  Hero->FinishSpawning(Hero->GetActorTransform());
  Hero->SetBenched(Index != 0);
  Heroes.Add(Hero);
 }
 if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
 {
  PC->Possess(Heroes[0]);
  PC->SetControlRotation(FRotator(-25,0,0));
 }
 const int32 Soldiers = Stage == 1 ? 36 : 64;
 EliteTotal = Stage == 1 ? 2 : 3;
 FRandomStream Random(20260918+Stage);
 for (int32 Index=0; Index<Soldiers+EliteTotal; ++Index)
 {
  const bool Elite = Index >= Soldiers;
  const float X = Elite ? 1700.f : Random.FRandRange(-300.f,1900.f);
  const float Y = Elite ? (Index-Soldiers-(EliteTotal-1)*.5f)*650.f : Random.FRandRange(-1800.f,1800.f);
  AARPGUnit* Enemy = GetWorld()->SpawnActorDeferred<AARPGUnit>(AARPGUnit::StaticClass(),FTransform(FRotator(0,180,0),FVector(X,Y,125)),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
  Enemy->bEnemy = true;
  Enemy->bElite = Elite;
  Enemy->FinishSpawning(Enemy->GetActorTransform());
  Enemies.Add(Enemy);
 }
 UE_LOG(LogTemp,Display,TEXT("ARPG_MISSION_READY stage=%d soldiers=%d elites=%d heroes=%d"),Stage,Soldiers,EliteTotal,Heroes.Num());
#if WITH_DEV_AUTOMATION_TESTS
 if (FParse::Param(FCommandLine::Get(),TEXT("ARPGSmokeTest"))) StartARPGSmokeTest(this);
#endif
}

AARPGUnit* AARPGGameMode::PlayerUnit() const { return Heroes.IsValidIndex(ActiveHero) ? Heroes[ActiveHero] : nullptr; }

bool AARPGGameMode::SwitchHero()
{
 if (bMenu || bFinished || Heroes.Num()!=2 || GetWorld()->GetTimeSeconds()<SwitchReadyAt) return false;
 const int32 Next = 1-ActiveHero;
 if (!IsValid(Heroes[Next]) || Heroes[Next]->bDead) return false;
 APlayerController* PC = GetWorld()->GetFirstPlayerController();
 if (!PC || !PlayerUnit()) return false;
 AARPGUnit* Old = PlayerUnit();
 const FTransform Transform = Old->GetActorTransform();
 const FRotator View = PC->GetControlRotation();
 Old->SetBenched(true);
 AARPGUnit* New = Heroes[Next];
 New->SetActorTransform(Transform,false,nullptr,ETeleportType::TeleportPhysics);
 New->SetBenched(false);
 ActiveHero = Next;
 PC->Possess(New);
 PC->SetControlRotation(View);
 SwitchReadyAt = GetWorld()->GetTimeSeconds()+.5f;
 return true;
}

void AARPGGameMode::UnitDied(AARPGUnit* Unit)
{
 if (bFinished) return;
 if (Unit->bEnemy)
 {
  ++Kills;
  if (Unit->bElite && ++EliteKilled >= EliteTotal) FinishMission(true);
 }
 else
 {
  SwitchReadyAt = 0;
  if (!SwitchHero()) FinishMission(false);
 }
}

void AARPGGameMode::FinishMission(bool bVictory)
{
 if (bFinished) return;
 bFinished = true;
 bWon = bVictory;
 for (AARPGUnit* Hero : Heroes) if (IsValid(Hero)) { Hero->AbilitySystem->CancelAllAbilities(); Hero->ChargeStarted=-1; Hero->GetCharacterMovement()->StopMovementImmediately(); }
 for (AARPGUnit* Enemy : Enemies) if (IsValid(Enemy)) { Enemy->AbilitySystem->CancelAllAbilities(); Enemy->GetCharacterMovement()->StopMovementImmediately(); }
 if (AARPGPlayerController* PC = Cast<AARPGPlayerController>(GetWorld()->GetFirstPlayerController())) PC->SetMenuInput(true);
 UE_LOG(LogTemp,Display,TEXT("ARPG_MISSION_END won=%d elites=%d/%d kills=%d"),bVictory,EliteKilled,EliteTotal,Kills);
}

void AARPGPlayerController::BeginPlay()
{
 Super::BeginPlay();
 Screen = CreateWidget<UARPGScreen>(this,UARPGScreen::StaticClass());
 Screen->AddToViewport();
 const AARPGGameMode* GM = GetWorld()->GetAuthGameMode<AARPGGameMode>();
 // Map name is reliable before GameMode::BeginPlay has run.
 SetMenuInput(UGameplayStatics::GetCurrentLevelName(this,true).Contains(TEXT("MainMenu")));
}

void AARPGPlayerController::SetMenuInput(bool bEnabled)
{
 bShowMouseCursor = bEnabled;
 if (bEnabled) { FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); SetInputMode(Mode); }
 else { FInputModeGameOnly Mode; SetInputMode(Mode); }
}

void AARPGPlayerController::SetupInputComponent()
{
 Super::SetupInputComponent();
 InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AARPGPlayerController::Light);
 InputComponent->BindKey(EKeys::RightMouseButton,IE_Pressed,this,&AARPGPlayerController::Charge);
 InputComponent->BindKey(EKeys::RightMouseButton,IE_Released,this,&AARPGPlayerController::Release);
 InputComponent->BindKey(EKeys::Q,IE_Pressed,this,&AARPGPlayerController::Burst);
 InputComponent->BindKey(EKeys::LeftShift,IE_Pressed,this,&AARPGPlayerController::Dodge);
 InputComponent->BindKey(EKeys::Tab,IE_Pressed,this,&AARPGPlayerController::Switch);
 InputComponent->BindKey(EKeys::SpaceBar,IE_Pressed,this,&AARPGPlayerController::JumpPressed);
 InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&AARPGPlayerController::ReturnToMenu);
 InputComponent->BindKey(EKeys::R,IE_Pressed,this,&AARPGPlayerController::Retry);
}

void AARPGPlayerController::PlayerTick(float DeltaSeconds)
{
 Super::PlayerTick(DeltaSeconds);
 const AARPGGameMode* GM = GetWorld()->GetAuthGameMode<AARPGGameMode>();
 AARPGUnit* Unit = Cast<AARPGUnit>(GetPawn());
 if (!GM || GM->bMenu || GM->bFinished || !Unit || Unit->bDead) return;
 const float Forward = (IsInputKeyDown(EKeys::W)?1.f:0.f)-(IsInputKeyDown(EKeys::S)?1.f:0.f);
 const float Right = (IsInputKeyDown(EKeys::D)?1.f:0.f)-(IsInputKeyDown(EKeys::A)?1.f:0.f);
 const FRotator Yaw(0,GetControlRotation().Yaw,0);
 const float Scale = Unit->IsAttacking() ? .18f : (Unit->ChargeStarted>=0 ? .35f : 1.f);
 Unit->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X),Forward*Scale);
 Unit->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y),Right*Scale);
 float MouseX=0,MouseY=0;
 GetInputMouseDelta(MouseX,MouseY);
 FRotator Look = GetControlRotation();
 Look.Yaw += MouseX * 1.7f;
 Look.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Look.Pitch)-MouseY*1.3f,-65.f,15.f);
 SetControlRotation(Look);
}

void AARPGPlayerController::Light() { if (auto* Unit=Cast<AARPGUnit>(GetPawn())) Unit->RequestAttack(EARPGAttack::Light); }
void AARPGPlayerController::Charge() { if (auto* Unit=Cast<AARPGUnit>(GetPawn())) Unit->BeginCharge(); }
void AARPGPlayerController::Release() { if (auto* Unit=Cast<AARPGUnit>(GetPawn())) Unit->ReleaseCharge(); }
void AARPGPlayerController::Burst() { if (auto* Unit=Cast<AARPGUnit>(GetPawn())) Unit->RequestAttack(EARPGAttack::Burst); }
void AARPGPlayerController::Dodge() { if (auto* Unit=Cast<AARPGUnit>(GetPawn())) Unit->RequestAttack(EARPGAttack::Dodge); }
void AARPGPlayerController::Switch() { if (auto* GM=GetWorld()->GetAuthGameMode<AARPGGameMode>()) GM->SwitchHero(); }
void AARPGPlayerController::JumpPressed() { if (auto* Unit=Cast<AARPGUnit>(GetPawn())) if (!Unit->bDead) Unit->Jump(); }
void AARPGPlayerController::StartStageOne() { UGameplayStatics::OpenLevel(this,TEXT("/Game/ARPG/Maps/L_Arena")); }
void AARPGPlayerController::StartStageTwo() { UGameplayStatics::OpenLevel(this,TEXT("/Game/ARPG/Maps/L_Fortress")); }
void AARPGPlayerController::ReturnToMenu() { UGameplayStatics::OpenLevel(this,TEXT("/Game/ARPG/Maps/L_MainMenu")); }
void AARPGPlayerController::Retry()
{
 if (const auto* GM=GetWorld()->GetAuthGameMode<AARPGGameMode>())
  if (GM->bFinished) { if (GM->Stage==2) StartStageTwo(); else StartStageOne(); }
}
