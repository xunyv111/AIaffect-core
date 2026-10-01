#include "AffectRuntime.h"

static FString AffectSourceName(EAffectSourceKind SourceKind)
{
    switch (SourceKind) { case EAffectSourceKind::ConversationReply: return TEXT("conversation_reply"); case EAffectSourceKind::SocialEvent: return TEXT("social_event"); case EAffectSourceKind::ConflictEvent: return TEXT("conflict_event"); case EAffectSourceKind::EnvironmentEvent: return TEXT("environment_event"); case EAffectSourceKind::AgentJudgment: return TEXT("agent_judgment"); default: return TEXT("system"); }
}

bool UAffectRuntime::InitializeCharacter(const FString& CharacterId, int32 BaselineValence, int32 BaselineArousal, int32 NowMinute)
{
    if (CharacterId.TrimStartAndEnd().IsEmpty()) return false;
    if (Characters.Contains(CharacterId)) return true;
    FAffectState State;
    State.CharacterId = CharacterId;
    State.Valence = Clamp(BaselineValence, -100, 100);
    State.Arousal = Clamp(BaselineArousal, 0, 100);
    State.BaselineValence = State.Valence;
    State.BaselineArousal = State.Arousal;
    State.LastSourceKind = TEXT("none");
    State.LastEventAtMinute = FMath::Max(0, NowMinute);
    State.LastDecayAtMinute = FMath::Max(0, NowMinute);
    State.LastChangedAtMinute = FMath::Max(0, NowMinute);
    Characters.Add(CharacterId, State);
    return true;
}

bool UAffectRuntime::ApplyEvent(const FString& UpdateId, const FString& CharacterId, EAffectSourceKind SourceKind,
    int32 OccurredAtMinute, const FString& CauseEventId, int32 ValenceDelta, int32 ArousalDelta,
    FAffectPresentation& OutPresentation)
{
    FAffectUpdate Update;
    Update.UpdateId = UpdateId;
    Update.CharacterId = CharacterId;
    Update.SourceKind = SourceKind;
    Update.OccurredAtMinute = OccurredAtMinute;
    Update.CauseEventId = CauseEventId;
    Update.Changes.ValenceDelta = ValenceDelta;
    Update.Changes.ArousalDelta = ArousalDelta;
    const FAffectApplyResult Result = Apply(Update);
    if (!Result.bOk) return false;
    OutPresentation = ProjectPresentation(Result.State);
    return true;
}

bool UAffectRuntime::AdvanceTime(const FString& CharacterId, int32 NowMinute, FAffectPresentation& OutPresentation)
{
    const FAffectApplyResult Result = Advance(CharacterId, NowMinute);
    if (!Result.bOk) return false;
    OutPresentation = ProjectPresentation(Result.State);
    return true;
}

bool UAffectRuntime::Describe(const FString& CharacterId, FAffectPresentation& OutPresentation) const
{
    const FAffectState* State = Characters.Find(CharacterId);
    if (State == nullptr) return false;
    OutPresentation = ProjectPresentation(*State);
    return true;
}

FAffectApplyResult UAffectRuntime::Apply(const FAffectUpdate& Update)
{
    if (Update.UpdateId.TrimStartAndEnd().IsEmpty() || Update.CharacterId.TrimStartAndEnd().IsEmpty()) return Failure(TEXT("update_identity_invalid"));
    if (!IsKnownSourceKind(Update.SourceKind)) return Failure(TEXT("source_kind_unknown"));
    if (Update.OccurredAtMinute < 0) return Failure(TEXT("occurred_at_invalid"));
    FAffectState* State = Characters.Find(Update.CharacterId);
    if (State == nullptr) return Failure(TEXT("character_not_initialized"));
    if (State->ProcessedUpdateIds.Contains(Update.UpdateId)) return Success(*State, false, true, 0, 0);
    if (Update.bHasExpectedRevision && Update.ExpectedRevision != State->Revision) return Failure(TEXT("revision_conflict"));
    if (Update.OccurredAtMinute < State->LastEventAtMinute) return Failure(TEXT("stale_update"));

    Advance(Update.CharacterId, Update.OccurredAtMinute);

    int32 ValenceCap = 0;
    int32 ArousalCap = 0;
    GetSourceCaps(Update.SourceKind, ValenceCap, ArousalCap);
    const int32 BeforeValence = State->Valence;
    const int32 BeforeArousal = State->Arousal;
    State->Valence = Clamp(State->Valence + Clamp(Update.Changes.ValenceDelta, -ValenceCap, ValenceCap), -100, 100);
    State->Arousal = Clamp(State->Arousal + Clamp(Update.Changes.ArousalDelta, -ArousalCap, ArousalCap), 0, 100);
    State->DominantCauseEventId = Update.CauseEventId;
    State->LastCauseEventId = Update.CauseEventId;
    State->LastSourceKind = AffectSourceName(Update.SourceKind);
    State->LastEventAtMinute = Update.OccurredAtMinute;
    State->LastChangedAtMinute = Update.OccurredAtMinute;
    ++State->Revision;
    State->ProcessedUpdateIds.Add(Update.UpdateId);
    if (State->ProcessedUpdateIds.Num() > 256) State->ProcessedUpdateIds.RemoveAt(0, State->ProcessedUpdateIds.Num() - 256);
    return Success(*State, true, false, State->Valence - BeforeValence, State->Arousal - BeforeArousal);
}

