#include "QuizWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Widget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformFilemanager.h"

#define LOCTEXT_NAMESPACE "QuizWidget"

void UQuizWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RestartQuiz();

    if (QuestionsDataTable)
    {
        Questions.Empty();
        static const FString ContextString(TEXT("Quiz Questions Context"));
        TArray<FQuizQuestion*> AllRows;
        QuestionsDataTable->GetAllRows<FQuizQuestion>(ContextString, AllRows);

        for (auto RowPtr : AllRows)
        {
            if (RowPtr) Questions.Add(*RowPtr);
        }
    }

    if (AnswerButton0) AnswerButton0->OnClicked.AddDynamic(this, &UQuizWidget::OnAnswer0);
    if (AnswerButton1) AnswerButton1->OnClicked.AddDynamic(this, &UQuizWidget::OnAnswer1);
    if (AnswerButton2) AnswerButton2->OnClicked.AddDynamic(this, &UQuizWidget::OnAnswer2);
    if (AnswerButton3) AnswerButton3->OnClicked.AddDynamic(this, &UQuizWidget::OnAnswer3);

    if (RestartButton) RestartButton->OnClicked.AddDynamic(this, &UQuizWidget::RestartQuiz);
    if (Level2Button) Level2Button->OnClicked.AddDynamic(this, &UQuizWidget::OpenNextLevel);

    LoadQuestion();
}

