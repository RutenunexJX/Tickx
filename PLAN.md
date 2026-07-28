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

## 横向工作

- 每个阶段结束后同步更新 `README.md`、`PLAN.md`、`GOAL.md`。
- 每项完成状态必须对应自动化测试或可复现手工验证。
- 保持生成物为派生产物，不将其反向解析成场景事实源。
- 在导入和 compare 阶段建立 1000 lane、百万 transition 的只读浏览基准。
- 未经用户明确要求不执行远端 push。
