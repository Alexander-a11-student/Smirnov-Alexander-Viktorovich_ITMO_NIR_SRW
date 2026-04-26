#include "EvaluationManager.h"
#include "UDoziLogicComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "TimerManager.h" // Обязательно для 4.27
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformFile.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UEvaluationManager::UEvaluationManager()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UEvaluationManager::BeginPlay()
{
    Super::BeginPlay();
    TargetDosimeter = GetOwner()->FindComponentByClass<UDoziLogicComponent>();
}

void UEvaluationManager::StartRun()
{
    if (bIsRecording || !TargetDosimeter) return;

    ResetRun();
    bIsRecording = true;
    RunStartTime = GetWorld()->GetTimeSeconds();

    GetWorld()->GetTimerManager().SetTimer(RecordTimerHandle, this, &UEvaluationManager::RecordSnapshot, RecordInterval, true);
    UE_LOG(LogTemp, Warning, TEXT("Evaluation: Run Started"));
}

void UEvaluationManager::RecordSnapshot()
{
    if (!TargetDosimeter) return;

    FRunSnapshot Snapshot;
    Snapshot.Timestamp = GetWorld()->GetTimeSeconds() - RunStartTime;
    Snapshot.PlayerLocation = GetOwner()->GetActorLocation();
    Snapshot.CurrentDoseRate = TargetDosimeter->CurrentDoseRate_Total;
    Snapshot.AccumulatedTotalDose = TargetDosimeter->AccumulatedDose_IED;

    CurrentRunData.Add(Snapshot);
}


float UEvaluationManager::CalculateSafetyIndicator(float AccDose)
{
    if (DoseLimit <= 0.0f) return 0.0f;
    float RawScore = ((DoseLimit - AccDose) / DoseLimit) * 100.0f;
    float RoundedScore = FMath::RoundToFloat(RawScore * 10.0f) / 10.0f;
    return FMath::Max(RoundedScore, 0.0f);
}

FString UEvaluationManager::GetSafetyLettergrade() const
{
    if (SafetyScore >= 90.0f) return TEXT("A");
    if (SafetyScore >= 80.0f) return TEXT("B");
    if (SafetyScore >= 70.0f) return TEXT("C");
    if (SafetyScore >= 60.0f) return TEXT("D");

    return TEXT("F");
}

FString UEvaluationManager::GetFinalGrade() const
{
    if (CorrectRoomsCount < 6)
    {
        return TEXT("F (Rooms failed)");
    }

    if (SafetyScore >= 90.0f) return TEXT("A");
    if (SafetyScore >= 80.0f) return TEXT("B");
    if (SafetyScore >= 70.0f) return TEXT("C");
    if (SafetyScore >= 60.0f) return TEXT("D");

    return TEXT("F (Low Safety)");
}

bool UEvaluationManager::IsSafetyPassed() const
{
    return SafetyScore >= PassThreshold;
}

void UEvaluationManager::StopRunAndExport(FString FileNamePrefix)
{
    if (!bIsRecording || !TargetDosimeter) return;

    bIsRecording = false;
    GetWorld()->GetTimerManager().ClearTimer(RecordTimerHandle);

    float FinalAccDose = TargetDosimeter->AccumulatedDose_IED;
    SafetyScore = CalculateSafetyIndicator(FinalAccDose);

        UE_LOG(LogTemp, Warning,
            TEXT("Evaluation Finished. Final Dose: %.4f, Safety Score: %.2f%%"),
            FinalAccDose,
            SafetyScore
        );

    FString CSVContent = FString::Printf(
        TEXT("Scenario Evaluation\n")
        TEXT("Limit Dose: %.2f, Final Dose: %.4f, Safety Score: %.2f%%\n\n"),
        DoseLimit,
        FinalAccDose,
        SafetyScore
    );

    CSVContent += TEXT("Time_s,LocX,LocY,LocZ,DoseRate_mkSv_h,AccumulatedDose_mkSv\n");

    for (const FRunSnapshot& Snap : CurrentRunData)
    {
        CSVContent += FString::Printf(
            TEXT("%.2f,%.1f,%.1f,%.1f,%.4f,%.4f\n"),
            Snap.Timestamp,
            Snap.PlayerLocation.X,
            Snap.PlayerLocation.Y,
            Snap.PlayerLocation.Z,
            Snap.CurrentDoseRate,
            Snap.AccumulatedTotalDose
        );
    }

    FString Directory = FPaths::LaunchDir() / TEXT("Evaluations");

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.DirectoryExists(*Directory))
    {
        PlatformFile.CreateDirectoryTree(*Directory);
    }

    FString TimestampStr = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));

    FString FullPath = Directory / FString::Printf(TEXT("%s_%s.csv"), *FileNamePrefix, *TimestampStr);

    bool bSaved = FFileHelper::SaveStringToFile(CSVContent, *FullPath);

    if (bSaved)
    {
        UE_LOG(LogTemp, Warning, TEXT("CSV SUCCESSFULLY SAVED: %s"), *FullPath);

        FPlatformProcess::ExploreFolder(*Directory);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("FAILED TO SAVE CSV: %s"), *FullPath);
    }
}

