# Wave Workbench 实施计划

更新时间：2026-07-24

状态定义：`完成` 表示具有可运行行为和自动化证据；`进行中` 表示正在实施；`未开始`
表示尚无可验收实现。文档中的完成状态不替代测试结果。

## 阶段 1：工程模型和基础画布

状态：完成

已交付：

- 模块化 CMake/C++20/Qt 6 工程。
- `Project`、`Scenario`、`Lane`、`Segment`、`ClockDomain` 及为后续阶段预留的
  `Event`、`Marker`、`Relation` 数据结构。
- 以 `std::int64_t` tick 为事实的时间模型，支持 ps/ns/us/ms 精确换算与 cycle 边沿。
- bit、bus、enum、clock lane；bit 支持 0/1/X/Z，bus 保留 X/Z pattern。
- segment 区间覆盖、拆分、相邻同值合并和清除。
- 版本 1 JSON 工程格式、十进制字符串形式的 64 位 tick、稳定 ID、未知字段保留、
  schema 0 到 1 迁移、`QSaveFile` 原子写入和相对资源路径。
- 基于 `QAbstractScrollArea` 的自绘画布；可见范围绘制、二分定位首个可见 segment、
  水平与垂直滚动、鼠标锚定缩放、Fit scenario、时间标尺和 clock edge 网格。
- bit/bus/enum 基础区间绘制和显式 command undo/redo。
- 工作台主界面、工具栏、左/右/底部面板和 tooltip。
- `examples/handshake` 示例工程。

验证证据：

```text
cmake --build build --parallel 4
Result: success

ctest --test-dir build --output-on-failure
wave-core-tests: passed
wave-app-smoke: passed
100% tests passed, 0 tests failed out of 2
Total Test time: 0.36 sec
```

核心测试入口包含 7 组测试：时间、clock/snapping、lane/segment、undo/redo、序列化、
迁移、工程目录移动。

## 阶段 2：场景、关系和验证

状态：完成

已交付：

- waveform-linked Event 与 Segment 使用稳定 ID 关联。
- 画布区间编辑同步 Event；步骤表 time/value 修改同步 Segment；删除 Event 清除关联区间。
- 步骤表排序、文本过滤、添加、删除和双击定位。
- Transition 工具拖动 Event 并移动对应波形边沿。
- Marker 点/区间绘制。
- 从 Event 到 Event 的 Relation 拖动创建、表格编辑和删除。
- Relation source/target/min/max/clock/condition/severity/description 展示。
- Validation 覆盖关系满足、违反、缺少 source/target、多目标、观察范围不足、
  clock domain 不一致、非法值、缺失 lane、越界事件和未定义区间。
- Validation 和 Relation 结果双击定位到 lane/tick。

验证证据：

```text
wave-tests
9/9 tests passed

ctest --test-dir build --output-on-failure
wave-core-tests: passed
wave-app-smoke: passed
wave-example-smoke: passed
100% tests passed, 0 tests failed out of 3
Total Test time: 0.67 sec
```

## 阶段 3：生成和导出

状态：完成

已交付：

- 基于确定性 `GenerationPlan` 的 SystemVerilog testbench 与 cocotb test；两者共享信号、
  step、clock、timeout 和 expected-check 语义。
- Relation 仅在精确 bit 边沿、明确 clock/reset 和精确 cycle delay 条件下转换为 SVA；
  其余情况输出诊断且不生成近似代码。
- 矢量文字 SVG、高 DPI PNG、多页 PDF 和 WaveDrom JSON。
- 全场景、当前选择或指定时间范围导出；可配置 PNG DPI、PDF 每页时间跨度，以及
  Relation、Marker、Annotation 附加层。
- `wave-generate` 无界面 CLI 和桌面 Export Artifacts 工作流。
- `examples/handshake/generated` 与 `examples/handshake/exports` 可复现示例。

验证证据：

