# 战斗系统交接说明（给下一个 AI 会话 + 给人看）

> **这个文件是什么**：它是一份"记忆种子"。在 DSH 的 UE 项目工作区里开启新会话后，
> 只要让新会话读这个文件 + `Docs/Design/` 目录，它就能立刻接上之前所有的设计上下文，
> 不需要你重新讲一遍。
>
> 生成时间：2026-10-05　生成者：DSH 会话 `session-0c6854af`（工作区：default-workspace）
> 目的：把"中速博弈 ARPG 战斗系统"的设计与实施方案，交接给 UE 项目工作区。

---

## 0. 一分钟交接摘要（读完这段就能接活）

**用户**：游戏战斗策划，非程序出身，使用 UE5.8 + Aura（编辑器内多智能体 AI）+ Unreal MCP。

**已有的东西**（不要重复造）：
- UE5.8 C++ 项目 `MyCppProject`，GAS 已接入 `Build.cs`
- 一套能跑的 GAS ARPG Demo：`Source/MyCppProject/ARPG/`（属性集、攻击能力、单位、GameMode、UI、双角色切换、关卡/结算）
- UE5 官方模板 `Variant_Combat`（连击/蓄力/受击/敌人 AI，非 GAS）
- `AGENTS.md` 里已有协作规则（**必须遵守**）

**要做的事**：把"能跑的原型"升级成"有博弈深度的战斗系统"。

**最大的坑**：现有 Demo 用的是**计时器 + 冷却**模型（0.38 秒攻击间隔、`BrokenUntil`、`DodgeReadyAt`），
而新设计要求**帧精确 + 预兆读招 + 主动权节奏**。两者不是一回事，**不能混着长**。
必须先决定"改造还是并存"，再动手（详见 §3）。

**下一步动作**：见 §6。

---

## 1. 完整设计资料在哪

上次会话产出的全部设计资料，已经在 `Docs/Design/` 目录下（不需要解压）：

| 文件 | 内容 | 什么时候用 |
|---|---|---|
| `战斗系统设计文档.md` | 完整设计：资源、节奏五带、交互流程、换人、三个 Boss、AI 策略、演出、成长、GAS 实现建议 | **每次动手前读一遍相关章节** |
| `新手上路-照着做.md` | 五个里程碑（M1~M5）+ 每个里程碑可直接复制给 Aura 的指令 | 排期与下发给 Aura 时用 |
| `提示词库.md` | 10 组完整提示词（P0~P9）+ 3 条元提示词 | 让 Aura 干活时用 |
| `AI协作开发工作流方案.md` | 三层分工、验证门、日常节奏 | 需要扩展工作流时用 |
| `img/01 ~ img/12` | 12 张设计图（总览/资源流向/节奏状态机/交互流程/换人/成立性 + 三个 Boss + 三张节奏编排） | 给人看、给自己对答案 |
| `img/13 ~ img/14` | AI 协作架构图、提示词流水线图 | 同上 |
| **`路线C-完美格挡切片.md`** | **已选定的路线 C 实施方案：三步走 + 可直接复制给 AURA 的指令** | **下次会话从这里开始** |
| **`../Git使用说明.md`** | 给非程序用户的 Git 说明与故障对照表 | 用户问版本控制时 |
| **`../../Scripts/backup.bat`** | 一键存档工具（双击即用） | 每次大改动前提醒用户跑一次 |
| **`../../Scripts/rollback.bat`** | 一键读档工具（回退前自动 rescue） | 改坏了的时候 |

### 用户已做的决定

**已选定路线 C（完美格挡垂直切片）**，并已授权把第一版 Demo 提交到 Git。
详细实施方案见 `路线C-完美格挡切片.md` —— **新会话的第一件事是读它，而不是读本文档 §3**。

**核心设计一句话**：中速博弈 —— 敌我同构的资源模型 + 主动权轴 + 节奏五带
（对峙 → 压制 → 守势解算 → 反转 → 终结）+ 三人换人 + 读招（每个敌招唯一预兆、至少两个正确解）。

---

## 2. 现有代码的准确现状（已核实，不是猜的）

### 2.1 已确认是 C++ 项目 ✅

| 证据 | 位置 |
|---|---|
| 模块声明 | `MyCppProject.uproject` → `"Modules": [{"Name": "MyCppProject", ...}]` |
| 构建规则 | `Source/MyCppProject/MyCppProject.Build.cs` |
| 目标文件 | `Source/MyCppProject.Target.cs`、`MyCppProjectEditor.Target.cs` |
| 解决方案 | `MyCppProject.sln`（有它说明 IDE 工程已生成过） |

