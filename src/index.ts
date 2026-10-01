export const AFFECT_SCHEMA_VERSION = 1;
export const DEFAULT_BASELINE = { valence: 0, arousal: 40 } as const;

export type SourceKind = "conversation_reply" | "social_event" | "conflict_event" | "environment_event" | "agent_judgment" | "system";
export type SourceCaps = { valence: number; arousal: number };
export type AffectPolicy = {
  valenceBounds: readonly [number, number]; arousalBounds: readonly [number, number]; sourceCaps: Record<SourceKind, SourceCaps>;
  decay: { intervalMinutes: number; valencePerInterval: number; arousalPerInterval: number };
  presentation: { distressedAt: number; unhappyAt: number; positiveAt: number; delightedAt: number; calmBelow: number; tenseAbove: number };
  processedUpdateIdLimit: number;
};
export type AffectPolicyConfig = {
  valenceBounds?: readonly [number, number]; arousalBounds?: readonly [number, number];
  sourceCaps?: Partial<Record<SourceKind, Partial<SourceCaps>>>; decay?: Partial<AffectPolicy["decay"]>;
  presentation?: Partial<AffectPolicy["presentation"]>;
  /** -1 retains all IDs; a positive value retains a bounded replay window. */ processedUpdateIdLimit?: number;
};

/** Defaults retain 0.1.0 behaviour and can be overridden per runtime. */
export const DEFAULT_POLICY: AffectPolicy = {
  valenceBounds: [-100, 100], arousalBounds: [0, 100],
  sourceCaps: {
    conversation_reply: { valence: 5, arousal: 6 }, social_event: { valence: 10, arousal: 12 },
    conflict_event: { valence: 20, arousal: 20 }, environment_event: { valence: 8, arousal: 10 },
    agent_judgment: { valence: 15, arousal: 15 }, system: { valence: 100, arousal: 100 },
  },
  decay: { intervalMinutes: 60, valencePerInterval: 2, arousalPerInterval: 4 },
  presentation: { distressedAt: -50, unhappyAt: -15, positiveAt: 15, delightedAt: 50, calmBelow: 30, tenseAbove: 65 },
  processedUpdateIdLimit: 256,
};

export type AffectState = {
  characterId: string; valence: number; arousal: number; baselineValence: number; baselineArousal: number;
  /** @deprecated Kept for v1 snapshot compatibility. Use lastCauseEventId. */ dominantCauseEventId: string;
  lastCauseEventId: string; lastSourceKind: SourceKind | "none";
  /** Last accepted event, used only for event ordering. */ lastEventAtMinute: number;
  /** Last fully settled decay boundary, independent of event frequency. */ lastDecayAtMinute: number;
  /** Last state mutation, retained for v1 consumers. */ lastChangedAtMinute: number;
  revision: number; processedUpdateIds: string[];
};
export type AffectUpdate = { updateId: string; characterId: string; sourceKind: SourceKind; occurredAtMinute: number; causeEventId?: string; changes: { valenceDelta?: number; arousalDelta?: number }; expectedRevision?: number };
export type ApplyResult =
  | { ok: true; applied: boolean; duplicate: boolean; state: AffectState; appliedDelta?: { valence: number; arousal: number } }
  | { ok: false; error: string; details?: Record<string, unknown> };
export type AffectSnapshot = { schemaVersion: number; characters: Record<string, AffectState> };
export type RestoreResult =
  | { ok: true; snapshot: AffectSnapshot; restoredCharacterIds: string[]; initializedCharacterIds: string[] }
  | { ok: false; error: string; details?: Record<string, unknown> };
export type AffectPresentation = {
  mood: "distressed" | "unhappy" | "neutral" | "positive" | "delighted";
  activation: "calm" | "alert" | "tense";
  dialogueTone: "withdrawn" | "guarded" | "urgent" | "friendly" | "neutral";
  behaviorDisposition: "seek_space" | "cautious" | "social" | "normal";
  recentCause: SourceKind | "none";
};

/** Deterministic state machine. The host owns its clock and any durable event ledger. */
export class AffectRuntime {
  private readonly characters = new Map<string, AffectState>();
  readonly policy: AffectPolicy;

