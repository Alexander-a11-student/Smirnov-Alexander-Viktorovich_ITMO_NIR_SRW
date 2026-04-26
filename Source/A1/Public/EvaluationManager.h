#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EvaluationManager.generated.h"

// Предварительное объявление, чтобы не было ошибок циклической зависимости
class UDoziLogicComponent;

USTRUCT(BlueprintType)
struct FRunSnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    float Timestamp;
    UPROPERTY()
    FVector PlayerLocation;
    UPROPERTY()
    float CurrentDoseRate;
    UPROPERTY()
    float AccumulatedTotalDose;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class A1_API UEvaluationManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UEvaluationManager();

    UFUNCTION(BlueprintCallable, Category = "Evaluation|Spawning")
    void SpawnScenarioSources();

protected:
    virtual void BeginPlay() override;

    // Класс точек спавна (твой SpawnSource_2)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation|Spawning")
    TSubclassOf<AActor> SpawnLocationClass;

    // Класс нейтронного источника (BP_PointSource_2_scenario_1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation|Spawning")
    TSubclassOf<AActor> NeutronSourceClass;

    // Класс обычного изотопа (BP_PointSource_scenario_1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation|Spawning")
    TSubclassOf<AActor> StandardSourceClass;

public:
    UFUNCTION(BlueprintCallable, Category = "Evaluation")
    void StartRun();

    UFUNCTION(BlueprintCallable, Category = "Evaluation")
    void StopRunAndExport(FString FileNamePrefix = TEXT("RunData"));

    UFUNCTION(BlueprintCallable, Category = "Evaluation")
    void ResetRun();

    float CalculateSafetyIndicator(float AccDose);

    UFUNCTION(BlueprintPure, Category = "Evaluation")
    bool IsSafetyPassed() const;

    // Сюда будешь записывать количество правильных комнат из Blueprints
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation")
    int32 CorrectRoomsCount = 0;

    // 1. Метод для получения оценки только по Safety (A, B, C, D, F)
    UFUNCTION(BlueprintPure, Category = "Evaluation")
    FString GetSafetyLettergrade() const;

    // 2. Метод для итоговой оценки с учетом комнат
    UFUNCTION(BlueprintPure, Category = "Evaluation")
    FString GetFinalGrade() const;


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation")
    float PassThreshold = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation")
    float RecordInterval = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation")
    float DoseLimit = 15.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Evaluation")
    float SafetyScore = 0.0f;

private:
    void RecordSnapshot();

    UPROPERTY()
    UDoziLogicComponent* TargetDosimeter;

    TArray<FRunSnapshot> CurrentRunData;
    FTimerHandle RecordTimerHandle;

    bool bIsRecording = false;
    float RunStartTime = 0.0f;
};