# Wave Workbench 实施计划

更新时间：2026-07-28

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
- Wave Edit 拖动 Bit Event 菱形并移动对应波形边沿，步骤表与 Segment 同步更新。
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
- Pulse 工具、transaction/event lane 区间编辑；该阶段的 snapping/`Alt` 桌面交互已在持续迭代 11 移除。
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

后续状态：该单一入口已在持续迭代 9 中替换为三个即时类型按钮。

## 持续迭代 7：光标模式增强

状态：完成

已交付：

- 左键创建或移动唯一活动光标，左右方向键按固定网格移动；活动光标时在左侧信号名称区
  显示各可见信号的采样值。
- Shift 单击创建会话级临时光标，直接拖动以起点作为临时光标、终点作为活动光标；两种
  操作均显示“临时 → 活动”的带符号时间差。
- Ctrl 单击创建持久锁定光标，Ctrl 拖动创建持久锁定区间；锁定对象使用独立颜色并保存
  为工程 Marker。
- 单击锁定对象可选中；拖动或方向键移动，Delete/Backspace 删除。移动与删除通过命令栈
  支持 Undo/Redo，并限制在场景时间范围内。
- Cursor 工具采用可取消的互斥动作；再次点击时退出光标编辑并清除活动、临时及选中状态，
  持久锁定对象保留。
- 新增独立离屏 smoke，覆盖活动/临时光标、正负时间差、Ctrl 锁定、选择、拖动、方向键、
  删除、锁定区间及二次点击退出；核心测试补充 Marker 修改与删除命令的 Undo/Redo。

验证证据：

```text
cmake --preset qtcreator-debug
cmake --build --preset qtcreator-debug
Result: success

ctest --preset qtcreator-debug --output-on-failure
18/18 tests passed
Total Test time: 8.16 sec

Offscreen cursor interaction smoke: all specified mouse and keyboard paths passed
Offscreen screenshot visual QA: sampled values, cursor colors and signed delta passed
Desktop interaction: none
```

## 持续迭代 8：Wave Edit 直接波形编辑

状态：完成

已交付：

- 初始版本新增独立 Wave Edit 工具动作；持续迭代 13 将其收敛为默认 Edit 工作状态，Cursor
  退出后直接返回 Edit，不再恢复或暴露 Selection/Draw 等分散模式。
- 单击非 Bit Segment 进行选择，以独立高亮和左右边界手柄显示当前对象；拖动任一边界时
  实时预览，释放后以单个命令提交，同时保持相邻连续 Segment 的边界一致。
- 双击 Clock、Bus、Enum、Transaction 或 Event 的现有 Segment 编辑其当前值；输入框以
  原值初始化，非法值或会造成重叠的边界修改整体拒绝。
- Bit lane 悬浮时高亮当前拍；单击始终只翻转该拍，不选中规范化后合并的同电平 Segment，
  连续点击同一拍也不会扩大操作范围。拖动覆盖范围后批量翻转多拍；存在有效 clock domain
  时按活动沿周期定义拍，否则使用 10 ns 固定网格。持续迭代 13 增加 Edit 中的 0/1/X/Z
  键盘与波形右键输入，不再依赖 Draw 工具。
- 新增 `EditSegmentCommand` 与 `ToggleBitRangeCommand`，完整支持 Undo/Redo，并同步维护
  Bit、Bus、Enum lane 的 Event 表示。
- 新增独立离屏 smoke，覆盖工具进入/退出、Segment 选择、左右边界拖动、双击修改现有值、
  Bit 悬浮拍、同一拍连续点击及多拍翻转；核心测试覆盖命令原子性、稳定 ID、相邻边界及
  Undo/Redo。

验证证据：

```text
cmake --preset qtcreator-debug
cmake --build --preset qtcreator-debug
Result: success

ctest --preset qtcreator-debug --output-on-failure
19/19 tests passed
Total Test time: 10.48 sec

Core test suite: 24/24 passed
Offscreen Wave Edit smoke: all specified mouse paths passed
Offscreen screenshot visual QA: single-beat hover, selection handles, edited values and layout passed
Desktop interaction: none
```

## 持续迭代 9：画布信号管理与工具收敛

状态：完成

已交付：

- 画布末尾改为 `+ CLK`、`+ BIT`、`+ BUS` 三个即时创建按钮；名称自动唯一，颜色从可读
  调色板随机选择并优先避开场景已有颜色。CLK 通过扩展的 `AddLaneCommand` 在同一撤销项中
  创建并关联默认 clock domain；完整 Add lane 对话框的初始颜色也采用相同随机策略。
- 左侧信号名支持拖动重排及插入线反馈，释放后只提交一个 `MoveLaneCommand`；双击使用短输入框
  和 `ChangeLaneCommand` 重命名；单击后 Delete/Backspace 复用确认与依赖清理路径。
- 右键信号名提供轻量参数编辑：Clock 支持 period/frequency，Bit 支持颜色、高度和 clock
  domain，Bus 额外支持 width、signedness 和 radix，均通过现有命令栈提交。
- 删除独立 Transition 工具，将 Bit Event 菱形拖动合并到 Wave Edit。拖动时不修改模型，以
  虚线绘制边沿移动后的实时波形；释放后通过一个 `ChangeEventCommand` 同步 Event 与 Segment。
- Waveform 工具栏不再显示 Undo/Redo，Edit 菜单、`Ctrl+Z`、`Ctrl+Y` 和命令栈保持有效。
- 扩展全离屏 smoke，覆盖三个按钮、随机颜色、默认时钟域、双击重命名、右键参数、标题拖动、
  Delete、快捷键、工具栏收敛、边沿非破坏预览、释放提交和撤销/重做；未操作桌面。

验证证据：

```text
cmake --build build --parallel 4
Result: success

Core test suite: 24/24 passed

QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
19/19 tests passed
Total Test time: 8.05 sec

Offscreen signal-management smoke: quick add, random colors, rename, parameters, reorder and delete passed
Offscreen Wave Edit smoke: dashed transition preview, commit, Undo/Redo and mode exit passed
Offscreen screenshot visual QA: three buttons, insertion line and transition preview passed
Desktop interaction: none
```

## 持续迭代 10：纯波形工作区与局部边沿预览

状态：完成

已交付：

- 主窗口中央组件直接使用 `WaveCanvas`，运行时不再创建 Project、Inspector、Scenario
  `QDockWidget`；同步移除 Modes 工具栏及依赖旧面板的桌面 Trace/Pinloom 入口。工程保存、
  波形编辑与导出继续保留，trace、compare 和跨应用能力仍由既有 CLI 与领域模块提供。
- 撤销、重做、选中、工程重载和 Event 选择路径不再刷新或访问已删除面板；compare URI 的
  兼容启动不会重新显示旧界面，仍可按 lane/tick 定位波形。
- Bit Event 菱形拖动预览增加显式局部范围：从原/新边沿中较早位置开始，到该 Event 关联的
  后继 Segment 结束。画笔以该范围裁剪虚线预览，前方未受影响波形不再被整条重绘为虚线。
- 用 `wave-waveform-only-smoke` 替换旧面板截图 smoke，断言中央组件、零 Dock、无 Modes
  工具栏及 Waveform 工具栏可见；Wave Edit smoke 同时断言局部预览范围和预览期间模型不变。

验证证据：

```text
Qt Creator Debug build: success
Core test suite: 24/24 passed

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
19/19 tests passed
Total Test time: 6.30 sec

Offscreen targeted smoke: 6/6 passed
Offscreen screenshot visual QA: single WaveCanvas layout and localized Bit edge preview passed
Desktop interaction: none
```

## 持续迭代 11：移除桌面吸附控件

状态：完成

已交付：

- Waveform 工具栏删除 Snap 下拉框，不再显示 No snap、固定网格、主刻度、时钟边沿、
  信号边沿或 Marker 等吸附选项。
- 该迭代先将 WaveCanvas 固定为无吸附的直接整数 tick；持续迭代 12 在不恢复设置控件的前提下，
  将其收敛为自动近距离轻吸附。领域层 snapping 算法继续作为独立时间库能力保留。
- `wave-waveform-only-smoke` 断言工具栏无 `QComboBox` 且画布为无吸附模式；Wave Edit smoke
  改为按像素实际映射 tick 验证 Segment 边界与 Bit 边沿拖动，移除对旧固定网格的隐含依赖。

验证证据：

```text
Qt Creator Debug build: success
Core test suite: 24/24 passed

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
19/19 tests passed
Total Test time: 6.53 sec

Offscreen targeted smoke: Wave Edit and waveform-only 2/2 passed
Desktop interaction: none
```

## 持续迭代 12：轻吸附、画布层次与 Bus 快捷值

状态：完成

已交付：

- 桌面端不恢复 Snap 下拉框；点击、拖动和光标移动在可见标尺刻度、Clock 边沿或任一信号
  Segment 边沿 7 像素内自动选择最近候选，超出半径保持原始整数 tick，同距离时优先边沿。
- 画布背景、标尺、标题区、网格和快速添加按钮调整为更浅的中性深色层次；选中 lane 的
  waveform 背景改为低透明度叠加，既有纵向刻度和网格线继续可见。
- Bus 未赋值区间统一按四态 `X` 绘制：使用居中红色虚线、淡红底和 `X` 标签；新建 Bus
  因而立即具有明确的未定义显示，光标采样空白区也返回 `X`。
- 单击 Bus 显示邻近浮动快捷框，提供 `Reserved`、`Don't care`、`X`、`Z` 和 `Value…`。
  预设既可单击也可拖放，自定义值可直接输入；插入后以当前 clock domain 周期或默认 10 ns
  创建一拍 Segment 并选中。快捷语义保存于 Segment 扩展，整个操作只产生一个可 Undo/Redo
  的 `SetLaneRangeCommand`。
- 核心测试覆盖快捷语义扩展的提交、撤销、重做及相邻同值 Segment 隔离；offscreen 快速添加
  smoke 覆盖隐式 X、完整浮层、按钮单击、Don't care 拖放、波形右键值操作、Undo/Redo、
  刻度轻吸附与边沿轻吸附。

验证证据：

```text
Qt Creator Debug build: success
Core test suite: 24/24 passed
Million-transition metric: 21 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
19/19 tests passed
Total Test time: 6.38 sec

Offscreen targeted smoke: core and canvas add-lane passed
Offscreen screenshot visual QA: lighter canvas, visible grid through selection, Bus X line and four-button palette passed
Desktop interaction: none
```

## 持续迭代 13：基础操作闭环与默认编辑模式

状态：完成

已交付：

- 无参数启动时进入 200 ns 空白工程；`File > New…` / `Ctrl+N` 提供工程名、时长和时间基准，
  创建一个可立即添加信号的空白 Scenario。
- 波形工具栏收敛为 Edit 与 Cursor 两个主模式，Edit 默认激活且承担选择、输入、移动、缩放、
  删除和范围操作；Cursor 再次点击后返回 Edit。
- 非 Bit Segment 可拖动主体整体移动，按 Delete/Backspace 清除，右键直接编辑常用值；每次
  移动或删除只提交一个命令并支持 Undo/Redo。
- 新 Bit 空白区按隐式 0 显示；在 Edit 中按 `0`、`1`、`X`、`Z` 写入当前拍。Shift 拖动改为
  跨 lane 时间范围选择，既保留 Bit 单拍/多拍翻转，也不再依赖旧 Select/Draw 模式。
- Bus 浮层的四个预设支持单击和拖放，并增加 `Value…`；波形右键可直接设置 Bit、Bus、Clock
  常用值、编辑完整 Segment 或清除为隐式状态。
- 增加中键拖动与空格加左键拖动平移；Edit 时间光标支持方向键。自动吸附在画布上显示时间
  提示，按住 Alt 可临时绕过。
- 新增/扩展 offscreen smoke，覆盖新建空项目、默认 Edit、Segment 整体移动与删除撤销、Bit
  四态键盘输入、Shift 跨 lane 框选、中键平移、Bus 按钮点击与波形右键菜单。

验证证据：

```text
Qt/MinGW incremental build: success
Core test suite: 24/24 passed

QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
20/20 tests passed
Total Test time: 7.76 sec

Offscreen targeted smoke: canvas management, Wave Edit and new project 3/3 passed
Offscreen screenshot visual QA: grid visibility, implicit Bus X, quick-value palette and localized edge preview passed
Desktop interaction: none
```
## 持续迭代 14：任务闭环、直接编辑与双视角验收

状态：完成

已交付：

- 空工程改为中央任务引导和三个直接入口。`+ CLK`、`+ BIT`、`+ BUS` 创建后在目标 lane 内
  就地补齐名称、period 或 width；Enter 提交、Esc 取消。Clock domain 创建、自动/就地时钟
  关联以及用户补齐的属性通过 `CommandStack::replaceLast` 合并为一个历史项，取消通过
  `discardLast` 回滚，不产生幽灵 Undo。
- `File > New` / `Ctrl+N` 改为立即创建 1 ps/tick、200 ns 的空白波形，不再弹出工程配置页。
  标尺右上角增加 `End` 直接输入；合法时长由 `ChangeScenarioDurationCommand` 提交，缩短到
  现有 Segment/Event/Marker 之前会原处报错，Esc 仅取消输入。
- 状态栏增加常驻保存状态：`Not saved`、`Unsaved changes`、`Saved`、
  `Recovery loaded · Save required`。恢复快照与正式保存不再混淆，首次保存 Untitled 工程从
  文件名推断项目名。
- 删除显式 Edit 按钮，波形编辑成为无模式默认行为；Cursor 收敛并更名为临时 `Measure`，
  再次点击或 Esc 返回直接编辑。工具栏只保留 Measure、Zoom in/out、Fit scenario；Export
  移至 File 菜单，Fit selection 不再占据主工具栏。
- Bus 浮层改为“上下文 + 直接值输入 + 0/X/Z/Don't care”。非法输入保留浮层、焦点与错误提示，
  正确输入按 Enter 即提交；旧 `Reserved` 扩展继续兼容为零值。Bit 悬浮拍显示翻转目标，Bit、
  Bus、Segment 与边沿提交后均显示时间范围、结果和 Ctrl+Z 提示。
- 保持此前 7 像素轻吸附、透明选中背景、Bus 隐式 X、Bit 单拍/多拍翻转、Segment 主体/边界
  编辑和局部虚线边沿预览；所有 GUI 自动化继续使用 `QT_QPA_PLATFORM=offscreen`，未操作桌面。
- 新增独立 `wave-user-journey-smoke`：从真实空白工程完成 CLK/Bit/Bus 内联创建、Bit 单拍编辑、
  Bus 非法值纠错与直接赋值、时间轴非法缩短与合法延长、Measure 拖动与 Esc 退出、Save As、
  文件回读及 1440×900 截图。该测试与开发回归分开执行。

开发视角验收：

```text
cmake --build build --parallel 4
Result: success

cmake --build build/qtcreator-debug --parallel 4
Result: success; wave-workbench/wave-tests and all CLI targets available

Core test executable: 25/25 passed
Million-transition metric: 26 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j 4
21/21 tests passed
Total Test time: 2.98 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure -j 4
21/21 tests passed
Total Test time: 2.72 sec

git diff --check
Result: clean
Desktop interaction: none
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R ^wave-user-journey-smoke$ --output-on-failure
1/1 passed
Total Test time: 0.36 sec

Saved project: build/qtcreator-debug/user-journey.wave.json
Screenshot: build/qtcreator-debug/user-journey-smoke.png (1440×900)
Visual QA: direct toolbar, three created lanes, transparent grid, Bus X baseline,
           500 ns End control, edit cursor and Saved state are legible
Desktop interaction: none
```

