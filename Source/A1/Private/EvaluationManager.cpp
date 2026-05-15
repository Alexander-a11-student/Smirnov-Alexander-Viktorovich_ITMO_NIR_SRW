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
#include "Components/BoxComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "APointSource.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "ShieldingWall.h"

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

    AActor* StartActor = InternalStartActor.Get();
    AActor* EndActor = InternalEndActor.Get();

    // Проверки
    if (!StartActor || !EndActor)
    {
        UE_LOG(LogTemp, Error, TEXT("StartActor or EndActor is NULL!"));
        return;
    }

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

    // --- ЛОГИКА ОБНАРУЖЕНИЯ ПИКОВ И ВРЕМЕНИ В ОПАСНОЙ ЗОНЕ ---
    if (Snapshot.CurrentDoseRate >= PeakThreshold)
    {
        DetectedPeaksCount++;
        PeakTimestamps.Add(Snapshot.Timestamp);

        // Добавляем интервал таймера к общему времени в опасной зоне
        TimeInDangerZone += RecordInterval;

        // Проверяем, прошло ли 5 секунд с момента последнего спавна маркера
        if (Snapshot.Timestamp - LastPeakRecordTime >= PeakRecordInterval)
        {
            LastPeakRecordTime = Snapshot.Timestamp;
            PeakLocations.Add(Snapshot.PlayerLocation);

            // Спавним маркер на карте, если назначен класс
            if (PeakMarkerClass)
            {
                GetWorld()->SpawnActor<AActor>(PeakMarkerClass, Snapshot.PlayerLocation, FRotator::ZeroRotator);
                UE_LOG(LogTemp, Warning, TEXT("Evaluation: Peak Marker spawned at X:%.1f Y:%.1f"), Snapshot.PlayerLocation.X, Snapshot.PlayerLocation.Y);
            }
        }
    }

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

    AActor* StartActor = InternalStartActor.Get();
    AActor* EndActor = InternalEndActor.Get();

    // 1. Рассчитываем идеальные показатели для сравнения
    float IdealFinalDose = 0.0f;
    TArray<FVector> IdealPath = (StartActor && EndActor) ? PathfindSafeRoute(StartActor->GetActorLocation(), EndActor->GetActorLocation()) : TArray<FVector>();

    float FinalAccDose = TargetDosimeter->AccumulatedDose_IED;
    SafetyScore = CalculateSafetyIndicator(FinalAccDose);
    FString FinalGradeStr = GetFinalGrade();

    // --- ГЕНЕРАЦИЯ CSV С РАСШИРЕННЫМ ОТЧЕТОМ ---
    FString CSVContent = FString::Printf(
        TEXT("Scenario Evaluation Report\n")
        TEXT("Final Grade: %s\n")
        TEXT("Safety Score: %.2f%%\n")
        TEXT("Limit Dose: %.2f, Player Final Dose: %.4f\n")
        TEXT("Errors (High Dose Ticks): %d\n")
        TEXT("Total Time in Danger Zone: %.1f seconds\n\n"),
        *FinalGradeStr, SafetyScore, DoseLimit, FinalAccDose, DetectedPeaksCount, TimeInDangerZone
    );

    // --- ТАБЛИЦА 1: ДАННЫЕ ИГРОКА ---
    CSVContent += TEXT("--- PLAYER DATA ---\n");
    CSVContent += TEXT("Time_s,LocX,LocY,LocZ,DoseRate_mkSv_h,AccumulatedDose_mkSv\n");

    for (const FRunSnapshot& Snap : CurrentRunData)
    {
        CSVContent += FString::Printf(
            TEXT("%.2f,%.1f,%.1f,%.1f,%.4f,%.4f\n"),
            Snap.Timestamp, Snap.PlayerLocation.X, Snap.PlayerLocation.Y, Snap.PlayerLocation.Z,
            Snap.CurrentDoseRate, Snap.AccumulatedTotalDose
        );
    }

    // --- ТАБЛИЦА 2: ДАННЫЕ ИДЕАЛЬНОГО МАРШРУТА (СИНХРОННЫЙ БОТ-БЕНЧМАРК) ---
    CSVContent += TEXT("\n--- IDEAL PATH DATA (BOT BENCHMARK) ---\n");
    CSVContent += TEXT("Time_s,LocX,LocY,LocZ,DoseRate_mkSv_h,AccumulatedDose_mkSv\n");

    float IdealTime = 0.0f;
    float RunningIdealDose = 0.0f;
    float ConstantSpeed = 600.0f; // Стандартная скорость (см/с)
    float NextLogTime = 0.0f;     // Тайм-стемп для следующей фиксации записи

    for (int32 i = 0; i < IdealPath.Num(); i++)
    {
        FVector CurrentPos = IdealPath[i];
        float CurrentRate = GetDoseRateAtLocation(CurrentPos);

        if (i > 0)
        {
            float Distance = FVector::Dist(CurrentPos, IdealPath[i - 1]);
            float DeltaT = Distance / ConstantSpeed;
            IdealTime += DeltaT;
            // Доза за микро-интервал сегмента: (мкЗв/ч / 3600) * сек
            RunningIdealDose += (CurrentRate / 3600.0f) * DeltaT;
        }

        // Условие записи: начальная точка, пересечение шага RecordInterval, или финальная точка
        if (i == 0 || IdealTime >= NextLogTime || i == IdealPath.Num() - 1)
        {
            CSVContent += FString::Printf(
                TEXT("%.2f,%.1f,%.1f,%.1f,%.4f,%.4f\n"),
                IdealTime, CurrentPos.X, CurrentPos.Y, CurrentPos.Z, CurrentRate, RunningIdealDose
            );

            // Вычисляем следующую ровную временную отметку на основе RecordInterval
            NextLogTime = FMath::FloorToFloat(IdealTime / RecordInterval) * RecordInterval + RecordInterval;
        }
    }
    IdealFinalDose = RunningIdealDose;

    // --- COMPARATIVE METRICS ---
    float DoseRatio = 0.0f;
    float DoseReductionPercent = 0.0f;

    if (IdealFinalDose > KINDA_SMALL_NUMBER)
    {
        DoseRatio = FinalAccDose / IdealFinalDose;

        DoseReductionPercent =
            (1.0f - (IdealFinalDose / FinalAccDose)) * 100.0f;
    }

    // Сравнение в конце файла
    CSVContent += FString::Printf(TEXT("\nSUMMARY: Player Dose %.4f vs Ideal Dose %.4f\n"), FinalAccDose, IdealFinalDose);

    CSVContent += FString::Printf(
        TEXT("Dose Ratio (Player/Ideal): %.2fx\n"),
        DoseRatio
    );

    CSVContent += FString::Printf(
        TEXT("Dose Reduction (Ideal Path): %.1f%%\n"),
        DoseReductionPercent
    );

    // --- КООРДИНАТЫ ПИКОВ ---
    CSVContent += TEXT("\n--- EXPOSURE PEAK LOCATIONS ---\n");
    CSVContent += TEXT("LocX,LocY,LocZ\n");
    for (const FVector& Loc : PeakLocations)
    {
        CSVContent += FString::Printf(TEXT("%.1f,%.1f,%.1f\n"), Loc.X, Loc.Y, Loc.Z);
    }

    // --- СОХРАНЕНИЕ ---
    FString Directory = FPaths::LaunchDir() / TEXT("Evaluations");
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.DirectoryExists(*Directory))
    {
        PlatformFile.CreateDirectoryTree(*Directory);
    }

    FString TimestampStr = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    FString FullPath = Directory / FString::Printf(TEXT("Grade_%s_%s.csv"), *FinalGradeStr.Left(1), *TimestampStr);

    if (FFileHelper::SaveStringToFile(CSVContent, *FullPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("CSV SAVED WITH IDEAL PATH: %s"), *FullPath);
        FPlatformProcess::ExploreFolder(*Directory);
    }
}

