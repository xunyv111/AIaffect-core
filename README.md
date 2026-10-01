# Affect Core

`@my-ai-town/affect-core` 是引擎无关的 TypeScript 参考实现。它只处理 JSON 状态，不依赖 Godot、Unity、虚幻、数据库、网络或 AI 模型。

## 合同

- `schema/affect-core.schema.json` 是跨引擎 JSON 合同；
- `src/index.ts` 是可编译的参考实现；
- 每个角色只有效价 `-100..100` 和唤醒度 `0..100`；
- 新事件会先结算截至事件时刻的衰减；宿主仍应按游戏时钟定期调用 `advance()`，使空闲角色持续回归基线；
- 快照为普通 JSON，可直接写进任意引擎的存档系统。

```ts
import { AffectRuntime } from "@my-ai-town/affect-core";

const affect = new AffectRuntime();
affect.initializeCharacter("lin-lan");
affect.apply({
  updateId: "decision-42:agent-affect",
  characterId: "lin-lan",
  sourceKind: "agent_judgment",
  occurredAtMinute: 120,
  changes: { valenceDelta: -4, arousalDelta: 5 },
});
```

## 使用于其他引擎

Unity、虚幻或网页游戏只需：在角色创建时初始化、在游戏事件发生时调用 `apply`、按游戏时钟调用 `advance`、在存档时写入 `snapshot()`。每个引擎自己决定如何生成事件与展示状态。UE 适配器另提供可选的 `AffectProfileComponent`：它将角色 Actor 新增的明确标签映射成事件，无需手写 `ApplyEvent` 调用；它不会扫描文本、资产、记忆或调用模型。

## 隐藏数值的表现层

游戏的对话、行为与 UI 不必读取或展示 `valence`、`arousal`。调用 `describe(characterId)` 会得到稳定的非数值标签：`mood`、`activation`、`dialogueTone` 与 `behaviorDisposition`。例如角色经历冲突后可返回 `unhappy / alert / guarded / normal`。项目只需将这些代码映射为本地化文本、台词策略、动画或行为权重；底层数值仍只在状态机与存档层使用。

## 验证与发布状态

- TypeScript 核心：运行 `npm run test`。
- Unreal Engine 5.7：已真实编译；自动化测试 `MyAiTown.AffectCore` 已通过，覆盖核心语义投影和 Actor 标签自动接入。
- Unity：包含 Unity Test Runner 测试源文件；本次验证机器未安装 Unity Editor，因此尚未在真实 Editor 中执行，公开发布时应如实保留这一边界。

本项目采用 [MIT License](LICENSE)。版本变更见 [CHANGELOG.md](CHANGELOG.md)。构建与测试需要 TypeScript：`npm run test`。
