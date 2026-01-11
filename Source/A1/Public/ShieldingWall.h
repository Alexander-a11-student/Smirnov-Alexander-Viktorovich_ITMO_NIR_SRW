#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "RadiationStructs.h"
#include "ShieldingWall.generated.h" // <--- ашкн AShieldingWall.generated.h

UCLASS()
class A1_API AShieldingWall : public AActor
{
    GENERATED_BODY()

public:
    AShieldingWall();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radiation Shielding")
    FDataTableRowHandle ShieldingMaterialRow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radiation Shielding")
    float Thickness_cm = 10.0f;



    FShieldingData GetShieldingInfo() const;

protected:
    void UpdateShieldingFromTable();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

private:
    FShieldingData CachedShieldingData;
};