**结论：不需要做"纯蓝图项目转 C++"。这件事已经完成了。**（详见下面的 FAQ）

### 2.2 插件配置（`.uproject`）

已启用：`StateTree`、`GameplayStateTree`、`GameplayAbilities`、`Aura`、
`ModelContextProtocol`、`AllToolsets`、`MCPClientToolset`、`ModelingToolsEditorMode`

**含义**：
- GAS ✅ 可用
- StateTree ✅ 可用（AI 应该用它，不要用 Behavior Tree）
- **Aura + Unreal MCP ✅ 已配置**，AI 可以直接驱动编辑器
- `.codex/config.toml` 里已把 MCP 指向 `http://127.0.0.1:8000/mcp`（给 Codex CLI 用；Aura 自己走自己的连接）

### 2.3 现有 ARPG 代码的真实结构

`Source/MyCppProject/ARPG/ARPGDemo.h`（218 行，一个文件里塞了 7 个类）：

| 类 | 作用 | 备注 |
|---|---|---|
| `UARPGAttributes` | 属性集：`Health` / `MaxHealth` / `Energy` / `Guard` | 只有 4 个属性 |
| `UARPGHealthEffect` / `UARPGEnergyEffect` / `UARPGGuardEffect` | 三个数值 GE | |
| `UARPGHeroDefinition` | **DataAsset**：英雄名、`bHeavyStyle`、`MaxHealth=220`、`WalkSpeed=620`、`AttackDamage=24`、`AttackInterval=0.38`、`Accent` | ✅ 这个设计很好：策划可在编辑器里调，不用改代码 |
| `EARPGAttack` | 枚举：`Light` / `Heavy` / `Burst` / `Dodge` | 只有 4 种动作 |
| `UARPGAttackAbility` | 一个能力类搞定所有攻击，靠 `EARPGAttack` 分支 | 与 GAS "一能力一职责" 的常规做法不同 |
| `AARPGUnit` | 玩家和敌人共用的单位类，`bEnemy` / `bElite` 开关 | 含 `Combo` / `ChargeStarted` / `BrokenUntil` / `BurstReadyAt` / `DodgeReadyAt` |
| `AARPGGameMode` | 主菜单、关卡、双英雄切换（`SwitchHero`）、精英计数、胜负结算 | ✅ 已完成 |
| `AARPGPlayerController` | 输入绑定：Light/Charge/Release/Burst/Dodge/Switch/Jump | |
| `UARPGScreen` | UMG 界面：状态、生命条、能量条、结算面板、菜单按钮 | |

`Scripts/create_arpg_assets.py` —— 建资产的 Python 脚本（3.5KB）
`Docs/Conversation.md` —— 上次会话的完整对话记录（17KB，很值得读）
`Content/ARPG/`、`Content/Anim/`、`Content/GameplayAbility_Test/` —— 未提交的新资产

### 2.4 Git 状态（⚠️ 有风险）

```
最近提交：3cd52f6 backup: sanitized UE 5.8 baseline for public repository
标签：baseline-before-arpg、public-baseline-before-arpg
未提交的改动：
  M  Config/DefaultEngine.ini
  M  MyCppProject.uproject
  M  Source/MyCppProject/MyCppProject.Build.cs
  ?? AGENTS.md / Config/DefaultGameplayTags.ini / Content/ARPG/ / Content/Anim/
  ?? Content/GameplayAbility_Test/ / Docs/ / Scripts/ / Source/MyCppProject/ARPG/
```

**整个 ARPG Demo（代码 + 资产 + 文档）都还没提交。**
只存在于工作区里。磁盘坏了或误操作就全没了。

**第一件该做的事：让用户确认后提交一次。**

---

## 3. ⚠️ 必须先解决的设计冲突

现有 Demo 和新区设计**不是同一个模型**。直接往上加会变成一锅粥。

