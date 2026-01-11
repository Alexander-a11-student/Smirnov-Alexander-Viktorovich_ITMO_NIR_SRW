#include "QuizWidget.h"
#include "Kismet/GameplayStatics.h" // Для функции OpenLevel
#include "Components/Widget.h"      // Для SetVisibility

#define LOCTEXT_NAMESPACE "QuizWidget"

void UQuizWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (AnswerButton0) AnswerButton0->OnClicked.AddDynamic(this, &UQuizWidget::OnAnswer0);
    if (AnswerButton1) AnswerButton1->OnClicked.AddDynamic(this, &UQuizWidget::OnAnswer1);
    if (AnswerButton2) AnswerButton2->OnClicked.AddDynamic(this, &UQuizWidget::OnAnswer2);
    if (AnswerButton3) AnswerButton3->OnClicked.AddDynamic(this, &UQuizWidget::OnAnswer3);

    // --- НОВАЯ ЛОГИКА: Привязка кнопок управления ---
    if (RestartButton) RestartButton->OnClicked.AddDynamic(this, &UQuizWidget::RestartQuiz);
    if (Level2Button) Level2Button->OnClicked.AddDynamic(this, &UQuizWidget::OpenNextLevel);

    // Устанавливаем начальную видимость, если она не установлена в UMG
    if (RestartButton) RestartButton->SetVisibility(ESlateVisibility::Collapsed);
    if (Level2Button) Level2Button->SetVisibility(ESlateVisibility::Collapsed);

    LoadQuestion();
}

void UQuizWidget::LoadQuestion()
{
    if (!QuestionText || Questions.Num() == 0)
        return;

    if (CurrentQuestionIndex >= Questions.Num())
    {
        // --- БЛОК: Конец теста ---

        int32 TotalQuestions = Questions.Num();

        // 1. Вывод результатов
        FText ResultText = FText::Format(
            LOCTEXT("QuizFinishedResults", "Test Comlite!\nCorrect answers: {0} / {1}."),
            FText::AsNumber(Score),
            FText::AsNumber(TotalQuestions)
        );
        QuestionText->SetText(ResultText);

        // 2. Деактивация и скрытие кнопок ответов
        UButton* AnswerButtons[] = { AnswerButton0, AnswerButton1, AnswerButton2, AnswerButton3 };
        UTextBlock* AnswerTexts[] = { AnswerText0, AnswerText1, AnswerText2, AnswerText3 };

        for (int i = 0; i < 4; ++i)
        {
            if (AnswerButtons[i])
            {
                AnswerButtons[i]->SetIsEnabled(false);
                AnswerButtons[i]->SetVisibility(ESlateVisibility::Collapsed);
            }
            if (AnswerTexts[i]) AnswerTexts[i]->SetText(FText::GetEmpty());
        }

        // 3. Появление кнопки РЕСТАРТ
        if (RestartButton)
        {
            // Появляется независимо от результата
            RestartButton->SetVisibility(ESlateVisibility::Visible);
        }

        // 4. Появление кнопки перехода на Level_2 (только при 100%)
        if (Level2Button)
        {
            if (Score == TotalQuestions && TotalQuestions > 0)
            {
                Level2Button->SetVisibility(ESlateVisibility::Visible);
            }
            else
            {
                Level2Button->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
        return;
    }

    // Логика загрузки следующего вопроса
    const FQuizQuestion& Q = Questions[CurrentQuestionIndex];

    // Убедимся, что кнопки ответов включены
    UButton* AnswerButtons[] = { AnswerButton0, AnswerButton1, AnswerButton2, AnswerButton3 };
    for (int i = 0; i < 4; ++i)
    {
        if (AnswerButtons[i])
        {
            AnswerButtons[i]->SetIsEnabled(true);
            AnswerButtons[i]->SetVisibility(ESlateVisibility::Visible);
        }
    }

    // Скрываем кнопки управления (если вдруг были видимы)
    if (RestartButton) RestartButton->SetVisibility(ESlateVisibility::Collapsed);
    if (Level2Button) Level2Button->SetVisibility(ESlateVisibility::Collapsed);

    QuestionText->SetText(Q.Question);

    if (Q.Answers.Num() >= 4)
    {
        AnswerText0->SetText(Q.Answers[0]);
        AnswerText1->SetText(Q.Answers[1]);
        AnswerText2->SetText(Q.Answers[2]);
        AnswerText3->SetText(Q.Answers[3]);
    }
}

void UQuizWidget::OnAnswerClicked(int32 AnswerIndex)
{
    if (CurrentQuestionIndex < Questions.Num())
    {
        if (AnswerIndex == Questions[CurrentQuestionIndex].CorrectAnswer)
            Score++;

        CurrentQuestionIndex++;
        LoadQuestion();
    }
}

void UQuizWidget::OnAnswer0() { OnAnswerClicked(0); }
void UQuizWidget::OnAnswer1() { OnAnswerClicked(1); }
void UQuizWidget::OnAnswer2() { OnAnswerClicked(2); }
void UQuizWidget::OnAnswer3() { OnAnswerClicked(3); }

// ---  Функции управления ---

void UQuizWidget::RestartQuiz()
{
    // Сброс состояния
    CurrentQuestionIndex = 0;
    Score = 0;

    // Скрываем кнопки управления
    if (RestartButton) RestartButton->SetVisibility(ESlateVisibility::Collapsed);
    if (Level2Button) Level2Button->SetVisibility(ESlateVisibility::Collapsed);

    // Загружаем первый вопрос
    LoadQuestion();
}

void UQuizWidget::OpenNextLevel()
{
    if (UWorld* World = GetWorld())
    {
        // Переход на следующий уровень. 
        UGameplayStatics::OpenLevel(World, FName(TEXT("Level_2")));
    }
}

#undef LOCTEXT_NAMESPACE