using NUnit.Framework;

namespace MyAiTown.AffectCore.Tests
{
    public sealed class AffectRuntimeTests
    {
        [Test]
        public void Apply_ClampsAndDeduplicatesByUpdateId()
        {
            var runtime = new AffectRuntime();
            runtime.InitializeCharacter("lin", 0, 40, 0);
            var update = new AffectUpdate
            {
                updateId = "event-1",
                characterId = "lin",
                sourceKind = "agent_judgment",
                occurredAtMinute = 5,
                changes = new AffectChanges { valenceDelta = -99, arousalDelta = 99 }
            };

            var first = runtime.Apply(update);
            var duplicate = runtime.Apply(update);

            Assert.That(first.ok, Is.True);
            Assert.That(first.state.valence, Is.EqualTo(-15));
            Assert.That(first.state.arousal, Is.EqualTo(55));
            Assert.That(duplicate.ok, Is.True);
            Assert.That(duplicate.applied, Is.False);
            Assert.That(duplicate.duplicate, Is.True);
            Assert.That(duplicate.state.revision, Is.EqualTo(1));
        }

        [Test]
        public void SnapshotRestoreAndAdvance_AreDeterministic()
        {
            var source = new AffectRuntime();
            source.InitializeCharacter("lin", 0, 40, 0);
            source.Apply(new AffectUpdate
            {
                updateId = "event-2",
                characterId = "lin",
                sourceKind = "conflict_event",
                occurredAtMinute = 0,
                causeEventId = "argument",
                changes = new AffectChanges { valenceDelta = -12, arousalDelta = 16 }
            });

            var restored = new AffectRuntime();
            var restoreResult = restored.Restore(source.CreateSnapshot());
            var advanceResult = restored.Advance("lin", 120);

            Assert.That(restoreResult.ok, Is.True);
            Assert.That(advanceResult.ok, Is.True);
            Assert.That(advanceResult.state.valence, Is.EqualTo(-8));
            Assert.That(advanceResult.state.arousal, Is.EqualTo(48));
            Assert.That(advanceResult.state.revision, Is.EqualTo(2));
        }

        [Test]
        public void Describe_ReturnsOnlySemanticLabels()
        {
            var runtime = new AffectRuntime();
            runtime.InitializeCharacter("lin");
            runtime.Apply(new AffectUpdate
            {
                updateId = "event-3",
                characterId = "lin",
                sourceKind = "conflict_event",
                occurredAtMinute = 1,
                changes = new AffectChanges { valenceDelta = -20, arousalDelta = 20 }
            });

            var presentation = runtime.Describe("lin");

            Assert.That(presentation.mood, Is.EqualTo("unhappy"));
            Assert.That(presentation.activation, Is.EqualTo("alert"));
            Assert.That(presentation.dialogueTone, Is.EqualTo("guarded"));
            Assert.That(presentation.behaviorDisposition, Is.EqualTo("normal"));
        }

        [Test]
        public void FrequentEvents_DoNotStarveDecay()
        {
            var runtime = new AffectRuntime();
            runtime.InitializeCharacter("lin", 0, 40, 0);
            runtime.Apply(new AffectUpdate { updateId = "event-0", characterId = "lin", sourceKind = "conflict_event", occurredAtMinute = 0, changes = new AffectChanges { valenceDelta = -20, arousalDelta = 20 } });
            runtime.Apply(new AffectUpdate { updateId = "event-30", characterId = "lin", sourceKind = "social_event", occurredAtMinute = 30, changes = new AffectChanges() });
            var result = runtime.Apply(new AffectUpdate { updateId = "event-60", characterId = "lin", sourceKind = "social_event", occurredAtMinute = 60, changes = new AffectChanges() });
            Assert.That(result.state.valence, Is.EqualTo(-18));
            Assert.That(result.state.arousal, Is.EqualTo(56));
        }

        [Test]
        public void Restore_InvalidSnapshot_PreservesExistingState()
        {
            var runtime = new AffectRuntime();
            runtime.InitializeCharacter("lin");
            var before = runtime.Get("lin");
            var result = runtime.Restore(new AffectSnapshot { characters = new System.Collections.Generic.List<AffectState> { new AffectState { characterId = "broken", valence = 999 } } });
            Assert.That(result.ok, Is.False);
            Assert.That(runtime.Get("lin").characterId, Is.EqualTo(before.characterId));
        }
    }
}
