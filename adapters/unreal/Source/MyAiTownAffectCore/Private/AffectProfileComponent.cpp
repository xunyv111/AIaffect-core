#include "AffectProfileComponent.h"

#include "AffectIntegrationSettings.h"
#include "AffectWorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UAffectProfileComponent::UAffectProfileComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UAffectProfileComponent::BeginPlay()
{
    Super::BeginPlay();
    const int32 NowMinute = GetWorld() != nullptr ? FMath::FloorToInt(GetWorld()->GetTimeSeconds() / 60.0f) : 0;
    InitializeRuntimeAtMinute(NowMinute);
    if (UWorld* World = GetWorld())
    {
        World->GetSubsystem<UAffectWorldSubsystem>()->RegisterProfile(this);
    }
}

void UAffectProfileComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetSubsystem<UAffectWorldSubsystem>()->UnregisterProfile(this);
    }
    Super::EndPlay(EndPlayReason);
}

void UAffectProfileComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bInitialized)
    {
        return;
    }

    ElapsedSincePollSeconds += DeltaTime;
    if (ElapsedSincePollSeconds < GetEffectivePollingIntervalSeconds())
    {
        return;
    }

    ElapsedSincePollSeconds = 0.0f;
    const int32 NowMinute = GetWorld() != nullptr ? FMath::FloorToInt(GetWorld()->GetTimeSeconds() / 60.0f) : 0;
    const FAffectApplyResult DecayResult = Runtime->Advance(GetResolvedCharacterId(), NowMinute);
    if (DecayResult.bOk && DecayResult.bApplied)
    {
        PublishPresentation();
    }
    RefreshActorTagsAtMinute(NowMinute);
}

FString UAffectProfileComponent::GetResolvedCharacterId() const
{
    if (!CharacterId.IsEmpty())
    {
        return CharacterId;
    }
    return GetOwner() != nullptr ? GetOwner()->GetFName().ToString() : GetFName().ToString();
}

void UAffectProfileComponent::InitializeRuntimeAtMinute(int32 NowMinute)
{
    Runtime = NewObject<UAffectRuntime>(this);
    Runtime->InitializeCharacter(GetResolvedCharacterId(), BaselineValence, BaselineArousal, NowMinute);
    ObservedActorTags.Reset();
    if (const AActor* Owner = GetOwner())
    {
        for (const FName& Tag : Owner->Tags)
        {
            ObservedActorTags.Add(Tag);
        }
    }
    TagOccurrences.Reset();
    ElapsedSincePollSeconds = 0.0f;
    bInitialized = true;
    PublishPresentation();
}

void UAffectProfileComponent::RefreshActorTagsAtMinute(int32 NowMinute)
{
    if (!bInitialized || !IsValid(Runtime) || GetOwner() == nullptr)
    {
        return;
    }

    TSet<FName> CurrentTags;
    for (const FName& Tag : GetOwner()->Tags)
    {
        CurrentTags.Add(Tag);
    }

    const TArray<FAffectAutoRule> Rules = GetRules();
    for (const FName& Tag : CurrentTags)
    {
        if (ObservedActorTags.Contains(Tag))
        {
            continue;
        }

        const int32 Occurrence = TagOccurrences.FindOrAdd(Tag) + 1;
        TagOccurrences.Add(Tag, Occurrence);
        for (int32 RuleIndex = 0; RuleIndex < Rules.Num(); ++RuleIndex)
        {
            const FAffectAutoRule& Rule = Rules[RuleIndex];
            if (!Rule.bApplyOnAdded || Rule.TriggerActorTag != Tag)
            {
                continue;
            }

            const FString CauseEventId = Rule.CauseEventId.IsEmpty() ? Tag.ToString() : Rule.CauseEventId;
            const FString UpdateId = FString::Printf(TEXT("auto:%s:%s:%d:%d"), *GetResolvedCharacterId(), *Tag.ToString(), RuleIndex, Occurrence);
            FAffectUpdate Update;
            Update.UpdateId = UpdateId;
            Update.CharacterId = GetResolvedCharacterId();
            Update.SourceKind = Rule.SourceKind;
            Update.OccurredAtMinute = NowMinute;
            Update.CauseEventId = CauseEventId;
            Update.Changes.ValenceDelta = Rule.ValenceDelta;
            Update.Changes.ArousalDelta = Rule.ArousalDelta;
            const FAffectApplyResult Result = Runtime->Apply(Update);
            if (Result.bOk && Result.bApplied)
            {
                PublishPresentation();
            }
        }
    }

    ObservedActorTags = MoveTemp(CurrentTags);
}

bool UAffectProfileComponent::GetCurrentPresentation(FAffectPresentation& OutPresentation) const
{
    return IsValid(Runtime) && Runtime->Describe(GetResolvedCharacterId(), OutPresentation);
}

float UAffectProfileComponent::GetEffectivePollingIntervalSeconds() const
{
    if (PollingIntervalSeconds > 0.0f)
    {
        return PollingIntervalSeconds;
    }
    return GetDefault<UAffectIntegrationSettings>()->DefaultPollingIntervalSeconds;
}

TArray<FAffectAutoRule> UAffectProfileComponent::GetRules() const
{
    TArray<FAffectAutoRule> Rules = GetDefault<UAffectIntegrationSettings>()->DefaultActorTagRules;
    Rules.Append(LocalActorTagRules);
    return Rules;
}

void UAffectProfileComponent::PublishPresentation()
{
    FAffectPresentation Presentation;
    if (GetCurrentPresentation(Presentation))
    {
        OnPresentationChanged.Broadcast(Presentation);
    }
}
