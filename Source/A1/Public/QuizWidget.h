#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/DataTable.h" 
#include "Kismet/GameplayStatics.h" 
#include "QuizWidget.generated.h"

// Обновленная структура вопроса с учетом веса
USTRUCT(BlueprintType)
struct FQuizQuestion : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz")
    int32 QuestionID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz")
    FText Question;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz")
    TArray<FText> Answers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz")
    int32 CorrectAnswer = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz")
    FString Category = TEXT("General");

    // НОВОЕ: Весовой коэффициент (от 1 до 5)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz")
    int32 Weight = 1;
};

UCLASS()
class A1_API UQuizWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;

    // --- UI Компоненты ---
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TitleText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* QuestionText;

    // Кнопки
    UPROPERTY(meta = (BindWidget))
    UButton* AnswerButton0;
    UPROPERTY(meta = (BindWidget))
    UButton* AnswerButton1;
    UPROPERTY(meta = (BindWidget))
    UButton* AnswerButton2;
    UPROPERTY(meta = (BindWidget))
    UButton* AnswerButton3;

    // Тексты ответов
    UPROPERTY(meta = (BindWidget))
    UTextBlock* AnswerText0;
    UPROPERTY(meta = (BindWidget))
    UTextBlock* AnswerText1;
    UPROPERTY(meta = (BindWidget))
    UTextBlock* AnswerText2;
    UPROPERTY(meta = (BindWidget))
    UTextBlock* AnswerText3;

    // Управление
    UPROPERTY(meta = (BindWidget))
    UButton* RestartButton;
    UPROPERTY(meta = (BindWidget))
    UButton* Level2Button;

    // --- Данные ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz Data")
    UDataTable* QuestionsDataTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz")
    TArray<FQuizQuestion> Questions;

    // --- Внутренние переменные логики ---
    int32 CurrentQuestionIndex = 0;
    int32 Score = 0; // Число правильных ответов

    // Новые переменные для взвешенной системы
    int32 TotalMaxWeight = 0;
    int32 CurrentUserWeight = 0;
    FString FinalGrade;

    // Карты статистики
    TMap<FString, int32> CategoryCorrectMap;
    TMap<FString, int32> CategoryTotalMap;
    TMap<FString, int32> CategoryMaxWeightMap;
    TMap<FString, int32> CategoryUserWeightMap;
    TMap<FString, FString> CategoryCorrectQuestionsNames;

    // --- Методы ---
    void LoadQuestion();
    void OnAnswerClicked(int32 AnswerIndex);
    void ExportResultsToCSV();

    UFUNCTION() void OnAnswer0();
    UFUNCTION() void OnAnswer1();
    UFUNCTION() void OnAnswer2();
    UFUNCTION() void OnAnswer3();
    UFUNCTION() void RestartQuiz();
    UFUNCTION() void OpenNextLevel();
};