## 持续迭代 15：信号名就地重命名

状态：完成

已交付：

- 左侧信号名双击或选中后按 `F2` 直接出现 lane 内编辑框，不再打开模态对话框。Enter、点击画布
  其他位置或真实失焦均提交，Esc 取消且不修改模型。
- 空名与不区分大小写的重名会在原编辑框中提示、保留焦点和草稿；合法名称通过单个
  `ChangeLaneCommand` 提交，状态栏明确显示结果与 `Ctrl+Z`，Undo/Redo 均保持单步语义。
- 保存、另存为、新建、打开、关闭、快速添加和画布右键会先同步提交重命名；校验失败时原地
  阻止后续动作。提交使用 ID/文本副本且不重置视口，避免成员清空或同次点击坐标变化。
- `wave-canvas-add-lane-smoke` 覆盖双击、F2、Esc、空名/重名、真实失焦、快速添加门禁、
  Enter、点击别处及 Undo/Redo；独立用户旅程验证重命名后的保存回读。

开发视角验收：

```text
cmake --build build --parallel 4
Result: success

cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 25/25 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j 4
21/21 tests passed
Total Test time: 2.66 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure -j 4
21/21 tests passed
Total Test time: 2.70 sec

git diff --check
Result: clean
Desktop interaction: none
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R ^wave-user-journey-smoke$ --output-on-failure
1/1 passed
Total Test time: 0.28 sec

Saved project: build/qtcreator-debug/user-journey.wave.json
Screenshot: build/qtcreator-debug/user-journey-smoke.png (1440×900)
Visual QA: req_valid rename, three created lanes, transparent grid, Bus X baseline,
           500 ns End control, edit cursor and Saved state are legible
Desktop interaction: none
```

## 持续迭代 16：未提交草稿统一收口

状态：完成

已交付：

- 新建、打开、关闭、保存、另存为、导出、快速添加、重命名和画布点击统一经过就地草稿门禁。
  有效的创建、信号名、Bus 值与 End 草稿先提交；非法草稿保留文本、位置与焦点并阻止后续动作，
  不再出现界面显示新值而保存旧模型的状态。
- Bus 与 End 同时待提交时采用条件顺序：Bus 仍在当前 End 内时先提交 Bus 再校验 End，防止缩短
  后位置错移；Undo/Redo 使 Bus 原位置超出 End 时先允许合法的 End 扩展，再将 Bus 写回原 anchor。
  End 仍不足或非法时原位阻断，绝不进入末拍钳制。
- End 的 Enter、Ctrl+S、真实鼠标失焦和点击画布共用同一管线。模型刷新保留 modified 草稿；
  提交具有重入保护，鼠标导致缩放变化时只吞掉触发该变化的画布输入，转焦 Bus 或其他控件时
  不残留误吞标志。
- 隐藏的 Bus 草稿仍按 lane ID、原 anchor 与 modified 状态识别；非法值或越界位置会重新显示浮层、
  聚焦输入框并给出恢复方式。快速添加提交信号使用字段副本，避免同步回调清空成员后产生悬空引用。
- `wave-canvas-add-lane-smoke` 新增点击别处提交、隐藏有效/非法草稿、Undo 后越界阻断、延长 End
  原位置恢复与两步 Undo；`wave-user-journey-smoke` 新增真实 Ctrl+S/Export 门禁、Bus→End 冲突、
  鼠标失焦坐标保护、End→Bus 焦点交接以及最终保存回读。

开发视角验收：

```text
cmake --build build --parallel 4
Result: success

cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 25/25 passed
Million-transition metric: 21 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j 4
21/21 tests passed
Total Test time: 2.67 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure -j 4
21/21 tests passed
Total Test time: 3.08 sec

git diff --check
Result: clean
Desktop interaction: none
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R ^wave-user-journey-smoke$ --output-on-failure
1/1 passed
Total Test time: 0.32 sec

Saved project: build/qtcreator-debug/user-journey.wave.json
Saved model: req_valid; durationTick=650000;
             data 60000..80000=0x1234, 260000..280000=0xabcd,
             460000..480000=0xbeef, 480000..500000=0xcafe
Screenshot: build/qtcreator-debug/user-journey-smoke.png (1440×900)
Visual QA: direct toolbar, three lanes, transparent grid, 650 ns End, Bus values,
           edit cursor and Saved state are legible
Desktop interaction: none
```

## 持续迭代 17：快速新增事务隔离与拖动取消

状态：完成

已交付：

- 快速新增在输入条关闭前保持为单一事务。删除、重排、参数编辑、分组、事件/关系、范围粘贴、
  End 和已接入的画布动作会先尝试完成新增；有效值原子补齐原 `AddLaneCommand` 后再继续，无效值
  原处保留并阻断，不再允许后续命令插入待完成的新增命令之后。
- 开始快速新增会关闭并清空 Bus 便携面板；Bus 直接值、预设、Insert Pulse 与 Segment 双击入口均有
  防御性门控。双击事件会统一处理 Quick/Rename/End/Bus 草稿，避免 Qt 双击序列绕过首次按下门禁。
- 命令栈新增基线恢复原语。若历史状态中已存在交错命令，Ctrl+Z 每次只撤销并丢弃基线之后的
  最新一项；全部后续项清除后，再按一次取消待新增信号。恢复过程同步还原 dirty/autosave 状态，
  不会继续撤销事务之前的命令。
- 信号标题拖动期间按 Esc 会立即清除拖动状态和插入位置；该行为不依赖 Wave Edit/Measure 模式，
  随后释放鼠标不会提交 `MoveLaneCommand`，正常拖动与 Undo/Redo 保持原行为。
- `wave-canvas-add-lane-smoke` 覆盖 Bus 面板替换、隐藏控件隔离、无效新增下 Segment 双击与删除阻断、
  Esc 恢复、有效新增先提交后删除、两步 Undo 以及 Marker 模式标题拖动取消；核心测试覆盖两个
  交错命令逐次恢复与基线边界。全部 GUI 路径使用 `QT_QPA_PLATFORM=offscreen`，未操作桌面。

开发视角验收：

```text
cmake --build build --parallel 4
Result: success

cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 25/25 passed
Million-transition metric: 25 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
21/21 tests passed
Total Test time: 7.37 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.66 sec

git diff --check
Result: clean
Desktop interaction: none
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R ^wave-canvas-add-lane-smoke$ --output-on-failure
1/1 passed
Total Test time: 0.42 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R ^wave-user-journey-smoke$ --output-on-failure
1/1 passed
Total Test time: 0.41 sec

Saved project: build/qtcreator-debug/user-journey.wave.json
Saved model: req_valid; durationTick=650000;
             data 60000..80000=0x1234, 260000..280000=0xabcd,
             460000..480000=0xbeef, 480000..500000=0xcafe
Screenshot: build/qtcreator-debug/user-journey-smoke.png (1440×900)
Visual QA: direct toolbar, three lanes, transparent grid, 650 ns End, Bus values,
           edit cursor and Saved state remain legible; no visual regression observed
Desktop interaction: none
```

## 持续迭代 18：持久范围选择与同类型批量赋值

状态：完成

已交付：

- `Shift` + 拖动在一个或多个 lane 上建立显式、持久的时间范围；选择覆盖层保持低透明度，
  不遮挡刻度和波形。缩放、Fit 和模型刷新后重新计算位置，删除、隐藏或失效 lane 时自动清除。
- 同类型 Bit/Bus 选择在附近显示轻量面板。Bit 支持 `0`、`1`、`X`、`Z` 按钮与键盘整段赋值；
  Bus 支持直接值及 `0`、`X`、`Z`、`Don't care`，多条不同位宽 Bus 按各自宽度生成预设值。
  混合类型或不支持的选择仅保留 Copy，不提供赋值入口。
- 新增 `SetLaneRangesCommand`，先验证所有 lane、范围和值，再以单个场景快照提交；任一目标不兼容
  时零写入且不产生历史项。成功批量赋值、Undo 和 Redo 后均保留选择。
- Esc 在画布或 Bus 输入框聚焦时均清除范围；第一次点击范围外只清除选择，不触发 Bit 翻转或
  Segment 编辑。隐藏的 Bus 草稿不会泄漏到后续选择，错误提示在纠正成功后复位。
- 核心测试覆盖多 Bit、不同位宽 Bus、完整区间与相邻值、原子失败和单步 Undo/Redo；
  `wave-wave-edit-smoke` 覆盖实际面板、键盘、缩放/Fit、模型刷新、混合选择与清除行为。
  全部 GUI 路径使用 `QT_QPA_PLATFORM=offscreen`，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 23 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.53 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.15 sec

git diff --check
Result: clean
Desktop interaction: none
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 0.65 sec

Screenshots:
  build/qtcreator-debug/wave-edit-smoke-range-selection.png
  build/qtcreator-debug/user-journey-smoke.png
Visual QA: 持久选择范围、同类型批量操作面板、波形与网格均清晰可辨；
           用户旅程的创建、编辑、保存状态无视觉回归
Desktop interaction: none
```
## 持续迭代 19：波形 Event 与 Relation 一致性

状态：完成

已交付：

- `synchronizeLaneEventsFromSegments` 不再先删除全部失配 Event。重分段时依次按 Segment ID、
  同 tick/value、同 tick 复用原 waveform-linked Event；边沿仍存在时稳定 Event ID 和 Relation
  端点保持不变，只更新新的 linked Segment ID、值与时间。
- 仅当波形边沿确实消失时，精确删除 source 或 target 引用该 Event 的 Relation；同一 Relation
  两端同时失效也只删除一次，无关 Relation 原对象保留。linked Event 和普通 Event 的显式删除
  同样清理依赖关系，所有路径均由场景快照支持完整 Undo/Redo。
- 批量范围赋值若删除依赖 Relation，状态栏明确显示 Relation 数量、删除原因以及 `Ctrl+Z` 会同时
  恢复波形和关系；未删除关系时保留原成功提示。
- 核心回归使用非真空夹具覆盖 source 消失、target 消失、同 tick 重映射、无关 Relation 保留、
  Event ID 唯一、linked Event 与 Lane/Segment/tick/value 一致，以及 linked/普通 Event 删除。
  `wave-wave-edit-smoke` 覆盖真实 Shift 范围赋值、状态提示、Relation 清理和 Undo。
  全部 GUI 路径使用 `QT_QPA_PLATFORM=offscreen`，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 19 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.44 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.06 sec

git diff --check
Result: clean
Desktop interaction: none
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 0.60 sec

Screenshots:
  build/qtcreator-debug/wave-edit-smoke-relation-cleanup.png
  build/qtcreator-debug/user-journey-smoke.png
Visual QA: 80–150 ns 批量赋值、选择范围与波形清晰；状态栏明确显示删除 1 个 Relation、
           原因及 Ctrl+Z 恢复波形和关系；完整用户旅程无视觉回归
Desktop interaction: none
```
## 持续迭代 20：范围赋值栏移出波形画布

状态：完成

已交付：

- 持久范围选择的 Bit/Bus/混合类型操作栏从 WaveCanvas viewport 浮层移到顶部
  `WaveformToolbar`，位于 Measure 之后、缩放和 Fit 之前；显隐、缩放、水平滚动和 Fit
  均不再覆盖波形或截获画布点击。
- Waveform 工具栏固定在顶部并禁止移动、浮动或隐藏；预留最大工具栏高度，范围栏显示和隐藏时
  toolbar 高度与 viewport 全局起点保持不变，消除原先的 8 像素画布跳动。
- Bit 四态批量赋值、Bus 直接值与 0/X/Z/Don't care、混合类型只读、Esc、范围外点击及
  Undo/Redo 保持原行为。无效 Bus 草稿会阻止进入 Measure，并保留文本、选择与焦点。
- `wave-wave-edit-smoke` 增加真实控件命中、960 像素宽度双重 containment、旧浮层区域点击、
  工具栏显隐、缩放/滚动/Fit、窗口缩放恢复及纵向布局稳定回归。全部 GUI 路径使用
  `QT_QPA_PLATFORM=offscreen`，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.26 sec

cmake --build build/qtcreator-release --parallel
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.04 sec

git diff --check
Result: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 0.66 sec

Screenshots:
  build/qtcreator-debug/wave-edit-smoke.png
  build/qtcreator-debug/wave-edit-smoke-range-selection.png
  build/qtcreator-debug/user-journey-smoke.png
Visual QA: 范围栏隐藏和显示时画布起点均为 y=68；范围栏不覆盖波形；
           960 像素宽度下 Measure 与全部 Bus 范围控件可见、可命中、可操作
Desktop interaction: none
```

## 持续迭代 21：持久范围复制闭环

状态：完成

已交付：

- 开发审计确认固定范围栏的文案与行为不一致：混合类型显示 `Copy only`，但没有可见 Copy
  按钮，仍展示不可用的 0/1/X/Z 控件，用户只能猜测 Ctrl+C 或进入 Edit 菜单。
- 固定范围栏新增始终可见的 `Copy` 按钮。Bit、Bus 和混合选择均可直接复制完整 lane 集合与
  整数时间宽度，复用既有 Wave Workbench range MIME 和状态栏成功反馈。
- 混合或不支持类型的范围选择隐藏值输入及全部赋值按钮，只保留上下文和 `Copy`；Bit/Bus
  继续显示各自有效控件。960 像素宽度下 Copy 也纳入包含、命中和无裁切检查。
- `wave-wave-edit-smoke` 使用真实鼠标事件点击 Copy，解析 clipboard JSON 并核对 lane 数量与
  durationTick，同时断言选择和模型不变；新增混合范围 Copy 状态截图。全部 GUI 路径继续使用
  `QT_QPA_PLATFORM=offscreen`，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.44 sec

cmake --build build/qtcreator-release --parallel
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.12 sec

git diff --check
Result: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 0.68 sec

Screenshots:
  build/qtcreator-debug/wave-edit-smoke-range-selection.png
  build/qtcreator-debug/wave-edit-smoke-mixed-copy.png
  build/qtcreator-debug/user-journey-smoke.png
Visual QA: Copy 在范围栏中可见且未造成工具栏拥挤；混合选择只保留 Copy；
           赋值控件无残留，波形、刻度与选择范围仍清晰
Desktop interaction: none
```

## 持续迭代 22：右键就地粘贴闭环

状态：完成

已交付：

- 开发审计确认 `pasteAtCursor` 仅由 Edit 菜单和 Ctrl+V 暴露。用户点击 Copy 后还要先清除旧范围、
  再定位光标、再记忆快捷键；第一次右键只清除范围而不打开菜单，形成额外一次操作。
- 当 clipboard 含 Wave Workbench range MIME 时，波形右键菜单顶部显示
  `Paste copied range here`。右键位置直接成为目标时间；存在旧范围时同一次右键先清除范围并
  继续打开菜单，不再要求第二次右键。
- 粘贴成功后状态栏显示 lane 数、目标时间、实际宽度及 `Ctrl+Z`；仍通过单个
  `PasteRangeCommand` 提交。无兼容 range 时不显示该入口，原有 Bit/Bus/Clock 菜单保持不变。
- `wave-wave-edit-smoke` 在混合范围 Copy 后通过真实 `QContextMenuEvent` 打开菜单、选择 Paste，
  验证目标范围、模型变化、范围栏关闭、成功反馈及单步 Undo 完整恢复。全部 GUI 路径继续使用
  `QT_QPA_PLATFORM=offscreen`，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 9.92 sec