| 维度 | 现有 Demo（Codex 版） | 新设计（中速博弈版） | 冲突程度 |
|---|---|---|---|
| 时间粒度 | 秒（`AttackInterval = 0.38f`）、`FTimerHandle` | 帧（60fps 基线，如轻击起手 7 帧） | 🔴 高 |
| 资源 | `Health` / `Energy` / `Guard` | 生命 VP / 行动值 AP / 气力 / 韧性 / 决意条 / 情绪 + 敌方反制值 / 体力 / 霸体 | 🔴 高 |
| 防御 | 没有格挡/完美格挡 | **完美格挡 6 帧窗口**是系统心脏 | 🔴 高 |
| 敌人 | `UpdateEnemy()` 计时器驱动，冷却到了就出招 | 三层决策（编队/评分/牌组）+ 预兆色板 + 出牌不重复 | 🔴 高 |
| 换人 | 只有 `SwitchHero()`（二选一，无时机差别） | 三种换人（连携/反击/援助），按**时机**区分结果 | 🟡 中 |
| 能力组织 | 一个 `UARPGAttackAbility` 分支处理 4 种攻击 | 一能力一职责 | 🟡 中 |
| 命名 | `AARPGUnit` / `ARPG` 命名空间 | 设计文档里叫 `AAM_CombatCharacter` | 🟢 低（改名即可） |

### 三条可选路线（**必须让用户选一条，不要自己决定**）

**路线 A · 改造（推荐给"想保留现有 Demo 观感"的情况）**
- 保留 `AARPGGameMode`（关卡/菜单/结算很好，别动）和 `UARPGHeroDefinition`（数据驱动很好）
- **重写** `AARPGUnit` 的战斗部分：把计时器换成帧驱动的 `UAbilityTask`
- **拆分** `UARPGAttackAbility` 为独立能力：`GA_LightAttack` / `GA_HeavyAttack` / `GA_Dodge` / `GA_Parry`
- 属性集**扩展**而非替换：新增 `AP` / `Stamina` / `Poise` / `Resolve` / `Emotion`，保留 `Health`
- 敌人 AI 从 `UpdateEnemy()` 换成 StateTree
- 代价：中期会有一段"半新半旧"的混乱期，需要 2~3 天
- 收益：不丢已有成果，用户能持续看到进展

**路线 B · 并存（推荐给"想快速验证新玩法"的情况）**
- 现有 Demo 原样保留（当作可玩的对照版本）
- 新建 `Source/MyCppProject/Combat/` 与 `Content/Combat/`，按新设计从零做一套
- 用一个新的 GameMode 切换两套系统
- 代价：两套代码并行，资产体积翻倍，长期要合并
- 收益：新设计不被旧代码污染，验证最快（1~2 天能跑起来）

**路线 C · 只做垂直切片（推荐给"想先确认好不好玩"的情况）**
- 完全不动现有项目
- 只做设计文档里的 **M2（格挡 + 完美格挡）** 一个功能，用最小的方式加进去
- 目的：**先回答"完美格挡手感对不对"这一个问题**
- 如果手感对 → 再决定 A 还是 B；如果不对 → 整个设计要重估，省下几周
- 代价：看起来"进度慢"
- 收益：**风险最低**，这是我在设计方案里标为"最高风险点"的那件事

> **我的建议：路线 C**。理由：设计文档里明确写了"P3 玩家能力集与 P6 敌 AI 是风险最高点，
> 如果反击换人不好玩，整个框架要重估"。先用最小成本买这个答案，再决定大方向。

---

## 4. 如果用户选了路线，下一步怎么做（按路线展开）

（见 §6 的"下一步动作"，那里是可直接执行的清单）

---

## 5. 协作规则提醒（来自项目 `AGENTS.md`，必须遵守）

1. 用中文沟通，用户是策划
2. **每个新需求：先只读分析 → 列出步骤/受影响文件/风险/验证方法 → 用户确认后才执行**
3. 每批修改前问是否可以 Git 备份
4. 所有对用户可见的自然语言对话追加到 `Docs/Conversation.md`（不含工具原始输出与内部推理）
5. 新功能优先放在 `Source/MyCppProject/ARPG` 与 `Content/ARPG`，**保留原模板**
6. 每次交付要说明：新增功能、验证结果、限制、Git 提交与回退方法
7. **不得把"编译通过"称为"完整游戏测试通过"**
8. Unreal MCP 工具调用**串行执行**（不要并发调用）
9. 仓库公开，不得上传令牌/密码/本机配置；不要 `push --all` 或 `push --tags`

---

## 6. 下一步动作（给接手会话的第一件事）

**不要直接开始写代码。** 按顺序做这三件事：

### 第一步：提交现有成果（保命）

向用户说明：整个 ARPG Demo 未提交，有丢失风险。请求授权提交：

```
是否现在把现有的 ARPG Demo 提交一次？建议信息：
"feat: 第一版 GAS ARPG Demo（双角色切换 / 连击 / 蓄力 / 破防 / 范围技 / 关卡结算）"
```

### 第二步：把 §3 的三条路线摆给用户选

用大白话解释，不要用本文档的术语。例如：

