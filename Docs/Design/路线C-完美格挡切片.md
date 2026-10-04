# 路线 C · 完美格挡垂直切片——实施方案

> **目标**：用最小代价回答一个问题 —— **"6 帧完美格挡窗口，玩起来爽不爽？"**
>
> 回答"爽" → 整个战斗系统框架成立，继续往下做。
> 回答"不爽" → 只损失 1~2 天，而不是两周。
>
> 前置阅读：`Docs/Design/HANDOFF.md`（项目现状与冲突分析）、`战斗系统设计文档.md` §3.2 与 §9

---

## 一、这个切片到底做什么

**只做一件事**：给现有的 `AARPGUnit` 加一个"格挡键"。

| 行为 | 结果 |
|---|---|
| 按住格挡键 | 举盾状态，受到伤害 −70%，每次挡下消耗 8 点体力 |
| 在敌人攻击命中前 **6 帧**内按下 | **完美格挡**：完全免伤 + 敌人硬直 0.4 秒 + 敌人"踉跄" |
| 完美格挡后 **12 帧**内的下一击 | 伤害 ×2（这就是"反打"） |
| 体力不足 | 格挡失效，进入"力竭"3 秒（移速 −35%、不能闪避、受创 +25%） |

**不做**：动画、特效、音效、UI、换人、Boss、StateTree。
这些都要等"手感确认"之后再补。

---

## 二、为什么不用新设计的命名

设计文档里写的是 `AAM_CombatCharacter` / `AP` / `Poise`。
但你的项目里**已经存在** `AARPGUnit` / `Energy` / `Guard`。

**路线 C 的原则是"最小改动"，所以适配现有命名，不引入第二套词汇。**

| 设计文档叫法 | 本项目实际用 |
|---|---|
| 行动值 AP | `Energy`（已存在，上限 100） |
| 韧性 Poise | `Guard`（已存在） |
| 完美格挡窗口 | 新增到 `DT_CombatNumbers` |

---

## 三、三步走（每步都能单独验证、单独回退）

### 第 1 步：只做"判定"，不做表现（半天）

新增格挡判定 + 数据表 + 自动化测试。**不接动画、不接输入**。
这一步做完，你就能在测试里确认"6 帧窗口"在代码层面是对的。

**产物**：
- `DT_CombatNumbers`（数据表，所有数字唯一来源）
- `UARPGAttributes` 增加 `Stamina`、`ParryWindowEnd`
- `AARPGUnit::RequestParry()` / `IsPerfectParry()` / `ResolveParry()`
- `Source/MyCppProject/ARPG/ARPGParryTest.cpp`（自动化测试）

**验收**：
- 编译零新增警告
- 自动化测试通过：窗口内判完美、窗口外判普通、体力不足进力竭

---

### 第 2 步：接输入 + 接敌人攻击（半天）

把格挡接到按键上，并让敌人的攻击能触发格挡判定（用现有 `UpdateEnemy()` 的攻击流程）。
表现只用最简的：角色颜色变化 + 日志。

**产物**：
- `AARPGPlayerController::Parry()` 输入绑定
- `AARPGEnemy` 的攻击命中时调用格挡判定
- 三个属性条接入现有的 `UARPGScreen`（体力 / 格挡状态）

**验收**：
- 你亲自按住格挡键，能挡住敌人攻击
- 看准时机按，能触发完美格挡
- **这一步做完就可以回答"爽不爽"了**

---

### 第 3 步：补表现（1 天，可选）

动画、特效、音效、顿帧。

**产物**：
- 格挡 / 完美格挡的 Montage 与动画通知
- GameplayCue（普通格挡蓝光、完美格挡白光 + 圆环）
- 顿帧（完美格挡 8 帧 + 白闪）

---

## 四、给 AURA 的指令（第 1 步，直接复制）

> ⚠️ 用之前请先双击一次 `Scripts\backup.bat` 存档。
> 这样万一 AURA 改坏了，一键就能退回来。