cmake --build build/qtcreator-release --parallel
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.69 sec

git diff --check
Result: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 0.70 sec

Interaction QA: Copy 后第一次右键即出现 Paste copied range here；
                粘贴到点击时间并显示明确反馈，Ctrl+Z 单步恢复
Screenshots:
  build/qtcreator-debug/wave-edit-smoke-mixed-copy.png
  build/qtcreator-debug/user-journey-smoke.png
Desktop interaction: none
```

## 持续迭代 23：粘贴结果持续可见

状态：完成

已交付：

- 用户审计确认 Paste 已修改模型，但 `pasteAtCursor` 随即关闭显式范围选择；多 lane 粘贴结果没有
  持续高亮，固定范围栏消失，用户只能依赖短暂状态消息确认是否生效。
- 粘贴完成后将目标时间范围和实际存在的 lane 保持为显式选择；刷新后自动显示固定范围栏。
  同类型结果可立即批量编辑，混合结果保持 Copy-only，用户无需重新框选。
- Undo 后目标选择继续保留，便于比较粘贴前后或 Redo；Esc 仍统一清除选择和范围栏。粘贴状态栏
  的 lane 数、目标时间、宽度和 Undo 提示保持不变。
- `wave-wave-edit-smoke` 验证 Paste 后范围、工具栏、上下文和纵向布局持续可见，Undo 完整恢复模型
  且不丢选择，Esc 最终收口；新增 `wave-edit-smoke-pasted-range.png`。全部 GUI 路径继续使用
  `QT_QPA_PLATFORM=offscreen`，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel
Result: success

Core test executable: 26/26 passed
Million-transition metric: 21 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.33 sec

cmake --build build/qtcreator-release --parallel
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.15 sec

git diff --check
Result: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 0.71 sec

Screenshots:
  build/qtcreator-debug/wave-edit-smoke-pasted-range.png
  build/qtcreator-debug/user-journey-smoke.png
Visual QA: 粘贴目标范围持续高亮，Copy-only 范围栏保持可见；
           波形、刻度和状态反馈均清晰，无画布遮挡或纵向跳动
Desktop interaction: none
```

## 持续迭代 24：范围清除闭环与关系安全撤销

状态：完成

已交付：

- 开发审计确认显式范围选中后 `Delete` / `Backspace` 无响应；用户只能逐个 Segment 清除，且混合
  Bit/Bus 范围工具栏仅显示 Copy，没有可发现的清除动作。单信号 `ClearLaneRangeCommand` 还只保存
  Segment 与 Event，事件同步删除的 Relation 无法由 Undo 恢复。
- 固定范围工具栏新增可见 `Clear`，同类型和混合类型选择均可直接清除；`Delete` / `Backspace`
  执行相同行为。成功后保持范围、lane 集合和工具栏，不要求用户重新框选；无显式值的范围给出
  “already uses implicit values”反馈且不污染命令历史。
- 新增 `ClearLaneRangesCommand`，在一个命令中清除所有实际相交的非 Group lane；完整 Scenario
  快照保证 Segment、Event、Relation 一起 Undo/Redo。单 lane Clear 同步改用完整快照，修复关系
  随边沿删除后 Undo 不恢复的问题。
- `wave-core-tests` 验证两 lane 清除只产生一条历史、隐式区间、Relation 精确删除和完整 Undo/Redo，
  并单独覆盖原单 lane Clear 的关系恢复；`wave-wave-edit-smoke` 真实点击 Clear、按 Delete、验证选区
  保持与两级原子 Undo。全部 GUI 路径继续使用 `QT_QPA_PLATFORM=offscreen`，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.33 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.15 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 0.74 sec

Screenshot:
  build/qtcreator-debug/wave-edit-smoke-pasted-range.png
Visual QA: 固定范围栏同时显示 Copy 与 Clear；混合范围动作可发现且可命中；
           范围高亮、刻度和波形无覆盖，清除后上下文持续保留
Desktop interaction: none
```

## 持续迭代 25：标准范围 Cut 与文本焦点保护

状态：完成

已交付：

- 用户审计确认移动一段波形需要依次 Copy、Clear、Paste；缺少标准 Cut，使一次常规移动多一步，
  且用户可能在清除源区间前改变选择。Edit 菜单也没有 `Ctrl+X`。
- 固定范围工具栏在 Copy 与 Clear 之间新增可见 `Cut`；同类型和混合类型范围均可直接使用。
  Cut 先写入与 Copy 相同的整数 tick/MIME 数据，再通过现有原子范围清除命令删除源值；成功后
  保持范围、lane 集合和工具栏，状态栏明确显示源区间可由 `Ctrl+Z` 恢复。移动范围缩为 Cut 后
  在目标时间 Paste 两步。
- Edit 菜单新增 `Cut range` 与标准 `Ctrl+X`。为避免全局 QAction 劫持内联值、名称或时长编辑，
  Cut/Copy/Paste 在 `QLineEdit` 获得焦点时分别执行文本 cut/copy/paste；其余焦点才路由到波形范围。
- `wave-app-smoke` 验证 Cut QAction 与标准快捷键；`wave-wave-edit-smoke` 在 960 像素窗口断言
  Copy/Cut/Clear 全部可见可命中，真实点击 Cut 后验证剪贴板、源清除、选区保持和单步 Undo，并
  验证 Bus 值输入框中的 Cut 不修改 Scenario。全部 GUI 路径继续使用 offscreen，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 21 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.46 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.04 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-app-smoke|wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
3/3 passed
Total Test time: 1.06 sec

Screenshot:
  build/qtcreator-debug/wave-edit-smoke-pasted-range.png
Visual QA: 固定范围栏按 Copy / Cut / Clear 排列，三项动作均可见可命中；
           范围上下文、刻度和波形无覆盖，960 像素窗口布局稳定
Desktop interaction: none
```

## 持续迭代 26：持久范围端点就地修正

状态：完成

已交付：

- 开发审计确认 Shift 拖动建立范围后没有可操作的端点；边界偏差一拍时只能重新跨时间和 lane
  框选，普通点击还会先清除整个选择。用户需要重复定位两个时间点和两条信号。
- 持久范围的左右边界新增可见手柄。任一已选 lane 上都可直接拖动端点；拖动期间显示虚线预览，
  释放后保持原 lane 集合、固定范围工具栏与上下文。端点调整只修改选择状态，不写 Scenario，
  不新增 Undo 历史。
- 端点拖动复用无设置项的 7 像素轻吸附，可对齐刻度、Clock 边沿和信号边沿；按住 `Alt` 临时
  绕过。状态栏实时显示范围，并在释放后明确报告起点、终点、宽度及信号数量。
- `wave-wave-edit-smoke` 通过真实鼠标事件分别拖动左右手柄，验证精确范围、lane 集合、工具栏、
  状态反馈和 Scenario 完全不变，再恢复原范围供后续批量编辑。全部 GUI 路径继续使用 offscreen，
  未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.42 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.02 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 0.71 sec

Screenshot:
  build/qtcreator-debug/wave-edit-smoke-range-selection.png
Visual QA: 两路范围的左右手柄清晰可见；范围高亮保持低透明度，刻度、波形与固定操作栏无遮挡；
           边界修正后无需重新选择信号或重开工具栏
Desktop interaction: none
```

## 持续迭代 27：范围选择后的同次安全重定向

状态：完成

已交付：

- 开发与用户审计确认显式范围存在时，所有普通左键首击都被“仅清除范围”吞掉。用户点击信号名
  选信号或点击标尺移动编辑光标时，必须重复点击一次；这两处本身不会修改波形，额外防护没有收益。
- 信号标题和时间标尺改为同一次点击先清除旧范围，再继续完成 lane 选择或编辑光标定位。范围栏
  同步隐藏，状态栏显示目标信号或目标时间以及 `range cleared`，用户能确认两项结果均已生效。
- 波形正文仍保留第一次点击只清除范围、不执行 Bit 翻转或 Segment 编辑的防误触策略；范围左右
  手柄命中优先级不变。此次改动只调整 UI 选择状态，不写 Scenario，也不新增 Undo 历史。
- `wave-wave-edit-smoke` 通过真实鼠标事件验证一次标题点击和一次标尺点击的完整结果、工具栏隐藏、
  状态反馈和 Scenario 不变，并继续执行旧波形正文点击回归。全部 GUI 路径使用 offscreen。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 22 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.32 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.08 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 0.73 sec

Screenshot:
  build/qtcreator-debug/wave-edit-smoke-range-ruler-retarget.png
Visual QA: 范围栏已在同次标尺点击后隐藏；60 ns 编辑光标和纵向参考线立即落位；
           状态栏明确显示 “Edit cursor 60 ns · range cleared”，波形、刻度与布局无跳动
Desktop interaction: none
```

## 持续迭代 28：Bit 写值与清除保持拍级选择

状态：完成

已交付：

- 用户审计确认 Bit 单击翻转已保持一拍，但随后按 `0/1/X/Z` 或通过右键 `Set beat` 写值时，
  通用选择逻辑会重新选中合并后的整段 Segment。用户再按 Delete 可能清除相邻同电平拍，
  与“单 Bit 操作始终一拍”的既有规则冲突。
- Bit 的键盘与右键写值现在保留请求的拍级时间范围，并明确清空 Segment 选择；右键打开后即使
  取消，也只保持当前拍。Bus、Clock 与其他非 Bit lane 仍沿用完整 Segment 选择和边界编辑。
- `Delete` / `Backspace` 对 Bit 拍级选择清除到隐式 0；右键动作改名为
  `Clear beat to implicit 0` 并执行同一逻辑。单拍或拖动形成的多拍范围均保持选择，成功反馈包含
  时间范围和 `Ctrl+Z`；已经是隐式 0 时不产生空 Undo。
- `wave-wave-edit-smoke` 真实执行键盘写值、右键 Set X、右键 Clear、Delete 及 Undo/Redo，
  验证相邻拍不变、选择从未扩展、Scenario 精确往返。全部 GUI 测试继续使用 offscreen。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.34 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.13 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 0.79 sec

Screenshot:
  build/qtcreator-debug/wave-edit-smoke-bit-beat-selection.png
Visual QA: req 的 60–70 ns 单拍单独高亮并显示 X；左右相邻 0 电平拍未进入选择；
           状态栏明确显示一拍范围、结果与 Ctrl+Z，波形刻度和布局无遮挡
Desktop interaction: none
```

## 持续迭代 29：Bit 拍级选择脱离悬浮后持续可见

状态：完成

已交付：

- 用户审计确认第 28 轮已保留拍级选择状态，但画布只绘制 hover 覆盖层。鼠标离开画布后高亮消失，
  Delete 和键盘写值仍作用于原拍，用户无法确认当前操作目标。
- 非显式范围、非 Segment 的 Bit 时间选择现在拥有独立持久选中框；hover 相同拍时不重复叠加，
  鼠标 `Leave` 只清理悬浮与吸附提示，不清理已选拍。单拍和拖动形成的多拍选择均适用。
- 跨信号 `revealLocation`、右键信号标题和开始拖动 Bit Event 边沿时清理旧 Wave Edit 选择，
  防止持久选中框被错误带到新 lane 或与边沿操作同时显示。普通中键平移仍保留当前选择。
- `wave-wave-edit-smoke` 在写入 X 后发送真实 `Leave`，断言 hover 已空、拍级选择仍存在并继续完成
  Clear/Delete；新增无 hover 状态截图。全部 GUI 路径继续使用 offscreen。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 21 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.38 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.11 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 0.78 sec

Screenshot:
  build/qtcreator-debug/wave-edit-smoke-bit-beat-persistent-selection.png
Visual QA: 鼠标已离开且无吸附提示线时，req 的 60–70 ns 单拍选中框仍清晰可见；
           左右相邻拍未被覆盖，状态栏、波形、刻度和布局保持一致
Desktop interaction: none
```

## 持续迭代 30：撤销/重做与 Segment 清除反馈闭环

状态：完成

已交付：

- 用户审计确认普通 `Ctrl+Z` / `Ctrl+Y` 只刷新波形，状态栏仍保留操作前消息，用户无法确认动作
  是否已经撤销或重做；非 Bit Segment 删除也只显示通用文本，不说明目标、结果或依赖变化。
- MainWindow 在执行前读取命令栈描述；成功撤销后显示 `Undid <command> · Ctrl+Y to redo`，
  成功重做后显示 `Redid <command> · Ctrl+Z to undo`。Edit 菜单与键盘快捷键共用同一反馈。
- Segment 清除反馈包含信号名、精确时间范围和隐式结果：Bit 为 0，Bus/Enum/Transaction/Event
  为 X，Clock 为正常时钟波形。若边沿消失导致 Relation 被移除，状态栏说明数量并明确
  `Ctrl+Z` 会同时恢复波形和关系。
- `wave-user-journey-smoke` 以真实 `Ctrl+Z` / `Ctrl+Y` 往返一拍 Bit 编辑并断言模型与消息；
  `wave-wave-edit-smoke` 为 Bus Segment 建立临时 Relation，验证 Delete 清除、依赖提示、
  Undo/Redo 精确往返及最终恢复。全部 GUI 路径继续使用 offscreen。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 18 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.44 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.21 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 1.24 sec

Screenshot:
  build/qtcreator-debug/wave-edit-smoke-segment-clear-feedback.png
Visual QA: data[7:0] 的 100–170 ns 清除区间显示居中红色虚线 X；状态栏同时显示信号、时间范围、
           隐式 X、移除 1 条 Relation 及 Ctrl+Z 同步恢复说明，波形和刻度无遮挡
