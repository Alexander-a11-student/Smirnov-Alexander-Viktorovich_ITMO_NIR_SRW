#include "APointSource.h"

APointSource::APointSource()
{
    PrimaryActorTick.bCanEverTick = false;
}

void APointSource::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateIsotopeFromTable();
}

void APointSource::BeginPlay()
{
    Super::BeginPlay();
    UpdateIsotopeFromTable();
}

void APointSource::UpdateIsotopeFromTable()
{
    if (!IsotopeRow.DataTable || IsotopeRow.RowName.IsNone()) return;

    static const FString ContextString(TEXT("Isotope Context"));
    FIsotopeData* RowData = IsotopeRow.GetRow<FIsotopeData>(ContextString);

    if (RowData)
    {
        CachedIsotopeInfo = *RowData;
    }
}

float APointSource::CalculateGeoFactor(float DistCentimeters) const
{
    // Переводим сантиметры UE в метры для физических формул
    float DistMeters = DistCentimeters / 100.0f;

    // Защита от деления на ноль (минимум 1 см)
    if (DistMeters < 0.01f) DistMeters = 0.01f;

    return 1.0f / (DistMeters * DistMeters);
}

float APointSource::GetRawGammaAtPoint(const FVector& Point) const
{
    // Если источник чисто нейтронный, гаммы нет
    if (CachedIsotopeInfo.Type == ERadiationType::Neutron) return 0.0f;

    float Dist = FVector::Distance(GetActorLocation(), Point);
    float GeoFactor = CalculateGeoFactor(Dist);

    // Формула: (Активность * ГаммаКонстанта) / R^2
    // Activity_MBq * GammaConstant дает мкЗв * м^2 / ч
    // Делим на м^2, получаем мкЗв/ч
    return CachedIsotopeInfo.Activity_MBq * CachedIsotopeInfo.GammaConstant * GeoFactor;
}

float APointSource::GetRawNeutronAtPoint(const FVector& Point) const
{
    // Если источник чисто гамма, нейтронов нет
    if (CachedIsotopeInfo.Type == ERadiationType::Gamma) return 0.0f;

    float Dist = FVector::Distance(GetActorLocation(), Point);
    float GeoFactor = CalculateGeoFactor(Dist);

    // Упрощенная формула для нейтронов: Активность * Выход * Геометрия
    // Результат нужно будет умножить на коэффициент качества в Дозиметре
    return CachedIsotopeInfo.Activity_MBq * CachedIsotopeInfo.NeutronYield * GeoFactor * 10000.0f; // *10000 - калибровочный множитель для игры
}