// Fill out your copyright notice in the Description page of Project Settings.


#include "SSCharacter.h"


// Sets default values
ASSCharacter::ASSCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ASSCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void ASSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