FAffectApplyResult UAffectRuntime::Advance(const FString& CharacterId, int32 NowMinute)
{
    FAffectState* State = Characters.Find(CharacterId);
    if (State == nullptr) return Failure(TEXT("character_not_initialized"));
    const int32 Intervals = (NowMinute - State->LastDecayAtMinute) / 60;
    if (Intervals <= 0) return Success(*State, false, false, 0, 0);
    const int32 BeforeValence = State->Valence;
    const int32 BeforeArousal = State->Arousal;
    State->Valence = MoveToward(State->Valence, State->BaselineValence, Intervals * 2);
    State->Arousal = MoveToward(State->Arousal, State->BaselineArousal, Intervals * 4);
    State->LastDecayAtMinute += Intervals * 60;
    if (State->Valence != BeforeValence || State->Arousal != BeforeArousal) State->LastChangedAtMinute = State->LastDecayAtMinute;
    const bool bChanged = State->Valence != BeforeValence || State->Arousal != BeforeArousal;
    if (bChanged) ++State->Revision;
    if (State->Valence == State->BaselineValence && State->Arousal == State->BaselineArousal) { State->DominantCauseEventId.Empty(); State->LastCauseEventId.Empty(); State->LastSourceKind = TEXT("none"); }
    return Success(*State, bChanged, false, State->Valence - BeforeValence, State->Arousal - BeforeArousal);
}

bool UAffectRuntime::GetInternalState(const FString& CharacterId, FAffectState& OutState) const
{
    const FAffectState* State = Characters.Find(CharacterId);
    if (State == nullptr) return false;
    OutState = *State;
    return true;
}

FAffectSnapshot UAffectRuntime::CreateSnapshot() const
{
    FAffectSnapshot Snapshot;
    Characters.GenerateValueArray(Snapshot.Characters);
    return Snapshot;
}

FAffectRestoreResult UAffectRuntime::Restore(const FAffectSnapshot& Snapshot, const TArray<FString>& RequiredCharacterIds)
{
    FAffectRestoreResult Result;
    if (Snapshot.SchemaVersion != SchemaVersion) { Result.Error = TEXT("snapshot_schema_unsupported"); return Result; }
    TMap<FString, FAffectState> Restored;
    for (const FAffectState& State : Snapshot.Characters)
    {
        if (!IsValidState(State) || Restored.Contains(State.CharacterId)) { Result.Error = TEXT("snapshot_state_invalid"); return Result; }
        FAffectState Migrated = State;
        if (Migrated.LastEventAtMinute == 0 && Migrated.LastDecayAtMinute == 0 && Migrated.LastChangedAtMinute > 0)
        {
            Migrated.LastEventAtMinute = Migrated.LastChangedAtMinute;
            Migrated.LastDecayAtMinute = Migrated.LastChangedAtMinute;
        }
        Restored.Add(Migrated.CharacterId, Migrated);
    }
    Characters = MoveTemp(Restored);
    for (const TPair<FString, FAffectState>& Pair : Characters) Result.RestoredCharacterIds.Add(Pair.Key);
    for (const FString& CharacterId : RequiredCharacterIds)
        if (!CharacterId.TrimStartAndEnd().IsEmpty() && !Characters.Contains(CharacterId)) { InitializeCharacter(CharacterId, 0, 40, 0); Result.InitializedCharacterIds.Add(CharacterId); }
    Result.bOk = true;
    Result.Snapshot = CreateSnapshot();
    return Result;
}

