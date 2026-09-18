// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyCppActor_One.generated.h"

UCLASS()
class MYCPPPROJECT_API AMyCppActor_One : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMyCppActor_One();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintReadWrite)
	float MyFloat;



	UFUNCTION(BlueprintCallable)
	float MyAddFunction(float DiYiGeCanShu, float DiErGeCanShu);


};
