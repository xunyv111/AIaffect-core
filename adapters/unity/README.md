# My AI Town Affect Core for Unity

这是 `@my-ai-town/affect-core` 的 Unity C# 适配器。它只负责确定性的数值状态机：情绪正负值（`valence`）、唤醒值（`arousal`）、来源上限、幂等去重、时间衰减与快照恢复；它不读取 NPC 记忆、不调用模型、不操纵场景对象。

## 安装

在 Unity 打开 **Window > Package Manager**，点击左上角 **+**，选择 **Add package from disk...**，选择本目录的 `package.json`：

`packages/affect-core/adapters/unity/package.json`

支持 Unity 2021.3 及更新版本。包不依赖第三方运行时库。

## 最小调用

```csharp
var affect = new AffectRuntime();
affect.InitializeCharacter("lin", baselineValence: 0, baselineArousal: 40, nowMinute: 0);

var result = affect.Apply(new AffectUpdate {
    updateId = "event-42:agent-affect",
    characterId = "lin",
    sourceKind = "agent_judgment",
    occurredAtMinute = 12,
    causeEventId = "event-42",
    changes = new AffectChanges { valenceDelta = -4, arousalDelta = 5 }
});
```

允许的 `sourceKind` 与 TypeScript 参考实现一致：`conversation_reply`、`social_event`、`conflict_event`、`environment_event`、`agent_judgment`、`system`。每个来源会将单次变化限制在既定范围内，`updateId` 重复时不会重复结算。

`AffectJsonAdapter.TryApplyUpdateJson` 可接收扁平 JSON 事件。为了让 JSON 输入没有“字段缺失被当成 0”的歧义，乐观并发字段只提供给强类型调用：设置 `hasExpectedRevision = true` 与 `expectedRevision`。

## 不向玩家暴露数值

`Describe(characterId)` 只返回 `AffectPresentation` 标签，不返回任何数值：`mood`、`activation`、`dialogueTone`、`behaviorDisposition`。当前标签如 `unhappy`、`alert`、`guarded`、`seek_space` 应由游戏映射为本地化文本、台词、动画或行为权重。数值仅在 `AffectRuntime` 与存档 DTO 内部结算；不要把 `Get()` 的结果直接绑定到玩家 UI。

## 存档边界

`CreateSnapshot()` 返回可深拷贝的 Unity DTO。Unity 自带 `JsonUtility` 不支持 `Dictionary`，因此 DTO 的 `characters` 是数组；若存档需要严格匹配上层 `schema/affect-core.schema.json` 的 `{ "characters": { "id": state } }` 结构，请在项目自己的存档层将数组按 `characterId` 映射成对象。这样不会把 JSON 库强加给游戏运行时。

## 验证

导入后，在 **Window > General > Test Runner** 运行 `MyAiTown.AffectCore.Unity.Tests`。两个测试覆盖：来源上限与重复事件去重，以及快照恢复后的 120 分钟确定性衰减。

当前发布边界：测试源已随包提供，但本次验证机器未安装 Unity Editor，因此无法执行真实 Unity Test Runner，也不能声称 Unity 测试已通过。首次用于具体项目时，应在目标 Unity 版本运行上述测试并记录结果。
