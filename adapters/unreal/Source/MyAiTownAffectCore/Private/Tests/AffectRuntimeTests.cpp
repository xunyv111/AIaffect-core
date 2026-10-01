#if WITH_DEV_AUTOMATION_TESTS

#include "AffectProfileComponent.h"
#include "AffectRuntime.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAffectRuntimeAutomationTest, "MyAiTown.AffectCore.Runtime.SemanticProjection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAffectRuntimeAutomationTest::RunTest(const FString& Parameters)
{
    UAffectRuntime* Runtime = NewObject<UAffectRuntime>();
    TestTrue(TEXT("Initialize character"), Runtime->InitializeCharacter(TEXT("lin"), 0, 40, 0));

    FAffectPresentation Presentation;
    TestTrue(TEXT("Apply bounded agent judgment"), Runtime->ApplyEvent(TEXT("event-1"), TEXT("lin"), EAffectSourceKind::AgentJudgment, 1, TEXT("argument"), -99, 99, Presentation));
    TestEqual(TEXT("Mood is semantic and non-numeric"), Presentation.Mood, FString(TEXT("unhappy")));
    TestEqual(TEXT("Activation is semantic and non-numeric"), Presentation.Activation, FString(TEXT("alert")));
    TestEqual(TEXT("Dialogue tone is semantic and non-numeric"), Presentation.DialogueTone, FString(TEXT("guarded")));
    TestEqual(TEXT("Disposition is semantic and non-numeric"), Presentation.BehaviorDisposition, FString(TEXT("normal")));

    FAffectPresentation DuplicatePresentation;
    TestTrue(TEXT("Duplicate does not fail"), Runtime->ApplyEvent(TEXT("event-1"), TEXT("lin"), EAffectSourceKind::AgentJudgment, 1, TEXT("argument"), -1, 1, DuplicatePresentation));
    FAffectState InternalState;
    TestTrue(TEXT("Internal state is available to C++ persistence layer"), Runtime->GetInternalState(TEXT("lin"), InternalState));
    TestEqual(TEXT("Duplicate event is not applied twice"), InternalState.Revision, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAffectActorTagBridgeAutomationTest, "MyAiTown.AffectCore.Integration.ActorTagBridge", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAffectActorTagBridgeAutomationTest::RunTest(const FString& Parameters)
{
    AActor* Actor = NewObject<AActor>();
    UAffectProfileComponent* Profile = NewObject<UAffectProfileComponent>(Actor);
    Profile->CharacterId = TEXT("lin");

    FAffectAutoRule ConflictRule;
    ConflictRule.TriggerActorTag = FName(TEXT("Affect.Event.Conflict"));
    ConflictRule.SourceKind = EAffectSourceKind::ConflictEvent;
    ConflictRule.ValenceDelta = -99;
    ConflictRule.ArousalDelta = 99;
    Profile->LocalActorTagRules.Add(ConflictRule);

    Profile->InitializeRuntimeAtMinute(0);
    Actor->Tags.Add(ConflictRule.TriggerActorTag);
    Profile->RefreshActorTagsAtMinute(1);

    FAffectPresentation Presentation;
    TestTrue(TEXT("Tag addition produces a semantic presentation"), Profile->GetCurrentPresentation(Presentation));
    TestEqual(TEXT("Conflict tag applies the source-bounded negative mood"), Presentation.Mood, FString(TEXT("unhappy")));
    TestEqual(TEXT("Conflict tag applies the source-bounded activation"), Presentation.Activation, FString(TEXT("alert")));
    TestEqual(TEXT("No explicit ApplyEvent call is required"), Presentation.DialogueTone, FString(TEXT("guarded")));

    Profile->RefreshActorTagsAtMinute(2);
    FAffectPresentation DuplicatePresentation;
    TestTrue(TEXT("Unchanged tags do not reapply"), Profile->GetCurrentPresentation(DuplicatePresentation));
    TestEqual(TEXT("The semantic state stays stable after a repeated poll"), DuplicatePresentation.Mood, FString(TEXT("unhappy")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAffectRuntimeRecoveryAutomationTest, "MyAiTown.AffectCore.Runtime.Recovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAffectRuntimeRecoveryAutomationTest::RunTest(const FString& Parameters)
{
    UAffectRuntime* Runtime = NewObject<UAffectRuntime>();
    Runtime->InitializeCharacter(TEXT("lin"), 0, 40, 0);
    FAffectUpdate First;
    First.UpdateId = TEXT("event-0"); First.CharacterId = TEXT("lin"); First.SourceKind = EAffectSourceKind::ConflictEvent; First.Changes.ValenceDelta = -20; First.Changes.ArousalDelta = 20;
    Runtime->Apply(First);
    FAffectUpdate Middle;
    Middle.UpdateId = TEXT("event-30"); Middle.CharacterId = TEXT("lin"); Middle.SourceKind = EAffectSourceKind::SocialEvent; Middle.OccurredAtMinute = 30;
    Runtime->Apply(Middle);
    FAffectUpdate Last;
    Last.UpdateId = TEXT("event-60"); Last.CharacterId = TEXT("lin"); Last.SourceKind = EAffectSourceKind::SocialEvent; Last.OccurredAtMinute = 60;
    Runtime->Apply(Last);
    FAffectState State;
    TestTrue(TEXT("State is available after frequent events"), Runtime->GetInternalState(TEXT("lin"), State));
    TestEqual(TEXT("Decay progresses despite 30-minute events"), State.Valence, -18);
    TestEqual(TEXT("Arousal decays despite 30-minute events"), State.Arousal, 56);

    const FAffectSnapshot Before = Runtime->CreateSnapshot();
    FAffectSnapshot Bad; FAffectState Invalid; Invalid.CharacterId = TEXT("broken"); Invalid.Valence = 999; Bad.Characters.Add(Invalid);
    const FAffectRestoreResult Restore = Runtime->Restore(Bad);
    TestFalse(TEXT("Invalid snapshot is rejected"), Restore.bOk);
    TestEqual(TEXT("Failed restore keeps prior characters"), Runtime->CreateSnapshot().Characters.Num(), Before.Characters.Num());
    return true;
}

#endif