void UEvaluationManager::ResetRun()
{
    CurrentRunData.Empty();
    PeakTimestamps.Empty();
    PeakLocations.Empty(); // Очищаем массив локаций

    DetectedPeaksCount = 0;
    SafetyScore = 0.0f;
    RunStartTime = 0.0f;
    TimeInDangerZone = 0.0f; // Сбрасываем время
    LastPeakRecordTime = -100.0f; // Сбрасываем кулдаун в отрицательное значение, чтобы первый пик записался сразу

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(RecordTimerHandle);
    }

    if (TargetDosimeter)
    {
        TargetDosimeter->AccumulatedDose_IED = 0.0f;
        TargetDosimeter->AccumulatedDose_Neutron = 0.0f;
        TargetDosimeter->CurrentDoseRate_Total = 0.0f;
        TargetDosimeter->MeasurementLog.Empty(); // Очищаем историю замеров
    }

    SimulateIdealRun();

    UE_LOG(LogTemp, Log, TEXT("Evaluation: Manager fully reset."));
}

void UEvaluationManager::SpawnScenarioSources()
{
    SafeZoneLocations.Empty();

    if (!SpawnLocationClass || !NeutronSourceClass || !StandardSourceClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Evaluation: Classes for spawning are not assigned in the Manager!"));
        return;
    }

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
        for (int32 i = Name.Len() - 1; i >= 0; --i) {
            if (FChar::IsDigit(Name[i])) {
                NumberStr.InsertAt(0, Name[i]);
            }
            else if (NumberStr.Len() > 0) {
                break;
            }
        }
        return FCString::Atoi(*NumberStr);
        };

    FoundLocations.Sort([&](const AActor& A, const AActor& B) {
        return GetIndexFromName(&A) < GetIndexFromName(&B);
        });

    for (int32 i = 0; i < FoundLocations.Num(); i += 4)
    {
        TArray<TSubclassOf<AActor>> SpawnPool = {
            NeutronSourceClass,
            StandardSourceClass,
            StandardSourceClass,
            nullptr
        };

        for (int32 j = SpawnPool.Num() - 1; j > 0; j--)
        {
            int32 SwapIndex = FMath::RandRange(0, j);
            SpawnPool.Swap(j, SwapIndex);
        }



        for (int32 j = 0; j < 4; j++)
        {
            if (i + j >= FoundLocations.Num()) break;

            TSubclassOf<AActor> ClassToSpawn = SpawnPool[j];

            if (ClassToSpawn != nullptr)
            {
                AActor* LocationActor = FoundLocations[i + j];
                FVector SpawnPos = LocationActor->GetActorLocation();
                FRotator SpawnRot = LocationActor->GetActorRotation();

                GetWorld()->SpawnActor<AActor>(ClassToSpawn, SpawnPos, SpawnRot);
            }
            else {
                // ЭТО БЕЗОПАСНОЕ ОКНО — записываем его!
                SafeZoneLocations.Add(FoundLocations[i + j]->GetActorLocation());
                UE_LOG(LogTemp, Log, TEXT("Evaluation: Registered Safe Zone at %s"), *FoundLocations[i + j]->GetActorLocation().ToString());
            }
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("Evaluation: Scenario Sources Spawned Successfully!"));
}