Desktop interaction: none
```

## 持续迭代 31：单信号写值关系安全与一拍 Pulse 一致性

状态：完成

已交付：

- 开发审计确认 `SetLaneRangeCommand` 只保存 Segment 与 Event；一拍写值、Pulse 或 Bus 快捷值若令
  被 Relation 引用的边沿消失，关系会被清理，但 `Ctrl+Z` 不会恢复。命令现在同时保存变更前后的
  Relation 集合，首次执行、Undo 与 Redo 对 Scenario 的波形、Event 和 Relation 保持一致。
- 用户审计确认 `Insert one-beat pulse` 使用 Clock 周期或主刻度宽度，而 Bit 单击、悬浮和键盘写值
  使用画布统一拍范围。同一位置可能得到 60–80 ns Pulse，而界面指示的一拍是 60–70 ns。Pulse
  现在直接复用 `beatRangeAt`，提交后保持精确拍级选择，并显示信号、范围、结果与撤销提示。
- Bus 预设、键盘/右键单信号写值、双击 Segment 改值均显示实际结果；非法输入明确说明未修改，
  双击输入规范化后与原值相同时不产生空历史。若写值删除关系，反馈说明数量及撤销会同步恢复。
- 核心回归新增单信号范围写值导致 Relation 清理后的完整 Scenario Undo/Redo；Wave Edit smoke
  通过真实右键菜单插入 req 60–70 ns Pulse，验证相邻拍不变、拍级选择、状态反馈以及精确
  Undo/Redo。全部 GUI 路径使用 offscreen，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 21 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.31 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.09 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 0.82 sec

Screenshot generated:
  build/qtcreator-debug/wave-edit-smoke-pulse-feedback.png
Automated QA: req 的 60–70 ns 单拍变为 1，55 ns 与 75 ns 相邻拍保持 0；选择范围、状态反馈和
              完整 Scenario Undo/Redo 均通过断言
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 32：拍级翻转与清除的依赖反馈闭环

状态：完成

已交付：

- 开发审计确认单拍/多拍 `ToggleBitRangeCommand` 与拍级 `ClearLaneRangeCommand` 已保存完整
  Scenario，因边沿消失而删除的 Relation 可以 Undo；但 `commitBitToggle` 与
  `clearSelectedBitRange` 只显示波形值或隐式 0，不说明依赖同步变化。Segment 移动/缩放也存在
  同类反馈分叉。
- 用户主流程中，单击一拍后依赖线可能直接消失。原状态栏只显示 `0 → 1` 或 `cleared to implicit 0`，
  用户无法判断是依赖清理还是误删。本轮统一复用 relation-aware 反馈：显示删除数量、边沿消失原因，
  并明确 `Ctrl+Z` 同时恢复波形与 Relation，不增加确认框或额外操作。
- 单拍/多拍翻转、拍级 Clear/Delete、Segment 清除及主体/边界编辑现在共用一致反馈；原有拍级选择、
  悬浮、相邻拍隔离和命令语义不变。
- 核心回归验证一拍 Toggle 删除 `relation-req-ack` 后完整 Scenario Undo/Redo；Wave Edit smoke
  通过真实单击、真实右键 Clear、临时 Relation 和 MainWindow Undo/Redo 验证两条高频路径。
  全部 GUI 路径使用 offscreen，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 24 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.38 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.34 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 1.35 sec

Screenshot generated:
  build/qtcreator-debug/wave-edit-smoke-bit-dependency-feedback.png
Automated QA: req 的 60–70 ns 拍级 Clear 保持精确选中且相邻拍不变；状态栏显示隐式 0、
              删除 1 条 Relation 及 Ctrl+Z 同步恢复说明；Toggle 与 Clear 均精确 Undo/Redo
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 33：重复写值的无效果命令收口

状态：完成

已交付：

- 开发审计确认 `CommandStack::execute` 无条件清除 redo 尾部并加入新命令；`SetLaneRangeCommand`
  即使最终波形相同，也会重建 Segment stable ID、同步 Event 并形成空 Undo。批量范围赋值存在同样问题。
- 用户主流程中，在已经为 X 的拍或范围上再次写 X 会误报修改并标脏。随后第一次 `Ctrl+Z` 波形不变，
  用户必须再次撤销才能回到上一真实状态；若此前处于 Undo 后，该空操作还会丢失 Redo。
- 命令增加实际效果判定。单信号与批量范围写值先检查规范化值、完整 Segment 覆盖及扩展语义；
  完全相同时不重建 Segment/Event ID。命令栈先安全执行，再仅对真实变化截断 redo 并入栈。
- 单信号键盘/右键值、Bus 预设和多 lane 范围赋值在无效果时保持当前选择，显示
  `already = … · no values changed`，不发送模型修改或命令可用性变化。隐式区间写入显式值仍属于真实编辑。
- 核心回归覆盖单信号/批量同值写入、Scenario 与 stable ID 不变、零历史及 Undo 后 Redo 保留；
  Wave Edit smoke 通过真实重复按 X、重复点击范围 X、一次 Undo/Redo 和状态消息验证用户路径。
  全部 GUI 路径使用 offscreen，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.73 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.54 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 0.98 sec

Screenshot generated:
  build/qtcreator-debug/wave-edit-smoke-no-effect-write-feedback.png
Automated QA: req 的 60–70 ns 仍为 X 且保持拍级选择；状态栏显示 already = X/no values changed；
              一次 Undo 直接恢复上一真实 0 值，Redo 精确返回，批量范围重复 X 亦不占历史
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 34：Clock Run 的结果反馈与空清除收口

状态：完成

已交付：

- 开发审计确认 `ClearLaneRangeCommand` 与 `ClearLaneRangesCommand` 始终被命令栈视为有效；
  Clock 右键 `Run for one period` 在本来没有覆盖的拍上也会加入空 Undo。该入口只刷新模型，
  不显示目标范围、恢复结果或无变化原因。
- 用户主流程中，选择 Run 后菜单直接消失，用户无法确认 gated/disabled 覆盖是否清除；再次选择
  Run 会得到不可见历史项，第一次撤销看不到任何波形变化。
- 单/批量 Clear 命令现在按完整 Scenario 前后快照报告真实效果；空清除不入栈并保留已有 Redo。
  Clock Run 成功时显示信号、精确一拍范围、`restored normal clock waveform` 与 `Ctrl+Z`；
  已正常时显示 `already uses normal clock waveform · no values changed`。
- Run 后清空 Segment 选择并保持所操作的一拍选中；成功路径才标记模型修改。Gate、Drive X、
  轻吸附和现有 Clock override 语义不变。
- 核心回归覆盖单/批量正常 Clock 空清除、覆盖清除、完整 Undo/Redo 及空清除后的 Redo 保留；
  Wave Edit smoke 真实执行右键 Gate→Run→重复 Run、一次 Undo/Redo、两次 Undo 恢复基线。
  全部 GUI 路径使用 offscreen，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.70 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.32 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 1.01 sec

Screenshot generated:
  build/qtcreator-debug/wave-edit-smoke-clock-run-no-effect-feedback.png
Automated QA: clk 的 60–70 ns 保持拍级选择且无覆盖；状态栏显示 already uses normal clock waveform/
              no values changed；一次 Undo 直接恢复 gated，Redo 精确返回 normal
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 35：快速参数编辑的真实结果与空历史收口

状态：完成

已交付：

- 开发审计确认画布左侧右键的 Clock/Bit/Bus 快速参数入口忽略 `CommandStack::execute()` 的结果；
  即使所有值保持不变，仍会调用 `markEdited()`、创建空 Undo 并触发未保存/autosave 状态。真实修改
  后只刷新画布，没有说明实际提交的参数。
- 用户主流程中，新建 Bus 后通常需要查看或修改位宽，新建 Clock 后需要确认周期。用户打开预填表单
  后直接确定，会被错误告知工程已修改；真正修改后只能从局部标题推断结果，无法确认 Undo 对应哪次操作。
- `ChangeLaneCommand` 与 `ChangeClockCommand` 现在根据规范化后的完整前后状态报告真实效果；无变化命令
  不入栈并保留既有 Redo。快速参数入口只在真实变化时刷新、标记未保存和安排 autosave。
- Clock 成功后显示信号名、规范化周期与 `Ctrl+Z`；Bit/Bus 成功后显示颜色、高度、Clock 关联以及
  Bus 的位宽、符号和进制。原值确认显示同一组当前参数与 `no properties changed`，不附错误撤销提示。
- 核心回归覆盖 Lane/Clock 原值确认、真实修改、完整 Undo/Redo 及空确认后的 Redo 保留；画布信号管理
  smoke 真实执行 Bus 8→16→原值确认→Undo/Redo，以及 Clock 默认周期→12 ns→原值确认→Undo/Redo。
  全部 GUI 路径使用 offscreen，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.77 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.46 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-canvas-add-lane-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 0.86 sec

Screenshot generated:
  build/qtcreator-debug/canvas-signal-management-smoke-quick-parameters-no-effect-feedback.png
Automated QA: Bus 显示 width 16/Ctrl+Z，原值确认显示 no properties changed；一次 Undo 直接恢复
              width 8，Redo 精确返回 16。Clock 显示 period 12 ns，原值确认及 Undo/Redo 同样精确
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 36：单 lane 目标感知 Paste 与空历史收口

状态：完成

已交付：

- 开发审计确认 `Paste copied range here` 只采用右键点击的时间，`pasteAtCursor()` 仍把 clipboard
  中的源 lane ID 直接作为目标；用户在 `ack` 上粘贴 `req`，实际修改的是 `req`。此外
  `PasteRangeCommand` 始终被视为有效，正常 Clock 空区间粘贴到另一正常区间也会制造空 Undo。
- 用户主流程中，“在此粘贴”的空间指向与真实结果矛盾；用户必须事后观察哪条波形改变。点击 Bus
  等不兼容信号时仍会静默写回源信号，而不是说明目标不可用；空粘贴后的第一次撤销没有可见变化。
- 单 lane clipboard 现在以当前点击或选中的 lane 为目标。类型必须一致，Bus/Enum 位宽必须一致，
  目标值还会按目标 lane 校验并规范化；不兼容时显示 source/target、类型或位宽原因，Scenario 不变。
  多 lane clipboard 保持原有完整源 lane 集合语义，避免隐式猜测配对。
- 成功跨 lane Paste 显示 `source → target`、目标时间、实际宽度及关系感知 Undo 提示，并保持实际目标
  范围选中。`PasteRangeCommand` 现在按完整 Scenario 前后快照报告真实效果；空 Paste 保持目标范围，
  显示 `already matches copied range · no values changed`，不标记修改、不入栈并保留 Redo。
- 核心回归覆盖空目标 Paste、真实目标写入、完整 Undo/Redo 以及空 Paste 后的 Redo 保留；Wave Edit
  smoke 真实执行 `req` Copy→Bit→Bus 拒绝→`ack` Paste→Undo、正常 Clock 空 Paste→Redo/Undo。
  全部 GUI 路径使用 offscreen，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 21 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.00 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.51 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 1.12 sec

Screenshots generated:
  build/qtcreator-debug/wave-edit-smoke-target-aware-paste.png
  build/qtcreator-debug/wave-edit-smoke-paste-no-effect-feedback.png
Automated QA: req 的 90–100 ns 内容仅写入 ack 的 50–60 ns，req 不变；Bit→Bus 显示类型拒绝；
              正常 Clock 空 Paste 显示 no values changed，一次 Redo 直接恢复真实跨 lane Paste
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 37：多 lane 显式目标 Paste 与可见入口

状态：完成

已交付：

- 开发审计确认第 36 轮仅使单 lane Paste 采用当前目标；多 lane clipboard 即使配合同数量显式目标
  选择，仍沿用源 lane ID。固定范围栏只有 Copy/Cut/Clear，用户必须记忆 `Ctrl+V`，也无法在提交前
  确认目标数量是否匹配。
- 用户主流程中，移动或复制两条关联波形到另外两条信号，需要先框选目标再粘贴；现有界面既没有
  Paste 按钮，也忽略已框选目标。用户只能逐 lane 操作，或接受内容回写到原信号。
- 固定范围栏在 Cut 与 Clear 之间新增可见 `Paste`，识别 Wave Workbench MIME 或等价文本 JSON；
  无有效 clipboard 时禁用并说明先 Copy，有效 clipboard 时 tooltip 显示复制/目标数量和目标起点。
  clipboard 改变时按钮状态即时刷新。
- 显式范围选择存在时，Paste 要求复制 lane 数与目标 lane 数完全相同，并按双方可见顺序一一映射。
  数量、类型、Bus/Enum 位宽或目标值校验任一失败时显示具体 source/target 原因，Scenario 零写入。
  右键多 lane Paste 未提供显式目标集合时继续保持原源集合，避免依据单个点击 lane 猜测批量配对。
- 成功后状态显示 `N copied signals → N selected signals`、目标时间、宽度及关系感知 Undo，目标范围
  持续选中。Wave Edit smoke 真实执行 req/ack 120–130 ns Copy、2→1 拒绝、reset/request 20–30 ns
  可见 Paste、原子 Undo/Redo，并将 Paste 纳入 960 像素全部可见/可命中/无裁切检查。
  全部 GUI 路径使用 offscreen，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 22 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.86 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.52 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 1.11 sec

Screenshot generated:
  build/qtcreator-debug/wave-edit-smoke-multi-target-paste.png
Automated QA: Paste 在固定范围栏可见可命中；2 个复制 lane 对 1 个目标明确拒绝；req/ack 按顺序
              写入 reset/request，源 ack 不变；Undo/Redo 原子恢复，960 像素下所有控件无裁切
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 38：自描述范围剪贴板与源删除后 Paste

状态：完成

已交付：

- 开发审计确认范围 clipboard 仅保存源 lane ID 和 Segment；Paste 即使已指定兼容目标，仍先要求源
  lane 存在。因此复制后删除、替换或跨场景重整信号列表会使已复制内容无条件失效。
- 用户主流程中，Copy 表示独立快照。用户会先复制波形，再删除临时信号或整理信号列表，随后把内容
  放到保留信号；要求先 Undo 删除才能取回剪贴板内容，既违反直觉，也破坏当前整理步骤。
- 新复制格式升级为 schema 2，每个 lane 快照携带名称、类型和位宽，并继续保存相对 Segment 与扩展。
  Paste 以快照元数据验证目标，不再读取当前源 lane；源被删除后，单 lane 右键目标和同数量显式多
  lane 目标仍可原子粘贴。没有目标且原 ID 已消失时零写入，并明确提示选择所需数量的目标信号。
- schema 1 继续受支持：源 lane 仍存在时从当前模型补全旧格式元数据，避免升级后丢失进程内旧剪贴板。
  schema 2 元数据缺失、类型错误或位宽非法会在写入前拒绝。
- Wave Edit smoke 真实执行 schema 1 目标 Paste，以及 req 90–100 ns Copy→确认删除 req→无目标恢复
  提示→右键 ack 50 ns→成功 Paste；随后分别 Undo Paste 和删除，完整恢复 Scenario。全部 GUI 路径
  使用 offscreen，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.91 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.77 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 1.14 sec

Screenshot generated:
  build/qtcreator-debug/wave-edit-smoke-source-deleted-paste.png
Automated QA: schema 2 快照含 req/bit/1-bit 元数据；删除 req 后直接 Paste 明确要求选择 1 个目标；
              右键 ack 后 90–100 ns 内容写入 50–60 ns；Paste 与删除可分别 Undo，旧 schema 1 可用
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```
## 持续迭代 39：Paste 完整宽度与 End 原子延长

状态：完成

已交付：

- 开发审计确认 `PasteRangeCommand` 将目标结束时间裁剪到 Scenario duration，超出 End 的相对 Segment
  也被跳过或截短；画布随后只显示实际剩余宽度，未明确说明复制内容已丢失。
- 用户主流程中，在时间轴尾部复用一段波形时，用户期待 Paste 保持复制宽度。静默截断会使波形看似
  成功但缺少尾部，必须逐段核对才能发现，属于高错误风险而非可接受的边界限制。
- Paste 现在允许目标起点等于 End，并在目标结束超过当前 duration 时先把 Scenario 延长到完整复制
  结束，再写入所有相对 Segment；加法溢出与真正位于 End 之外的起点仍在命令执行前拒绝。
- End 延长、波形写入、Event 同步和 Relation 变化保存在同一个完整 Scenario 命令快照中，一次
  Undo/Redo 同时恢复。没有越过 End 的重复 Paste 继续保持无效果命令抑制。
- 画布状态显示 `End extended to …` 和 `Ctrl+Z`，保留当前缩放比例，仅水平滚动到新尾部，粘贴后的
  完整目标范围继续显式选中。核心测试覆盖从 End 本身粘贴；Wave Edit smoke 从实际 End 前 5 ns
  粘贴 10 ns，验证完整宽度、新 End、尾部可见、目标值及单步 Undo。全部 GUI 路径使用 offscreen。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 19 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.44 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.90 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-wave-edit-smoke|wave-user-journey-smoke)$" --output-on-failure
2/2 passed
Total Test time: 1.19 sec

Screenshot generated:
  build/qtcreator-debug/wave-edit-smoke-paste-extends-end.png
