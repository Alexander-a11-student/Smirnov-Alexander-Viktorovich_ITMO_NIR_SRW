#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h" // <<< ОЧЕНЬ ВАЖНО для DataTables
#include "RadiationStructs.generated.h" // <<< Генерация кода Unreal

// Тип излучения 
UENUM(BlueprintType)
enum class ERadiationType : uint8
{
    Gamma       UMETA(DisplayName = "Gamma"),
    Neutron     UMETA(DisplayName = "Neutron"),
    Mixed       UMETA(DisplayName = "Mixed")
};

// --- FIsotopeData ---
USTRUCT(BlueprintType)
struct FIsotopeData : public FTableRowBase // Наследуем, чтобы использовать в DataTable
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ERadiationType Type;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GammaConstant;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Activity_MBq;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NeutronYield;
};

// Структура записи в журнал замеров
USTRUCT(BlueprintType)
struct FDoseLogEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FVector Location;

    UPROPERTY(BlueprintReadOnly)
    float DoseRate; // Мощность в этот момент

    UPROPERTY(BlueprintReadOnly)
    float Timestamp;

    UPROPERTY(BlueprintReadOnly)
    bool bIsThreat; // Превышен ли порог
};

// --- FShieldingData ---
USTRUCT(BlueprintType)
struct FShieldingData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MaterialName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LinearAttenuation_Gamma;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MacroscopicSection_Neutron;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Density;
};

// **ВАЖНО:** Вам не нужен файл .cpp для этого!
// Это просто Header, содержащий определения данных.