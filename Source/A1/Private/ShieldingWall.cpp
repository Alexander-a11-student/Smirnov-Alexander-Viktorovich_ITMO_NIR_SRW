#include "ShieldingWall.h" // <--- БЫЛО AShieldingWall.h, СТАЛО ShieldingWall.h
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h" 
#include "RadiationStructs.h" 


// AShieldingWall.cpp
// AShieldingWall.cpp
AShieldingWall::AShieldingWall()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldingMesh123"));
	RootComponent = MeshComp;

	// Включаем коллизию для запросов
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// 1. По умолчанию всё БЛОКИРУЕМ (чтобы игрок не проваливался)
	MeshComp->SetCollisionResponseToAllChannels(ECR_Block);

	// 2. ВАЖНО: Принудительно ставим Overlap для канала, который будем использовать в дозиметре
	// Используем ECC_Camera, так как он менее подвержен коллизии со WorldStatic объектами
	MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Overlap);
}
// ... остальной код AShieldingWall.cpp без изменений ...

void AShieldingWall::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateShieldingFromTable();
}

void AShieldingWall::BeginPlay()
{
	Super::BeginPlay();
	UpdateShieldingFromTable();
}

void AShieldingWall::UpdateShieldingFromTable()
{
	if (!ShieldingMaterialRow.DataTable || ShieldingMaterialRow.RowName.IsNone())
	{
		CachedShieldingData = FShieldingData();
		return;
	}

	static const FString ContextString(TEXT("Shielding Data Context"));
	FShieldingData* RowData = ShieldingMaterialRow.GetRow<FShieldingData>(ContextString);

	if (RowData)
	{
		CachedShieldingData = *RowData;
	}
	else
	{
		CachedShieldingData = FShieldingData();
		// Можно добавить лог для отладки, если нужно
	}
}

FShieldingData AShieldingWall::GetShieldingInfo() const
{
	return CachedShieldingData;
}