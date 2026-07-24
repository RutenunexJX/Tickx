# Wave Workbench 长期目标

更新时间：2026-07-24

## 目标

交付一个可独立运行的 Qt 6/C++20 桌面应用，使用户能够人工定义 FPGA 预期数字波形和
验证场景，以同一场景模型编辑画布与步骤表，生成 SystemVerilog/SVA/cocotb 和文档级
时序图，导入标准仿真 trace，并对 Expected 与 Actual 结果执行可导航的比较。

该目标已登记为长期 goal，当前状态为 `完成`。阶段 1 至阶段 6、最终加固、Clock
可编辑性与覆盖语义迭代、Lane/Group 属性编辑迭代以及依赖感知删除迭代均已完成并
验收；Lane/Group 显示顺序、Relation condition 求值和画布末尾添加信号入口迭代也已
完成。最终静默回归为 23/23 核心测试和 17/17 CTest，持续迭代在此版本结束。明确保留范围记录于
`README.md` 和 `PLAN.md`。

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
| 可运行桌面应用 | 完成 | Qt 工作台、编辑、验证、生成导出、trace、Compare、恢复快照和集成接口可运行 |
| 示例工程 | 阶段 1 完成 | `examples/handshake/project.wave.json` |
| 自动化测试 | 完成 | 核心 23 组及 17 个 CTest 入口均通过 |
| README/PLAN/GOAL | 完成 | 根目录三个文档 |
| 工程格式说明 | 阶段 1 完成 | `docs/project-format.md` |
| SystemVerilog/cocotb 示例 | 阶段 3 完成 | `examples/handshake/generated` |
| SVG/PNG/PDF 示例 | 阶段 3 完成 | `examples/handshake/exports` |
| VCD 导入示例 | 阶段 4 完成 | `examples/handshake/traces/handshake_actual.vcd` |
| Clock 覆盖与属性编辑 | 持续迭代 1 完成 | gated/disabled、项目级 cycle 重定位及跨输出一致性 |
| Lane/Group 属性编辑 | 持续迭代 2 完成 | stable ID、结构校验、撤销/重做及离屏对话框回归 |
| Lane/Group 删除 | 持续迭代 3 完成 | Event/Relation/trace 依赖清理、Group 解组及离屏 Undo 回归 |
| Lane/Group 显示顺序 | 持续迭代 4 完成 | 可撤销重排、跨视图顺序一致性及生成确定性 |
| Relation condition | 持续迭代 5 完成 | Expected/Actual 整数 tick 采样、四态条件、错误定位及报告可见性 |
| 画布添加信号入口 | 持续迭代 6 完成 | lane 列表末尾按钮、原属性流程复用及离屏点击/Undo 回归 |
| 跨应用契约 | 阶段 6 完成 | `docs/integration-contracts.md` 与 `wave-bridge` |
| 修改文件清单 | 完成 | 最终交付报告及 git 状态 |
| 构建和测试结果 | 完成 | `PLAN.md` 最终验证证据 |
| 性能测试结果 | 完成 | 百万 transition 可见范围基准记录于 `PLAN.md` |
| 已知限制和后续计划 | 完成 | `README.md` 与 `PLAN.md` |

矩阵中的必需交付项均具有实际行为和测试证据；最终静默回归已通过，该 goal 已结束。
未实现的非目标或第三方依赖范围已明确列出。