void UEvaluationManager::SpawnNeutronInRandomBox()
{
    // Ищем все SpawnSource_8
    TArray<AActor*> FoundBoxes;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASpawnSource_8, FoundBoxes);

    if (FoundBoxes.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No SpawnSource_8 actors found!"));
        return;
    }

    if (!NeutronSourceClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("NeutronSourceClass is NULL!"));
        return;
    }

    // Проходим по всем найденным Box-акторам
    for (AActor* AreaActor : FoundBoxes)
    {
        if (!AreaActor)
            continue;

        // Ищем BoxComponent
        UBoxComponent* BoxComp = AreaActor->FindComponentByClass<UBoxComponent>();

        if (!BoxComp)
        {
            UE_LOG(LogTemp, Warning, TEXT("BoxComponent not found in %s"), *AreaActor->GetName());
            continue;
        }

        // Случайная точка внутри Box
        FVector SpawnPos = UKismetMathLibrary::RandomPointInBoundingBox(
            BoxComp->GetComponentLocation(),
            BoxComp->GetScaledBoxExtent()
        );

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        // Спавним источник
        AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(
            NeutronSourceClass,
            SpawnPos,
            FRotator::ZeroRotator,
            SpawnParams
        );

        if (SpawnedActor)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Neutron source spawned in %s at location %s"),
                *AreaActor->GetName(),
                *SpawnPos.ToString()
            );
        }
    }
}

