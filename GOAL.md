# Wave Workbench 长期目标

更新时间：2026-07-28

## 目标

交付一个可独立运行的 Qt 6/C++20 波形桌面应用，使用户能够人工定义与直接编辑 FPGA
预期数字波形；同一场景模型继续支撑 SystemVerilog/SVA/cocotb、文档图、trace 导入及
Expected/Actual 比较，这些派生与比较能力由独立 CLI 和领域模块提供。

该目标已登记为长期 goal，当前状态为 `持续迭代（Goal 模式已恢复）`。阶段 1 至阶段 6、最终加固、Clock
可编辑性与覆盖语义迭代、Lane/Group 属性编辑迭代以及依赖感知删除迭代均已完成并
验收；Lane/Group 显示顺序、Relation condition 求值和画布末尾添加信号入口迭代也已
完成，光标模式增强、Wave Edit 直接波形编辑、画布信号管理、工具收敛、纯波形工作区、
局部边沿预览、桌面吸附控件移除、轻吸附/画布层次/Bus 快捷值、基础操作闭环、任务闭环/
直接编辑/双视角验收、信号名就地重命名、未提交草稿统一收口、快速新增事务隔离与标题拖动取消，
以及持久范围选择、同类型批量赋值、波形 Event 稳定重映射、Relation 依赖一致性、非遮挡固定范围栏、
可见范围 Copy、右键就地 Paste、粘贴结果持续可见、范围原子清除与关系安全撤销、标准范围 Cut 与文本焦点保护，
以及持久范围端点就地修正、范围选择后的同次安全重定向、Bit 写值与清除拍级一致性、拍级选择持久可见、
撤销/重做与 Segment 清除反馈闭环、单信号写值关系安全与一拍 Pulse 一致性、拍级翻转与清除依赖反馈闭环、
重复写值无效果命令收口、Clock Run 结果反馈与空清除收口、快速参数编辑真实结果与空历史收口、单 lane 目标感知 Paste 与空历史收口、多 lane 显式目标 Paste 与可见入口迭代亦已完成。最近一次静默回归为 26/26 核心测试、Debug/Release 各
21/21 CTest 及 Wave Edit/用户主流程 2/2；Qt Creator Debug 与 Release 构建均通过。
明确保留范围记录于 `README.md` 和 `PLAN.md`。

## 不变量

- Scenario、Event、Lane、Segment、Relation 是唯一事实源。
- 时间事实仅使用整数 tick 或显式有理数；不持久化浮点时间。
- 内部关系使用稳定 ID，显示名称不作为主键。
- 领域、时间、验证、生成和比较逻辑不依赖 QtWidgets。
- 生成的 testbench、assertion、图片和报告是单向派生产物。
- 工程使用可读、版本化、可迁移的格式，未知字段不被静默丢弃。
- 外部 trace 保持为工程相对或可解析引用，不复制完整大文件到 JSON。
- 不直接读取其他应用的内部数据库。

## 验收矩阵

