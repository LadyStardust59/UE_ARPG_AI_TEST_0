# AURA × GPT-6 × DSH 协作开发方案
## —— 用自然语言把「中速博弈 ARPG」的战斗系统落进 UE5.8

> 配套：`战斗系统设计文档.md`（设计真值源）、`img/13_ai_workflow_architecture.png`（架构与验证门）、`img/14_prompt_pipeline.png`（提示词结构与流水线）、`提示词库.md`（可直接复制的 10 组提示词）

---

## 0. 先说结论：提示词不是关键，契约才是

大多数团队用 AI 做游戏失败，不是因为提示词写得不好，而是因为**把 AI 当成了一个需要被"说服"的人**。它其实是一个**需要接口文档的执行器**。

所以这套方案的地基是：

```
              策划案的数值（人写）
                     ↓
        Contract Pack（机器可读的 YAML / 表 / 标签册）  ← 唯一真值源
                     ↓
        提示词（只写"引用哪一条"，不重复任何数字）
                     ↓
     AI 生成代码 / 资产 → 硬门（编译·测试·预算）→ 软门（手感·可读性）
                     ↓
              结论回写成新的契约（闭环）
```

**四条不可违反的原则：**

| # | 原则 | 为什么 |
|---|---|---|
| 1 | **数值不进提示词，进契约包** | 数值一旦被抄进对话，就产生了第二个真值源，之后必然漂移 |
| 2 | **一次只改一个子系统** | 一个提示词 = 一个可编译、可测试、可回滚的增量；否则失败无法归因 |
| 3 | **生成者 ≠ 验证者** | Aura Verification Agent / DSH 子代理 / 人 三者互相复核，避免同源放大错误 |
| 4 | **不接受"已完成"，只接受"证据"** | 日志 / 测试输出 / 帧数采样 / 截图 —— 没有证据就是没做完 |

---

## 1. 工具分工：谁负责什么

先把三个 AI 的角色分清楚，否则会出现"两个 AI 同时改一个文件"的灾难。

| 工具 | 强项 | 在本项目中的职责 | 不要让它做 |
|---|---|---|---|
| **Aura（多智能体）** | 直接驱动 UE 编辑器：Python 批量、蓝图图表、材质、关卡灰盒、**Verification Agent 能自己跑游戏抓 bug**、Sandbox Mode 隔离改动 | P1/P2/P5/P7/P8 的资产生成与校验；编辑器内验收 | 定数值、定手感、改 GDD |
| **GPT-6（对话式强推理）** | 长上下文推理、架构设计、C++ 与 GAS 的复杂逻辑、代码审查、把策划语言翻译成契约 | P0 契约包起草、P3 玩家能力集、P6 AI 评分模型的代码、跨系统接口设计 | 批量重复操作（浪费额度）、直接改二进制资产 |
| **DSH（本环境）** | 多子代理并行、跨文件重构、大批量审计、跑命令与测试 | 并行推进互相独立的模块；对 Aura/GPT-6 的产出做独立审计；硬门执行 | 替代编辑器内的资产操作 |
| **UE5.8 Unreal MCP** | 编辑器进程内 MCP server，任何 MCP 客户端可调用；**可自研 Toolset** | 把本项目的重复操作固化成自定义 Tool（见 §6） | 直接暴露给不可信网络（仅本机回环） |

