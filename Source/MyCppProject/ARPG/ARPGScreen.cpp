#include "ARPGDemo.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"

void UARPGScreen::AddText(UVerticalBox* Box, const FString& Text, int32 Size, FLinearColor Color)
{
 UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
 Label->SetText(FText::FromString(Text));
 FSlateFontInfo Font = Label->GetFont(); Font.Size = Size; Label->SetFont(Font);
 Label->SetColorAndOpacity(FSlateColor(Color));
 Label->SetAutoWrapText(true);
 Box->AddChildToVerticalBox(Label)->SetPadding(FMargin(0,5));
}

void UARPGScreen::AddButton(UVerticalBox* Box, const FString& Caption, FName Function)
{
 UButton* Button = WidgetTree->ConstructWidget<UButton>();
 Button->SetBackgroundColor(FLinearColor(.08f,.26f,.33f,1));
 UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
 Label->SetText(FText::FromString(Caption));
 FSlateFontInfo Font = Label->GetFont(); Font.Size = 20; Label->SetFont(Font);
 Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
 Button->AddChild(Label);
 FScriptDelegate Delegate; Delegate.BindUFunction(this,Function); Button->OnClicked.Add(Delegate);
 Box->AddChildToVerticalBox(Button)->SetPadding(FMargin(0,10));
}

void UARPGScreen::NativeOnInitialized()
{
 Super::NativeOnInitialized();
 UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("Root"));
 WidgetTree->RootWidget = Canvas;
 const bool bMenu = UGameplayStatics::GetCurrentLevelName(this,true).Contains(TEXT("MainMenu"));
 UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
 Panel->SetBrushColor(FLinearColor(.012f,.02f,.035f,bMenu ? .98f : .85f));
 Panel->SetPadding(FMargin(28));
 UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Panel);
 CanvasSlot->SetPosition(bMenu ? FVector2D(100,90) : FVector2D(25,25));
 CanvasSlot->SetSize(bMenu ? FVector2D(660,620) : FVector2D(570,330));
 UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
 Panel->SetContent(Box);
 if (bMenu)
 {
  AddText(Box,TEXT("ARPG  /  COMBAT LAB"),16,FLinearColor(.25f,.8f,1));
  AddText(Box,TEXT("双星战阵"),48,FLinearColor::White);
  AddText(Box,TEXT("GAS 动作原型 · 双角色即时切换"),20,FLinearColor(.65f,.73f,.8f));
  AddButton(Box,TEXT("01  演武场  /  36 名士兵 · 2 名精英"),TEXT("PlayOne"));
  AddButton(Box,TEXT("02  要塞战  /  64 名士兵 · 3 名精英"),TEXT("PlayTwo"));
  AddText(Box,TEXT("胜利：击败全部精英。失败：两名角色均倒下。"),18,FLinearColor(1,.73f,.3f));
  AddText(Box,TEXT("WASD 移动  ·  鼠标转向  ·  空格跳跃\n左键连击  ·  按住右键蓄力，松开释放\nQ 满能量大招  ·  Shift 闪避  ·  Tab 切换\nEsc 返回主菜单"),18,FLinearColor(.8f,.85f,.9f));
  AddText(Box,TEXT("疾风：连续命中快速积攒能量\n磐石：蓄力打破精英防御，破防后伤害提高"),18,FLinearColor(.45f,.8f,.85f));
 }
 else
 {
  Objective = WidgetTree->ConstructWidget<UTextBlock>();
  FSlateFontInfo Font=Objective->GetFont(); Font.Size=22; Objective->SetFont(Font);
  Objective->SetColorAndOpacity(FSlateColor(FLinearColor(1,.75f,.3f)));
  Box->AddChildToVerticalBox(Objective);
  HeroStatus=WidgetTree->ConstructWidget<UTextBlock>();
  HeroStatus->SetAutoWrapText(true);
  Box->AddChildToVerticalBox(HeroStatus)->SetPadding(FMargin(0,8));
  HealthBar=WidgetTree->ConstructWidget<UProgressBar>();
  HealthBar->SetFillColorAndOpacity(FLinearColor(.12f,.8f,.5f));
  Box->AddChildToVerticalBox(HealthBar)->SetPadding(FMargin(0,5));
  EnergyBar=WidgetTree->ConstructWidget<UProgressBar>();
  EnergyBar->SetFillColorAndOpacity(FLinearColor(.1f,.6f,1));
  Box->AddChildToVerticalBox(EnergyBar)->SetPadding(FMargin(0,5));
  Status=WidgetTree->ConstructWidget<UTextBlock>();
  Status->SetAutoWrapText(true);
  Box->AddChildToVerticalBox(Status)->SetPadding(FMargin(0,8));
  AddText(Box,TEXT("左键 连击 | 右键 蓄力 | Q 大招 | Shift 闪避\nTab 切换 | Space 跳跃 | Esc 菜单"),16,FLinearColor(.7f,.8f,.9f));
 }
}

void UARPGScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
 Super::NativeTick(MyGeometry,InDeltaTime);
 AARPGGameMode* GM=GetWorld()->GetAuthGameMode<AARPGGameMode>();
 if (!GM || GM->bMenu || !Objective) return;
 Objective->SetText(FText::FromString(FString::Printf(TEXT("%s  |  精英 %d / %d  |  击破 %d"),GM->Stage==1?TEXT("演武场"):TEXT("要塞战"),GM->EliteKilled,GM->EliteTotal,GM->Kills)));
 if (AARPGUnit* Hero=GM->PlayerUnit())
 {
  const AARPGUnit* Reserve=GM->Heroes.IsValidIndex(1-GM->ActiveHero)?GM->Heroes[1-GM->ActiveHero]:nullptr;
  HeroStatus->SetText(FText::FromString(FString::Printf(TEXT("%s  HP %.0f / %.0f\n后备：%s  HP %.0f"),*Hero->HeroName().ToString(),Hero->Attributes->GetHealth(),Hero->Attributes->GetMaxHealth(),Reserve?*Reserve->HeroName().ToString():TEXT("--"),Reserve?Reserve->Attributes->GetHealth():0)));
  HealthBar->SetPercent(Hero->HealthFraction());
  EnergyBar->SetPercent(Hero->Attributes->GetEnergy()/100);
  const float Cooldown=FMath::Max(0.f,Hero->BurstReadyAt-GetWorld()->GetTimeSeconds());
  Status->SetText(FText::FromString(FString::Printf(TEXT("能量 %.0f / 100  |  连击 %d  |  蓄力 %.0f%%\nQ：%s  |  冷却 %.1fs"),Hero->Attributes->GetEnergy(),Hero->Combo,Hero->GetChargeFraction()*100,(Hero->Attributes->GetEnergy()>=100 && Cooldown<=0)?TEXT("可以释放"):TEXT("积攒能量"),Cooldown)));
 }
 if (GM->bFinished && !bShowingResult)
 {
  bShowingResult=true;
  UCanvasPanel* Canvas=Cast<UCanvasPanel>(WidgetTree->RootWidget);
  UBorder* Result=WidgetTree->ConstructWidget<UBorder>();
  Result->SetBrushColor(FLinearColor(.015f,.025f,.04f,.97f));
  Result->SetPadding(FMargin(32));
  UCanvasPanelSlot* CanvasSlot=Canvas->AddChildToCanvas(Result);
  CanvasSlot->SetAnchors(FAnchors(.5f,.5f)); CanvasSlot->SetAlignment(FVector2D(.5f,.5f)); CanvasSlot->SetPosition(FVector2D::ZeroVector); CanvasSlot->SetSize(FVector2D(570,370));
  ResultPanel=WidgetTree->ConstructWidget<UVerticalBox>(); Result->SetContent(ResultPanel);
  AddText(ResultPanel,GM->bWon?TEXT("任务完成"):TEXT("挑战失败"),40,GM->bWon?FLinearColor(.2f,1,.6f):FLinearColor(1,.3f,.25f));
  AddText(ResultPanel,FString::Printf(TEXT("击破 %d 人  ·  精英 %d / %d"),GM->Kills,GM->EliteKilled,GM->EliteTotal),22,FLinearColor::White);
  AddButton(ResultPanel,TEXT("重新挑战  [R]"),TEXT("Restart"));
  AddButton(ResultPanel,TEXT("返回主菜单  [Esc]"),TEXT("Menu"));
 }
}

void UARPGScreen::PlayOne() { if (auto* PC=Cast<AARPGPlayerController>(GetOwningPlayer())) PC->StartStageOne(); }
void UARPGScreen::PlayTwo() { if (auto* PC=Cast<AARPGPlayerController>(GetOwningPlayer())) PC->StartStageTwo(); }
void UARPGScreen::Menu() { if (auto* PC=Cast<AARPGPlayerController>(GetOwningPlayer())) PC->ReturnToMenu(); }
void UARPGScreen::Restart() { if (auto* PC=Cast<AARPGPlayerController>(GetOwningPlayer())) PC->Retry(); }