```text
wave-tests
11/11 tests passed

ctest --test-dir build --output-on-failure
wave-core-tests: passed
wave-app-smoke: passed
wave-example-smoke: passed
wave-generate-smoke: passed
100% tests passed, 0 tests failed out of 4

Python py_compile: generated cocotb test passed
Vivado xvlog: generated SystemVerilog testbench passed
PNG visual QA: passed
PDF info/render/visual QA: A4 landscape, 1 page, passed
```

## 阶段 4：实际波形导入

状态：完成

已交付：

- 标准 VCD timescale、scope、digital scalar/vector、alias 和 `$dumpvars` 初值解析。
- timestamped CSV、引号字段、显式 ps/ns/us/ms 单位、整数项目 tick 及冗余值压缩。
- 不可由项目 timebase 精确表示的时间明确拒绝，不使用浮点舍入。
- 外部 trace 路径、可重建内存索引、稳定 signal ID 和 `lane ID → trace signal ID` 映射。
- 自动名称映射、可编辑映射和实际信号显示筛选。
- 手动 offset、marker start 和 clock edge/cycle 三类整数时间对齐。
- Qt Concurrent 后台解析、取消、进度状态及 project/trace/generation identity 校验。
- 只绘制可见 signal 和可见 transition 的 Actual Trace `QAbstractScrollArea`。
- `examples/handshake/traces/handshake_actual.vcd` 标准 VCD 示例。

验证证据：

```text
wave-tests
14/14 tests passed

[METRIC] 1000 lanes / 1000000 transitions / 100000 queries: 19 ms

ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 4
Total Test time: 0.94 sec

Native UI visual QA: 6 signals / 59 transitions, mapping and rendering passed
```

FST 需要兼容格式的第三方解析库；当前工具链未捆绑该库，因此不实现自定义或不兼容方言。

## 阶段 5：Expected/Actual Compare

状态：完成

已交付：

- 独立 `wavecompare` 区间扫描引擎，不按采样点逐点比较。
- exact、ignore any X、expected X wildcard、edge tolerance、time window、bus mask、
  enum equivalence 和 relation-only。
- 缺失 signal、width mismatch、trace offset、差异区间、差异数和 first mismatch。
- Expected/Actual 上下分屏、差异时间高亮、结果表双击导航。
- JSON、CSV、独立 HTML 报告及安全写入。
- `wave-compare` 无界面 CLI、CTest smoke 和 `examples/handshake/compare` 示例报告。

验证证据：

```text
wave-tests
16/16 tests passed

ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 5
Total Test time: 1.01 sec

Example exact compare:
4 differences, first mismatch 110 ns

Example tolerance compare:
5 ns tolerance, 4 tolerated edge intervals, no differences

Native Compare UI visual QA: passed
```

## 阶段 6：跨应用接口

状态：完成

已交付：

- `wave-bridge describe/import-signals` ZeroSlack 工作区描述和 signal-list schema 1。
- `wave-generate` 将场景派生产物发送到显式 workspace。
- Private Frame sample 的 frame project/frame/sample 稳定 ID、encoded bytes、hash、
  summary、source path 和可选 Transaction Segment 目标。
- 失效 Transaction 目标保留原始摘要并报告 unresolved；桌面 Resources 显示文件状态。
- `wave-bridge pinloom-entry` 和桌面 Export and Open in Pinloom。
- `waveworkbench://open`、`waveworkbench://compare` 及 `--uri`。
- `docs/integration-contracts.md` 版本化契约。
- 所有接口只读写显式文件或 URI，不访问共享数据库。

验证证据：

```text
wave-tests
17/17 tests passed

ctest --test-dir build --output-on-failure
10/10 tests passed

wave-uri-smoke: passed
wave-bridge-describe-smoke: passed
wave-bridge-signals-smoke: passed
wave-bridge-frame-smoke: passed
wave-bridge-pinloom-smoke: passed
```

## 最终加固

状态：完成

已交付：