  constructor(policy: AffectPolicyConfig = {}) { this.policy = normalizePolicy(policy); }

  initializeCharacter(characterId: string, baselineValence = 0, baselineArousal = 40, nowMinute = 0): AffectState | null {
    if (!characterId.trim()) return null;
    const existing = this.characters.get(characterId);
    if (existing) return clone(existing);
    const state = this.makeInitialState(characterId, baselineValence, baselineArousal, nowMinute)!;
    this.characters.set(characterId, state);
    return clone(state);
  }

  apply(update: AffectUpdate): ApplyResult {
    const invalid = validateUpdate(update);
    if (invalid) return { ok: false, error: invalid };
    const state = this.characters.get(update.characterId);
    if (!state) return { ok: false, error: "character_not_initialized" };
    if (state.processedUpdateIds.includes(update.updateId)) return { ok: true, applied: false, duplicate: true, state: clone(state) };
    if (update.expectedRevision !== undefined && update.expectedRevision !== state.revision) return { ok: false, error: "revision_conflict", details: { expectedRevision: update.expectedRevision, actualRevision: state.revision } };
    if (update.occurredAtMinute < state.lastEventAtMinute) return { ok: false, error: "stale_update" };

    this.advanceState(state, update.occurredAtMinute, false);
    const beforeEvent = { valence: state.valence, arousal: state.arousal };
    const caps = this.policy.sourceCaps[update.sourceKind];
    state.valence = clamp(state.valence + clamp(update.changes.valenceDelta ?? 0, -caps.valence, caps.valence), ...this.policy.valenceBounds);
    state.arousal = clamp(state.arousal + clamp(update.changes.arousalDelta ?? 0, -caps.arousal, caps.arousal), ...this.policy.arousalBounds);
    state.lastCauseEventId = update.causeEventId ?? ""; state.dominantCauseEventId = state.lastCauseEventId; state.lastSourceKind = update.sourceKind;
    state.lastEventAtMinute = update.occurredAtMinute; state.lastChangedAtMinute = update.occurredAtMinute; state.revision += 1;
    state.processedUpdateIds.push(update.updateId);
    if (this.policy.processedUpdateIdLimit >= 0 && state.processedUpdateIds.length > this.policy.processedUpdateIdLimit) state.processedUpdateIds.splice(0, state.processedUpdateIds.length - this.policy.processedUpdateIdLimit);
    return { ok: true, applied: true, duplicate: false, state: clone(state), appliedDelta: { valence: state.valence - beforeEvent.valence, arousal: state.arousal - beforeEvent.arousal } };
  }

  advance(characterId: string, nowMinute: number): ApplyResult {
    const state = this.characters.get(characterId);
    if (!state) return { ok: false, error: "character_not_initialized" };
    const before = { valence: state.valence, arousal: state.arousal };
    const changed = this.advanceState(state, nowMinute);
    return { ok: true, applied: changed, duplicate: false, state: clone(state), appliedDelta: { valence: state.valence - before.valence, arousal: state.arousal - before.arousal } };
  }

  get(characterId: string): AffectState | null { return this.characters.has(characterId) ? clone(this.characters.get(characterId)!) : null; }
  describe(characterId: string): AffectPresentation | null { const state = this.characters.get(characterId); return state ? projectPresentation(state, this.policy) : null; }
  snapshot(): AffectSnapshot { return { schemaVersion: AFFECT_SCHEMA_VERSION, characters: Object.fromEntries([...this.characters].map(([id, state]) => [id, clone(state)])) }; }

  restore(snapshot: AffectSnapshot, requiredCharacterIds: string[] = []): RestoreResult {
    if (snapshot.schemaVersion !== AFFECT_SCHEMA_VERSION || !isRecord(snapshot.characters)) return { ok: false, error: "snapshot_schema_unsupported" };
    const restored = new Map<string, AffectState>();
    for (const [id, raw] of Object.entries(snapshot.characters)) {
      const state = normalizeRestoredState(raw, id);
      if (!state) return { ok: false, error: "snapshot_state_invalid", details: { characterId: id } };
      restored.set(id, state);
    }
    const initializedCharacterIds: string[] = [];
    for (const id of requiredCharacterIds) if (!restored.has(id)) {
      const state = this.makeInitialState(id, 0, 40, 0);
      if (!state) return { ok: false, error: "required_character_invalid", details: { characterId: id } };
      restored.set(id, state); initializedCharacterIds.push(id);
    }
    this.characters.clear(); for (const [id, state] of restored) this.characters.set(id, state);
    return { ok: true, snapshot: this.snapshot(), restoredCharacterIds: [...restored.keys()], initializedCharacterIds };
  }

