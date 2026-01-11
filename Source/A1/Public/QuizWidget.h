#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
// Добавим UGameplayStatics для перехода на уровень
#include "Kismet/GameplayStatics.h" 
#include "QuizWidget.generated.h"

USTRUCT(BlueprintType)
struct FQuizQuestion
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Question;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FText> Answers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CorrectAnswer = 0; // индекс правильного ответа
};

UCLASS()
class A1_API UQuizWidget : public UUserWidget
{
    GENERATED_BODY()

protected:

    virtual void NativeConstruct() override;

    // Тексты
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TitleText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* QuestionText;

    // Кнопки + тексты ответов
    UPROPERTY(meta = (BindWidget))
    UButton* AnswerButton0;

    UPROPERTY(meta = (BindWidget))
    UButton* AnswerButton1;

    UPROPERTY(meta = (BindWidget))
    UButton* AnswerButton2;

    UPROPERTY(meta = (BindWidget))
    UButton* AnswerButton3;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* AnswerText0;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* AnswerText1;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* AnswerText2;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* AnswerText3;

    // --- НОВЫЕ КНОПКИ УПРАВЛЕНИЯ ---
    UPROPERTY(meta = (BindWidget))
    UButton* RestartButton; // Кнопка перезапуска теста (должна быть Collapsed в UMG)

    UPROPERTY(meta = (BindWidget))
    UButton* Level2Button; // Кнопка перехода на Level_2 (должна быть Collapsed в UMG)
    // ----------------------------------

    // Логика теста
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quiz")
    TArray<FQuizQuestion> Questions;

    int32 CurrentQuestionIndex = 0;
    int32 Score = 0;

    void LoadQuestion();
    void OnAnswerClicked(int32 AnswerIndex);

    UFUNCTION()
    void OnAnswer0();

    UFUNCTION()
    void OnAnswer1();

    UFUNCTION()
    void OnAnswer2();

    UFUNCTION()
    void OnAnswer3();

    // --- НОВЫЕ ФУНКЦИИ УПРАВЛЕНИЯ ---
    UFUNCTION()
    void RestartQuiz();

    UFUNCTION()
    void OpenNextLevel();
    // ----------------------------------
};