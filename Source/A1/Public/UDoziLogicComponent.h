#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RadiationStructs.h"
#include "UDoziLogicComponent.generated.h"

// Делегат для уведомления UI о новом замере (по желанию)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeasurementTaken, FDoseLogEntry, Entry);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class A1_API UDoziLogicComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDoziLogicComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // --- Параметры симуляции ---
    UPROPERTY(BlueprintReadOnly, Category = "Radiation Stats")
    float CurrentDoseRate_Total = 0.0f; // Текущая мощность (мкЗв/ч)

    // --- НОВОЕ: Текущая мощность только нейтронов ---
    UPROPERTY(BlueprintReadOnly, Category = "Radiation Stats")
    float CurrentDoseRate_Neutron = 0.0f;

    // --- НОВОЕ: Накопленная доза только нейтронов ---
    UPROPERTY(BlueprintReadOnly, Category = "Radiation Stats")
    float AccumulatedDose_Neutron = 0.0f;



    UPROPERTY(BlueprintReadOnly, Category = "Radiation Stats")
    float AccumulatedDose_IED = 0.0f;   // Накопленная доза (мкЗв)

    UPROPERTY(EditAnywhere, Category = "Radiation Config")
    float AlarmThreshold_Rate = 50.0f;  // Порог тревоги по мощности

    UPROPERTY(EditAnywhere, Category = "Radiation Config")
    float FatalThreshold_IED = 50000.0f; // Порог смертельной дозы (50 мЗв = 50000 мкЗв)

    // --- Журнал замеров ---
    UPROPERTY(BlueprintReadOnly, Category = "Radiation Log")
    TArray<FDoseLogEntry> MeasurementLog;

    UPROPERTY(BlueprintAssignable, Category = "Radiation Log")
    FOnMeasurementTaken OnMeasurementTaken;

    // Добавлен флаг для отладки лучей
    UPROPERTY(EditAnywhere, Category = "Debug")
    bool bDrawDebugLines = true;

    // Функция для кнопки "Измерить"
    UFUNCTION(BlueprintCallable, Category = "Radiation Input")
    void RecordMeasurement();

protected:
    void CalculateTotalDose();
    float CalculateShieldingFactor(FVector Start, FVector End, AActor* SourceActor, ERadiationType Type);
};