  private makeInitialState(characterId: string, baselineValence: number, baselineArousal: number, nowMinute: number): AffectState | null {
    if (!characterId.trim()) return null;
    const now = Math.max(0, Math.trunc(nowMinute));
    return { characterId, valence: clamp(baselineValence, ...this.policy.valenceBounds), arousal: clamp(baselineArousal, ...this.policy.arousalBounds), baselineValence: clamp(baselineValence, ...this.policy.valenceBounds), baselineArousal: clamp(baselineArousal, ...this.policy.arousalBounds), dominantCauseEventId: "", lastCauseEventId: "", lastSourceKind: "none", lastEventAtMinute: now, lastDecayAtMinute: now, lastChangedAtMinute: now, revision: 0, processedUpdateIds: [] };
  }

  private advanceState(state: AffectState, nowMinute: number, incrementRevision = true): boolean {
    const now = Math.max(0, Math.trunc(nowMinute));
    const intervals = Math.floor((now - state.lastDecayAtMinute) / this.policy.decay.intervalMinutes);
    if (intervals <= 0) return false;
    const beforeValence = state.valence; const beforeArousal = state.arousal;
    state.valence = moveToward(state.valence, state.baselineValence, intervals * this.policy.decay.valencePerInterval);
    state.arousal = moveToward(state.arousal, state.baselineArousal, intervals * this.policy.decay.arousalPerInterval);
    state.lastDecayAtMinute += intervals * this.policy.decay.intervalMinutes;
    const changed = state.valence !== beforeValence || state.arousal !== beforeArousal;
    if (changed && incrementRevision) { state.revision += 1; state.lastChangedAtMinute = state.lastDecayAtMinute; }
    if (state.valence === state.baselineValence && state.arousal === state.baselineArousal) { state.lastCauseEventId = ""; state.dominantCauseEventId = ""; state.lastSourceKind = "none"; }
    return changed;
  }
}