void UEvaluationManager::ResetRun()
{
    CurrentRunData.Empty();
    SafetyScore = 0.0f;
    RunStartTime = 0.0f;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(RecordTimerHandle);
    }

    if (TargetDosimeter)
    {
        TargetDosimeter->AccumulatedDose_IED = 0.0f;
        TargetDosimeter->AccumulatedDose_Neutron = 0.0f;
        TargetDosimeter->CurrentDoseRate_Total = 0.0f;
    }

    UE_LOG(LogTemp, Log, TEXT("Evaluation: Manager and Dosimeter have been fully reset."));
}

void UEvaluationManager::SpawnScenarioSources()
{
    // Проверяем, что ты не забыл назначить классы в редакторе
    if (!SpawnLocationClass || !NeutronSourceClass || !StandardSourceClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Evaluation: Classes for spawning are not assigned in the Manager!"));
        return;
    }

    // 1. Получаем всех экторов-точек спавна
    TArray<AActor*> FoundLocations;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), SpawnLocationClass, FoundLocations);

    if (FoundLocations.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Evaluation: No Spawn Locations found on the level!"));
        return;
    }


    auto GetIndexFromName = [](const AActor* InActor) -> int32 {
        FString Name = InActor->GetName();
        FString NumberStr = TEXT("");
        // Идем с конца строки и собираем цифры
        for (int32 i = Name.Len() - 1; i >= 0; --i) {
            if (FChar::IsDigit(Name[i])) {
                NumberStr.InsertAt(0, Name[i]);
            }
            else if (NumberStr.Len() > 0) {
                break; // Цифры закончились
            }
        }
        return FCString::Atoi(*NumberStr);
        };

    FoundLocations.Sort([&](const AActor& A, const AActor& B) {
        return GetIndexFromName(&A) < GetIndexFromName(&B);
        });

    // 3. Обработка группами по 4
    for (int32 i = 0; i < FoundLocations.Num(); i += 4)
    {
        // Создаем пул того, что нужно заспавнить в этой четверке:
        // 1 нейтрон, 2 обычных, 1 пустышка (представлена как nullptr)
        TArray<TSubclassOf<AActor>> SpawnPool = {
            NeutronSourceClass,
            StandardSourceClass,
            StandardSourceClass,
            nullptr
        };

        // 4. Перемешиваем массив (алгоритм Фишера-Йетса) для случайного порядка
        for (int32 j = SpawnPool.Num() - 1; j > 0; j--)
        {
            int32 SwapIndex = FMath::RandRange(0, j);
            SpawnPool.Swap(j, SwapIndex);
        }

        // 5. Спавним акторов по отсортированным локациям
        for (int32 j = 0; j < 4; j++)
        {
            // Защита от выхода за пределы массива, если количество точек не кратно 4
            if (i + j >= FoundLocations.Num()) break;

            TSubclassOf<AActor> ClassToSpawn = SpawnPool[j];

            // Если в этой ячейке массива оказался nullptr, значит место остается пустым
            if (ClassToSpawn != nullptr)
            {
                AActor* LocationActor = FoundLocations[i + j];
                FVector SpawnPos = LocationActor->GetActorLocation();
                FRotator SpawnRot = LocationActor->GetActorRotation();

                GetWorld()->SpawnActor<AActor>(ClassToSpawn, SpawnPos, SpawnRot);
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Evaluation: Scenario Sources Spawned Successfully!"));
}