| 交付项 | 当前状态 | 证据或缺口 |
|---|---|---|
| 可运行桌面应用 | 完成 | 单一 WaveCanvas 工作区、波形编辑/导出与恢复快照可运行；trace/Compare/集成由 CLI 和领域模块保留 |
| 示例工程 | 阶段 1 完成 | `examples/handshake/project.wave.json` |
| 自动化测试 | 完成 | 核心 26 组及 21 个 CTest 入口均通过；范围专项验证 Event/Relation 一致性，独立用户旅程验证多草稿门禁与 650 ns 保存回读 |
| README/PLAN/GOAL | 完成 | 根目录三个文档 |
| 工程格式说明 | 阶段 1 完成 | `docs/project-format.md` |
| SystemVerilog/cocotb 示例 | 阶段 3 完成 | `examples/handshake/generated` |
| SVG/PNG/PDF 示例 | 阶段 3 完成 | `examples/handshake/exports` |
| VCD 导入示例 | 阶段 4 完成 | `examples/handshake/traces/handshake_actual.vcd` |
| Clock 覆盖与属性编辑 | 持续迭代 1/34/35 完成 | gated/disabled、项目级 cycle 重定位、跨输出一致性、拍级 Run 以及右键周期编辑的结果/无效果反馈 |
| Lane/Group 属性编辑 | 持续迭代 2/35 完成 | stable ID、结构校验、右键轻量参数结果/无效果反馈、Redo 保留及离屏对话框回归 |
| Lane/Group 删除 | 持续迭代 3 完成 | Event/Relation/trace 依赖清理、Group 解组及离屏 Undo 回归 |
| Lane/Group 显示顺序 | 持续迭代 4 完成 | 可撤销重排、跨视图顺序一致性及生成确定性 |
| Relation condition | 持续迭代 5 完成 | Expected/Actual 整数 tick 采样、四态条件、错误定位及报告可见性 |
| 画布信号管理 | 持续迭代 9/14/15/17/35 完成 | 三按钮、随机颜色、lane 内联属性、双击/F2 就地重命名、原处校验、快速参数真实结果/空历史抑制、快速新增事务门禁、标题拖动 Esc 取消及时钟自动关联 |
| 跨应用契约 | 阶段 6 完成 | `docs/integration-contracts.md` 与 `wave-bridge` |
| 光标模式增强 | 持续迭代 7 完成 | 活动/临时/锁定光标、带符号测量、采样值、键鼠编辑及离屏回归 |
| Wave Edit 直接编辑 | 持续迭代 8/9/10/13/18/19/20/21/22/23/24/25/26/27/28/29/30/31/32/33/34/36/37 完成 | Segment 选择/移动/缩放/删除及结果反馈、Bit 单拍/多拍/四态输入与精确一拍 Pulse、拍级 Toggle/Clear/Delete 的依赖提示、重复写值/Clock Run/Paste 无空历史及持久选中框、局部边沿虚线预览、固定非遮挡范围赋值、可见 Copy/Cut/Paste/Clear、单/多 lane 目标感知 Paste 与不兼容拒绝、粘贴结果持久选择、范围端点手柄、安全重定向及关系安全清理 |
| 持久范围批量编辑 | 持续迭代 18/19/20/21/22/23/24/25/26/27/33/36/37 完成 | Bit/Bus 同类型多 lane 整段赋值、不同 Bus 位宽生成、重复赋值/Paste 无空历史、混合类型保留 Copy/Cut/Paste/Clear、右键目标时间与单/多 lane 目标信号粘贴、数量/类型/位宽原子校验、两步移动范围、端点就地修正、标题/标尺同次重定向及单步 Undo |
| Event/Relation 一致性 | 持续迭代 19/24/30/31/32 完成 | 同 tick Event ID 稳定重映射、消失边沿依赖精确清理、无关 Relation 保留、范围、Segment Clear、单信号写值与拍级 Toggle/Clear 的完整 Scenario Undo/Redo 及用户可见依赖提示 |
| 工具栏收敛 | 持续迭代 9/10/13/14/20/21/24/25/37 完成 | 无模式直接编辑；常驻仅保留临时 Measure、缩放与 Fit，范围操作按选择临时显示且不覆盖画布；Copy/Cut/Paste/Clear 直接可见，混合选择不显示无效赋值控件，960 像素无裁切 |
| 纯波形桌面布局 | 持续迭代 10 完成 | 无 Project/Inspector/Scenario Dock，WaveCanvas 直接占据中央区，离屏布局回归通过 |
| 直接时间交互 | 持续迭代 11/12/13/14/26/27 完成 | 7 像素轻吸附、范围端点就地拖动、标尺同次清除范围并定位、Alt 临时绕过、平移/方向键及可撤销 End 直接输入，非法缩短原处反馈 |
| 画布视觉层次 | 持续迭代 12/29 完成 | 较浅中性背景、透明选中态、选中区域网格可见性及脱离 hover 的 Bit 拍级持久选中框通过离屏截图验收 |
| Bus 快捷值 | 持续迭代 12/13/14 完成 | 隐式 X、0/X/Z/Don't care、直接值输入、非法值纠错、旧 Reserved 兼容及 Undo/Redo |
| 未提交草稿安全 | 持续迭代 16/17 完成 | Save/New/Open/Close/Export/点击门禁、Bus→End 条件顺序、快速新增有效先提交/无效原位阻断、交错命令防御恢复与双击/Bus 控件隔离 |
| 基础操作闭环 | 持续迭代 13/14/16/17/22/24/25/27/28/29/30/31/32/33/34/35/36/37 完成 | 无配置空白工程、内联创建、默认直接编辑、统一草稿与快速新增事务门禁、Bit 拍级写值/Toggle/Clear/Delete/Pulse 的依赖反馈、同值写入/Clock Run/快速参数/Paste 无空 Undo/Redo 损失及持久选择、可见且目标感知的范围 Copy/Cut/Paste/Clear、Undo/Redo 可见确认、标题/标尺无死首击、文本焦点保护及用户旅程均通过离屏回归 |
| 修改文件清单 | 完成 | 最终交付报告及 git 状态 |
| 构建和测试结果 | 完成 | `PLAN.md` 最终验证证据 |
| 性能测试结果 | 完成 | 百万 transition 可见范围基准记录于 `PLAN.md` |
| 已知限制和后续计划 | 完成 | `README.md` 与 `PLAN.md` |

矩阵中的必需交付项均具有实际行为和测试证据；第 37 轮开发视角 Debug/Release 全量回归、
多 lane 显式目标 Paste、数量/兼容性拒绝、可见入口、960 像素布局和用户视角独立旅程均已静默通过。持续迭代 Goal 保持 active，
后续每轮继续执行双视角审计、offscreen 验收与通过后 push；迭代阶段不打包。
