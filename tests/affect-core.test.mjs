import assert from "node:assert/strict";
import test from "node:test";
import { AffectRuntime } from "../dist/index.js";

test("bounded, idempotent engine-neutral updates", () => {
  const runtime = new AffectRuntime();
  assert.equal(runtime.initializeCharacter("lin")?.arousal, 40);
  const applied = runtime.apply({ updateId: "event-1", characterId: "lin", sourceKind: "agent_judgment", occurredAtMinute: 1, changes: { valenceDelta: -99, arousalDelta: 99 } });
  assert.deepEqual(applied.appliedDelta, { valence: -15, arousal: 15 });
  assert.equal(runtime.apply({ updateId: "event-1", characterId: "lin", sourceKind: "agent_judgment", occurredAtMinute: 1, changes: { valenceDelta: -1 } }).duplicate, true);
});

test("snapshot restoration and time decay are deterministic", () => {
  const original = new AffectRuntime();
  original.initializeCharacter("lin");
  original.apply({ updateId: "event-2", characterId: "lin", sourceKind: "conflict_event", occurredAtMinute: 0, changes: { valenceDelta: -20, arousalDelta: 20 } });
  original.advance("lin", 120);
  const restored = new AffectRuntime();
  assert.equal(restored.restore(original.snapshot()).ok, true);
  assert.deepEqual(restored.get("lin"), original.get("lin"));
});

test("semantic presentation hides raw affect values", () => {
  const runtime = new AffectRuntime();
  runtime.initializeCharacter("lin");
  runtime.apply({ updateId: "event-3", characterId: "lin", sourceKind: "conflict_event", occurredAtMinute: 1, changes: { valenceDelta: -20, arousalDelta: 20 } });
  assert.deepEqual(runtime.describe("lin"), {
    mood: "unhappy",
    activation: "alert",
    dialogueTone: "guarded",
    behaviorDisposition: "normal",
    recentCause: "conflict_event",
  });
});

test("frequent events do not starve decay", () => {
  const runtime = new AffectRuntime();
  runtime.initializeCharacter("lin");
  runtime.apply({ updateId: "event-0", characterId: "lin", sourceKind: "conflict_event", occurredAtMinute: 0, changes: { valenceDelta: -20, arousalDelta: 20 } });
  runtime.apply({ updateId: "event-30", characterId: "lin", sourceKind: "social_event", occurredAtMinute: 30, changes: {} });
  const result = runtime.apply({ updateId: "event-60", characterId: "lin", sourceKind: "social_event", occurredAtMinute: 60, changes: {} });
  assert.equal(result.ok, true);
  assert.equal(result.state.valence, -18);
  assert.equal(result.state.arousal, 56);
});

test("restore is atomic and policy values are configurable", () => {
  const runtime = new AffectRuntime({ sourceCaps: { conflict_event: { valence: 3 } }, presentation: { unhappyAt: -2 } });
  runtime.initializeCharacter("lin");
  runtime.apply({ updateId: "event", characterId: "lin", sourceKind: "conflict_event", occurredAtMinute: 0, changes: { valenceDelta: -99 } });
  assert.equal(runtime.get("lin")?.valence, -3);
  assert.equal(runtime.describe("lin")?.mood, "unhappy");
  const before = runtime.get("lin");
  const bad = { schemaVersion: 1, characters: { lin: before, broken: { characterId: "broken" } } };
  assert.equal(runtime.restore(bad).ok, false);
  assert.deepEqual(runtime.get("lin"), before);
});
