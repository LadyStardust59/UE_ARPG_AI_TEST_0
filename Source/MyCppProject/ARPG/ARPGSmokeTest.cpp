#include "ARPGDemo.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "TimerManager.h"

// Opt-in integration run in a real game world, never enabled in normal play.
struct FARPGSmokeRun : TSharedFromThis<FARPGSmokeRun>
{
 TWeakObjectPtr<AARPGGameMode> Mode;
 FTimerHandle Timer;
 int32 Step=0, Failures=0;
 float SoldierHealth=0;
 TWeakObjectPtr<AARPGUnit> Elite;
 void Check(bool bPass, const TCHAR* Name)
 {
  if (!bPass) ++Failures;
  UE_LOG(LogTemp,Display,TEXT("ARPG_TEST %s %s"),bPass?TEXT("PASS"):TEXT("FAIL"),Name);
 }
 void Tick()
 {
  AARPGGameMode* GM=Mode.Get();
  if (!GM || GM->Heroes.Num()!=2) { UE_LOG(LogTemp,Error,TEXT("ARPG_TEST_FAIL missing mission")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
  AARPGUnit* A=GM->Heroes[0]; AARPGUnit* B=GM->Heroes[1];
  switch(Step++)
  {
  case 0:
   Check(A->Definition && B->Definition && !A->IsHeavy() && B->IsHeavy(),TEXT("distinct hero definitions"));
   Check(GM->Enemies.Num()==(GM->Stage==1?38:67),TEXT("mission enemy count"));
   for (AARPGUnit* Enemy:GM->Enemies) { Enemy->SetActorTickEnabled(false); Enemy->GetCharacterMovement()->DisableMovement(); if (Enemy->bElite) Elite=Enemy; }
   A->ApplyAttributeDelta(UARPGHealthEffect::StaticClass(),-17);
   A->ApplyAttributeDelta(UARPGEnergyEffect::StaticClass(),45);
   Check(A->RequestAttack(EARPGAttack::Light),TEXT("GAS ability activation"));
   Check(GM->SwitchHero(),TEXT("switch during attack"));
   Check(!A->IsAttacking() && A->bBenched && !B->bBenched,TEXT("switch cancels outgoing attack"));
   break;
  case 1:
   Check(GM->SwitchHero(),TEXT("switch back"));
   Check(FMath::IsNearlyEqual(A->Attributes->GetHealth(),203.f) && FMath::IsNearlyEqual(A->Attributes->GetEnergy(),45.f),TEXT("reserve health and energy persist"));
   A->SetActorRotation(FRotator::ZeroRotator);
   GM->Enemies[0]->SetActorLocation(A->GetActorLocation()+FVector(130,0,0));
   SoldierHealth=GM->Enemies[0]->Attributes->GetHealth();
   A->RequestAttack(EARPGAttack::Light);
   break;
  case 2:
   Check(GM->Enemies[0]->Attributes->GetHealth()<SoldierHealth,TEXT("ability timer deals GAS damage"));
   Check(A->Attributes->GetEnergy()>45,TEXT("hit generates energy"));
   Check(A->RequestAttack(EARPGAttack::Dodge),TEXT("dodge activates"));
   A->ReceiveCombatHit(GM->Enemies[0],30,0);
   Check(FMath::IsNearlyEqual(A->Attributes->GetHealth(),203.f),TEXT("dodge invulnerability"));
   break;
  case 3:
   Check(GM->SwitchHero(),TEXT("switch to breaker"));
   B->SetActorRotation(FRotator::ZeroRotator);
   if (Elite.IsValid()) Elite->SetActorLocation(B->GetActorLocation()+FVector(180,0,0));
   B->BeginCharge();
   B->ChargeStarted=GM->GetWorld()->GetTimeSeconds()-1.21f;
   B->ReleaseCharge();
   break;
  case 4:
   Check(Elite.IsValid() && Elite->Attributes->GetGuard()==0 && Elite->BrokenUntil>GM->GetWorld()->GetTimeSeconds(),TEXT("full charge breaks elite guard"));
   break;
  case 5:
   B->ApplyAttributeDelta(UARPGEnergyEffect::StaticClass(),100);
   Check(B->RequestAttack(EARPGAttack::Burst),TEXT("full energy burst activation"));
   Check(B->Attributes->GetEnergy()==0,TEXT("burst consumes energy"));
   break;
  case 6:
   for (AARPGUnit* Enemy:GM->Enemies) if (IsValid(Enemy) && Enemy->bElite && !Enemy->bDead) Enemy->ApplyAttributeDelta(UARPGHealthEffect::StaticClass(),-10000);
   Check(GM->bFinished && GM->bWon && GM->EliteKilled==GM->EliteTotal,TEXT("all elites killed triggers victory"));
   Check(!GM->SwitchHero() && !B->RequestAttack(EARPGAttack::Light),TEXT("result locks combat"));
   break;
  case 7:
   GM->bFinished=false;
   GM->bWon=false;
   GM->PlayerUnit()->ApplyAttributeDelta(UARPGHealthEffect::StaticClass(),-10000);
   Check(!GM->bFinished && GM->PlayerUnit()==A && !A->bDead,TEXT("active hero death switches to reserve"));
   A->ApplyAttributeDelta(UARPGHealthEffect::StaticClass(),-10000);
   Check(GM->bFinished && !GM->bWon,TEXT("both heroes dead triggers defeat"));
   UE_LOG(LogTemp,Display,TEXT("ARPG_TEST_COMPLETE stage=%d failures=%d"),GM->Stage,Failures);
   GM->GetWorldTimerManager().ClearTimer(Timer);
   FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
   break;
  }
 }
};

void StartARPGSmokeTest(AARPGGameMode* Mode)
{
 TSharedRef<FARPGSmokeRun> Run=MakeShared<FARPGSmokeRun>();
 Run->Mode=Mode;
 Mode->GetWorldTimerManager().SetTimer(Run->Timer,FTimerDelegate::CreateLambda([Run](){ Run->Tick(); }),.7f,true,1.0f);
}
#endif