int32 UAffectRuntime::Clamp(int32 Value, int32 Minimum, int32 Maximum) { return FMath::Clamp(Value, Minimum, Maximum); }
int32 UAffectRuntime::MoveToward(int32 Value, int32 Target, int32 Amount) { return Value < Target ? FMath::Min(Value + Amount, Target) : FMath::Max(Value - Amount, Target); }
bool UAffectRuntime::IsKnownSourceKind(EAffectSourceKind SourceKind) { return SourceKind == EAffectSourceKind::ConversationReply || SourceKind == EAffectSourceKind::SocialEvent || SourceKind == EAffectSourceKind::ConflictEvent || SourceKind == EAffectSourceKind::EnvironmentEvent || SourceKind == EAffectSourceKind::AgentJudgment || SourceKind == EAffectSourceKind::System; }

void UAffectRuntime::GetSourceCaps(EAffectSourceKind SourceKind, int32& OutValenceCap, int32& OutArousalCap)
{
    switch (SourceKind)
    {
    case EAffectSourceKind::ConversationReply: OutValenceCap = 5; OutArousalCap = 6; break;
    case EAffectSourceKind::SocialEvent: OutValenceCap = 10; OutArousalCap = 12; break;
    case EAffectSourceKind::ConflictEvent: OutValenceCap = 20; OutArousalCap = 20; break;
    case EAffectSourceKind::EnvironmentEvent: OutValenceCap = 8; OutArousalCap = 10; break;
    case EAffectSourceKind::AgentJudgment: OutValenceCap = 15; OutArousalCap = 15; break;
    default: OutValenceCap = 100; OutArousalCap = 100; break;
    }
}

FAffectPresentation UAffectRuntime::ProjectPresentation(const FAffectState& State)
{
    FAffectPresentation Presentation;
    Presentation.Mood = State.Valence <= -50 ? TEXT("distressed") : State.Valence <= -15 ? TEXT("unhappy") : State.Valence < 15 ? TEXT("neutral") : State.Valence < 50 ? TEXT("positive") : TEXT("delighted");
    Presentation.Activation = State.Arousal < 30 ? TEXT("calm") : State.Arousal <= 65 ? TEXT("alert") : TEXT("tense");
    Presentation.DialogueTone = State.Valence <= -50 ? TEXT("withdrawn") : State.Valence <= -15 ? TEXT("guarded") : State.Arousal > 65 ? TEXT("urgent") : State.Valence >= 15 ? TEXT("friendly") : TEXT("neutral");
    Presentation.BehaviorDisposition = State.Valence <= -50 && State.Arousal > 65 ? TEXT("seek_space") : State.Arousal > 65 ? TEXT("cautious") : State.Valence >= 15 ? TEXT("social") : TEXT("normal");
    Presentation.RecentCause = State.LastSourceKind;
    return Presentation;
}

bool UAffectRuntime::IsValidState(const FAffectState& State)
{
    return !State.CharacterId.TrimStartAndEnd().IsEmpty() && State.Valence >= -100 && State.Valence <= 100
        && State.Arousal >= 0 && State.Arousal <= 100 && State.BaselineValence >= -100 && State.BaselineValence <= 100
        && State.BaselineArousal >= 0 && State.BaselineArousal <= 100 && State.LastEventAtMinute >= 0 && State.LastDecayAtMinute >= 0 && State.LastChangedAtMinute >= 0 && State.Revision >= 0;
}

FAffectApplyResult UAffectRuntime::Failure(const FString& Error) { FAffectApplyResult Result; Result.Error = Error; return Result; }
FAffectApplyResult UAffectRuntime::Success(const FAffectState& State, bool bApplied, bool bDuplicate, int32 ValenceDelta, int32 ArousalDelta)
{
    FAffectApplyResult Result;
    Result.bOk = true; Result.bApplied = bApplied; Result.bDuplicate = bDuplicate; Result.State = State;
    Result.AppliedValenceDelta = ValenceDelta; Result.AppliedArousalDelta = ArousalDelta;
    return Result;
}