Automated QA: 示例 End 220 ns；req 的 10 ns 快照从 215 ns 粘贴后完整选中 215–225 ns，
              End 变为 225 ns，新尾部滚入视野，状态明确提示，单步 Undo 恢复 End 与波形
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```
## 持续迭代 40：信号删除影响与恢复反馈

状态：完成

已交付：

- 开发审计确认 `RemoveLaneCommand` 已原子清理 Lane、所属 Event、引用这些 Event 的 Relation 和
  trace mapping，但主窗口确认框只给出泛化描述；完成删除后没有新状态消息，可能继续显示上一操作。
- 用户主流程中，删除信号是高风险操作。用户需要在确认前知道会连带清理多少可见 Event/Relation，
  完成后确认删除确实生效并知道如何恢复；仅看到波形消失不足以判断关系是否一并清理。
- 删除前按当前 Scenario 精确收集该 lane 的 Event ID、受影响 Relation 数和 Group 直接成员数。
  普通信号确认框显示 `N events`、`N relations` 及 `Ctrl+Z`；Group 确认框显示将保留并解除分组的
  member signal 数，若 Group 自身有 Event/Relation 也同时列出。
- 提交后状态显示删除的信号或 Group 名称、每类实际影响和 `Ctrl+Z to undo`；无依赖信号保持简短。
  Undo/Redo 继续使用统一状态反馈，取消确认仍不产生命令或修改。
- `wave-lane-removal-smoke` 真实选中 `req`、读取确认框、点击 Yes、验证 3 个 Event/1 条 Relation
  反馈和模型清理，再 Undo 并验证信号/Relation 恢复及 `Ctrl+Y`。快速新增删除、Wave Edit 与用户
  旅程一并定向回归。全部 GUI 路径使用 offscreen，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 21 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.83 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.61 sec

Git diff check: clean
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-lane-removal-smoke|wave-canvas-add-lane-smoke|wave-wave-edit-smoke|wave-user-journey-smoke)$" \
  --output-on-failure
4/4 passed
Total Test time: 1.70 sec

Automated QA: req 确认框显示 3 events、1 relation 和 Ctrl+Z；完成状态显示相同清理结果；
              Undo 恢复 req/Relation 并显示 Ctrl+Y；相关创建、编辑和保存流程无回归
Desktop interaction: none
```
## 持续迭代 41：信号重排结果与恢复反馈

状态：完成

已交付：

- 开发审计确认菜单上移/下移与画布标题拖动均已正确提交单个 `MoveLaneCommand`，边界拒绝和 Esc
  取消也已有保护；但成功提交后没有操作结果，标题拖回原位置时同样静默结束。
- 用户主流程中，信号重排依赖相邻行位置变化，长列表、相似名称或短距离拖动时难以确认是否已经
  放下。用户也无法立即知道可用 Ctrl+Z 恢复；原位放下无反馈会被误判为拖动失效。
- 菜单和画布拖动成功后统一显示信号名、1-based 原位置、新位置及 `Ctrl+Z to undo`。原位放下明确
  显示当前位置和 `No order changed`，不执行命令、不标记修改，也不占用 Undo 历史。
- `wave-lane-reorder-smoke` 覆盖菜单向上/向下的精确位置反馈及 Undo 后 `Ctrl+Y`；
  `wave-canvas-add-lane-smoke` 通过真实标题拖动覆盖插入目标、跨位置提交、水平原位拖放、无空历史、
  Undo/Redo 顺序与恢复提示。全部 GUI 路径使用 offscreen，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 19 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.22 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.69 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "wave-(lane-reorder|canvas-add-lane)-smoke" --output-on-failure
2/2 passed
Total Test time: 0.65 sec

Automated QA: req 菜单移动精确报告 4→3/4→5；Quick Bus 跨位置拖动报告名称、原/新位置和 Ctrl+Z；
              同位置水平拖放保持顺序并报告 No order changed；随后一次 Undo 直接恢复真实重排，
              Redo 再次生效并分别显示 Ctrl+Y/Ctrl+Z
Visual desktop review: not performed; offscreen-only requirement observed
Desktop interaction: none
```

## 持续迭代 42：锁定光标结果与恢复反馈

状态：完成

已交付：

- 开发审计确认 Marker 的创建、Change、Remove 命令和 Undo/Redo 已完整存在，活动光标移动也会显示
  当前时间；但持久锁定点/区间的创建、拖动、方向键移动与删除提交后均没有结果消息，边界移动静默。
- 用户在 Measure 中依赖细线和半透明区间判断结果。短距离移动、相邻锁定对象或高缩放下，仅凭位置
  变化不能确认目标、精确时间和是否已经提交，也不知道 Delete 或移动可用 Ctrl+Z 恢复。
- 新增统一 Marker 位置格式：点显示精确时间，区间显示起止时间与带符号 `Δ`。创建、拖动/方向键
  移动、Delete 分别显示对象名、旧/新位置和 `Ctrl+Z`；单击选择说明拖动、方向键和 Delete 操作。
- 到达时间轴边界时明确显示对象未移动，不执行命令、不标记修改、不占用 Undo。命令异常同时显示
  tooltip 与状态消息；引用对象已经消失时清除陈旧选择并解释原因。
- `wave-cursor-mode-smoke` 真实创建锁定点，验证创建、拖动、方向键、原位选择、Delete 及删除
  Undo/Redo；再创建带 `Δ` 的锁定区间，逐步移动到零边界，验证额外左移无效果，随后一次 Undo
  直接恢复最后一次真实移动并可 Redo。全部 GUI 路径使用 offscreen，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 22 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.39 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.97 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-cursor-mode-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.20 sec

Screenshot generated:
  build/qtcreator-debug/cursor-mode-smoke.png
Automated QA: 锁定点创建后报告名称/时间/Ctrl+Z；拖动和方向键报告旧→新位置；单击说明编辑方式；
              Delete 报告结果并可 Undo/Redo；锁定区间显示起止与 Δ；零边界无空历史，
              一次 Undo 直接恢复最后一次真实移动并显示 Ctrl+Y
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 43：活动光标最终测量反馈

状态：完成

已交付：

- 开发审计确认活动光标和临时参考点的绘制、采样值与带符号 `Δ` 已存在；但普通单击释放和 Shift
  参考点不提交状态消息。完成测量后移动鼠标还会把悬浮指针时间标成 `Cursor`，与画布上保留的实际
  活动光标不一致；方向键只显示单一时间，已有参考点时会丢失 `Δ` 语境。
- 用户完成测量后需要读取“参考点、活动点、差值”这一组事实。若状态随悬浮指针改写，用户可能把
  未放置的指针位置误认为活动光标；若释放后没有最终结果，则必须依赖最后一次 MouseMove 是否发生。
- 新增统一活动测量文本：单一活动点显示 `Cursor` 精确时间并说明采样值位于信号名旁；有参考点时
  固定显示 `Reference`、`Cursor` 与以 Reference→Cursor 计算的带符号 `Δ`。
- 左键单击/拖动释放、Shift 点击、方向键和 Measure 内鼠标移动共用同一格式。Shift 只改变参考点，
  不把参考点写成活动 Cursor；测量完成后的普通悬浮保留实际活动测量，尚未放置光标时才显示
  `Pointer … · click to place cursor`。
- `wave-cursor-mode-smoke` 精确断言单击与方向键最终 Cursor、Shift 的负 Δ、反向/正向拖动释放后的
  Reference/Cursor 与 `Δ -`/`Δ +`，并继续覆盖第 42 轮全部锁定光标路径。全部 GUI 使用 offscreen。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 28 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.17 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.74 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-cursor-mode-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.27 sec

Screenshot generated:
  build/qtcreator-debug/cursor-mode-smoke.png
Automated QA: 单击和右方向键报告最终 Cursor/采样值位置；Shift 点击保留活动点并报告负 Δ；
              右→左拖动报告负 Δ，左→右拖动报告正 Δ，释放后的 Reference/Cursor 均匹配模型；
              锁定点/区间创建、移动、边界、删除和 Undo/Redo 无回归
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 44：持久 Marker 唯一命名

状态：完成

已交付：

- 开发审计确认锁定点/区间名称直接使用 `scenario.markers.size()+1`。创建多个锁定点、删除较早对象后
  再创建时，当前数量回退，新对象会复用仍存在对象的编号；例如两个 `Locked cursor 4` 可同时存在。
- 用户依赖第 42–43 轮新增的对象名确认创建、移动和删除结果。重名会使状态栏无法唯一指代目标，
  相邻光标或高缩放场景中也无法根据反馈判断编辑了哪一个持久对象。
- 新建 Marker 现在按点/区间分别生成 `Locked cursor N` / `Locked range N`，从原候选编号开始逐个
  检查当前 Scenario；与任何现有名称不区分大小写冲突时继续递增，直至得到唯一名称。
- 只影响新建对象，不改写已有工程名称、ID、时间或 Undo/Redo 快照。创建结果继续显示最终唯一名称、
  精确时间/区间和 `Ctrl+Z`。
- `wave-cursor-mode-smoke` 在完整 Measure 流程中新增“创建两点→保留后一点→删除前一点→再创建”
  序列；断言新对象不复用幸存名称、场景所有 Marker 名称唯一且状态栏引用新名称。全部 GUI offscreen。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 22 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.16 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.88 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-cursor-mode-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.20 sec

Screenshot generated:
  build/qtcreator-debug/cursor-mode-smoke.png
Automated QA: 连续创建两个锁定点时名称不同；删除较早点后再创建不会复用幸存点名称；
              场景中 Transfer、锁定区间及全部锁定点名称不区分大小写地唯一；创建状态引用新名称；
              活动/临时/锁定光标的结果、边界和恢复流程无回归
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 45：Measure 模式切换状态隔离

状态：完成

已交付：

- 开发审计确认 `setTool()` 只清理了光标、标题拖动与波形编辑状态，未取消进行中的时间轴平移、
  空格手势、临时吸附绕过和吸附引导；Wave Edit 中打开的 Bus 直接编辑面板也会残留到 Measure。
- 用户从 Bus 就地赋值切换到测量时，只需要新的测量画布，不应继续看到上一任务的编辑控件；若在
  中键平移尚未释放时通过 Measure 按钮或 Esc 退出，返回直接编辑后的首次移动也不应继续滚动时间轴。
- 工具切换现在统一终止平移、空格手势、临时吸附绕过和吸附引导。由 Wave Edit 进入其他工具时，
  同步关闭 Bus 直接编辑面板；无效未提交草稿仍由既有门禁原处阻止切换，不会被静默丢弃。
- `wave-cursor-mode-smoke` 先打开 Bus 面板再进入 Measure，断言浮层隔离；随后分别在中键按下后通过
  Measure 按钮和 Esc 退出，继续发送移动/释放事件，断言直接编辑模式与滚动位置保持稳定。全部 GUI offscreen。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 21 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 7.99 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.94 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-cursor-mode-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.73 sec

Screenshot generated:
  build/qtcreator-debug/cursor-mode-smoke.png
Automated QA: Bus 直接编辑面板不会进入 Measure；按钮和 Esc 均在鼠标释放前终止平移；
              返回直接编辑后额外移动不改变滚动位置；全部活动/临时/锁定光标流程无回归
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 46：Measure 只测量边界与选择归属

状态：完成

已交付：

- 开发审计确认第 45 轮只关闭了进入 Measure 前已有的 Bus 面板；`mousePressEvent()` 仍对所有工具
  响应 Bus 点击并重新打开赋值控件，`contextMenuEvent()` 也未限制工具，Measure 内仍可执行写值、
  Clear、Pulse、Clock override 和 Paste 等直接编辑命令。
- 用户进入 Measure 后真正需要的是读取时间和值。测量 Bus 时出现赋值控件会遮挡波形，右键仍可改值
  会造成模式含义不可靠；锁定 Marker 选中后再点击信号标题，两种高亮并存且 Delete 实际优先删除信号。
- Bus 直接编辑面板现在只在 Wave Edit 中响应点击。Measure 内波形右键不创建编辑菜单，仅在状态栏提示
  按 Esc 返回直接编辑；标题左键、右键或双击会明确取消锁定 Marker 选中和其拖动上下文。
- `wave-cursor-mode-smoke` 在 Measure 内真实点击 Bus、发送右键上下文事件并侦测任何弹出菜单，随后
  创建锁定点、切换到 Bus 标题并重新选择该点；断言无编辑浮层、无波形菜单、选择互斥且 Marker 可继续编辑。
  全部 GUI offscreen，未操作桌面。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 19 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.18 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.95 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-cursor-mode-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.68 sec

Screenshot generated:
  build/qtcreator-debug/cursor-mode-smoke.png
Automated QA: Measure 内点击 Bus 只更新活动光标，Bus 赋值面板保持隐藏；波形右键不出现编辑菜单，
              状态栏指明 Esc 返回编辑；点击信号标题取消锁定 Marker 选择，重新单击可继续编辑；
              活动/临时/锁定光标、退出与中断平移流程无回归
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 47：信号标题选择目标反馈

状态：完成

已交付：

- 开发审计确认标题左键只发出 `selectionChanged`，而主窗口处理函数仅更新排序动作，不更新状态栏。
  因此标题选中后会继续显示上一项波形、Marker 或 Undo 结果，实际键盘目标已经变化但文字未变化。
- 用户在 Measure 中选中锁定光标时会看到“Selected … Delete”；随后点击信号标题虽然取消了 Marker
  高亮，旧提示仍声称 Delete 操作该光标，而按键实际进入信号删除确认。这是高风险的目标反馈错位。
- 每次左键信号标题选择现在统一报告 `Selected signal <name>`、`Delete removes signal` 和 `F2 renames`。
  若同次取消锁定光标/区间或持久范围，状态末尾追加 `locked cursor/range deselected` / `range cleared`，
  当前目标和被替换目标在一条结果中可确认。
- `wave-canvas-add-lane-smoke` 断言普通编辑中标题选择先报告名称与两个按键目标，再执行 Delete；
  `wave-cursor-mode-smoke` 断言从锁定 Marker 切到 Bus 标题时 Marker 取消、名称/按键目标和取消原因同步更新，
  随后仍可重新选择并编辑原 Marker。全部 GUI offscreen。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.39 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 8.83 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-canvas-add-lane-smoke|wave-cursor-mode-smoke)$" --output-on-failure
2/2 passed
Total Test time: 1.20 sec

Screenshot generated:
  build/qtcreator-debug/cursor-mode-smoke.png
Automated QA: 普通信号标题选择报告当前名称、Delete 与 F2 目标后再允许删除；
              Marker→Bus 标题切换同步报告新信号目标和 locked cursor/range deselected；
              Marker 可重新选择，删除、Undo/Redo、Measure 退出与平移隔离无回归
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 48：标题右键与重命名取消的目标恢复

状态：完成

已交付：

- 开发审计确认第 47 轮只覆盖了标题左键。标题右键会取消锁定 Marker 并同步打开参数菜单，但菜单
  或后续参数对话框取消时没有新状态，仍显示旧 Marker 的 `Selected … Delete`；目标已经变为信号。
- 双击/F2 重命名入口只显示通用 `Rename signal`，多个相邻信号时无法从状态确认对象；Esc 取消只显示
  `Rename cancelled`，随后信号仍选中且 Delete/F2 有效，但恢复后的目标与可用动作未说明。
- 标题右键现在在打开参数菜单前报告当前信号、Delete/F2 目标，以及同次取消的 Marker/范围状态；
  若菜单或参数对话框取消，该反馈保持。重命名入口显示信号名，Esc 取消后恢复当前信号和 Delete/F2。
- `wave-canvas-add-lane-smoke` 断言双击入口包含信号名、Enter/Esc，并在 Esc 后确认原名未改且目标反馈恢复；
  `wave-cursor-mode-smoke` 从选中锁定点右键 Bus 标题，真实打开并取消参数菜单，断言 Marker 取消、
  新信号名与 Delete/F2 目标保留，随后可重新选择原 Marker。全部 GUI offscreen。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 19 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.01 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.83 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^(wave-canvas-add-lane-smoke|wave-cursor-mode-smoke)$" --output-on-failure
2/2 passed
Total Test time: 1.20 sec

Screenshot generated:
  build/qtcreator-debug/cursor-mode-smoke.png
Automated QA: 双击重命名入口显示目标信号名与 Enter/Esc，Esc 后恢复原名和信号 Delete/F2 目标；
              Marker→右键 Bus 标题→取消菜单后，状态保留新信号目标及 Marker 已取消说明；
              原 Marker 可重新选择，后续移动、删除、Undo/Redo 与 Measure 退出无回归
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 49：轻量参数原位校验与纠错

状态：完成

已交付：

- 开发审计确认 Clock 与 Bit/Bus 轻量参数窗口在点击 OK 后才校验时间、频率或颜色。无效值会先关闭窗口，
  再显示独立警告；用户刚输入的其他参数、错误字段焦点与选择范围全部丢失，只能重新打开并重复输入。
- 用户视角验收确认该路径属于高频纠错阻塞：输入格式错误本应在原处修正一次，旧流程需要关闭警告、重新定位
  信号、再次打开参数窗口并恢复所有字段，且无法确认哪些输入被保留。
- Clock 参数窗口现于 OK 提交前解析 period/frequency；Bit/Bus 参数窗口于提交前校验 HTML 颜色。错误信息直接
  显示在同一窗口，完整保留其他字段，聚焦并选中错误输入；修正后可在同一次对话中提交。模式或字段改变会清除旧错误。
- `wave-canvas-add-lane-smoke` 改为真实点击对话框 OK，分别输入无效 Clock 时间和 Bit 颜色，断言窗口未关闭、
  草稿/模式/焦点均保留，再纠正并验证模型、状态反馈、Undo。全部 GUI 路径使用 offscreen。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.03 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.80 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-canvas-add-lane-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.96 sec

Automated QA: Bit 参数输入无效颜色后窗口、颜色文本、Height=64 与颜色字段焦点保留；纠正颜色后一次提交成功。
              Clock 选择 period 并输入无效时间后窗口、period 模式、文本与焦点保留；纠正为 12000 ticks 后提交成功。
              两次提交均显示真实结果并可 Undo；原值确认与已有快速添加、重命名、删除、重排路径无回归。
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 50：导出参数原位校验与纠错

状态：完成

已交付：

- 开发审计确认桌面 Export 的指定范围与 PDF page span 在参数窗口关闭后才解析。结束早于开始、时间格式错误或
  负分页跨度会关闭全部 Export 设置，再弹独立警告；重新尝试时范围、Logical width、PNG DPI 和附加层选择全部复位。
- 用户视角从空白工程创建并编辑波形、Save As 后进入 Export。此时真正目标是生成交付物；一次输入错误不应迫使用户
  重新打开菜单、重选 scope 并恢复所有导出选项，因此该问题直接阻断完整任务收尾。
- Export 现在于 OK 提交前依次校验选择范围、指定 start/end 和 PDF span。错误信息在同一窗口显示，聚焦并选中
  具体错误字段，保留 scope、范围、width、DPI 与三个附加层勾选；修改相应时间字段或 scope 时清除旧错误，全部有效后才进入目录选择。
- `wave-user-journey-smoke` 扩展为完整创建→编辑→保存→导出路径：先输入 80–40 ns，断言 end 错误原位保留；
  再修正为 80–120 ns 并输入 -1 tick PDF span，断言第二次原位纠错；最后改为 25 ns 并确认进入目录选择后静默取消。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.20 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.85 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-user-journey-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.47 sec

Screenshot generated:
  build/qtcreator-debug/user-journey-smoke.png
Automated QA: Export 的 80–40 ns 无效范围保留全部字段并聚焦 End；修正范围后的 -1 tick PDF span
              继续保留 scope、范围、2048 width、144 DPI 和附加层选择并聚焦 span；改为 25 ns 后进入目录选择。
              空白工程创建、重命名、波形编辑、双草稿门禁、测量、650 ns Save As 与保存回读无回归。
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 51：项目文件过滤与保存命名闭环

状态：完成

已交付：

- 开发审计确认 New 已直接创建可编辑的 200 ns 空白波形，首要剩余问题在项目文件入口：Open 与 Save As 的首选
  name filter 写成字面 `project.wave.json`，而非 `*.wave.json`，因此自定义命名的工程可能在默认过滤器下不可见。
- 用户从首次创建到再次打开工程时，期望只输入工程名即可保存，并在 Open 中立即看见该文件。旧流程既未保证补全
  `.wave.json`，又要求用户理解并切换到宽泛 JSON filter，增加一次猜测且可能生成难以识别的无扩展名文件。
- Open/Save As 首选过滤器现统一为 `*.wave.json`。Save As 返回路径没有扩展名时自动追加 `.wave.json`；用户显式
  输入其他扩展名时保持原意。窗口取消仍不改变当前工程。
- `wave-user-journey-smoke` 在真实 Save As 对话框中仅输入 `user-journey`，断言首选 filter 包含通配符、最终只生成
  `user-journey.wave.json` 并成功回读；随后真实打开 Open 对话框，断言相同通配 filter，取消后 650 ns/3 lane 工程不变。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.24 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.94 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-user-journey-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.42 sec

Screenshot generated:
  build/qtcreator-debug/user-journey-smoke.png
Automated QA: Save As 首选 filter 为 *.wave.json，仅输入 user-journey 后保存/回读 user-journey.wave.json，
              未生成无扩展名旁路文件；Open 使用同一通配 filter，取消后当前 650 ns/3 lane 工程保持。
              完整创建、编辑、双草稿门禁、测量、导出两级原位纠错与目录交接无回归。
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 52：项目重新打开与结果反馈闭环

状态：完成

已交付：

- 开发审计确认普通 `.wave.json` 成功加载时，窗口标题和波形虽会变化，但状态栏仅在 recovery 或 warning 分支
  显示消息；无警告的正常 Open 会保留旧状态文字，无法明确区分成功加载、取消或仍在旧工程。
- 用户从保存后的工程执行 New，再重新 Open，真正需要确认的是磁盘文件已被读取且当前画布对应所选路径。只依赖
  视觉内容变化会在相似波形或大工程加载时增加误判风险。
- 普通工程成功加载后状态栏现显示 `Opened <path>`；恢复快照继续显示必须 Save 的目标路径，迁移/解析 warning
  继续优先显示，不覆盖更重要的恢复或兼容性说明。
- `wave-user-journey-smoke` 在保存并验证 filter 后先取消一次 Open，随后执行 New，断言回到干净 200 ns 空白工程；
  再从真实 Open 对话框选择刚保存的文件，断言恢复 650 ns/3 lane、永久状态为 Saved 且成功消息包含文件名。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 32 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.20 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.72 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-user-journey-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.47 sec

Screenshot generated:
  build/qtcreator-debug/user-journey-smoke.png
Automated QA: 保存 650 ns/3 lane 工程后，Open 取消保持当前工程；New 生成干净 200 ns 空白波形；
              再 Open 已保存文件恢复全部内容，SaveState=Saved，状态为 Opened <user-journey.wave.json>。
              后续 Export 两级原位纠错、目录交接和最终截图均基于重新加载的磁盘工程通过。
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 53：脏工程 Open 的选择与确认顺序

状态：完成

已交付：

- 开发审计确认 `openProject()` 在文件选择前调用 `confirmDiscardChanges()`。用户取消文件选择仍必须先处理未保存提示；
  选择 Discard 后再取消 Open 时，当前编辑实际仍保留，使“Discard”文字与结果矛盾。
- 用户在脏工程中点击 Open 时，首先要确认目标文件是否存在并决定是否继续。未选定目标前要求 Save/Discard 增加一次
  无效决定，也会让误触 Open 变成有风险感的流程。
- Open 现在先显示文件选择；路径为空立即返回，不触发未保存确认。只有选定目标后才执行 Save/Discard/Cancel；取消确认
  不加载目标，Save 成功或 Discard 后才加载。所有未提交内联草稿仍在打开文件选择前通过既有门禁。
- `wave-user-journey-smoke` 将重新打开的 650 ns 工程改到 660 ns：第一次 Open 直接取消文件选择，断言未出现未保存提示、
  660 ns 与 Unsaved changes 保留；第二次选择已保存文件，断言随后才出现确认，Discard 后恢复 650 ns/3 lane 与 Saved。

开发视角验收：

```text
cmake --build build/qtcreator-debug --parallel 4
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.09 sec

cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.82 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-user-journey-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.55 sec

Screenshot generated:
  build/qtcreator-debug/user-journey-smoke.png
Automated QA: 脏工程 Open 首先出现文件选择；取消后无 Save/Discard 提示，660 ns 编辑与 Unsaved changes 保留。
              再次 Open 选定目标后才出现未保存确认；Discard 后加载 650 ns/3 lane 磁盘工程并显示 Saved/Opened。
              快速新增/Bus/End 草稿门禁、项目保存回读、Export 原位纠错与目录交接无回归。
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 54：高级 Lane/Group 属性原位纠错与空提交收口

状态：完成

已交付：

- 开发审计确认 Edit 菜单的完整 Lane/Group 属性窗口在名称、Bus/Enum 位宽、Enum 映射或颜色无效时，会先关闭编辑窗口再弹出警告；
  Clock domain 与 Group 引用错误也会在命令执行后才拒绝。用户必须重新打开窗口并恢复全部字段，纠错成本与低频入口不匹配。
- 用户从特殊 lane 或 group 的完整属性窗口修改多个字段时，真正需要的是在当前字段附近修正错误并保留其余草稿；确认原值则只应退出，不能把工程改为
  Unsaved changes，也不能生成一个无意义的 Undo 项。
- 完整属性窗口新增稳定对象标识和窗口内错误区。名称、1..4294967295 位宽、`NAME=VALUE` Enum 映射、现有 clock/group 引用及 Qt 颜色
  均在 OK 时原位校验；失败时窗口保持打开、其他字段不回滚，错误字段获得焦点并全选，修改相关字段后错误提示自动清除。
- `editLaneById()` 使用命令栈的真实执行结果区分属性变更与空确认。无变化时不刷新模型、不标记 dirty、不安排 autosave、不改变 Undo/Redo，
  仅恢复目标选择并显示 `No properties changed for <name>`；真实变更显示结果和 `Ctrl+Z`。
- `wave-lane-dialog-smoke` 通过真实 OK 点击连续提交空名称、零位宽、错误 Enum 映射和非法颜色，断言每次均原位纠错、草稿与焦点保留；
  最后恢复全部原值并确认，断言模型不变、SaveState 仍为 Saved、Undo 不可用且状态栏给出无变化反馈。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.37 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 8.04 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-lane-dialog-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.56 sec

Screenshot generated:
  build/qtcreator-debug/lane-dialog-smoke.png
Automated QA: 空名称、零位宽、错误 Enum 映射和非法颜色均不关闭窗口，错误字段获得焦点，其余字段草稿保持。
              恢复原始 req 属性后点击 OK，模型与 Saved 状态不变，Undo 仍不可用，并显示明确的无变化反馈。
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 55：高级属性结构兼容性原位预检

状态：完成

已交付：

- 开发审计确认第 54 轮已把名称、数值格式、引用与颜色错误移入属性窗口，但类型/位宽变更仍只在窗口关闭后由
  `ChangeLaneCommand` 检查。Group 成员转换、Clock/Group 持有 Event、位宽缩减导致 Event 值溢出等冲突仍会弹出独立警告并丢失全部草稿。
- 用户把示例 `data[7:0]` 从 8 位误改为 4 位时，真正需要的是看到 `0x35` 已超出新位宽、直接把 Width 改回；关闭窗口、确认警告、
  重新打开并恢复其他字段没有产生任何有效工作，只增加重复输入和误放弃风险。
- 完整属性窗口在 OK 时先构造一个不执行的 `ChangeLaneCommand`，复用正式提交的同一结构规范化与兼容性规则；只有预检成功才关闭窗口。
  预检不修改 Project、Scenario 或命令栈。失败时在现有错误区显示命令原因，并根据 Enum、位宽/值或类型冲突把焦点放回 Enum map、Width 或 Kind。
- 实际提交仍保留原异常防线，避免窗口确认与命令执行之间的状态变化绕过校验；新增 lane/group 因尚不存在于场景而跳过变更预检，继续由
  `AddLaneCommand` 负责最终一致性。
- `wave-lane-dialog-smoke` 改用真实 `data[7:0]`：在四类基础输入错误后提交 8→4 位，断言窗口不关闭、提示现有 `0x35` 超出位宽、Width 获得焦点、
  名称/类型/颜色等草稿保留；恢复 8 位后确认仍保持原模型、Saved 和空 Undo。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 22 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.10 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 7.88 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-lane-dialog-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.52 sec

Screenshot generated:
  build/qtcreator-debug/lane-dialog-smoke.png
Automated QA: data[7:0] 的 8→4 位草稿未关闭窗口；提示说明现有 0x35 超出 lane width，焦点回到 Width，其他草稿保留。
              改回 8 位确认后模型、Saved 状态和 Undo 均不变，并显示 No properties changed for data[7:0]。
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 56：高级属性名称唯一性收口

状态：完成

已交付：

- 开发审计确认快速新增通过 `uniqueLaneName()` 生成大小写不敏感的唯一名称，就地重命名也原位拒绝重复名称；完整 Lane/Group 属性窗口却仅检查非空，
  因而 Edit 菜单仍可创建重名对象或把现有信号改成已有名称。
- Relation condition 允许使用唯一显示名称引用 lane，出现重复名称时会明确报告 ambiguous。用户在画布中也无法仅凭相同标题区分 Delete、F2、测量值或编辑目标，
  因此重名并非低风险格式问题，而是由属性入口制造的后续歧义。
- 完整属性窗口现在以 stable ID 排除当前对象，对所有其他 lane/group 执行大小写不敏感名称比较；发现重复时窗口保持打开，显示
  `Another signal or group already uses this name.`，焦点回到 Name 并全选，类型、位宽、颜色等草稿不变。
- 同一校验同时覆盖编辑现有属性、Edit > Add lane 和 Add group，使快速新增、就地重命名与完整属性入口采用一致的名称策略；领域模型仍保留读取旧有重名工程的能力。
- `wave-lane-dialog-smoke` 在 `data[7:0]` 的空名称错误后输入已有 `req` 的大小写变体 `REQ`，真实点击 OK 并断言原位拒绝、Name 焦点与其他草稿保留；
  随后继续完成位宽、Enum、颜色、结构冲突和最终空提交的全部回归链。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.13 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 8.00 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-lane-dialog-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.56 sec

Screenshot generated:
  build/qtcreator-debug/lane-dialog-smoke.png
Automated QA: data[7:0] 改为已有 REQ 时窗口保持，提示名称已被使用，Name 获得焦点，原 Kind/Color 等草稿保留。
              修正名称后继续通过基础字段与结构兼容性纠错，恢复原值确认仍保持模型、Saved 和空 Undo。