function normalizePolicy(config: AffectPolicyConfig): AffectPolicy {
  const sourceCaps = {} as Record<SourceKind, SourceCaps>;
  for (const kind of Object.keys(DEFAULT_POLICY.sourceCaps) as SourceKind[]) {
    const override = config.sourceCaps?.[kind]; const fallback = DEFAULT_POLICY.sourceCaps[kind];
    sourceCaps[kind] = { valence: positiveInt(override?.valence, fallback.valence), arousal: positiveInt(override?.arousal, fallback.arousal) };
  }
  const bounds = (value: readonly [number, number] | undefined, fallback: readonly [number, number]) => value && value.length === 2 && value[0] < value[1] ? [Math.trunc(value[0]), Math.trunc(value[1])] as const : fallback;
  return { valenceBounds: bounds(config.valenceBounds, DEFAULT_POLICY.valenceBounds), arousalBounds: bounds(config.arousalBounds, DEFAULT_POLICY.arousalBounds), sourceCaps, decay: { intervalMinutes: positiveInt(config.decay?.intervalMinutes, DEFAULT_POLICY.decay.intervalMinutes), valencePerInterval: positiveInt(config.decay?.valencePerInterval, DEFAULT_POLICY.decay.valencePerInterval), arousalPerInterval: positiveInt(config.decay?.arousalPerInterval, DEFAULT_POLICY.decay.arousalPerInterval) }, presentation: { ...DEFAULT_POLICY.presentation, ...config.presentation }, processedUpdateIdLimit: config.processedUpdateIdLimit === -1 ? -1 : positiveInt(config.processedUpdateIdLimit, DEFAULT_POLICY.processedUpdateIdLimit) };
}
function positiveInt(value: number | undefined, fallback: number): number { return Number.isFinite(value) && value! > 0 ? Math.trunc(value!) : fallback; }
function validateUpdate(value: AffectUpdate): string | null { if (!value || !value.updateId?.trim() || !value.characterId?.trim()) return "update_identity_invalid"; if (!(value.sourceKind in DEFAULT_POLICY.sourceCaps)) return "source_kind_unknown"; if (!Number.isInteger(value.occurredAtMinute) || value.occurredAtMinute < 0) return "occurred_at_invalid"; return !isRecord(value.changes) || (!Number.isInteger(value.changes.valenceDelta ?? 0) && !Number.isInteger(value.changes.arousalDelta ?? 0)) ? "changes_invalid" : null; }
function normalizeRestoredState(value: unknown, id: string): AffectState | null {
  if (!isRecord(value) || value.characterId !== id || !Array.isArray(value.processedUpdateIds) || !value.processedUpdateIds.every(item => typeof item === "string")) return null;
  const valence = value.valence as number; const arousal = value.arousal as number; const baselineValence = value.baselineValence as number; const baselineArousal = value.baselineArousal as number; const lastChangedAtMinute = value.lastChangedAtMinute as number; const revision = value.revision as number;
  const integers = [valence, arousal, baselineValence, baselineArousal, lastChangedAtMinute, revision];
  if (!integers.every(Number.isInteger) || valence < -100 || valence > 100 || arousal < 0 || arousal > 100 || baselineValence < -100 || baselineValence > 100 || baselineArousal < 0 || baselineArousal > 100 || lastChangedAtMinute < 0 || revision < 0) return null;
  const legacyTime = value.lastChangedAtMinute as number;
  const lastEventAtMinute = Number.isInteger(value.lastEventAtMinute) && (value.lastEventAtMinute as number) >= 0 ? value.lastEventAtMinute as number : legacyTime;
  const lastDecayAtMinute = Number.isInteger(value.lastDecayAtMinute) && (value.lastDecayAtMinute as number) >= 0 ? value.lastDecayAtMinute as number : legacyTime;
  const source = typeof value.lastSourceKind === "string" && value.lastSourceKind in DEFAULT_POLICY.sourceCaps ? value.lastSourceKind as SourceKind : "none";
  const cause = typeof value.lastCauseEventId === "string" ? value.lastCauseEventId : typeof value.dominantCauseEventId === "string" ? value.dominantCauseEventId : "";
  return { characterId: id, valence, arousal, baselineValence, baselineArousal, dominantCauseEventId: cause, lastCauseEventId: cause, lastSourceKind: source, lastEventAtMinute, lastDecayAtMinute, lastChangedAtMinute: legacyTime, revision, processedUpdateIds: [...value.processedUpdateIds] as string[] };
}
function isRecord(value: unknown): value is Record<string, unknown> { return typeof value === "object" && value !== null && !Array.isArray(value); }
function clamp(value: number, min: number, max: number): number { return Math.max(min, Math.min(max, Math.trunc(value))); }
function moveToward(value: number, target: number, amount: number): number { return value < target ? Math.min(value + amount, target) : Math.max(value - amount, target); }
function clone(state: AffectState): AffectState { return { ...state, processedUpdateIds: [...state.processedUpdateIds] }; }
function projectPresentation(state: AffectState, policy: AffectPolicy): AffectPresentation {
  const p = policy.presentation;
  const mood = state.valence <= p.distressedAt ? "distressed" : state.valence <= p.unhappyAt ? "unhappy" : state.valence < p.positiveAt ? "neutral" : state.valence < p.delightedAt ? "positive" : "delighted";
  const activation = state.arousal < p.calmBelow ? "calm" : state.arousal <= p.tenseAbove ? "alert" : "tense";
  const dialogueTone = state.valence <= p.distressedAt ? "withdrawn" : state.valence <= p.unhappyAt ? "guarded" : state.arousal > p.tenseAbove ? "urgent" : state.valence >= p.positiveAt ? "friendly" : "neutral";
  const behaviorDisposition = state.valence <= p.distressedAt && state.arousal > p.tenseAbove ? "seek_space" : state.arousal > p.tenseAbove ? "cautious" : state.valence >= p.positiveAt ? "social" : "normal";
  return { mood, activation, dialogueTone, behaviorDisposition, recentCause: state.lastSourceKind };
}
