// Fill out your copyright notice in the Description page of Project Settings.


#include "MyCppActor_One.h"

// Sets default values
AMyCppActor_One::AMyCppActor_One()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AMyCppActor_One::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMyCppActor_One::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


}

float AMyCppActor_One::MyAddFunction(float DiYiGeCanShu,float DiErGeCanShu)
{ 
	return DiYiGeCanShu + DiErGeCanShu;


}