- 多 lane 矩形选择、`Ctrl` 增减选择、稳定 ID/相对 tick 复制粘贴、单命令撤销/重做。
- Pulse 工具、`Alt` 临时绕过 snapping、transaction/event lane 区间编辑。
- 信号边沿和 marker 排序索引；鼠标吸附由全量扫描改为二分查找。
- Groups 页签显示实际稳定 ID 分组及 unresolved 引用。
- 1.5 秒防抖的 Qt Concurrent autosave；复制 Project 快照但不复制外部 trace 数据，
  使用 generation 丢弃旧状态，并以同目录 `.autosave` 文件提供恢复入口。
- Transaction 示例及 SystemVerilog/cocotb/SVG/PNG/PDF/WaveDrom 派生产物重新生成。
- README、工程格式、限制和最终验收矩阵同步。

最终验证证据：

```text
cmake --build build --parallel 4
Result: success

wave-tests
19/19 tests passed
[METRIC] 1000 lanes / 1000000 transitions / 100000 queries: 20 ms

ctest --test-dir build --output-on-failure
11/11 tests passed
Total Test time: 3.93 sec

Python py_compile: generated cocotb test passed
Vivado 2022.2 xvlog -sv: generated SystemVerilog testbench passed
Example exact compare: 4 differences, first mismatch 110 ns
Example 5 ns tolerance compare: 0 differences, 4 tolerated edge intervals
PNG visual QA: passed
PDF info/render/visual QA: A4 landscape, 1 page, passed
Native Windows Compare UI visual QA: passed
```

明确保留范围：

- FST 等待兼容第三方库；WDB 仍为非目标。
- SVA 对不可无损转换的关系只给出诊断。

## 持续迭代 1：Clock 可编辑性与覆盖语义

状态：完成

已交付：

- Clock lane 使用兼容 schema 1 的 `gated`/`disabled` 半开区间；Draw 工具支持门控、
  禁用和清除覆盖，桌面画布、SVG、PNG、PDF、WaveDrom 与 Compare 共用同一取值语义。
- Clocks 页签双击编辑 period、phase、有理数 duty、active edge 和 reset/disable
  condition；`ChangeClockCommand` 以单一撤销项批量重定位项目内全部 Scenario 的 cycle
  Event 及关联 Segment。
- 批量重定位先计算全部目标边界再统一验证，避免周期增大时被旧 segment 末端误判；
  结果越界或区间反转时整项拒绝，不产生部分更新。
- SystemVerilog 与 cocotb 生成显式时钟覆盖调度。覆盖影响可见边沿后，cycle Event 按模型
  `timeTick` 调度，避免门控造成边沿计数漂移。
- 独立 SVA 产物改为可单独编译的模块。Clock 覆盖与关系评估窗口相交时不生成近似
  assertion，并返回 `sva-clock-override-overlap` 诊断。
- 示例工程增加 170–190 ns gated 和 200–210 ns disabled 区间；VCD、HDL、Python、
  图形导出和 Compare 报告同步更新。
- 窄覆盖区间根据字体测量使用完整标签或 `G`/`X`，消除 PDF 和桌面画布文字裁切。

验证证据：

```text
cmake --build build --parallel 4
Result: success

wave-tests
19/19 tests passed
[METRIC] 1000 lanes / 1000000 transitions / 100000 queries: 20 ms

ctest --test-dir build --output-on-failure
11/11 tests passed
Total Test time: 3.94 sec

Python py_compile: generated cocotb test passed
Vivado 2022.2 xvlog -sv: generated testbench and standalone assertion module passed
Example exact compare: 4 differences, first mismatch 110 ns
Example 5 ns tolerance compare: 0 differences, 4 tolerated edge intervals
PNG visual QA: clock gated/disabled overlays passed
PDF info/render/visual QA: A4 landscape, 1 page, no clipped labels
Native Windows Compare UI visual QA: clock overlays and actual trace passed
```

## 持续迭代 2：Lane/Group 属性编辑

状态：完成