> 现在项目里有一个能跑的 Demo（Codex 做的），但它用的是"冷却计时"那套老办法。
> 新设计要的是"帧精确 + 读招"。这三条路你选一条：
> - A：把老的改造成新的（能保留现有内容，但中间会乱两三天）
> - B：新的单独做一套（最快看到新玩法，但有两套代码）
> - C：**先只做一个"完美格挡"试试手感**（1~2 天，风险最低，我推荐这个）

### 第三步：根据选择，读对应章节后给方案

- 选 C → 读 `新手上路-照着做.md` 的 M2，但**要把里面的命名改成适配现有代码**
  （现有是 `AARPGUnit`，不是 `AAM_CombatCharacter`；属性是 `Guard` 不是 `Poise`）
- 选 A → 先读 `战斗系统设计文档.md` §3、§4，再出重构方案
- 选 B → 读 `新手上路-照着做.md` 全部，按 M1→M5 走

---

## 7. FAQ：两个用户问过的问题（答案留档）

### Q1：对话能不能跨工作区带过来？

**不能自动带。** DSH 的会话按工作区目录隔离存放：

```
C:\Users\Administrator\.dsh\sessions\<工作区路径编码>\<会话ID>\session.v4.jsonl.zstd
```

例如：
- 上次设计会话：`...\--C-Users-Administrator-Documents-deepseek-harness-default-workspace--\`
- 本项目工作区：`...\--D-UE_Project-MyCppProject--\`

**正确做法**（也就是本文件存在的理由）：
把上下文写成项目里的文件，新会话一读就有记忆。
`AGENTS.md` 里已经规定"不得声称保存了不可访问的其他会话" —— 本文件遵守这条。

### Q2：这是不是 C++ 项目？纯蓝图项目能不能转？

**这已经是 C++ 项目**（见 §2.1 的四项证据）。不需要转换。

**顺带回答"纯蓝图能不能转"**（以后可能用得上）：
- 判断方法：项目根目录有 `Source/` 和 `.sln` 就是 C++；只有 `Content/` 就是纯蓝图
- 转换方法（官方支持）：UE 编辑器 → `Tools` → `New C++ Class` → 按提示走，
  编辑器会自动创建 `Source/`、模块声明和构建文件
- 前提：装好 Visual Studio 2022 + "使用 C++ 的游戏开发"工作负载 + Windows SDK
- 注意：**转换不可逆**（会往 `.uproject` 写 `Modules`），转换前必须 Git 备份
- 对 Aura 的影响：Aura 的 Unreal MCP 自定义 Toolset 用 Python 或 C++ 都行，
  但**涉及引擎反射（USTRUCT / UFUNCTION）的自定义 Tool 必须用 C++**。
  本项目已是 C++，这一步没有障碍。

---

## 8. 关键设计数值速查（从设计文档摘出，供快速对照）

```
玩家：生命 VP / 行动值 AP（上限 100，+9/s）/ 气力 100 / 韧性 100 / 决意条 0~3 / 情绪 100
帧数据（60fps）：轻击 7/4/12　重击 22/6/30　蓄力斩 40+/8/34
                闪避 3f起手+11f无敌/30f收招（耗 AP 20）
                完美格挡 1f起手 + 6f窗口 + 10f收招
敌方反制值上限：杂兵 60（一击即破）/ 人形 Boss 600 / 巨型躯干 1200
反制值增益：普攻 +3　完美格挡 +18　打断起手 +25　处决后清零
破绽：硬直 2~4s，处决伤害 8~12% 最大生命
节奏五带：对峙 3~8s → 压制 4~12s → 守势解算 2~6s → 反转 1.5~4s → 终结 3~6s
换人冷却：连携 18s / 反击 100s / 援助 25s
预兆色板（7 类，唯一映射）：
  红=重击可格挡　黄=多段快攻　金=投技不可格挡　白=直线突进
  紫=镜面术　青=领域召唤　金紫=觉醒全场
```

---

## 9. 这张交接单的使用方式

**给人看**：读 §0 和 §3，做路线选择。

**给 AI 看**：把这句话连同本文件路径一起发给新会话：

```
请先读 Docs/Design/HANDOFF.md 和 Docs/Design/ 下的资料，
然后按 HANDOFF.md §6 的"下一步动作"执行 —— 先不要写代码，先向我汇报。
```

---

*本文件由 DSH 会话生成，内容已核实（读取过项目文件与 Git 状态）。*
*如与项目实际不符，以项目实际为准，并请更新本文件。*
