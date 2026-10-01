using System;
using System.Collections.Generic;

namespace MyAiTown.AffectCore
{
    [Serializable]
    public sealed class AffectChanges
    {
        public int valenceDelta;
        public int arousalDelta;
    }

    [Serializable]
    public sealed class AffectUpdate
    {
        public string updateId = "";
        public string characterId = "";
        public string sourceKind = "";
        public int occurredAtMinute;
        public string causeEventId = "";
        // JsonUtility cannot distinguish an omitted integer from zero. Use this
        // pair for typed callers that need optimistic-concurrency protection.
        public bool hasExpectedRevision;
        public int expectedRevision;
        public AffectChanges changes = new AffectChanges();
    }

    [Serializable]
    public sealed class AffectState
    {
        public string characterId = "";
        public int valence;
        public int arousal = 40;
        public int baselineValence;
        public int baselineArousal = 40;
        public string dominantCauseEventId = "";
        public string lastCauseEventId = "";
        public string lastSourceKind = "none";
        public int lastEventAtMinute;
        public int lastDecayAtMinute;
        public int lastChangedAtMinute;
        public int revision;
        public List<string> processedUpdateIds = new List<string>();

        public AffectState Clone()
        {
            return new AffectState
            {
                characterId = characterId,
                valence = valence,
                arousal = arousal,
                baselineValence = baselineValence,
                baselineArousal = baselineArousal,
                dominantCauseEventId = dominantCauseEventId,
                lastCauseEventId = lastCauseEventId,
                lastSourceKind = lastSourceKind,
                lastEventAtMinute = lastEventAtMinute,
                lastDecayAtMinute = lastDecayAtMinute,
                lastChangedAtMinute = lastChangedAtMinute,
                revision = revision,
                processedUpdateIds = new List<string>(processedUpdateIds)
            };
        }
    }

    [Serializable]
    public sealed class AffectSnapshot
    {
        public int schemaVersion = AffectRuntime.SchemaVersion;
        // A list is intentional: Unity JsonUtility does not serialize Dictionary.
        // Convert it at your storage boundary if the shared JSON-schema object map is required.
        public List<AffectState> characters = new List<AffectState>();
    }

    public sealed class ApplyResult
    {
        public bool ok;
        public bool applied;
        public bool duplicate;
        public string error = "";
        public AffectState state;
        public int appliedValenceDelta;
        public int appliedArousalDelta;
    }

    /// <summary>Batch restore outcome; unlike ApplyResult it never implies one representative character.</summary>
    public sealed class RestoreResult
    {
        public bool ok;
        public string error = "";
        public AffectSnapshot snapshot;
        public List<string> restoredCharacterIds = new List<string>();
        public List<string> initializedCharacterIds = new List<string>();
    }

    /// <summary>Non-numeric labels intended for UI, dialogue and behaviour layers.</summary>
    public sealed class AffectPresentation
    {
        public string mood = "neutral";
        public string activation = "alert";
        public string dialogueTone = "neutral";
        public string behaviorDisposition = "normal";
        public string recentCause = "none";
    }

    /// <summary>
    /// Deterministic affect state machine. It deliberately owns no Unity scene
    /// objects, clock, memories, relationships, networking, or LLM calls.
    /// </summary>
    public sealed class AffectRuntime
    {
        public const int SchemaVersion = 1;
        public const int DefaultBaselineValence = 0;
        public const int DefaultBaselineArousal = 40;
        public const int ProcessedUpdateIdLimit = 256;
        public const int DecayIntervalMinutes = 60;
        public const int ValenceDecayPerInterval = 2;
        public const int ArousalDecayPerInterval = 4;

        private readonly Dictionary<string, AffectState> characters = new Dictionary<string, AffectState>();

        public AffectState InitializeCharacter(string characterId, int baselineValence = DefaultBaselineValence,
            int baselineArousal = DefaultBaselineArousal, int nowMinute = 0)
        {
            if (string.IsNullOrWhiteSpace(characterId)) return null;
            AffectState existing;
            if (characters.TryGetValue(characterId, out existing)) return existing.Clone();

            var state = new AffectState
            {
                characterId = characterId,
                valence = Clamp(baselineValence, -100, 100),
                arousal = Clamp(baselineArousal, 0, 100),
                baselineValence = Clamp(baselineValence, -100, 100),
                baselineArousal = Clamp(baselineArousal, 0, 100),
                lastEventAtMinute = Math.Max(0, nowMinute),
                lastDecayAtMinute = Math.Max(0, nowMinute),
                lastChangedAtMinute = Math.Max(0, nowMinute)
            };
            characters.Add(characterId, state);
            return state.Clone();
        }