void UQuizWidget::LoadQuestion()
{
    if (!QuestionText || Questions.Num() == 0) return;

    if (CurrentQuestionIndex >= Questions.Num())
    {
        // 1. Calculate Overall Result
        float TotalPercent = (TotalMaxWeight > 0) ? ((float)CurrentUserWeight / (float)TotalMaxWeight) * 100.0f : 0.0f;

        // 2. Safety Critical Check
        FString SafetyCat = TEXT("Work rules and safety");
        float SafetyPercent = 100.0f;
        if (CategoryMaxWeightMap.Contains(SafetyCat) && CategoryMaxWeightMap[SafetyCat] > 0)
        {
            SafetyPercent = ((float)CategoryUserWeightMap[SafetyCat] / (float)CategoryMaxWeightMap[SafetyCat]) * 100.0f;
        }

        // 3. Final Overall Grade
        if (SafetyPercent < 70.0f)      FinalGrade = TEXT("F (Safety Violation)");
        else if (TotalPercent >= 90.0f) FinalGrade = TEXT("A (Excellent)");
        else if (TotalPercent >= 80.0f) FinalGrade = TEXT("B (Good)");
        else if (TotalPercent >= 70.0f) FinalGrade = TEXT("C (Satisfactory)");
        else if (TotalPercent >= 60.0f) FinalGrade = TEXT("D (Marginal)");
        else                            FinalGrade = TEXT("F (Fail)");

        // 4. Building the Report
        FString FinalReport = FString::Printf(TEXT("QUIZ COMPLETED!\n\nFINAL GRADE: %s\nTOTAL SCORE: %.1f%% (%d/%d)\n\nCATEGORY GRADES:\n"),
            *FinalGrade, TotalPercent, CurrentUserWeight, TotalMaxWeight);

        for (auto& It : CategoryMaxWeightMap)
        {
            FString CatName = It.Key;
            int32 MaxW = It.Value;
            int32 UserW = CategoryUserWeightMap.Contains(CatName) ? CategoryUserWeightMap[CatName] : 0;
            float CatP = (MaxW > 0) ? ((float)UserW / (float)MaxW) * 100.0f : 0.0f;

            // Calculate Grade for this specific category
            FString CatGrade;
            if (CatP >= 90.0f)      CatGrade = TEXT("A");
            else if (CatP >= 80.0f) CatGrade = TEXT("B");
            else if (CatP >= 70.0f) CatGrade = TEXT("C");
            else if (CatP >= 60.0f) CatGrade = TEXT("D");
            else                    CatGrade = TEXT("F");

            // Format: [Grade] CategoryName: Score%
            FinalReport += FString::Printf(TEXT("[%s] %s: %.1f%%\n"), *CatGrade, *CatName, CatP);
        }

        QuestionText->SetText(FText::FromString(FinalReport));

        // 5. Cleanup UI
        UButton* Btns[] = { AnswerButton0, AnswerButton1, AnswerButton2, AnswerButton3 };
        UTextBlock* Txts[] = { AnswerText0, AnswerText1, AnswerText2, AnswerText3 };
        for (int32 i = 0; i < 4; ++i)
        {
            if (Btns[i]) { Btns[i]->SetVisibility(ESlateVisibility::Collapsed); Btns[i]->SetIsEnabled(false); }
            if (Txts[i]) { Txts[i]->SetText(FText::GetEmpty()); Txts[i]->SetVisibility(ESlateVisibility::Collapsed); }
        }

        if (RestartButton) RestartButton->SetVisibility(ESlateVisibility::Visible);
        if (Level2Button && !FinalGrade.StartsWith(TEXT("F"))) Level2Button->SetVisibility(ESlateVisibility::Visible);

        return;
    }

    const FQuizQuestion& Q = Questions[CurrentQuestionIndex];
    QuestionText->SetText(Q.Question);

    UTextBlock* Txts[] = { AnswerText0, AnswerText1, AnswerText2, AnswerText3 };
    UButton* Btns[] = { AnswerButton0, AnswerButton1, AnswerButton2, AnswerButton3 };

    for (int32 i = 0; i < 4; ++i)
    {
        if (Btns[i] && Txts[i])
        {
            if (Q.Answers.IsValidIndex(i) && !Q.Answers[i].IsEmpty())
            {
                Btns[i]->SetVisibility(ESlateVisibility::Visible);
                Btns[i]->SetIsEnabled(true);
                Txts[i]->SetVisibility(ESlateVisibility::Visible);
                Txts[i]->SetText(Q.Answers[i]);
            }
            else
            {
                Btns[i]->SetVisibility(ESlateVisibility::Collapsed);
                Txts[i]->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
    }
}

void UQuizWidget::OnAnswerClicked(int32 AnswerIndex)
{
    if (!Questions.IsValidIndex(CurrentQuestionIndex)) return;

    const FQuizQuestion& CurrentQ = Questions[CurrentQuestionIndex];
    FString Cat = CurrentQ.Category.IsEmpty() ? TEXT("General") : CurrentQ.Category;
    int32 QWeight = (CurrentQ.Weight > 0) ? CurrentQ.Weight : 1;

    if (!CategoryTotalMap.Contains(Cat))
    {
        CategoryTotalMap.Add(Cat, 0); CategoryCorrectMap.Add(Cat, 0);
        CategoryMaxWeightMap.Add(Cat, 0); CategoryUserWeightMap.Add(Cat, 0);
        CategoryCorrectQuestionsNames.Add(Cat, TEXT(""));
    }

    CategoryTotalMap[Cat]++;
    CategoryMaxWeightMap[Cat] += QWeight;
    TotalMaxWeight += QWeight;

    if (AnswerIndex == CurrentQ.CorrectAnswer)
    {
        Score++;
        CategoryCorrectMap[Cat]++;
        CategoryUserWeightMap[Cat] += QWeight;
        CurrentUserWeight += QWeight;

        FString& DigitList = CategoryCorrectQuestionsNames[Cat];
        if (!DigitList.IsEmpty()) DigitList += TEXT(", ");
        DigitList += FString::FromInt(CurrentQ.QuestionID);
    }

    CurrentQuestionIndex++;
    if (CurrentQuestionIndex >= Questions.Num()) ExportResultsToCSV();
    LoadQuestion();
}

void UQuizWidget::ExportResultsToCSV()
{
    float TotalPercent = (TotalMaxWeight > 0) ? ((float)CurrentUserWeight / (float)TotalMaxWeight) * 100.0f : 0.0f;
    FString CSVContent = TEXT("Quiz Detailed Report\n");
    CSVContent += FString::Printf(TEXT("Date:,%s\nFinal Grade:,%s\nScore:,%d/%d,(%.1f%%)\n\n"),
        *FDateTime::Now().ToString(), *FinalGrade, CurrentUserWeight, TotalMaxWeight, TotalPercent);
    CSVContent += TEXT("Category,Correct,Total,UserWeight,MaxWeight,Percent,IDs\n");

    for (auto& It : CategoryMaxWeightMap)
    {
        FString CatName = It.Key;
        int32 MaxW = It.Value;
        int32 UserW = CategoryUserWeightMap.Contains(CatName) ? CategoryUserWeightMap[CatName] : 0;
        float Prc = (MaxW > 0) ? ((float)UserW / (float)MaxW) * 100.0f : 0.0f;
        CSVContent += FString::Printf(TEXT("%s,%d,%d,%d,%d,%.1f%%,\"%s\"\n"),
            *CatName, CategoryCorrectMap[CatName], CategoryTotalMap[CatName], UserW, MaxW, Prc, *CategoryCorrectQuestionsNames[CatName]);
    }

    FString FullPath = FPaths::ProjectSavedDir() / TEXT("QuizReports") / FString::Printf(TEXT("Report_%s.csv"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*(FPaths::ProjectSavedDir() / TEXT("QuizReports")));
    FFileHelper::SaveStringToFile(CSVContent, *FullPath, FFileHelper::EEncodingOptions::ForceUTF8);
}

void UQuizWidget::RestartQuiz()
{
    CurrentQuestionIndex = 0; Score = 0; CurrentUserWeight = 0; TotalMaxWeight = 0;
    FinalGrade = TEXT("In Progress");
    CategoryCorrectMap.Empty(); CategoryTotalMap.Empty();
    CategoryMaxWeightMap.Empty(); CategoryUserWeightMap.Empty();
    CategoryCorrectQuestionsNames.Empty();

    if (RestartButton) RestartButton->SetVisibility(ESlateVisibility::Collapsed);
    if (Level2Button) Level2Button->SetVisibility(ESlateVisibility::Collapsed);
    if (Questions.Num() > 0) LoadQuestion();
}

void UQuizWidget::OpenNextLevel() { UGameplayStatics::OpenLevel(this, FName(TEXT("Level_2"))); }
void UQuizWidget::OnAnswer0() { OnAnswerClicked(0); }
void UQuizWidget::OnAnswer1() { OnAnswerClicked(1); }
void UQuizWidget::OnAnswer2() { OnAnswerClicked(2); }
void UQuizWidget::OnAnswer3() { OnAnswerClicked(3); }

#undef LOCTEXT_NAMESPACE