> 关于 Aura 的代理构成（Dragon/Python、Telos/Blueprint、Material Agent、Level Design Agent、Verification Agent、Aura Skills）见 [Aura 15.0 发布说明](https://www.gamespress.com/en-GB/Aura-150-Releases-with-New-Features-and-Unlimited-Usage-for-Unreal-Eng)；Unreal MCP 的启用、`.mcp.json` 生成与 Toolset 编写见 [Unreal MCP 官方文档](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-mcp-in-unreal-editor)。

---

## 2. 第 0 步（最重要）：把策划案变成 Contract Pack

在写第一行代码之前，先花 1~2 天做这件事。它决定了后面所有提示词能不能短、能不能稳。

建议目录结构：

```
<ProjectRoot>/
├─ Docs/
│  ├─ 战斗系统设计文档.md          # 人读的真值源（已有）
│  └─ CombatGDD.yaml               # 机器读的真值源 ★核心
├─ Config/AI/
│  ├─ naming_registry.yaml          # 命名注册表（AI 查表用，不许猜）
│  ├─ tag_registry.yaml             # GameplayTag 全量清单
│  ├─ ability_registry.yaml         # 能力 ↔ Cue ↔ SetByCaller 对照
│  └─ forbidden_zones.yaml          # AI 禁改区清单
├─ Tools/                          # 自定义 MCP Toolset（Python）
├─ Tests/                          # 自动化测试与验收脚本
└─ Content/                        # 按命名注册表的目录结构
```

### 2.1 CombatGDD.yaml 示例（直接把设计文档翻译过来）

```yaml
meta:
  project: 契·三身
  engine: "5.8"
  schema_version: 1
  truth_source: Docs/战斗系统设计文档.md   # 人读版；本文件是机读版，二者必须一致

player:
  attributes:
    AP:        { max: 100, regen_per_sec: 9, on_hit_gain: 6, dodge_cost: 20, charge_cost: 35 }
    Stamina:   { max: 100, on_normal_hit: 4, on_perfect_parry: 12, on_combo_finisher: 20 }
    Poise:     { max: 100, regen_per_sec: 5, on_hit_loss: 8, break_stun_sec: 2.0 }
    Resolve:   { max: 3, gain_on_hit_taken: 1, gain_on_perfect_parry: 1, per_point_regen_sec: 20 }
    Emotion:   { max: 100, transform_cost: 50 }
  exhaust_penalty: { move_speed_pct: -35, cannot_dodge: true, damage_taken_pct: 25 }

frames:                       # 60fps 基线
  light_attack:  { startup: 7,  active: 4, recovery: 12, cancel_window: 12 }
  heavy_attack:  { startup: 22, active: 6, recovery: 30, cancel_window: 0 }
  charged_slash: { startup: 40, active: 8, recovery: 34, cancel_window: 40 }
  dodge:         { startup: 3,  iframe: 11, recovery: 30 }
  parry_perfect: { startup: 1,  active: 6,  recovery: 10, counter_bonus_frames: 12 }

enemy_counter_gauge:          # 反制值
  grunt:  { max: 60,   break_count_to_vulnerable: 1 }
  humanoid_boss: { max: 600, break_count_to_vulnerable: 2, first_full_kneel_sec: 0.8 }
  colossal_torso: { max: 1200, break_count_to_vulnerable: 2 }
  gains: { per_hit: 3, on_perfect_parry: 18, on_interrupt: 25, on_execution: "reset_to_0" }
  decay_per_sec: 2
  suppressed_decay_modifier: 0.6      # 被压制时衰减减半（攻势更耐久）

tempo_bands:
  neutral:  { duration_sec: [3, 8] }
  pressure: { duration_sec: [4, 12], stack_max: 5 }
  read:     { duration_sec: [2, 6] }
  reversal: { duration_sec: [1.5, 4] }
  decide:   { duration_sec: [3, 6] }
  anti_snowball:
    force_disengage_sec: 1.2
    pressure_stack_threshold: 4
    pressure_attacker_ap_regen_pct: -25
    resolve_escape_cost: 1

execution: { damage_pct_max_hp: [8, 12], window_sec: [2, 4] }

swap:
  combo:   { cost_ap: 15, cd_sec: 18 }
  counter: { cd_sec: 100, window_pct_of_startup: 60, bullet_time_sec: 0.35, fail_damage_pct: 150 }
  assist:  { cd_sec: 25, ult_gauge_cost_pct: 50 }

vulnerability_telegraphs:      # 预兆色板（唯一映射，全项目不得新增）
  red:    { type: heavy,        blockable: true }
  yellow: { type: multi_hit,    blockable: true }
  gold:   { type: grab,         blockable: false }
  white:  { type: dash_thrust,  blockable: true }
  purple: { type: mirror_art,   blockable: true }
  cyan:   { type: domain_summon,blockable: false }
  gold_purple: { type: awakening, blockable: false }
```

**关键点**：这份 YAML 一旦生成，就要写一个 `Tools/validate_gdd.py`，让它在每次提交时校验 —— 设计文档里的数字、YAML 里的数字、代码里的常量，三者必须一致（这就是硬门 C）。

---

## 3. 八段式提示词结构

每一段都不可省略，顺序固定。权重（影响 AI 决策的比重）标在图 `14` 上。

| 段 | 作用 | 缺了会怎样 |
|---|---|---|
| **1 角色与目标** | 限定身份 + 本轮唯一交付物 | AI 顺手重构半个项目 |
| **2 系统上下文** | 引擎版本、插件、基类、既有约定 | AI 编出不存在的 API |
| **3 契约注入** | 粘贴 Contract Pack 的相关片段 | AI 自己造一套数值体系 |
| **4 硬约束** | 禁止项清单 | AI 引入第三方依赖、改坏公共接口 |
| **5 交付物** | 文件清单 + 资产路径 + 命名 | 产物散落，无法机器校验 |
| **6 验收标准** | 可被机器判定的完成条件 | "看起来好了"就交付 |
| **7 证据要求** | 强制 AI 附验证记录 | 你无法判断它是否真的验证过 |
| **8 自检与回滚** | 上下文不足时停下提问 | AI 用占位实现糊过去 |

### 一条最重要的写法技巧

```
❌ 错误写法：
"完美格挡窗口设为 6 帧，成功时敌方架势 +18，消耗 10 点 AP。"

✅ 正确写法：
"完美格挡的所有数值以 Contract Pack §frames.parry_perfect 与
 §enemy_counter_gauge.gains 为准 —— 不要在本轮对话中重述或推导任何数字。"
```

前者会让 AI 在后续所有轮次里"记住"这组数字并可能与 YAML 漂移；后者让数值只有一个来源。

---

## 4. 十个接力任务（每步一个提示词）

顺序不可颠倒 —— 后一个任务的输入是前一个任务的产物。

| 编号 | 任务 | 主执行者 | 关键产物 | 依赖 |
|---|---|---|---|---|
| **P0** | 项目宪法与契约包 | 人 + GPT-6 | `CombatGDD.yaml`、`naming_registry.yaml`、`tag_registry.yaml`、`validate_gdd.py` | 设计文档 |
| **P1** | 骨架与插槽契约 | Aura(Dragon) + GPT-6 | 骨架校验脚本、插槽/碰撞/材质槽规范、占位网格体 | P0 |
| **P2** | GAS 数据层 | GPT-6 | `AAM_AttributeSet`、数值 GE、SetByCaller 标签表 | P1 |
| **P3** | 玩家能力集 | GPT-6（人审手感） | 移动/闪避/完美格挡/破防技/换人 五条能力 + 测试 | P2 |
| **P4** | 动画层 | Aura(Telos) + 人 | AnimBP、Montage、通知窗口、Motion Matching 配置 | P3 |
| **P5** | 材质与表现契约 | Aura(Material) | MI 参数契约、受击/破绽/异常状态表现 | P1 |
| **P6** | 敌 AI（StateTree） | Aura(Telos) + GPT-6 | 三层决策：编队层/评分层/牌组分支 + 数据表 | P2, P3 |
| **P7** | 关卡与空间语法 | Aura(Level Design) | 灰盒、危险区/契约柱/高台标记 Actor | P6 |
| **P8** | 特效与音效 | Aura + 人 | GameplayCue 全表、预兆色板、音乐分层事件 | P3, P5 |
| **P9** | 镜头与演出 | Aura + 人 | 相机绑定、顿帧、处决镜头、验证序列 | P4, P6, P8 |

> **必须在 P3 与 P6 之间插入一次"手感评审"**。理由：玩家能力集与敌 AI 是整个战斗系统的风险最高点 —— 如果"反击换人"不好玩，后面所有资产都会白做。这也是设计文档里标为第一优先级验证项的原因。

---

## 5. 验证门：把能自动化的全部自动化

完整清单见 `img/13_ai_workflow_architecture.png`。这里只强调**怎么落地**：

### 5.1 六个硬门（机器判定，不通过就阻断）

| 门 | 实现方式 |
|---|---|
| A · 编译 | `Build.bat <Project>Editor Win64 Development -WaitMutex`；解析日志中的 warning 数 |
| B · 能力单测 | UE Automation Test（`GASTest.*`），校验 Cost / Cooldown / 标签授予与移除 |
| C · 数据一致性 | `Tools/validate_gdd.py`：比对 YAML ↔ DataTable ↔ 代码常量（**最重要的一道门**） |
| D · 资产规范 | `Tools/validate_assets.py`：命名 / 目录 / LOD / 材质槽 / 碰撞预设 |
| E · 性能预算 | `stat unit` + `stat scenerendering` 自动化采样，写入门限 |
| F · 客户端表现 | Aura Verification Agent 跑测试序列 → 截图 → 与基线比对 |

### 5.2 四个软门（人判定，但要求 AI 提供"可判定的材料"）

| 门 | 人只看三件事 |
|---|---|
| 手感 | 完美格挡"按下去"是否跟手；取消窗口是否顺手；力竭是否可感知 |
| 可读性 | 关掉画面只听声音，能否判断来的是什么招（盲测） |
| 节奏 | 一场 90 秒里主动权是否至少易手 4 次 |
| 演出 | 每个视觉元素是否都承担信息功能（不承担的就删） |

> **软门的输出必须是数字或规则，不能是"感觉不太行"**。例如："完美格挡窗口从 6 帧改到 8 帧"、"预兆红色饱和度降低 15%"。这样它才能被回写成契约，AI 才能执行。

---

## 6. 进阶：自研 MCP Toolset（把你的重复操作变成 AI 的一次调用）

这是整套方案里**投入产出比最高的一步**。Unreal MCP 支持用 Python 或 C++ 注册自定义 Tool，注册后所有 AI 客户端都能调用它。

例如把"按契约创建一个 GAS 能力包"做成一个 Tool：

```python
# <Project>/Plugins/CombatAITools/Content/Python/combat_tools.py
import unreal, toolset_registry
from toolset_registry.toolsets.core.utils import require_editable  # 命名以引擎实际版本为准

@unreal.uclass()
class CombatTools(unreal.ToolsetDefinition):
    """本项目专用工具：按 Contract Pack 生成战斗资产骨架。"""

    @toolset_registry.tool_call
    @staticmethod
    def create_ability_pack(ability_name: str, pack_type: str) -> str:
        """按命名注册表与契约生成一个能力资产包（Ability + Cost GE + GameplayCue）。

        Args:
            ability_name: 能力名（不含前缀），如 "Parry"
            pack_type: 能力类型，取值 "attack" | "defense" | "swap" | "ultimate"

        Returns:
            生成的资产路径列表（换行分隔）
        """
        ...
```

把它注册后，提示词里就只需要写：

> "调用 `CombatTools.create_ability_pack('Parry', 'defense')`，然后按契约补全窗口判定逻辑。"

而不是让 AI 每次自己去猜资产该放哪、该叫什么。**AI 的自由度越低的地方，就越应该做成 Tool。**

---

## 7. 每天的实际操作节奏（推荐）

| 时段 | 做什么 | 谁做 |
|---|---|---|
| 上午 1 | 定契约：把昨天的评审结论回写成 YAML / 表 | 人 + GPT-6 |
| 上午 2 | 并行生成：3~4 个子任务同时跑（互不冲突的模块） | Aura 各代理 + DSH 子代理 |
| 下午 1 | 过硬门：编译 / 测试 / 数据一致性 / 资产规范 | 脚本 + Verification Agent |
| 下午 2 | 软门评审：自己玩 15 分钟，只记录"不顺手"的三件事 | 人（唯一裁判） |
| 收尾 | 把结论回写成契约，提交（小步提交，可回滚） | 人 |

**每日只推进 1~2 个 P 任务**。用 AI 最容易犯的错是"一天生成完半个项目"，结果无法验证、无法回滚、无法归因。

---

## 8. 版本控制与安全

| 项 | 做法 |
|---|---|
| 版本控制 | Git + Git LFS（`.uasset` / `.umap` 走 LFS）；每次 AI 生成一个独立提交 |
| 回滚 | 优先用 Aura Sandbox Mode（隔离 + 可逆）；其次 `git reset` |
| 禁改区 | `Config/AI/forbidden_zones.yaml` 列出 AI 不可修改的文件（公共接口、数值真值、他人模块） |
| 提交前 | 硬门全过 + 人签字；AI 不得直接 push 到主干 |
| 安全 | Unreal MCP 仅绑定 `127.0.0.1`，无鉴权，**绝不可暴露到局域网**；共享机器上注意端口占用 |

---

## 9. 常见失败模式与对策

| 失败模式 | 症状 | 对策 |
|---|---|---|
| **数值漂移** | 图片、文档、代码里同一个机制有三个值 | 硬门 C（三方比对脚本）；数值只进 YAML |
| **符号幻觉** | AI 编出不存在的 API / 资产路径 | 段 2 给足上下文；段 4 明令禁止；要求 AI 只引用既有符号 |
| **改坏公共接口** | 一个模块的修改让另一个模块编译失败 | 禁改区清单 + 一次只改一个子系统 |
| **同源验证** | 生成者自己说"测试通过" | 强制生成者 ≠ 验证者（Aura Verification / DSH 子代理） |
| **资产地狱** | 同一资产出现 5 个变体，没人知道哪个是对的 | 命名注册表 + 硬门 D 自动检查 + 单一真值目录 |
| **过度生成** | 一天产出巨大 diff，无法审阅 | 小步提交；每个提示词限定交付物清单 |
| **手感退化** | 每项指标都对，但玩起来不对 | 软门不可省；每天亲自玩 15 分钟 |

---

## 10. 两周启动清单（可直接照做）

| 天 | 任务 | 完成标志 |
|---|---|---|
| D1 | 装 Aura + UE5.8 Unreal MCP，生成 `.mcp.json`，跑通"当前选中了哪些 Actor" | AI 能读到编辑器状态 |
| D2 | 写 `CombatGDD.yaml` 与命名注册表（只覆盖 P1~P3 需要的部分） | YAML 通过 `validate_gdd.py` |
| D3 | 用 GPT-6 生成 `AAM_AttributeSet`（P2）+ 单测 | 硬门 A、B 通过 |
| D4~D5 | P1 骨架契约 + 占位资产 | 硬门 D 通过 |
| D6~D8 | P3 玩家能力集（含完美格挡） | 硬门 A、B、C 通过 |
| **D9** | **手感评审（半天）** | 结论回写 YAML |
| D10~D12 | P6 敌 AI（只做一个精英敌人） | 能跑通一次完整节奏五带 |
| D13 | P8 预兆特效与音效（只做 4 色） | 盲测能分辨来招 |
| D14 | 端到端：一次 90 秒的对局 + 硬门全跑 + 记录主动权易手次数 | 主动权 ≥4 次易手 → 框架成立 |

**D14 的结论决定项目走向**：如果主动权易手不足 4 次，问题在 AI 权重与数值真值，回到 D2 改契约；如果"反击换人"不好玩，问题在系统设计，需要回到设计文档重估 —— 这两件事必须在两周内知道答案。

---

## 11. 一句话总结

> **把 AI 当编译器，不当合作者。**
> 你给它契约，它给你产物；产物必须过机器门，手感必须过你自己。
> 凡是"需要它猜"的地方，都是你契约没写完的地方。
