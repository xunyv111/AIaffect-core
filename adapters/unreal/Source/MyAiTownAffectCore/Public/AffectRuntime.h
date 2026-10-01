#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AffectRuntime.generated.h"

UENUM(BlueprintType)
enum class EAffectSourceKind : uint8
{
    ConversationReply,
    SocialEvent,
    ConflictEvent,
    EnvironmentEvent,
    AgentJudgment,
    System
};

USTRUCT()
struct MYAITOWNAFFECTCORE_API FAffectChanges
{
    GENERATED_BODY()

    int32 ValenceDelta = 0;
    int32 ArousalDelta = 0;
};

USTRUCT()
struct MYAITOWNAFFECTCORE_API FAffectUpdate
{
    GENERATED_BODY()

    FString UpdateId;
    FString CharacterId;
    EAffectSourceKind SourceKind = EAffectSourceKind::System;
    int32 OccurredAtMinute = 0;
    FString CauseEventId;
    FAffectChanges Changes;
    bool bHasExpectedRevision = false;
    int32 ExpectedRevision = 0;
};

USTRUCT()
struct MYAITOWNAFFECTCORE_API FAffectState
{
    GENERATED_BODY()

    FString CharacterId;
    int32 Valence = 0;
    int32 Arousal = 40;
    int32 BaselineValence = 0;
    int32 BaselineArousal = 40;
    FString DominantCauseEventId;
    FString LastCauseEventId;
    FString LastSourceKind = TEXT("none");
    int32 LastEventAtMinute = 0;
    int32 LastDecayAtMinute = 0;
    int32 LastChangedAtMinute = 0;
    int32 Revision = 0;
    TArray<FString> ProcessedUpdateIds;
};

/** Safe for Blueprints and player-facing presentation: no raw values are returned. */
USTRUCT(BlueprintType)
struct MYAITOWNAFFECTCORE_API FAffectPresentation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Affect") FString Mood = TEXT("neutral");
    UPROPERTY(BlueprintReadOnly, Category = "Affect") FString Activation = TEXT("alert");
    UPROPERTY(BlueprintReadOnly, Category = "Affect") FString DialogueTone = TEXT("neutral");
    UPROPERTY(BlueprintReadOnly, Category = "Affect") FString BehaviorDisposition = TEXT("normal");
    UPROPERTY(BlueprintReadOnly, Category = "Affect") FString RecentCause = TEXT("none");
};

USTRUCT()
struct MYAITOWNAFFECTCORE_API FAffectApplyResult
{
    GENERATED_BODY()

    bool bOk = false;
    bool bApplied = false;
    bool bDuplicate = false;
    FString Error;
    FAffectState State;
    int32 AppliedValenceDelta = 0;
    int32 AppliedArousalDelta = 0;
};

USTRUCT()
struct MYAITOWNAFFECTCORE_API FAffectSnapshot
{
    GENERATED_BODY()

    int32 SchemaVersion = 1;
    TArray<FAffectState> Characters;
};

/** Batch restore outcome; intentionally does not pretend to represent one character. */
USTRUCT()
struct MYAITOWNAFFECTCORE_API FAffectRestoreResult
{
    GENERATED_BODY()
    bool bOk = false;
    FString Error;
    FAffectSnapshot Snapshot;
    TArray<FString> RestoredCharacterIds;
    TArray<FString> InitializedCharacterIds;
};

/**
 * Runtime state holder. Blueprint users can initialize, apply events, advance time
 * and describe a character, but raw numeric state is intentionally C++ only.
 */
UCLASS(BlueprintType)
class MYAITOWNAFFECTCORE_API UAffectRuntime : public UObject
{
    GENERATED_BODY()

public:
    static constexpr int32 SchemaVersion = 1;

    UFUNCTION(BlueprintCallable, Category = "Affect")
    bool InitializeCharacter(const FString& CharacterId, int32 BaselineValence, int32 BaselineArousal, int32 NowMinute);

    UFUNCTION(BlueprintCallable, Category = "Affect")
    bool ApplyEvent(const FString& UpdateId, const FString& CharacterId, EAffectSourceKind SourceKind,
        int32 OccurredAtMinute, const FString& CauseEventId, int32 ValenceDelta, int32 ArousalDelta,
        FAffectPresentation& OutPresentation);

    UFUNCTION(BlueprintCallable, Category = "Affect")
    bool AdvanceTime(const FString& CharacterId, int32 NowMinute, FAffectPresentation& OutPresentation);

    UFUNCTION(BlueprintPure, Category = "Affect")
    bool Describe(const FString& CharacterId, FAffectPresentation& OutPresentation) const;

    FAffectApplyResult Apply(const FAffectUpdate& Update);
    FAffectApplyResult Advance(const FString& CharacterId, int32 NowMinute);
    bool GetInternalState(const FString& CharacterId, FAffectState& OutState) const;
    FAffectSnapshot CreateSnapshot() const;
    FAffectRestoreResult Restore(const FAffectSnapshot& Snapshot, const TArray<FString>& RequiredCharacterIds = {});

private:
    TMap<FString, FAffectState> Characters;

    static int32 Clamp(int32 Value, int32 Minimum, int32 Maximum);
    static int32 MoveToward(int32 Value, int32 Target, int32 Amount);
    static bool IsKnownSourceKind(EAffectSourceKind SourceKind);
    static void GetSourceCaps(EAffectSourceKind SourceKind, int32& OutValenceCap, int32& OutArousalCap);
    static FAffectPresentation ProjectPresentation(const FAffectState& State);
    static bool IsValidState(const FAffectState& State);
    static FAffectApplyResult Failure(const FString& Error);
    static FAffectApplyResult Success(const FAffectState& State, bool bApplied, bool bDuplicate, int32 ValenceDelta, int32 ArousalDelta);
};