// 1. Считаем дозу в абстрактной точке
float UEvaluationManager::GetDoseRateAtLocation(FVector Location)
{
    TArray<AActor*> FoundSources;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APointSource::StaticClass(), FoundSources);

    float TotalDoseRate = 0.0f;

    for (AActor* Actor : FoundSources)
    {
        APointSource* Source = Cast<APointSource>(Actor);
        if (!Source) continue;

        // 1. Считаем "чистую" физику (расстояние)
        float Gamma = Source->GetRawGammaAtPoint(Location);
        float Neutron = Source->GetRawNeutronAtPoint(Location);

        // 2. Учитываем стены (Экранирование) через компонент игрока
        if (TargetDosimeter)
        {
            // Передаем 4 аргумента: Откуда, Куда, Игнорируемый актер (Source), Тип
            Gamma *= TargetDosimeter->CalculateShieldingFactor(Source->GetActorLocation(), Location, Source, ERadiationType::Gamma);
            Neutron *= TargetDosimeter->CalculateShieldingFactor(Source->GetActorLocation(), Location, Source, ERadiationType::Neutron);
        }

        // Веса: 1.0 для Гаммы, 10.0 для Нейтронов
        TotalDoseRate += (Gamma * 1.0f) + (Neutron * 10.0f);
    }
    return TotalDoseRate;
}

// 2. Основная логика визуализации
void UEvaluationManager::VisualizeOptimalPath()
{
    AActor* StartActor = InternalStartActor.Get();
    AActor* EndActor = InternalEndActor.Get();

    if (!StartActor || !EndActor) return;

    FVector StartPos = StartActor->GetActorLocation();
    FVector EndPos = EndActor->GetActorLocation();

    // Генерируем путь (упрощенно: 10 сегментов для примера)
    // В идеале тут должен быть A*, но для визуализации концепции сделаем "умный" обход источников
    TArray<FVector> Path = PathfindSafeRoute(StartPos, EndPos);

    for (int32 i = 0; i < Path.Num() - 1; i++)
    {
        DrawDebugLine(
            GetWorld(),
            Path[i],
            Path[i + 1],
            FColor::Green,
            false, 5000.0f, 0, 5.0f // Висит 20 секунд, толщина 5
        );
    }
}