Manual visual read: not performed because the permission service blocked screenshot access
Desktop interaction: none
```

## 持续迭代 57：正式保存与恢复快照生命周期闭环

状态：完成

已交付：

- 开发审计确认 autosave 将当前工程写入 `<project>.autosave`，但 `writeToPath()` 正式保存成功后仅停止定时器、递增 generation，未删除已有恢复文件。
  若 Qt Concurrent 写入已开始，generation 只让结果被判为 stale，后台任务仍可能在正式保存之后重新留下旧快照。
- 用户完成正式保存后真正需要的是磁盘工程成为唯一最新版本。Open 过滤器显式暴露 Recovery snapshot，若同目录仍保留旧 `.autosave`，用户无法仅凭文件名判断其过期，
  误开后会看到 `Save required` 并可能把旧数据覆盖回正式文件。
- 正式保存成功后现在删除目标工程对应的恢复快照；Save As 时也清理旧工程路径的快照。清理失败不把成功保存误报为失败，但状态栏会明确列出仍残留的快照路径。
- 每次后台写入记录独立的在途路径。正式保存、新建或加载使 generation 失效后，worker 完成时若当前工程已干净，会对该确切路径再次清理；stale 错误或成功消息不再覆盖
  更晚的 `Saved` 反馈。watcher 已完成但 `finished` 尚未处理时禁止启动下一任务，避免在途路径被覆盖。
- `wave-autosave-smoke` 先验证 1.5 秒防抖生成可读取快照，再正式保存并断言快照立即消失；随后直接启动后台写入并在同一事件循环内正式保存，
  等待 stale worker 完成后断言 `.autosave` 未重现、正式文件仍可读取、SaveState 与状态栏仍为 Saved。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.99 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 8.79 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-autosave-smoke$" --output-on-failure
1/1 passed
Total Test time: 3.41 sec

Automated QA: 修改后生成的恢复快照可由正式加载器读取；正式保存后文件立即删除，磁盘工程与 SaveState=Saved 有效。
              后台快照与保存重叠时，过期 worker 完成后未重新留下 .autosave，状态栏仍保留 Saved 结果。
Desktop interaction: none
```

## 持续迭代 58：崩溃后更新快照自动发现

状态：完成

已交付：

- 开发审计确认第 57 轮已保证正式保存后不残留过期快照，但应用启动和 File > Open 始终直接加载用户指定的 `.wave.json`；自动生成的
  `<project>.autosave` 只有用户知道命名规则并在 Open 过滤器中手工选择时才会生效。
- 用户在异常退出后最先做的是重新打开原工程，不会先研究恢复文件。若应用静默显示旧的正式波形，即使同目录已有更新快照，也会让用户误以为最近编辑已经丢失；
  反过来无条件采用任何 `.autosave` 又可能把旧文件或损坏文件置于正式工程之前。
- 新增 app 层 `preferredProjectLoadPath()`：显式打开 `.autosave` 时保持用户选择；打开正式工程时，仅在同路径快照存在、是普通文件、修改时间晚于正式工程且能被
  正式加载器完整解析时返回快照，否则继续返回正式路径。恢复检查只影响桌面入口，不改变领域模型或 CLI 的显式文件语义。
- 文件关联、命令行项目参数和 `waveworkbench://open` 在初次加载前应用该规则；File > Open 也使用同一规则。采用快照后 MainWindow 继续将正式 `.wave.json`
  作为保存目标，工程标记 dirty，常驻状态显示 `Recovery loaded · Save required`，状态栏说明保存将提交到正式路径。
- `wave-autosave-smoke` 在生命周期清理后构造内容与时戳均更新的有效快照，按真实启动顺序执行选择、加载和第二个 MainWindow 构造，断言恢复内容、Save required、
  标题星号、正式路径 tooltip 和恢复说明；随后将同一快照改旧并改为更新但损坏的内容，分别断言均回退正式工程。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 8.71 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 8.78 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-autosave-smoke$" --output-on-failure
1/1 passed
Total Test time: 3.42 sec

Automated QA: 正式工程旁的有效更新快照被自动选中，恢复后的场景时长来自快照，显示 Recovery loaded · Save required 与正式保存路径。
              快照时间早于正式工程时不替代；快照更新但 JSON 损坏时也回退正式工程。既有正式保存清理和在途 worker 清理继续通过。
Desktop interaction: none
```

## 持续迭代 59：恢复内容 Discard 的持久语义

状态：完成

已交付：

- 开发审计确认 New/Open/Close 的未保存提示在选择 Discard 时只返回 `true`，不停止 autosave 或删除 `<project>.autosave`。第 58 轮自动发现更新快照后，
  用户明确放弃的内容会在下一次打开时再次恢复；若后台 worker 尚未完成，它还可能在 Discard 后重新创建文件。
- 用户从恢复状态选择 Discard，表达的是永久放弃这份恢复内容，而非仅关闭当前内存视图。再次看到同一内容会使 Discard 失去可信度，也可能促使用户误保存本已放弃的旧状态。
- `discardRecoverySnapshots()` 现在停止防抖定时器、递增 generation、清除 pending 标志，收集当前正式工程和在途 worker 对应的确切快照路径；若 worker 在运行，
  先等待其完成，再删除所有相关文件。New/Open/Close 共享的 `confirmDiscardChanges()` 只有清理成功才继续。
- 在途路径登记为显式放弃；其 queued `finished` 到达后会再次尝试删除，但不会显示 autosave 成功、不会重新调度。若文件删除失败，Discard 被取消，当前修改保持打开，
  提示具体残留路径并恢复 autosave；Open 在后续加载失败时也为仍在内存中的脏工程恢复 autosave。
- `wave-autosave-smoke` 在自动恢复窗口中重新创建更新快照并直接启动后台写入，随后调用 New、真实点击 QMessageBox 的 Discard；断言 worker 已完成、快照不存在、
  `preferredProjectLoadPath()` 回到正式工程，窗口变为无星号的 Untitled/Not saved 且空 lane，等待 100 ms 后仍无重新写入。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 9.13 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 8.81 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-autosave-smoke$" --output-on-failure
1/1 passed
Total Test time: 3.57 sec

Automated QA: Recovery loaded 窗口存在有效更新快照且 autosave worker 在途时执行 New，未保存提示真实点击 Discard。
              worker 完成后 .autosave 未重现，自动选择回到正式工程；当前窗口为 Untitled、Not saved、无标题星号和空 lane。
Desktop interaction: none
```

## 持续迭代 60：首次保存前的 Untitled 自动恢复

状态：完成

已交付：

- 开发审计确认 autosave 路径由 `projectFile_` 派生，未命名工程因路径为空在调度、启动和连续写入三个位置被直接跳过。用户首次打开应用后即使已经绘制波形，
  在第一次 Save As 前异常退出仍没有恢复入口；这是新用户最早遇到的数据安全缺口。
- 用户在空白页的真实顺序是先添加信号和修改波形，再决定工程名与保存目录。要求先保存才能获得崩溃保护会增加一次无关决策，也无法从界面得知当前编辑不受保护。
- 新增稳定的 Untitled 恢复路径：测试可通过 `WAVEWORKBENCH_RECOVERY_DIR` 隔离，正常运行使用应用本地数据目录下的 `recovery/untitled.wave.json.autosave`；
  autosave 在首次写入前创建目录，继续使用原子保存、generation 和单 worker 串行语义。各 GUI CTest 使用独立恢复子目录，允许并行执行且不触碰真实用户恢复数据。
- 未命名工程不再被 autosave 调度条件排除。无参数正常启动会先检查该快照，仅在文件存在、是普通文件且能由正式加载器完整解析时采用；无有效快照仍直接显示 200 ns 空白页。
  自动化入口保持显式输入与确定性，不会意外采用其他测试留下的快照。
- 从 Untitled 快照恢复后，窗口保留空正式路径，常驻状态显示 `Recovery loaded · Save required`，状态栏说明使用 Save 选择工程文件，Save 继续进入 Save As；
  正式保存会同时清理 Untitled 快照，Discard 会等待在途 worker 并永久清理该路径。
- `wave-new-project-smoke` 真实提交 300 ns 时间轴修改，等待 1.5 秒防抖与后台原子写入，按无参数启动顺序选择并加载快照，再构造恢复窗口；
  断言内容、标题星号、未正式保存 tooltip 和恢复状态，随后真实取消 Save As、执行 New 并点击 Discard，确认快照不再存在且窗口回到干净 200 ns 空白页。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 10.99 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 10.51 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-new-project-smoke$" --output-on-failure
1/1 passed
Total Test time: 1.91 sec

Automated QA: Untitled 的 300 ns 修改在首次 Save As 前生成可读取的隔离快照；按无参数启动顺序恢复后显示 Recovery loaded · Save required、标题星号和未正式保存说明。
              Save 仍打开 Save As；取消后快照保留。New→Discard 后快照永久消失，窗口恢复 Untitled、Not saved、200 ns、无 lane。
Desktop interaction: none
```

## 持续迭代 61：最近工程与文件目录记忆

状态：完成

已交付：

- 开发审计确认 `openProject()` 在未命名工程中使用 `QFileInfo("").absolutePath()`，`saveProjectAs()` 仅传入相对文件名 `project.wave.json`。
  应用从快捷方式或安装目录启动时，首次保存和 New 后打开都会落到进程工作目录；没有最近工程入口，用户每次继续已有工作都必须重新导航文件系统。
- 用户真正想做的是继续最近的波形，而不是重新确认目录结构。首次保存应进入个人文档目录，后续同一工作流应记住刚使用的位置；常用工程应能从 File 菜单一步打开，
  同时不能因误点当前工程而重载并丢弃内存修改。
- 首次 Save As 现在从 Documents 开始；若该位置不可用则使用 Home。成功 Save/Save As/Open 或命令行加载后记录正式工程的绝对路径和父目录，New 不再清空目录记忆。
  Open 与未命名 Save As 共享同一目录选择规则。
- File 菜单新增 `Open Recent` 子菜单，最多保留 5 个去重工程。主标签只显示文件名与父目录名，完整路径放在 tooltip，避免主菜单被长路径撑宽；缺失目标点击后移除陈旧记录并给出状态反馈。
- 点击当前已经打开的最近工程先按规范化路径识别并显示 `Already open`，不提交草稿、不重载、不弹出 Save/Discard，也不改变 dirty 状态。其他最近工程继续复用既有草稿门禁、未保存确认、
  恢复快照选择和加载反馈。菜单刷新通过事件队列执行，避免在 QAction 自身触发期间删除发送者。
- 最近列表和目录使用应用设置持久化。所有 GUI CTest 获得独立 `WAVEWORKBENCH_SETTINGS_DIR`，启动自动化时先清空各自设置；测试可验证持久交互，又不会读取、覆盖或污染真实用户设置，
  并可与第 60 轮独立恢复目录并行运行。
- `wave-user-journey-smoke` 扩展为：首次 Save As 验证 Documents/Home 与文件过滤；保存后验证最近项路径；New 后验证 Open 初始目录仍为保存目录；在 660 ns 脏工程中点击当前最近项，
  断言无模态提示且内容/dirty 不变；完成既有 Open/Export 流程后再次 New，再点击最近项一步恢复 650 ns、3 lane、Saved 和 `Opened <path>`。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 22 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 11.36 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 10.94 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-user-journey-smoke$" --output-on-failure
1/1 passed
Total Test time: 1.04 sec

Automated QA: 首次 Save As 从 Documents/Home 开始并生成最近项；New 后 Open 初始目录仍为刚保存目录。
              660 ns 脏工程点击自身最近项只显示 Already open，无确认、无重载；再次 New 后点击最近项一步恢复 650 ns/3 lane、Saved 与 Opened 反馈。
              GUI 测试 QSettings 逐测试隔离，未访问真实用户设置。
Desktop interaction: none
```

## 持续迭代 62：工程文件直接拖放打开

状态：完成

已交付：

- 开发审计确认 WaveCanvas 已设置 `acceptDrops`，但其 `dragEnterEvent()` / `dropEvent()` 只识别内部 Bus preset MIME。资源管理器提供的 URL MIME 被明确 ignore，
  主窗口也没有拖放入口；用户将 `.wave.json` 放到最显眼的工作区不会得到任何反馈，只能改走 File > Open 或 Open Recent。
- 用户从文件系统看到工程时，最直接的动作是把它拖进已经打开的工作台。该动作应等价于已选定目标后的 Open，不应要求重新浏览同一路径；同时 Bus 值拖放是高频编辑动作，不能被工程级处理抢占。
- MainWindow 在 WaveCanvas 与 viewport 上安装事件过滤器，仅当 MIME 包含一个本地普通文件且扩展名为 `.json` 或 `.autosave` 时接管 DragEnter/DragMove/Drop；
  `.wave.json` 自然属于 JSON。多文件、远程 URL、目录和其他扩展名继续下放，原有 Bus preset MIME 路径不变。
- 工程 Drop 在事件返回后排入下一事件周期，避免在拖放分发栈中打开模态确认。随后复用规范化路径、当前工程安全无操作、草稿提交、Save/Discard/Cancel、
  `preferredProjectLoadPath()`、正式加载、Saved/Recovery 反馈以及 Recent/目录记忆；解析失败仍由现有 Open failed 反馈处理。
- 最近工程和拖放共享 `openProjectPath()`，避免两套未保存确认或加载语义。加载后的 Recent 菜单仍按上一轮设计在后续事件周期刷新，不会在 QAction 触发过程中删除发送者。
- `wave-user-journey-smoke` 在完成保存与 Recent 重开后复制当前工程、将名称改为 Dropped waveform 并把 End 延长至 700 ns，原子写入独立文件；随后向真实 canvas viewport 发送
  URL DragEnter/Drop，断言两个事件被接受、700 ns/3 lane 内容加载、SaveState=Saved、状态为 `Opened <drop path>` 且 Recent 第一项切换到该文件。
- `wave-wave-edit-smoke` 同轮定向回归 Bus `Don't care` MIME DragEnter/Drop、单拍写入及 Undo/Redo，证明工程过滤器只消费 URL 文件，不影响波形内部拖放。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 21 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 10.89 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 10.86 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-user-journey-smoke$" --output-on-failure
1/1 passed
Total Test time: 1.05 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-(wave-edit|user-journey)-smoke$" --output-on-failure
2/2 passed
Total Test time: 1.39 sec

Automated QA: 一个本地 user-journey-dropped.wave.json 拖到 canvas viewport 后，DragEnter/Drop 均接受；窗口加载 Dropped waveform、700 ns/3 lane，
              显示 Saved 与 Opened <drop path>，Recent 第一项更新为拖入文件。既有 Bus Don't care 预设拖放、单拍结果和 Undo/Redo 无回归。