已交付：

- `ChangeLaneCommand` 以 stable ID 定位目标，并将名称、类型、位宽、符号性、进制、
  enum map、clock domain、group、颜色、高度和可见性作为单个撤销项提交。
- 属性变更在执行前规范化结构并验证现有 Segment/Event；非法位宽缩减、失效 clock/group
  引用、Clock/Group 持有 Event，以及含成员 Group 的类型转换均整体拒绝。
- Edit 菜单提供 Add lane、Add group 和 Lane / group properties；Signals 与 Groups
  条目支持双击编辑，Inspector 同步显示分组和显示属性。
- 示例工程新增 `group-handshake`，成员关系仅引用稳定 ID；group 改名不影响成员。
- 新增 Lane/Group 命令测试和 Lane 属性对话框离屏截图 smoke。Windows 离屏测试显式
  配置字体目录，避免无桌面环境中的缺字或错误字体回退。

验证证据：

```text
cmake --build build --parallel 4
Result: success

wave-tests
20/20 tests passed
[METRIC] 1000 lanes / 1000000 transitions / 100000 queries: 20 ms

ctest --test-dir build --output-on-failure
12/12 tests passed
Total Test time: 5.12 sec

Offscreen main-window visual QA: group navigation, waveform and Actual Trace passed
Offscreen lane-dialog visual QA: readable font, complete fields, disabled states and layout passed
```

## 持续迭代 3：依赖感知 Lane 删除与 radix 边界

状态：完成

已交付：

- `RemoveLaneCommand` 删除普通 lane 时同步清理其 Event、相关 Relation 以及不再被其他
  Scenario 使用的 trace 映射；删除 Group 时保留成员 lane 并解除分组。
- 删除、依赖清理和恢复属于单个撤销项。Trace 映射采用细粒度恢复，Undo 不会覆盖删除
  之后新导入的 trace。
- Edit 菜单提供 Remove selected lane / group，并在执行前说明普通 lane 与 Group 的不同
  依赖语义。新增全离屏界面 smoke，覆盖选择、确认删除、聚合清理和 Undo 恢复。
- Lane 属性对话框补齐 Octal radix。Bus 校验进一步检查非 3/4 位整倍数宽度的最高有效
  数字，拒绝 `8-bit 0o400`、`10-bit 0x400`，同时保留合法的部分高位 X/Z pattern。

验证证据：

```text
cmake --build build --parallel 4
Result: success

wave-tests
21/21 tests passed
[METRIC] 1000 lanes / 1000000 transitions / 100000 queries: 20–25 ms

ctest --test-dir build --output-on-failure
13/13 tests passed
Total Test time: 5.46 sec

Python py_compile: generated cocotb test passed
Vivado 2022.2 xvlog -sv: generated testbench and standalone assertion module passed
Example exact compare: 4 differences, first mismatch 110 ns
Example 5 ns tolerance compare: 0 differences, 4 tolerated edge intervals
PNG visual QA: passed
PDF info/render/visual QA: A4 landscape, 1 page, passed
Offscreen GUI QA: main window, property dialog, removal confirmation and Undo passed
```

## 持续迭代 4：Lane/Group 显示顺序

状态：完成

已交付：

- `MoveLaneCommand` 以 stable ID 将 Lane/Group 移至最终数组索引，并对缺失目标、越界和
  无变化操作明确拒绝；Undo/Redo 精确恢复数组顺序。
- Edit 菜单增加 Move selected lane up/down 与 `Alt+Up` / `Alt+Down`；首尾边界动作
  自动禁用。Signals/Groups 点击现在同步画布选择和 Inspector，重排后保持目标选中。
- `Scenario::lanes` 顺序继续驱动画布、导航树和图形导出；生成计划仍按 stable ID 排序。
  自动化测试证明显示重排不会改变 SystemVerilog 或 cocotb 文本。