// 3. Упрощенный алгоритм (дискретизация пути)
TArray<FVector> UEvaluationManager::PathfindSafeRoute(FVector Start, FVector End)
{
    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!NavSys || SafeZoneLocations.Num() == 0) return { Start, End };

    float FixedZ = Start.Z;

    // --- 1. СОРТИРОВКА БЕЗОПАСНЫХ ОКОН ПО ДИСТАНЦИИ ОТ СТАРТА ---
    // Это важно, чтобы путь не метался между комнатами хаотично
    TArray<FVector> SortedWindows = SafeZoneLocations;
    SortedWindows.Sort([Start](const FVector& A, const FVector& B) {
        return FVector::DistSquaredXY(Start, A) < FVector::DistSquaredXY(Start, B);
        });

    // --- 2. ПОСТРОЕНИЕ СОСТАВНОГО ПУТИ ЧЕРЕЗ ВСЕ ОКНА ---
    TArray<FVector> FullNavPoints;
    FVector CurrentLegStart = Start;

    // Добавляем точки: Старт -> Окно 1 -> Окно 2 ... -> Конец
    TArray<FVector> Waypoints = SortedWindows;
    Waypoints.Add(End);

    for (const FVector& NextTarget : Waypoints)
    {
        UNavigationPath* NavPath = NavSys->FindPathToLocationSynchronously(GetWorld(), CurrentLegStart, NextTarget);
        if (NavPath && NavPath->PathPoints.Num() > 1)
        {
            // Добавляем все точки сегмента, кроме последней (чтобы не дублировать)
            for (int32 i = 0; i < NavPath->PathPoints.Num() - 1; i++)
            {
                FVector P = NavPath->PathPoints[i];
                P.Z = FixedZ;
                FullNavPoints.Add(P);
            }
            CurrentLegStart = NextTarget;
        }
    }
    FullNavPoints.Add(End); // Финальная точка

    // --- 3. РЕСЕМПЛИНГ (Дробим путь для гибкости) ---
    TArray<FVector> Subdivided;
    for (int32 i = 0; i < FullNavPoints.Num() - 1; i++)
    {
        float Dist = FVector::Dist(FullNavPoints[i], FullNavPoints[i + 1]);
        int32 Steps = FMath::Max(1, FMath::RoundToInt(Dist / 30.0f)); // Точки каждые 30см
        for (int32 j = 0; j < Steps; j++)
        {
            Subdivided.Add(FMath::Lerp(FullNavPoints[i], FullNavPoints[i + 1], (float)j / Steps));
        }
    }
    Subdivided.Add(End);

    // --- 4. ФИНАЛЬНАЯ КОРРЕКЦИЯ (ОТТАЛКИВАНИЕ ОТ ИСТОЧНИКОВ) ---
    // Теперь, когда путь ИТАК проходит через окна, нам нужно лишь слегка 
    // отодвинуть его от источников в тех местах, где он проходит мимо них
    TArray<AActor*> Sources;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APointSource::StaticClass(), Sources);

    for (int32 Iter = 0; Iter < 15; Iter++)
    {
        TArray<FVector> NextPath = Subdivided;
        for (int32 i = 1; i < Subdivided.Num() - 1; i++)
        {
            FVector Current = Subdivided[i];

            // Если точка уже находится очень близко к безопасному окну, не трогаем её силами радиации
            bool bInSafeWindow = false;
            for (const FVector& SW : SafeZoneLocations) {
                if (FVector::DistSquaredXY(Current, SW) < 2500.0f) { // 50 см радиус
                    bInSafeWindow = true; break;
                }
            }
            if (bInSafeWindow) continue;

            FVector Force = FVector::ZeroVector;
            for (AActor* Src : Sources)
            {
                APointSource* S = Cast<APointSource>(Src);
                FVector Dir = Current - S->GetActorLocation();
                Dir.Z = 0;
                float DistSq = Dir.SizeSquared();
                float Weight = S->GetRawGammaAtPoint(Current) + (S->GetRawNeutronAtPoint(Current) * 10.0f);
                Force += Dir.GetSafeNormal() * (Weight * 150000.0f / FMath::Max(DistSq, 100.0f));
            }

            FVector Spring = (Subdivided[i - 1] + Subdivided[i + 1]) * 0.5f - Current;
            FVector Move = (Force + Spring * 0.3f).GetClampedToMaxSize(10.0f);

            FHitResult Hit;
            if (!GetWorld()->LineTraceSingleByChannel(Hit, Current, Current + Move, ECC_Visibility))
            {
                NextPath[i] = Current + Move;
            }
        }
        Subdivided = NextPath;
    }

    return Subdivided;
}

void UEvaluationManager::SimulateIdealRun()
{
    AActor* StartActor = InternalStartActor.Get();
    AActor* EndActor = InternalEndActor.Get();
    if (!StartActor || !EndActor) return;

    // 1. Получаем оптимальный путь
    TArray<FVector> IdealPath = PathfindSafeRoute(StartActor->GetActorLocation(), EndActor->GetActorLocation());

    IdealAccumulatedDose = 0.0f;

    // 2. Идем по сегментам пути
    for (int32 i = 0; i < IdealPath.Num() - 1; i++)
    {
        FVector CurrentPoint = IdealPath[i];
        FVector NextPoint = IdealPath[i + 1];

        float Distance = FVector::Dist(CurrentPoint, NextPoint);

        // Время прохождения сегмента в секундах (t = S / V)
        float TimeInSeconds = Distance / SimulationSpeed;

        // Получаем мощность дозы в текущей точке (мкЗв/ч)
        float DoseRate = GetDoseRateAtLocation(CurrentPoint);

        // Накопленная доза за сегмент: (мкЗв/ч / 3600) * сек
        float SegmentDose = (DoseRate / 3600.0f) * TimeInSeconds;

        IdealAccumulatedDose += SegmentDose;
    }

    UE_LOG(LogTemp, Warning, TEXT("Evaluation: Ideal Run Simulated. Benchmark Dose: %.4f mkSv"), IdealAccumulatedDose);
}