        public ApplyResult Apply(AffectUpdate update)
        {
            var validationError = ValidateUpdate(update);
            if (validationError != "") return Failure(validationError);

            AffectState state;
            if (!characters.TryGetValue(update.characterId, out state)) return Failure("character_not_initialized");
            if (state.processedUpdateIds.Contains(update.updateId)) return Success(state, false, true, 0, 0);
            if (update.hasExpectedRevision && update.expectedRevision != state.revision) return Failure("revision_conflict");
            if (update.occurredAtMinute < state.lastEventAtMinute) return Failure("stale_update");

            Advance(update.characterId, update.occurredAtMinute);

            var caps = GetSourceCaps(update.sourceKind);
            var beforeValence = state.valence;
            var beforeArousal = state.arousal;
            state.valence = Clamp(state.valence + Clamp(update.changes.valenceDelta, -caps.valence, caps.valence), -100, 100);
            state.arousal = Clamp(state.arousal + Clamp(update.changes.arousalDelta, -caps.arousal, caps.arousal), 0, 100);
            state.dominantCauseEventId = update.causeEventId ?? "";
            state.lastCauseEventId = state.dominantCauseEventId;
            state.lastSourceKind = update.sourceKind;
            state.lastEventAtMinute = update.occurredAtMinute;
            state.lastChangedAtMinute = update.occurredAtMinute;
            state.revision += 1;
            state.processedUpdateIds.Add(update.updateId);
            if (state.processedUpdateIds.Count > ProcessedUpdateIdLimit) state.processedUpdateIds.RemoveRange(0, state.processedUpdateIds.Count - ProcessedUpdateIdLimit);
            return Success(state, true, false, state.valence - beforeValence, state.arousal - beforeArousal);
        }

        public ApplyResult Advance(string characterId, int nowMinute)
        {
            AffectState state;
            if (!characters.TryGetValue(characterId, out state)) return Failure("character_not_initialized");
            var intervals = (nowMinute - state.lastDecayAtMinute) / DecayIntervalMinutes;
            if (intervals <= 0) return Success(state, false, false, 0, 0);

            var beforeValence = state.valence;
            var beforeArousal = state.arousal;
            state.valence = MoveToward(state.valence, state.baselineValence, intervals * ValenceDecayPerInterval);
            state.arousal = MoveToward(state.arousal, state.baselineArousal, intervals * ArousalDecayPerInterval);
            state.lastDecayAtMinute += intervals * DecayIntervalMinutes;
            if (state.valence != beforeValence || state.arousal != beforeArousal) state.lastChangedAtMinute = state.lastDecayAtMinute;
            var changed = state.valence != beforeValence || state.arousal != beforeArousal;
            if (changed) state.revision += 1;
            if (state.valence == state.baselineValence && state.arousal == state.baselineArousal) { state.dominantCauseEventId = ""; state.lastCauseEventId = ""; state.lastSourceKind = "none"; }
            return Success(state, changed, false, state.valence - beforeValence, state.arousal - beforeArousal);
        }

        public AffectState Get(string characterId)
        {
            AffectState state;
            return characters.TryGetValue(characterId, out state) ? state.Clone() : null;
        }

        /// <summary>Projects internal numbers into stable labels without returning the numbers.</summary>
        public AffectPresentation Describe(string characterId)
        {
            AffectState state;
            return characters.TryGetValue(characterId, out state) ? ProjectPresentation(state) : null;
        }

        public AffectSnapshot CreateSnapshot()
        {
            var snapshot = new AffectSnapshot();
            foreach (var pair in characters) snapshot.characters.Add(pair.Value.Clone());
            return snapshot;
        }

        public RestoreResult Restore(AffectSnapshot snapshot, IList<string> requiredCharacterIds = null)
        {
            if (snapshot == null || snapshot.schemaVersion != SchemaVersion || snapshot.characters == null) return RestoreFailure("snapshot_schema_unsupported");
            var restored = new Dictionary<string, AffectState>();
            foreach (var state in snapshot.characters)
            {
                if (!IsValidState(state) || restored.ContainsKey(state.characterId)) return RestoreFailure("snapshot_state_invalid");
                var migrated = state.Clone();
                if (migrated.lastEventAtMinute == 0 && migrated.lastDecayAtMinute == 0 && migrated.lastChangedAtMinute > 0)
                {
                    migrated.lastEventAtMinute = migrated.lastChangedAtMinute;
                    migrated.lastDecayAtMinute = migrated.lastChangedAtMinute;
                }
                restored.Add(migrated.characterId, migrated);
            }
            characters.Clear();
            foreach (var pair in restored) characters.Add(pair.Key, pair.Value);
            var initialized = new List<string>();
            if (requiredCharacterIds != null)
                foreach (var characterId in requiredCharacterIds)
                    if (!characters.ContainsKey(characterId)) { InitializeCharacter(characterId); initialized.Add(characterId); }
            return new RestoreResult { ok = true, snapshot = CreateSnapshot(), restoredCharacterIds = new List<string>(restored.Keys), initializedCharacterIds = initialized };
        }