- 新增离屏重排 smoke，覆盖上移、下移、两次 Undo；新增 Edit 菜单离屏截图回归，验证
  动作文本、快捷键、图标、启用态及布局。

验证证据：

```text
cmake --build build --parallel 4
Result: success

wave-tests
22/22 tests passed
[METRIC] 1000 lanes / 1000000 transitions / 100000 queries: 21 ms

ctest --test-dir build --output-on-failure
15/15 tests passed
Total Test time: 5.98 sec

Offscreen lane reorder interaction: up/down and exact Undo passed
Offscreen Edit menu visual QA: actions, shortcuts, enabled state and layout passed
```

## 持续迭代 5：Relation condition 求值与可见诊断

状态：完成

已交付：

- `wavevalidate` 增加共享的确定性条件解析器，支持 `!`、`&&`、`||`、括号、
  `==`/`!=`/`===`/`!==`、布尔常量、稳定 lane ID、唯一显示名和引号。
- bit/bus/enum/clock 通过共享 lane value 归一化执行精确四态比较；修正带下划线 enum
  符号、显式正号和多位 bus mask 的边界行为。
- Validation 在预期 source Event tick 采样。false 条件返回
  `relation-not-applicable`，错误条件返回带表达式字节偏移和 lane ID 的
  `invalid-relation-condition`。
- relation-only Compare 在实际 source transition tick 采样。false 守卫跳过 target
  检查并保留诊断；语法、映射、宽度或采样错误生成
  `condition-evaluation-error`，使比较失败。
- Compare 的内存诊断现同步显示于结果表和摘要 tooltip，并序列化到 JSON、CSV、HTML
  报告。
- 新增条件结果离屏 smoke，验证 relation-only 控件、诊断行、摘要计数和窗口截图；全程
  使用 `QT_QPA_PLATFORM=offscreen`，不操作桌面。

验证证据：

```text
wave-tests
23/23 tests passed
[METRIC] 1000 lanes / 1000000 transitions / 100000 queries: 20 ms

ctest --test-dir build --output-on-failure
16/16 tests passed
Total Test time: 7.62 sec

Offscreen condition Compare visual QA: false guard diagnostic row and summary passed
Python py_compile: generated cocotb test passed
Vivado 2022.2 xvlog -sv: generated testbench and standalone assertion module passed
Example exact compare: 4 differences, first mismatch 110 ns
Example 5 ns tolerance compare: 0 differences, 4 tolerated edge intervals
Example relation-only compare: 0 differences
PNG visual QA: passed
PDF info/render/visual QA: A4 landscape, 1 page, passed
```

## 持续迭代 6：画布末尾添加信号入口

状态：完成

已交付：

- 波形画布在所有可见 lane 之后提供 `+ Add signal` 行，空场景也保留直接添加入口。
- 入口使用真实、可聚焦的 Qt 按钮，并允许单击整行；按钮具备 tooltip、辅助功能名称及
  hover/focus/pressed 状态。
- 点击复用既有 `MainWindow::addLane()`、Lane 属性校验、stable ID、Undo/Redo 和模型
  刷新流程，不引入第二套创建语义。
- 新增离屏 smoke，验证按钮位于列表末尾、打开属性对话框、创建 Bit lane 以及完整 Undo。

验证证据：

```text
cmake --build --preset qtcreator-debug
Result: success

ctest --preset qtcreator-debug --output-on-failure
17/17 tests passed
Total Test time: 7.84 sec

Offscreen canvas add-signal interaction: visible control, dialog, creation and Undo passed
Offscreen screenshot visual QA: placement, contrast and lane-row alignment passed
```

## 横向工作

- 每个阶段结束后同步更新 `README.md`、`PLAN.md`、`GOAL.md`。
- 每项完成状态必须对应自动化测试或可复现手工验证。
- 保持生成物为派生产物，不将其反向解析成场景事实源。
- 在导入和 compare 阶段建立 1000 lane、百万 transition 的只读浏览基准。
- 未经用户明确要求不执行远端 push。
