# My AI Town Affect Core for Unreal Engine

这是 Unreal Engine 5 的 Runtime 插件适配器。它使用与 TypeScript、Unity 适配器相同的更新来源上限、重复事件去重、60 分钟衰减，以及“内部数值、外部语义标签”的规则。

## 自动接入 Actor 标签（推荐）

这是对普通 UE 角色 Actor 的可选桥接层。给角色挂一次 `AffectProfileComponent`，随后由玩法或蓝图给该角色新增一个明确的 Actor Tag；组件会匹配规则并自动写入有限幅的情感更新。该路径不需要手动调用 `ApplyEvent`。

1. 给需要情感状态的角色 Actor 添加 `AffectProfileComponent`。
2. 在 **Project Settings → My AI Town Affect** 配置全局标签规则，或直接在组件的 **Local Actor Tag Rules** 配置角色专属规则。
3. 事件发生时添加标签，例如 `Affect.Event.Conflict`。

示例规则：`Affect.Event.Conflict` → `ConflictEvent`、效价 `-12`、唤醒 `+16`。运行时仍会按来源类型限幅，防止配置错误造成异常跃迁。

游戏开始时角色已有的标签只作为初始状态；仅之后新增的标签会产生更新。移除后再次添加同一标签，才代表一次新的事件。默认每 0.5 秒检查一次，可按组件覆盖。`OnPresentationChanged` 和 `GetCurrentPresentation` 只提供 `unhappy`、`alert`、`guarded` 这类语义标签，不向蓝图 UI 暴露原始 MVU 数值。

`AffectWorldSubsystem` 会登记活跃角色，使其他系统按角色 ID 查询其语义状态。它不会扫描项目资产、对话、任务、记忆或任意文本，也绝不会调用 AI 模型。

## 安装

将整个 `unreal` 文件夹复制到你的 UE 项目：

```text
YourProject/Plugins/MyAiTownAffectCore/
```

其中 `MyAiTownAffectCore.uplugin` 必须位于该文件夹根目录。关闭并重新打开 Unreal Editor；出现“需要重新编译插件”时选择 **Yes**。已在 UE 5.7 验证；UE 5.3–5.6 尚未实际验证，因此不承诺兼容性。插件使用标准 Runtime C++ 模块，不依赖第三方库。

## 使用原则

- 使用 `UAffectRuntime` 保存角色内部状态；创建它后先调用 `InitializeCharacter`。
- 发生事件时调用 `ApplyEvent`，传入稳定且唯一的 `UpdateId`。
- 游戏时间推进时调用 `AdvanceTime`。
- UI、台词与行为蓝图只调用 `Describe`，或读取上述两个函数输出的 `FAffectPresentation`。

`FAffectPresentation` 仅含四个文字标签：`Mood`、`Activation`、`DialogueTone`、`BehaviorDisposition`，例如 `unhappy / alert / guarded / normal`；不会把效价或唤醒值暴露给蓝图 UI。原始状态只提供给 C++ 存档层的 `GetInternalState` 与 `CreateSnapshot`。

## 验证插件是否生效

1. 打开项目，确认 Output Log 中没有 `MyAiTownAffectCore` 的编译或加载错误。
2. 进入 **Edit > Plugins**，搜索 `My AI Town Affect Core`，确认插件已启用；重启编辑器一次。
3. 打开 **Tools > Test Automation**，搜索 `MyAiTown.AffectCore.Runtime.SemanticProjection`，运行该测试。通过表示事件限幅、重复去重和语义投影均可工作。
   搜索 `MyAiTown.AffectCore.Integration.ActorTagBridge`，可额外验证角色新增 Actor Tag 时无需手动调用 `ApplyEvent` 也会更新语义状态。
4. 在一个测试蓝图中创建 `Affect Runtime` 对象，依次调用：
   - `Initialize Character("lin", 0, 40, 0)`；
   - `Apply Event("demo-1", "lin", Agent Judgment, 1, "argument", -99, 99)`；
   - 将输出的 `Mood`、`Activation`、`Dialogue Tone` 打印到屏幕。

预期显示 `unhappy`、`alert`、`guarded`、`normal`；不会显示 `-15` 或 `55` 等内部数值。再次以同一 `UpdateId` 调用事件，输出应保持不变。

## 未验证边界

验证证据：本插件曾在 UE 5.7 通过 UnrealHeaderTool、C++ 编译和 Automation Test Runner。公开仓库不包含机器路径或构建日志；请在目标 UE 版本按第 1 至 3 步重跑，作为该项目的兼容性证据。
