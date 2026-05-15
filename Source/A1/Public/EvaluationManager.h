#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DrawDebugHelpers.h"

#include "EvaluationManager.generated.h"


// Предварительное объявление, чтобы не было ошибок циклической зависимости
class APointSource;
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

    UFUNCTION(BlueprintCallable, Category = "Evaluation|Spawning")
    void SpawnNeutronInRandomBox();

    // Функция для вызова из Blueprints или кода
    UFUNCTION(BlueprintCallable, Category = "Evaluation|Pathfinding")
    void VisualizeOptimalPath();

protected:
    virtual void BeginPlay() override;

    // Класс точек спавна (твой SpawnSource_2)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation|Spawning")
    TSubclassOf<AActor> SpawnLocationClass;

    // Класс точек спавна (твой SpawnSource_8)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation|Spawning")
    TSubclassOf<AActor> ASpawnSource_8;

    // Класс нейтронного источника (BP_PointSource_2_scenario_1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation|Spawning")
    TSubclassOf<AActor> NeutronSourceClass;

    // Класс обычного изотопа (BP_PointSource_scenario_1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation|Spawning")
    TSubclassOf<AActor> StandardSourceClass;


    //Поиск пути
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation|Pathfinding", meta = (AllowedClasses = "Actor"))
    TSoftObjectPtr<AActor> InternalStartActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation|Pathfinding", meta = (AllowedClasses = "Actor"))
    TSoftObjectPtr<AActor> InternalEndActor;

public:
    // Класс маркера, который будет спавниться на месте пика (назначь в Blueprints!)
    UPROPERTY(EditAnywhere, Category = "Evaluation|Peaks")
    TSubclassOf<AActor> PeakMarkerClass;

    // Интервал между спавном маркеров, чтобы не заспамить одну точку (в секундах)
    UPROPERTY(EditAnywhere, Category = "Evaluation|Peaks")
    float PeakRecordInterval = 5.0f;


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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evaluation")
    float PeakThreshold = 1000.0f;

    // Счетчик зафиксированных пиков (ошибок)
    int32 DetectedPeaksCount = 0;

    // Массив временных меток, когда были пойманы пики (для детального отчета)
    TArray<float> PeakTimestamps;

    TArray<FVector> SafeZoneLocations;

private:
    void SimulateIdealRun();

    // Время, проведенное в опасной зоне
    float TimeInDangerZone = 0.0f;
    
    // Время последнего зафиксированного пика (для кулдауна в 5 сек)
    float LastPeakRecordTime = -100.0f; 

    // Массив координат, где были зафиксированы пики (для экспорта)
    TArray<FVector> PeakLocations;


    void RecordSnapshot();

    // Вспомогательная функция для расчета дозы в любой точке (без дозиметра)
    float GetDoseRateAtLocation(FVector Location);

    // Простая реализация сетки и поиска (A*)
    TArray<FVector> PathfindSafeRoute(FVector Start, FVector End);


    UPROPERTY()
    UDoziLogicComponent* TargetDosimeter;

    TArray<FRunSnapshot> CurrentRunData;
    FTimerHandle RecordTimerHandle;

    bool bIsRecording = false;
    float RunStartTime = 0.0f;

    float IdealAccumulatedDose = 0.0f; // Результат симуляции
    float SimulationSpeed = 600.0f;    // Скорость (WalkSpeed)
};