Desktop interaction: none
```

## 持续迭代 63：Undo/Redo 正式保存点

状态：完成

已交付：

- 开发审计确认普通 `undo()` / `redo()` 无条件把工程设为 dirty。用户在正式保存后修改一个值，再 Ctrl+Z 回到完全相同的磁盘版本，界面仍显示
  `Unsaved changes`、窗口仍带星号、关闭仍会要求保存，已完成或在途 autosave 还可能作为下一次启动的过期恢复来源。
- 用户真正需要的是明确知道“当前内容是否等于正式保存版本”。回到保存版本应立即显示 Saved 且无需再次保存；离开保存点才应显示 Unsaved changes，
  不应要求用户记忆自己执行过几次编辑或手工覆盖同一文件。
- `CommandStack` 为每个历史位置分配稳定状态 ID。Undo 返回原状态会恢复原 ID，Redo 恢复对应 ID；Undo 后新建分支、替换快速新增命令、取消命令、
  截断后续历史和清空文档均维护独立状态，避免仅按栈深度判断造成同深度分支误报 Saved。无效果命令不改变状态 ID。
- MainWindow 记录最近一次正式保存/加载/新建的命令状态和直接模型修改版本。命令式编辑与 trace 等非命令修改均纳入 dirty 判定；正式保存会更新保存点，
  恢复快照或迁移结果在保存前仍保持 dirty。快速新增信号的临时命令取消后也重新同步保存点，不污染后续直接修改识别。
- Undo/Redo 每次都根据当前状态与保存点重新计算 dirty。回到保存版本时 SaveState 变为 `Saved`、标题星号消失，并显示 `back to saved version`；
  Redo 离开保存点时恢复 `Unsaved changes`。未命名工程撤销全部修改时使用 `all changes undone` 反馈。
- 回到保存点会停止待执行 autosave、递增快照代次并删除已完成快照；若 worker 正在写入，完成回调将其识别为过期并删除。删除失败时状态栏保留准确路径，
  不以模态窗口中断操作。
- 核心回归覆盖初始/编辑/Undo/Redo 状态 ID、Undo 后分支 ID 唯一、clear 新根状态以及无效果命令不改变状态。用户旅程在 650 ns 正式保存版本上改为
  660 ns、启动真实异步 autosave、Undo 回到 650 ns/Saved 并等待在途快照删除，再 Redo 回 660 ns/Unsaved changes；同一旅程前段继续验证 Ctrl+Z/Ctrl+Y 快捷键。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 19 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
21/21 tests passed
Total Test time: 11.26 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
21/21 tests passed
Total Test time: 10.91 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-user-journey-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.59 sec

Automated QA: 650 ns 正式保存版本改为 660 ns 后显示 Unsaved changes；真实异步恢复快照启动后 Undo 返回 650 ns、Saved、无标题星号，
              在途 .autosave 完成后被清除并显示 back to saved version；Redo 返回 660 ns 和 Unsaved changes。
Desktop interaction: none
```

## 持续迭代 64：隐藏项可发现恢复

状态：完成

已交付：

- 开发审计确认高级 Lane/Group 属性允许关闭 `Visible`，但纯波形工作区已移除 Signals、Project 和 Scenario 导航窗口。隐藏后该 lane 会从唯一工作区消失，除立即 Undo 或手工修改 JSON 外没有应用内恢复路径；示例工程本身还包含一个隐藏 group，说明这不是仅由误操作产生的边缘状态。
- 用户真正需要的是在隐藏动作完成后仍能看到“有内容被隐藏”以及直接恢复入口，而不是记住高级属性位置或撤销历史。恢复动作应一次完成、结果立即可见，并能用一次 Ctrl+Z 撤销。
- 画布末尾增加上下文 `Show N hidden items` 按钮；Edit 菜单同步提供同名动态动作。无隐藏项时两者均不占用主界面，有隐藏项时即使所有可见 lane 都消失，入口仍位于快速新增按钮附近。
- `ShowHiddenLanesCommand` 在创建时捕获当前场景全部隐藏 lane，作为单个命令统一恢复，并支持原子 Undo/Redo；无隐藏项时不产生历史或状态 ID。恢复结果进入现有保存点和 autosave 判定。
- 关闭 `Visible` 后会立即清理不可见 lane 的标题选择、波形选择、编辑状态和 Bus 便携面板，防止键盘操作继续作用于不可见目标；定位请求也不再重新选中隐藏 lane。状态栏明确说明底部和 Edit 菜单的恢复入口。
- 空画布提示统一使用 hidden item，准确覆盖 signal 与 group。新增 `wave-hidden-lane-smoke` 使用真实 Lane properties 对话框关闭 `req`，验证两处入口、恢复、Undo、Redo 及两次 Undo 回到示例工程原保存基线。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 19 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
22/22 tests passed
Total Test time: 11.47 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
22/22 tests passed
Total Test time: 11.10 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-hidden-lane-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.21 sec

Automated QA: 示例工程初始有 1 个隐藏 group，画布末尾和 Edit 菜单均显示 Show 1 hidden item；
              在 req 的真实 Lane properties 对话框关闭 Visible 后，req 消失、不可见选择清空、两处入口更新为 Show 2 hidden items，状态栏说明恢复位置；
              点击画布按钮后 group 与 req 同时恢复且显示 Ctrl+Z；Undo 后两者重新隐藏，Redo 后重新恢复；再 Undo 两次回到 req 可见、group 隐藏的 Saved 基线，无标题星号。
Desktop interaction: none
```

## 持续迭代 65：Group 画布管理闭环

状态：完成

已交付：

- 开发审计确认纯波形工作区会渲染可见 Group 行，但标题单击、右键、双击和 F2 均显式排除 `LaneKind::Group`。从第 64 轮入口恢复隐藏 Group 或通过 Edit 添加后，用户能看到空白 Group 行，却无法从唯一工作区选中、重命名、删除、重排或再次进入属性，形成不可管理对象。
- 用户真正需要的是把 Group 当作信号列表中的可操作分隔项：看到后直接点击即可知道目标，沿用已经学会的 F2、双击、拖动和 Delete，不应为 Group 猜测另一套隐藏入口。
- Group 标题现与信号标题共用单击选择和拖动状态机；选择反馈明确显示 Group 名称、Delete 目标及 F2。Marker/范围选择清理、Esc 中断拖动、插入位置反馈继续沿用既有规则。
- F2 与双击打开同一个非模态就地重命名框，辅助名称、入口、取消、空名/重名纠错、提交结果均区分 Group；名称修改仍通过 `ChangeLaneCommand` 保持 stable ID 和成员引用。右键 Group 标题提供 `Group properties…`，进入现有完整属性对话框。
- Delete/Backspace 复用依赖感知删除，确认框说明成员信号将保留并解组；完成反馈显示实际解组数量。`AddLaneCommand`、`ChangeLaneCommand`、`MoveLaneCommand`、`RemoveLaneCommand` 对 Group 返回 Add/Change/Move/Remove group 描述，使 Undo/Redo 不再误称 lane。
- 新增 `wave-group-header-smoke`：从示例工程的隐藏 Group 恢复开始，真实点击选择、F2 重命名、Undo/Redo、双击与 Esc、右键属性并取消、拖动重排与 Undo、Delete 确认 5 个成员解组及 Undo，最后逐步回到原始隐藏 Group 的 Saved 基线。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
23/23 tests passed
Total Test time: 11.63 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
23/23 tests passed
Total Test time: 11.55 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-group-header-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.23 sec

Automated QA: Show 1 hidden item 恢复 Group 后，标题单击立即显示 Group/Delete/F2 目标；
              F2 与双击均打开 Group 就地重命名，Esc 恢复目标，右键可进入并取消 Group properties；
              标题拖动显示插入位置并以 Move group 单步撤销；Delete 前说明 5 个成员将解组，完成后成员保留且 Group 消失，Undo 原子恢复；
              再撤销重命名与显示命令后回到原始隐藏 Group、Saved、无标题星号和 Show 1 hidden item。
Desktop interaction: none
```

## 持续迭代 66：长层级信号名可辨识性与列宽直接调整

状态：完成

已交付：

- 开发审计确认 Signals 列仍固定为 190 px，名称只做右侧省略；两个共享长层级前缀、仅后缀不同的信号会显示为近似相同文本，且标题没有完整名称提示或列宽调整入口。该问题不改变模型，但会直接导致用户选错信号。
- 用户视角从打开长层级工程开始复现：首先需要在左侧确认具体信号，再编辑对应波形；原界面要求依赖记忆或逐行猜测，无法确认省略部分。用户真正需要的是在原位置辨认名称并立即扩展空间，而不是打开属性窗口。
- 名称渲染改为中间省略，保留层级前缀与区分后缀；悬浮信号或 Group 标题显示完整名称。离屏视觉复核中，默认 190 px 列宽已能分别显示 `request_valid` 与 `acknowledge_ready` 的差异，波形和网格边界保持连续。
- Signals 与波形之间增加 11 px 命中带：悬浮显示水平分隔鼠标并提示“拖动调整、双击适配”；拖动在 140–480 px 范围内实时更新标尺、波形、底部添加按钮、内联输入框、End 输入和 Bus 面板位置，释放后给出当前像素宽度。
- 双击分隔线按当前可见 Lane/Group 的名称与类型说明自动适配；超长名称安全限制到 480 px。拖动期间按 Esc 恢复上次提交宽度，恢复当前编辑工具对应的鼠标形态。
- 确认后的列宽写入应用级界面偏好，新窗口启动时先恢复列宽再执行时间轴自动适配。列宽调整不修改 Project、不进入命令栈、不触发 autosave，也不改变 Saved/标题星号状态。
- 新增 `wave-signal-header-smoke`：构造共享长前缀的 `request`/`ack` 名称，覆盖完整名称提示、分隔线提示与鼠标形态、190→300 px 实时拖动和提交、300→360 px 后 Esc 恢复、双击自动适配到 480 px、QSettings 跨窗口恢复、启动时间轴重新适配，以及全过程 Saved/Undo 隔离。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure -j 4
24/24 tests passed
Total Test time: 3.74 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure -j 4
24/24 tests passed
Total Test time: 3.73 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-signal-header-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.23 sec

Automated QA: 默认 190 px 下，两个共享长前缀的名称均可悬浮读取完整文本，并以中间省略保留不同后缀；
              分隔线悬浮明确显示可拖动/双击，拖至 300 px 后立即提交并持久化且仍为 Saved；
              第二次拖至 360 px 后 Esc 恢复 300 px；双击按可见名称适配至 480 px，新窗口恢复同一宽度并重新适配完整时间轴；
              全过程 Undo 保持不可用、无标题星号、无工程 dirty。
Offscreen visual QA: build/round66-long-names-after-fonts.png
Desktop interaction: none
```

## 持续迭代 67：长信号列表跨视口拖动重排

状态：完成

已交付：

- 开发审计确认标题拖动只能在当前可见区域内更新放置位置；当信号数量超过画布高度时，拖到上下边缘不会滚动，无法在一次手势中将首行移到末行。键盘重复移动或中断拖动后手动滚动会增加操作次数，并使插入目标难以确认。
- 用户视角以 20 条 Bit 信号复现：用户要把第一条移动到列表末尾，首先会按住标题向下拖；原行为停在当前视口底部，插入线无法到达真实末行。期望是保持一次拖动，由边缘自动推进列表，并允许随时 Esc 安全取消。
- 标题拖动进入画布上/下 48 px 边缘后，以 30 ms 间隔、每次 28 px 持续滚动；每个计时周期重新计算真实放置索引，插入线与状态提示同步更新。离开边缘、失去左键、释放、Esc、切换工具或切换文档均立即停止计时器。
- Esc 或异常失去左键会恢复拖动开始前的垂直视口，不改变模型、Undo 栈或 Saved 状态；状态栏明确显示信号仍处于原位置。成功释放保留目标视口，并继续以单个 `MoveLaneCommand` 提交，Ctrl+Z 一步恢复。
- 取消、释放和工具切换统一恢复当前工具的默认鼠标形态，Selection、Wave Edit、Measure 及兼容工具不会残留拖动光标。
- 新增 `wave-lane-autoscroll-smoke`：构造 20 条独立 Bit 信号，覆盖下边缘启动、Esc 停止并恢复首行视口、首行→末行单次拖动、末行→首行单次拖动、真实位置状态、计时器释放停止、一步 Undo、Saved/dirty 隔离及离屏截图。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 21 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure -j 4
25/25 tests passed
Total Test time: 3.78 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure -j 4
25/25 tests passed
Total Test time: 3.75 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-lane-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 2.09 sec

Automated QA: 按住 signal_00 拖入下边缘后列表持续推进且鼠标为 ClosedHand；
              Esc 立即停止并恢复首行视口、原始顺序、Saved 与空 Undo，状态显示 signal_00 仍为第 1 位；
              再以一次拖动将 signal_00 放到第 20 位，释放后停留目标并只产生一个 Undo；
              从末行反向拖到上边缘同样到达第 1 位，释放停止计时器，Ctrl+Z 一步恢复精确顺序。
Offscreen visual QA: build/qtcreator-debug/lane-autoscroll-smoke.png
Desktop interaction: none
```
## 持续迭代 68：Wave Edit 水平跨视口拖动

状态：完成

已交付：

- 开发审计确认 Wave Edit 的拖动位置只在收到鼠标移动事件时按当前水平滚动值计算；放大长时间轴后，Shift 范围、范围端点、Bit 多拍、Bit 边沿以及 Segment 移动/改边界到达视口左右边缘即停止推进，现有实现没有水平边缘计时器。
- 用户视角从放大后的长时间轴移动一个 Segment：用户会保持鼠标按下并拖到屏幕边缘，期望画布继续向目标方向推进；原行为要求取消、缩小或先平移再重新定位对象，打断一次直接编辑手势，也增加选错时间的风险。
- 所有 Wave Edit 拖动进入波形区左右 48 px 后，以 30 ms 间隔、每次 28 px 持续水平滚动；每个计时周期重新执行指针到整数 tick 的换算、7 像素轻吸附、约束及预览更新，状态栏显示 `Auto-scroll left/right`。
- 自动滚动在实际超过系统拖动阈值后才启动，避免靠近边缘的普通单击被误判。离开边缘、到达滚动边界、释放、Esc、异常失去左键、工具切换或文档切换均停止计时器。
- 释放保留目标视图并沿用原交互的提交语义：Segment/Bit 边沿仍只产生一个可撤销命令，纯范围选择仍不修改模型。Esc 或异常失去左键恢复拖动开始前的水平视图，清除预览且不修改模型、Undo 栈或 Saved 状态。
- 新增 `wave-wave-edit-autoscroll-smoke`：构造 1 µs 长时间轴并放大约 6 倍，覆盖右边缘 Segment 模型外预览、Esc 停止与视图恢复、再次拖动后的单命令提交、释放后计时器停止、一步 Undo 回到精确 Saved 基线，以及左边缘 Shift 范围持续扩展与可操作选择保留。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure -j 4
26/26 tests passed
Total Test time: 4.10 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure -j 4
26/26 tests passed
Total Test time: 5.10 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-wave-edit-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 1.73 sec

Automated QA: 第一次将 data[7:0] Segment 保持在右边缘时，视图持续向右推进、目标范围实时变化而模型/Undo/Saved 不变；
              Esc 停止滚动并恢复起始视图，随后释放不提交；第二次拖动释放后只提交一个 Segment move，状态提供 Ctrl+Z；
              释放后滚动值保持稳定，Undo 精确恢复原 Segment 与 Saved；从时间轴中部 Shift 拖向左边缘时持续扩展范围，释放后保留可直接编辑的范围且不产生 Undo。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke.png
Desktop interaction: none
```
## 横向工作

- 每个阶段结束后同步更新 `README.md`、`PLAN.md`、`GOAL.md`。
- 每项完成状态必须对应自动化测试或可复现手工验证。
- 保持生成物为派生产物，不将其反向解析成场景事实源。
- 在导入和 compare 阶段建立 1000 lane、百万 transition 的只读浏览基准。
- 未经用户明确要求不执行远端 push。
