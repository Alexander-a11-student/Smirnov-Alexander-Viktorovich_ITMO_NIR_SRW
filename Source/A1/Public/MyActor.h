// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyActor.generated.h"

UCLASS()
class A1_API AMyActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMyActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TestValue") int32 MyValueA;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TestValue") int32 MyValueB;

	UFUNCTION(BlueprintCallable, Category = "TestFunction") int32 MyFunction (int32 val1, int32 val2);
	UFUNCTION(BlueprintImplementableEvent, Category = "TestFunction") void OnCalculateMyFunction();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
