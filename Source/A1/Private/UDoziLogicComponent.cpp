#include "UDoziLogicComponent.h"
#include "APointSource.h"
#include "ShieldingWall.h" 
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/Set.h"

#define ECC_Radiation ECollisionChannel::ECC_GameTraceChannel1

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

    if (CurrentDoseRate_Neutron > 0.0f)
    {
        AccumulatedDose_Neutron += (CurrentDoseRate_Neutron / 3600.0f) * DeltaTime;
    }
}

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

        float RawGamma = Source->GetRawGammaAtPoint(SensorLoc);
        float RawNeutron = Source->GetRawNeutronAtPoint(SensorLoc);

        float ShieldGamma = 1.0f;
        float ShieldNeutron = 1.0f;

        if (RawGamma > 0.01f)
        {
            ShieldGamma = CalculateShieldingFactor(SourceLoc, SensorLoc, Source, ERadiationType::Gamma);
        }

        if (RawNeutron > 0.01f)
        {
            ShieldNeutron = CalculateShieldingFactor(SourceLoc, SensorLoc, Source, ERadiationType::Neutron);
        }

        TotalGamma_DoseRate += RawGamma * ShieldGamma;
        TotalNeutron_DoseRate += RawNeutron * ShieldNeutron;
    }

    float EquivalentGamma = TotalGamma_DoseRate * 1.0f;
    float EquivalentNeutron = TotalNeutron_DoseRate * 10.0f;

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
    const ECollisionChannel TraceChannel = ECC_Visibility;

    bool bHit = GetWorld()->LineTraceMultiByChannel(Hits, Start, End, TraceChannel, QueryParams);

    if (!bHit) return 1.0f;

    float AttenuationTotal = 1.0f;
    TSet<AActor*> ProcessedWalls;

    for (const FHitResult& HitResult : Hits)
    {
        AActor* HitActor = HitResult.GetActor();
        AShieldingWall* Wall = Cast<AShieldingWall>(HitActor);

        if (Wall && !ProcessedWalls.Contains(HitActor))
        {
            ProcessedWalls.Add(HitActor);

            FShieldingData Data = Wall->GetShieldingInfo();
            float Coeff = (Type == ERadiationType::Gamma) ? Data.LinearAttenuation_Gamma : Data.MacroscopicSection_Neutron;

            if (Coeff > 0.0f)
            {
                float WallAttenuation = FMath::Exp(-Coeff * Wall->Thickness_cm);
                AttenuationTotal *= WallAttenuation;
            }
        }
    }

    return AttenuationTotal;
}

void UDoziLogicComponent::RecordMeasurement()
{
    FDoseLogEntry NewEntry;
    NewEntry.Location = GetOwner()->GetActorLocation();
    NewEntry.DoseRate = CurrentDoseRate_Total;
    NewEntry.Timestamp = GetWorld()->GetTimeSeconds();
    NewEntry.bIsThreat = (CurrentDoseRate_Total > AlarmThreshold_Rate);

    MeasurementLog.Add(NewEntry);

    if (OnMeasurementTaken.IsBound())
    {
        OnMeasurementTaken.Broadcast(NewEntry);
    }
}