        private static string ValidateUpdate(AffectUpdate update)
        {
            if (update == null || string.IsNullOrWhiteSpace(update.updateId) || string.IsNullOrWhiteSpace(update.characterId)) return "update_identity_invalid";
            if (!IsKnownSourceKind(update.sourceKind)) return "source_kind_unknown";
            if (update.occurredAtMinute < 0) return "occurred_at_invalid";
            return update.changes == null ? "changes_invalid" : "";
        }

        private static bool IsValidState(AffectState state)
        {
            return state != null && !string.IsNullOrWhiteSpace(state.characterId) && state.processedUpdateIds != null
                && state.valence >= -100 && state.valence <= 100 && state.arousal >= 0 && state.arousal <= 100
                && state.baselineValence >= -100 && state.baselineValence <= 100 && state.baselineArousal >= 0 && state.baselineArousal <= 100
                && state.lastEventAtMinute >= 0 && state.lastDecayAtMinute >= 0 && state.lastChangedAtMinute >= 0 && state.revision >= 0;
        }

        private static bool IsKnownSourceKind(string sourceKind)
        {
            return sourceKind == "conversation_reply" || sourceKind == "social_event" || sourceKind == "conflict_event"
                || sourceKind == "environment_event" || sourceKind == "agent_judgment" || sourceKind == "system";
        }

        private static SourceCaps GetSourceCaps(string sourceKind)
        {
            if (sourceKind == "conversation_reply") return new SourceCaps(5, 6);
            if (sourceKind == "social_event") return new SourceCaps(10, 12);
            if (sourceKind == "conflict_event") return new SourceCaps(20, 20);
            if (sourceKind == "environment_event") return new SourceCaps(8, 10);
            if (sourceKind == "agent_judgment") return new SourceCaps(15, 15);
            return new SourceCaps(100, 100);
        }

        private static int Clamp(int value, int minimum, int maximum) { return Math.Max(minimum, Math.Min(maximum, value)); }
        private static int MoveToward(int value, int target, int amount) { return value < target ? Math.Min(value + amount, target) : Math.Max(value - amount, target); }
        private static AffectPresentation ProjectPresentation(AffectState state)
        {
            var mood = state.valence <= -50 ? "distressed" : state.valence <= -15 ? "unhappy" : state.valence < 15 ? "neutral" : state.valence < 50 ? "positive" : "delighted";
            var activation = state.arousal < 30 ? "calm" : state.arousal <= 65 ? "alert" : "tense";
            var dialogueTone = state.valence <= -50 ? "withdrawn" : state.valence <= -15 ? "guarded" : state.arousal > 65 ? "urgent" : state.valence >= 15 ? "friendly" : "neutral";
            var behaviorDisposition = state.valence <= -50 && state.arousal > 65 ? "seek_space" : state.arousal > 65 ? "cautious" : state.valence >= 15 ? "social" : "normal";
            return new AffectPresentation { mood = mood, activation = activation, dialogueTone = dialogueTone, behaviorDisposition = behaviorDisposition, recentCause = state.lastSourceKind };
        }
        private static ApplyResult Failure(string error) { return new ApplyResult { ok = false, error = error }; }
        private static RestoreResult RestoreFailure(string error) { return new RestoreResult { ok = false, error = error }; }
        private static ApplyResult Success(AffectState state, bool applied, bool duplicate, int valenceDelta, int arousalDelta)
        {
            return new ApplyResult { ok = true, applied = applied, duplicate = duplicate, state = state.Clone(), appliedValenceDelta = valenceDelta, appliedArousalDelta = arousalDelta };
        }
        private AffectState FirstStateOrEmpty()
        {
            foreach (var pair in characters) return pair.Value;
            return new AffectState();
        }

        private struct SourceCaps
        {
            public readonly int valence;
            public readonly int arousal;
            public SourceCaps(int valenceCap, int arousalCap) { valence = valenceCap; arousal = arousalCap; }
        }
    }
}
