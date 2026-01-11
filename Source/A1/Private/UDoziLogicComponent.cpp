#include "UDoziLogicComponent.h"
#include "APointSource.h"
#include "ShieldingWall.h" 
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"
#include "DrawDebugHelpers.h" // Для отладочных линий
#include "Containers/Set.h"


#define ECC_Radiation ECollisionChannel::ECC_GameTraceChannel1
// Предварительная настройка EventTick
UDoziLogicComponent::UDoziLogicComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;
}

void UDoziLogicComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 1. Считаем физику
    CalculateTotalDose();

    // 2. Накопление дозы (мкЗв/ч -> делим на 3600 -> мкЗв/сек)
    if (CurrentDoseRate_Total > 0.0f)
    {
        AccumulatedDose_IED += (CurrentDoseRate_Total / 3600.0f) * DeltaTime;
    }

    // --- НОВОЕ: Накопление НЕЙТРОННОЙ дозы ---
    if (CurrentDoseRate_Neutron > 0.0f)
    {
        AccumulatedDose_Neutron += (CurrentDoseRate_Neutron / 3600.0f) * DeltaTime;
    }

}
//Считаем накопленную дозу с учетом экранирущих материалов
void UDoziLogicComponent::CalculateTotalDose()
{
    UWorld* World = GetWorld();
    if (!World) return;

    TArray<AActor*> FoundSources;
    UGameplayStatics::GetAllActorsOfClass(World, APointSource::StaticClass(), FoundSources);

    FVector SensorLoc = GetOwner()->GetActorLocation();

    float TotalGamma_DoseRate = 0.0f;
    float TotalNeutron_DoseRate = 0.0f;

    for (AActor* Actor : FoundSources)
    {
        APointSource* Source = Cast<APointSource>(Actor);
        if (!Source) continue;

        FVector SourceLoc = Source->GetActorLocation();

        // А. Расчет "чистой" дозы (без стен)
        float RawGamma = Source->GetRawGammaAtPoint(SensorLoc);
        float RawNeutron = Source->GetRawNeutronAtPoint(SensorLoc);

        // Б. Расчет ослабления стенами (от 0.0 до 1.0)
        float ShieldGamma = 1.0f;
        float ShieldNeutron = 1.0f;

        // Если есть смысл считать (доза > 0.01 мкЗв/ч)
        if (RawGamma > 0.01f)
        {
            ShieldGamma = CalculateShieldingFactor(SourceLoc, SensorLoc, Source, ERadiationType::Gamma);
        }

        if (RawNeutron > 0.01f)
        {
            ShieldNeutron = CalculateShieldingFactor(SourceLoc, SensorLoc, Source, ERadiationType::Neutron);
        }

        // В. Суммируем
        TotalGamma_DoseRate += RawGamma * ShieldGamma;
        TotalNeutron_DoseRate += RawNeutron * ShieldNeutron;
    }

    // Г. Перевод в Эквивалентную дозу (Зиверты)
    // Гамма: Q = 1. Нейтроны: Q = 10 (упрощенно)
    float EquivalentGamma = TotalGamma_DoseRate * 1.0f;
    float EquivalentNeutron = TotalNeutron_DoseRate * 10.0f;

    // Записываем отдельно нейтронную составляющую
    CurrentDoseRate_Neutron = EquivalentNeutron;

    CurrentDoseRate_Total = EquivalentGamma + EquivalentNeutron;
}

float UDoziLogicComponent::CalculateShieldingFactor(FVector Start, FVector End, AActor* SourceActor, ERadiationType Type)
{
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());
    QueryParams.AddIgnoredActor(SourceActor);
    QueryParams.bReturnPhysicalMaterial = false;

    TArray<FHitResult> Hits;

    // Используем ECC_Camera (или ECC_GameTraceChannel1, если вы вернули его)
    const ECollisionChannel TraceChannel = ECC_GameTraceChannel1; // Ваш новый канал Radiation

    // LineTraceMulti - должен вернуть ВСЕ попадания, если коллизия Overlap
    bool bHit = GetWorld()->LineTraceMultiByChannel(Hits, Start, End, TraceChannel, QueryParams);

    // --- ОТЛАДКА: ГЛАВНЫЙ ИНДИКАТОР ---
    if (bDrawDebugLines)
    {
        // Красный - во что-то попал, Зеленый - чисто.
        DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Red : FColor::Green, false, 0.1f, 0, 1.0f);
    }
    // ------------------------------------

    if (!bHit) return 1.0f;

    float AttenuationTotal = 1.0f;
    TSet<AActor*> ProcessedWalls;

    // Логирование всех попавших акторов для диагностики
    UE_LOG(LogTemp, Warning, TEXT("--- LineTraceMulti Hits Detected (%d total) ---"), Hits.Num());

    for (const FHitResult& HitResult : Hits)
    {
        AActor* HitActor = HitResult.GetActor();
        FString HitName = HitActor ? HitActor->GetName() : TEXT("NULL Actor");

        // Если HitResult.bBlockingHit = true, это указывает на проблему!
        UE_LOG(LogTemp, Warning, TEXT("Hit: %s | Blocking: %s | Location: %s"),
            *HitName,
            HitResult.bBlockingHit ? TEXT("TRUE") : TEXT("FALSE"),
            *HitResult.ImpactPoint.ToString());

        if (!HitActor || ProcessedWalls.Contains(HitActor))
        {
            continue;
        }

        AShieldingWall* Wall = Cast<AShieldingWall>(HitActor);

        if (Wall)
        {
            ProcessedWalls.Add(HitActor);

            // ... (остальная логика расчета ослабления без изменений) ...
            FShieldingData Data = Wall->GetShieldingInfo();
            float Coeff = (Type == ERadiationType::Gamma) ? Data.LinearAttenuation_Gamma : Data.MacroscopicSection_Neutron;

            if (Coeff > 0.0f)
            {
                float WallAttenuation = FMath::Exp(-Coeff * Wall->Thickness_cm);
                AttenuationTotal *= WallAttenuation;

                UE_LOG(LogTemp, Warning, TEXT("[SHIELDING] Wall: %s | Factor: %f"), *HitName, WallAttenuation);
            }

            if (bDrawDebugLines)
            {
                // Желтая точка - попадание в нашу стену
                DrawDebugPoint(GetWorld(), HitResult.ImpactPoint, 15.0f, FColor::Yellow, false, 0.1f);
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("--- Trace End. Total Attenuation: %f ---"), AttenuationTotal);

    return AttenuationTotal;
}

void UDoziLogicComponent::RecordMeasurement()
{
    FDoseLogEntry NewEntry;
    NewEntry.Location = GetOwner()->GetActorLocation();
    NewEntry.DoseRate = CurrentDoseRate_Total;
    NewEntry.Timestamp = GetWorld()->GetTimeSeconds();

    // Определяем угрозу
    NewEntry.bIsThreat = (CurrentDoseRate_Total > AlarmThreshold_Rate);

    MeasurementLog.Add(NewEntry);

    // Сообщаем UI или Level Blueprint
    if (OnMeasurementTaken.IsBound())
    {
        OnMeasurementTaken.Broadcast(NewEntry);
    }

    // Дебаг сообщение на экран
    if (NewEntry.bIsThreat)
    {
        // Дозиметр кричит, если превышен порог
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, FString::Printf(TEXT("DANGER! Dose Rate: %.2f mkSv/h"), CurrentDoseRate_Total));
    }
    else
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::Printf(TEXT("Safe. Dose Rate: %.2f mkSv/h"), CurrentDoseRate_Total));
    }
}