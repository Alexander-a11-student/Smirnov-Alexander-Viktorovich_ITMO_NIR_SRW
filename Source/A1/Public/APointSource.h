#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "RadiationStructs.h"
#include "APointSource.generated.h"

UCLASS()
class A1_API APointSource : public AActor
{
    GENERATED_BODY()

public:
    APointSource();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radiation Config")
    FDataTableRowHandle IsotopeRow;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Radiation Debug")
    FIsotopeData CachedIsotopeInfo;

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

    // Возвращает "чистую" мощность Гаммы в точке (мкЗв/ч) без учета стен
    float GetRawGammaAtPoint(const FVector& Point) const;

    // Возвращает "чистую" мощность Нейтронов в точке (усл. ед -> мкЗв/ч) без учета стен
    float GetRawNeutronAtPoint(const FVector& Point) const;

protected:
    void UpdateIsotopeFromTable();
    // Помощник для закона обратных квадратов (возвращает 1/R^2)
    float CalculateGeoFactor(float DistCentimeters) const;
};