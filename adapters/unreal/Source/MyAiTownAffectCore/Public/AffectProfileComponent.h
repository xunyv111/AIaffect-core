#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AffectAutoRule.h"
#include "AffectRuntime.h"
#include "AffectProfileComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAffectPresentationChanged, FAffectPresentation, Presentation);

/**
 * Attach once to a character Actor. Newly-added Actor Tags are translated through
 * configured rules into bounded affect updates; no text, assets, or model output is read.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class MYAITOWNAFFECTCORE_API UAffectProfileComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAffectProfileComponent();

    /** Empty means the owning Actor's stable object name is used. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affect")
    FString CharacterId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affect", meta = (ClampMin = "-100", ClampMax = "100"))
    int32 BaselineValence = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affect", meta = (ClampMin = "0", ClampMax = "100"))
    int32 BaselineArousal = 40;

    /** Zero uses the project-wide default from Project Settings. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Automatic integration", meta = (ClampMin = "0.0"))
    float PollingIntervalSeconds = 0.0f;

    /** Rules applied after project-wide defaults for this specific character. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Automatic integration")
    TArray<FAffectAutoRule> LocalActorTagRules;

    /** Safe semantic state for UI, dialogue selection, and behaviour trees. */
    UPROPERTY(BlueprintAssignable, Category = "Affect")
    FAffectPresentationChanged OnPresentationChanged;

    UFUNCTION(BlueprintPure, Category = "Affect")
    bool GetCurrentPresentation(FAffectPresentation& OutPresentation) const;

    FString GetResolvedCharacterId() const;
    void InitializeRuntimeAtMinute(int32 NowMinute);
    void RefreshActorTagsAtMinute(int32 NowMinute);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UAffectRuntime> Runtime;

    TSet<FName> ObservedActorTags;
    TMap<FName, int32> TagOccurrences;
    float ElapsedSincePollSeconds = 0.0f;
    bool bInitialized = false;

    float GetEffectivePollingIntervalSeconds() const;
    TArray<FAffectAutoRule> GetRules() const;
    void PublishPresentation();
};