```
【这一轮只做一件事】
给现有的 AARPGUnit 加"格挡判定"，包含普通格挡与完美格挡两种情况。
本轮不做动画、不做特效、不做音效、不做界面按钮、不动敌人 AI 逻辑。
做完就停。

【项目情况】
- 引擎：Unreal Engine 5.8
- 项目：MyCppProject（C++ 项目，已接入 GAS）
- 现有 ARPG 代码在 Source/MyCppProject/ARPG/，主要类是 AARPGUnit、UARPGAttributes、AARPGGameMode
- UARPGAttributes 现有属性：Health、MaxHealth、Energy、Guard（上限均为 100）
- 现有的攻击流程：AARPGUnit::RequestAttack(EARPGAttack) → StartAttack → 定时器 ResolveHit()
- 现有枚举：EARPGAttack { Light, Heavy, Burst, Dodge }
- 项目规范见根目录 AGENTS.md，请遵守

【设计依据（请先读，不要凭猜测实现）】
- 项目根目录 Docs/Design/HANDOFF.md
- 项目根目录 Docs/Design/战斗系统设计文档.md 的 §3.2（帧数据表）
- 只读，不要修改这两个文件

【你要做的内容】
1. 新建设计数据表 DT_CombatNumbers，放在 Content/ARPG/Data/，
   先包含这些行（数值全部以 Design 文档 §3.2 为准）：
     - Parry_WindowFrames = 6      （完美格挡判定窗口）
     - Parry_StartupFrames = 1     （格挡起手）
     - Parry_RecoveryFrames = 10   （格挡收招）
     - Parry_CounterBonusFrames = 12 （完美格挡后伤害翻倍的持续时间）
     - Parry_StaminaCost = 8       （每次成功格挡消耗）
     - Parry_DamageReduction = 0.7 （普通格挡减伤比例）
     - Parry_CounterDamageMultiplier = 2.0
     - Parry_ExhaustSeconds = 3    （体力不足后的力竭时长）
     - Parry_EnemyStaggerSeconds = 0.4 （完美格挡使敌人硬直）
   所有数值必须从这张表读取，代码与蓝图里不得出现裸数字。

2. UARPGAttributes 增加两个属性：
     - Stamina（体力，上限 100，随时间回复）
     - ParryWindowEnd（记录完美格挡窗口的结束时刻，float，不显示给玩家）
   不要删除或重命名现有属性。

3. AARPGUnit 增加三个成员函数（并在头文件中声明）：
     - RequestParry(bool bPressed)  ：按下 / 松开格挡键时调用，开始或结束格挡
     - IsPerfectParry() const       ：判断当前是否处于完美格挡窗口内
     - ResolveParry(float IncomingDamage, AActor* Attacker) -> float
          返回实际受到的伤害。内部逻辑：
            · 不在格挡状态 → 返回原伤害
            · 在完美格挡窗口内 → 返回 0，触发敌人硬直 Parry_EnemyStaggerSeconds，
              并把"反打增益"标记为有效（有效期内伤害 × Parry_CounterDamageMultiplier）
            · 普通格挡 → 伤害 × (1 - Parry_DamageReduction)，
              扣除 Parry_StaminaCost 体力；体力不足则格挡失效并进入力竭 Parry_ExhaustSeconds
   力竭期间：移动速度 ×0.65、不能闪避、受到伤害 ×1.25。

4. 新建自动化测试 Source/MyCppProject/ARPG/ARPGParryTest.cpp，至少覆盖：
     - 恰好第 6 帧按下格挡 → 判定为完美格挡，伤害为 0
     - 第 7 帧才按下 → 判定为普通格挡，伤害为原伤害 × 0.3
     - 体力不足 8 点时格挡 → 格挡失效，进入力竭，3 秒后自动恢复
     - 完美格挡后 12 帧内的攻击，伤害是原伤害的 2 倍
     - 完美格挡后超过 12 帧的攻击，伤害恢复正常

【硬性限制】
- 不得修改 Source/MyCppProject/Variant_* 下的任何模板代码
- 不得修改 AGENTS.md 与 Docs/Design/ 下的文件
- 不得引入任何新的第三方插件
- 不要一次性把输入绑定、动画、特效都接上 —— 本轮只做判定与测试
- 全部用 C++ 实现；如需蓝图资产（数据表），用 Scripts/create_arpg_assets.py 的方式创建
- 不确定的地方请停下来问我，不要自己选一个方案先做

【做完的标准】
1. 编译通过，且不新增警告
2. 上面 5 条自动化测试全部通过
3. 告诉我实际的判定帧数与 DT_CombatNumbers 中的值是否一致

【要给我看的证据】
1. 新建 / 修改的文件清单
2. 编译输出（含警告数量）
3. 自动化测试的完整输出（每个用例名 + 结果）
4. 一段说明：你是如何在代码层面实现"6 帧窗口"的（用了什么计时方式）

【报告怎么写】
按 AGENTS.md 的要求，把这次对话追加到 Docs/Conversation.md。
另外单独给我一段总结：新增功能、验证结果、限制、回退方法。

【卡住了怎么办】
如果 6 帧窗口在你的实现方式下无法稳定判定（例如依赖 FrameRate 或 DeltaTime 精度），
请把问题、你的分析、两到三种可选方案及其代价列出来，停下来等我决定。
不要用"大概差不多"的方式绕过去。

【最后】
如果你认为本方案与现有代码有冲突（例如 AARPGUnit 里已经有类似机制），
请先只读分析并告诉我冲突点，等我确认后再改代码。
```

---

## 五、第 1 步做完后，你自己要试什么

第 1 步只有代码和测试，还不能玩。**做完第 1 步后先别急着往下，把 AURA 的回复贴给我**，
我确认没问题再给你第 2 步的指令。

第 2 步做完后，你要亲自试这 5 件事（这是整个路线 C 的核心）：

1. **一直按住格挡** → 能不能挡住？掉血是不是明显变少了？
2. **看准了按** → 有没有"叮"一下弹开的感觉？
3. **弹开后立刻打** → 伤害是不是明显更高？
4. **一直按着不放** → 体力会不会掉光？掉光后是不是很难受？
5. **凭感觉按**（不刻意数帧）→ 你觉得窗口是"太宽（随便按都行）"还是"太窄（根本按不到）"？

第 5 问的答案最重要。如果太宽 → 我们收窄到 4 帧；如果太窄 → 放宽到 8 帧并考虑加"输入缓冲"。

---

## 六、回退方法（万一改坏了）

```
双击 Scripts\rollback.bat
→ 选 "e9d049b 第一版 GAS ARPG Demo"（如果还没提交新存档）
→ 输入 YES 确认
```

或者告诉我，我帮你处理。

---

## 七、这一步在整个项目里的位置

```
✅ 第一版 ARPG Demo（已完成，已存档 e9d049b）
🔵 路线 C · 完美格挡垂直切片   ← 你在这里
     ├─ 第 1 步：判定 + 测试
     ├─ 第 2 步：接输入 + 接敌人  ← 这一步就能回答"爽不爽"
     └─ 第 3 步：补表现（可选）
⬜ 路线 A 或 B（等手感确认后再决定）
     ├─ 属性集扩展（AP / 气力 / 韧性 / 决意条）
     ├─ 敌人 AI 换成 StateTree + 预兆色板
     ├─ 三种换人（连携 / 反击 / 援助）
     └─ 人形 Boss
```
