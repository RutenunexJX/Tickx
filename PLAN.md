# Wave Workbench 实施计划

更新时间：2026-08-19

状态定义：`完成` 表示具有可运行行为和自动化证据；`进行中` 表示正在实施；`未开始`
表示尚无可验收实现。文档中的完成状态不替代测试结果。

## ZeroSlack Wave Simulation S12.6：FST/Wellen 按需读取

状态：完成

- 新增独立 Rust 辅助程序，固定使用 Wellen 0.25.6 读取标准 FST；C++ 未实现第二套 FST
  解析器。元数据请求不解码 transition，信号请求每批最多 64 个稳定 ID。
- `wavefst` 提供纯数据元数据、按信号加载和身份校验合并接口。时间换算使用精确整数，
  不可精确换算、超限响应、未知信号、文件变化、取消或旧 generation 均明确拒绝。
- Actual 层级树先显示全部 FST scope/信号；初始优先加载映射信号，无映射时最多加载首 32
  个信号，用户复选后再加载对应 transition。Compare 与 Checks 在所需信号完成前延后执行，
  不使用不完整数据。
- `wavewidgets` C ABI 保持 v1，仅在辅助程序与共享库相邻时声明
  `on-demand-fst-trace/v1`。portable install 同时包含辅助程序与归属文档；关闭 Wellen
  构建选项后 VCD/CSV、CLI 和共享库仍可构建。
- 官方 Wellen counter 配对 VCD/FST fixture 验证值序列完全一致；GUI 冒烟验证从 8 个
  元数据 signal 中先加载 1 个映射 signal，再勾选并仅新增第 2 个。界面证据位于
  `artifacts/ui/wave/s12-fst-on-demand.png`。
- 大型官方 fixture `picorv32.vcd.fst` 实测元数据 495 个 signal、0 transition、31 ms；
  单信号请求仅返回 1 个 signal，耗时 9 ms。Wellen 可关闭构建与真实 portable 安装闭包
  均单独验证。

## ZeroSlack Wave Simulation S12.5：轻量 trace 检查

状态：完成

- Stimulus Scenario v5 持久化三类检查定义：指定时刻取值、半开区间稳定性、源边沿到目标
  边沿的响应窗口；v1-v4 文件继续兼容读取。
- 检查结果严格由当前 Actual TraceIndex 与既有 lane-to-trace mapping 派生，不持久化，也不
  成为第二套波形或语义事实源。无映射、无可用值和未激励源边沿明确报告 unavailable。
- Review 新增 Checks 页，可创建、编辑、删除和运行检查；结果表给出状态、首个失败 tick 和
  原因，选择结果会定位对应期望 lane、实际 signal 与时间位置。
- `wavewidgets` C ABI 继续为 v1，新增可选能力声明 `lightweight-trace-checks/v1`；ABI 测试
  校验运行动作和结果表，ZeroSlack 的真实共享库门禁同步校验该契约。
- `wave-expected-actual-ui-smoke` 使用确定性 simulator fixture 运行失败检查并验证导航状态；
  截图证据位于 `artifacts/ui/wave/s12-lightweight-trace-checks.png`。
- 全量 offscreen CTest `92/92` 通过。本机未安装真实 Verilator，外部编译/运行仍由确定性
  fixture 验证，不表述为真实 RTL 仿真。

## ZeroSlack Wave Simulation S12.4：Expected/Actual 比较

状态：完成

- Stimulus Scenario v4 为 watch lane 增加独立 `expectedSegments`。期望区间可部分覆盖，
  不参与 DUT 运行计划；v1/v2/v3 文件继续按原契约读取。
- Simulation Result 新增 Compare 动作、X 策略、边沿容差、摘要和差异表。比较范围严格
  限于具有期望区间的 watch lane，不要求其他未映射输出参加比较。
- 不一致区间同步标注在期望画布和实际波形；选择差异行同时定位期望 lane、实际 signal
  和对应 tick。场景或结果变化会使旧比较结果立即变为 stale。
- `wavewidgets` C ABI 继续为 v1，并新增可选能力声明
  `expected-actual-compare/v1`；ZeroSlack 的真实共享库门禁校验该能力和界面控制。
- `wave-expected-actual-ui-smoke` 使用确定性 simulator fixture 跑通生成结果、载入 Actual、
  执行比较与截图，证据位于 `artifacts/ui/wave/s12-expected-actual-compare.png`。
- 全量 offscreen CTest `92/92` 通过。本机未安装真实 Verilator，外部编译/运行仍由确定性
  fixture 验证，不表述为真实 RTL 仿真。

## ZeroSlack Wave Simulation S12.3：struct、array 和 interface 输入编辑

状态：完成

- Module Manifest v3 消费 ZeroSlack Slang 产出的结构化 leaf selector，不在消费者侧重新
  解释 SystemVerilog。packed struct、固定 unpacked array 和显式 modport interface 均导入
  为分组、可直接编辑的 lane。
- Stimulus Scenario v3 持久化结构化 binding；结构化根重命名后可按 selector/type 唯一迁移
  leaf 刺激和自动 group，同时保留显示顺序与可见性。
- runner 生成确定性 wrapper 重建 struct、array 和 interface 根端口，VCD flat trace 名稳定
  映射回语义 leaf。无安全事实时明确拒绝，不使用字符串扫描或总位宽猜测。
- `wave-structured-input-ui-smoke` 使用 ZeroSlack 实际生成的 manifest fixture 编辑三类输入并
  渲染真实 WaveCanvas；截图位于 `artifacts/ui/wave/s12-structured-input-editor.png`。
- 当前 interface 限于无构造端口且显式指定 modport；`inout/ref`、动态数组和超过 64 bit 的
  leaf 明确拒绝。本机未安装真实 Verilator，wrapper 与运行链由确定性 fixture 验证。
- 全量 offscreen CTest `91/91` 通过。

## ZeroSlack Wave Simulation S12.2：多时钟和异步事件

状态：完成

- Module Manifest 的所有有效语义时钟候选分别导入为独立 ClockDomain；多候选诊断不再
  丢弃全部时钟，唯一候选的既有建议元数据保持兼容。
- Simulation Result 工具栏新增 `Clocks (N)` 入口，可逐个编辑周期、相位、占空比和
  有效边沿；结果视图同时公开现有 `Timing` 动作，可切换到 1 tick 异步编辑。
- runner 继续按每个输入端口独立生成运行计划。自动测试确认两个不等周期/相位/占空比
  的时钟不会合并，普通输入在非网格 tick 的事件不会被吸附。
- 构建缓存身份仍只包含源码、manifest、harness 和工具链；修改时钟或普通刺激仅生成新
  运行计划，复用同一已编译模型。
- `wavewidgets` C ABI 与 simulation workspace contract 仍为 v1，新增可选能力声明
  `multi-clock-async-events/v1`。
- 全量 offscreen CTest `90/90` 通过；结果窗口截图确认时钟域入口和 Timing 动作在双画布
  布局中可见。本机未安装真实 Verilator，外部编译/运行仍由确定性 fixture 验证。

## ZeroSlack Wave Simulation S12.1：内部信号层级浏览

状态：完成

- `wavetrace` 保留 VCD 原始 scope 组件并生成独立纯数据层级，包含稳定 trace signal ID、
  完整名称和位宽；包含点号的 scope 不再被 UI 字符串拆分误判。
- Simulation Result 的 Actual 区域新增可搜索、可复选的实例/信号树。scope 复选批量控制
  子树，单信号复选立即更新 Actual 波形；选择叶节点会揭示对应波形行。
- 初始可见集合来自场景 trace mapping，未映射内部信号保持可发现但不挤占结果视图；用户
  自定义集合在同一会话重跑时按稳定 ID 保留。
- Verilator harness 使用 99 层 trace 深度，并启用 VCD、struct 与下划线信号追踪；编译缓存
  指纹继续包含 harness，因此不会错误复用旧追踪模型。
- `wavewidgets` C ABI 与 simulation workspace contract 仍为 v1，新增
  `internal-signal-hierarchy/v1` 能力声明。
- 全量 offscreen CTest `90/90` 通过；真实界面截图确认层级树、位宽、搜索区和 Actual 波形
  同时可见。本机未安装真实 Verilator，外部编译/运行仍由确定性 fixture 验证。

## ZeroSlack 共享控件嵌入切片 S11

状态：完成

- `wavewidgets` 以版本化 C ABI 创建完整 simulation workspace；独立应用与 ZeroSlack
  嵌入形态消费同一 MainWindow、刺激画布、结果画布和工程模型。
- `wavetrace` 拥有 trace 实现，`waveimport` 作为兼容 target；`TimelineViewport` 统一
  StimulusCanvas 与 TraceCanvas 的 tick/pixel 映射。
- Portable component 安装独立应用、共享控件库和全部 CLI/契约资产；ABI、时间轴、
  独立应用、固定 fixture 及安装闭包均有自动化覆盖。
- 全量 offscreen CTest `90/90` 通过。真实共享库已由 ZeroSlack 动态加载并打开示例工程；
  当前机器未安装真实 Verilator，外部编译/运行路径继续使用确定性 fixture 验证。

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

FST 已通过 Wellen 辅助程序按需读取；不实现自定义或不兼容方言。

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

- FST 只支持 Wellen 可解析的标准数字 bit-vector 信号；WDB 仍为非目标。
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
## 持续迭代 69：显式范围的上下文 Fit

状态：完成

已交付：

- 开发审计确认 `WaveCanvas::fitSelection()` 已具备整数时间范围缩放能力，但生产界面没有连接到该槽；工具栏唯一 Fit 动作固定执行 `fitScenario()`。该能力对用户不可达，已有代码不能减少任何实际步骤。
- 用户视角从长波形中 Shift 框选关注区间：下一步是放大该区间继续改值或检查边沿；原界面的 `Fit scenario` 只会缩回全局，用户必须反复 Ctrl+滚轮并重新保持指针锚点。选区越窄，重复缩放和定位成本越高。
- 保留原有单个 `FitScenarioAction`，不新增工具栏按钮。显式范围出现时，动作文本和提示就地变为 `Fit selection`；范围被清除后恢复 `Fit scenario`，因此按钮数量和位置始终不变。
- 点击 `Fit selection` 后，范围起点与终点准确映射到波形视口左右边界；显式范围、所选 signals、固定范围栏及后续编辑能力全部保留。状态栏明确提示 Esc 清除范围，以及随后使用 Fit scenario 返回概览。
- 点击 `Fit scenario` 恢复完整时间轴、水平滚动起点和零溢出视图。两种缩放均只改变视图，不修改 Scenario、命令栈、autosave 或 Saved 状态。
- 扩展 `wave-wave-edit-autoscroll-smoke`：在既有长时间轴左向 Shift 范围之后，覆盖初始 Fit scenario 文案、范围出现后的 Fit selection 文案/提示、选区满宽几何、固定栏持续可见、第二张离屏截图、模型/Undo/Saved 隔离、Esc 文案恢复及完整概览返回。工具栏收敛断言继续禁止独立 Fit selection 按钮，仅允许同一 `FitScenarioAction` 动态切换。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 20 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure -j 4
26/26 tests passed
Total Test time: 4.12 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure -j 4
26/26 tests passed
Total Test time: 4.10 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-wave-edit-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 1.75 sec

Automated QA: Saved 长时间轴中，Shift 范围出现后同一按钮从 Fit scenario 变为 Fit selection；
              点击后约 382.7 ns–528 ns 的范围准确占满波形宽度，固定范围栏仍位于工具栏且选区保持可编辑；
              状态说明 Esc 与返回概览方式，Scenario/Undo/Saved 不变；Esc 后按钮恢复 Fit scenario，再次点击回到水平滚动值和最大值均为 0 的完整时间轴。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke-fit-selection.png
Desktop interaction: none
```
## 持续迭代 70：长时间轴 Home/End 边界导航

状态：完成

已交付：

- 开发审计确认 Wave Edit 只处理左右方向键的步进移动，未处理 Home/End；QAbstractScrollArea 的默认按键行为不代表时间轴首尾。长波形保持放大时，用户无法用键盘直接到达 0 或 End。
- 用户视角在局部放大后需要核对起始复位或末端状态：原路径是拖水平滚动条、Fit 后重新放大，或连续按方向键。用户真正需要的是保持当前缩放与编辑上下文，只改变当前时间位置。
- Wave Edit 画布获得焦点、未处于拖动且无 Shift/Alt 等组合时，Home 与 End 分别将编辑光标设置为 0 和 Scenario End，并通过既有 `ensureCursorVisible()` 将水平视图滚到相应边界；Ctrl+Home 与 Ctrl+End 使用相同语义。
- 导航保留缩放比例、水平滚动范围、所选 signal、显式范围和固定范围栏；状态栏显示 `Timeline start/end`、精确光标时间及反向边界键。导航不触发命令、autosave 或 Saved 变化。
- 未注册全局 QAction 快捷键。End、重命名、Bus 值和范围值等内联 QLineEdit 获得焦点时继续自行处理 Home/End；Measure 的锁定 Marker 移动语义不变，避免一个导航键在不同选择状态下意外修改模型。
- 继续扩展 `wave-wave-edit-autoscroll-smoke`：Fit scenario 回到全局后放大 5 级，覆盖普通 End 到场景末端、滚动值等于最大值、End 输入框 Home 仅将文本光标移到 0、Ctrl+Home 回到时间 0、滚动最大值/缩放保持、Scenario/Undo/Saved 隔离、状态反馈及第三张离屏截图。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: 19 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure -j 4
26/26 tests passed
Total Test time: 3.97 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure -j 4
26/26 tests passed
Total Test time: 4.58 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-wave-edit-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 1.77 sec

Automated QA: Saved 的 1 µs 时间轴先保持 5 级放大，End 将编辑光标与视图直接移动到 1 µs，滚动值等于既有最大值；
              End 输入框获得焦点后按 Home 只把文本光标移至开头，时间轴仍在 1 µs；
              画布重新获得焦点后 Ctrl+Home 将光标与视图移到 0 ps，滚动最大值不变，证明缩放未丢失；全过程模型、Undo、Saved 和 Fit scenario 状态不变。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke-timeline-home.png
Desktop interaction: none
```
## 持续迭代 71：当前信号相邻边沿导航

状态：完成

已交付：

- 开发审计确认 Wave Edit 的左右方向键固定移动 10 ns，Home/End 只能到达全局边界；已选信号的实际变化点没有可达键盘路径。检查稀疏 Bus 区段或高频 Clock 时，用户仍需缩放、目测并点击，且点击精度受像素比例影响。
- 用户视角从“选中一个信号，逐个核对它什么时候变化”出发：需要的是严格跳过当前位置并到达前一个或后一个真实边沿，不是再移动一个固定时间步长。无相邻边沿时应保留上下文并说明全局边界键，不能静默移动或修改波形。
- Wave Edit 画布获得焦点且未拖动时，`Ctrl+Left` / `Ctrl+Right` 按当前所选 signal 导航。Bit、Bus、Enum 使用 Segment 起止边界；Clock 另外计算周期上升/下降沿，并纳入 Gate/Drive X 覆盖 Segment 边界。候选严格位于当前 tick 的前后，结果限制在 Scenario 范围内。
- 导航沿用 `ensureCursorVisible()`，保留缩放与信号选择；到达边沿时状态栏显示信号名、精确时间和反向快捷键，无候选时保持光标原位并提示 Home/End。信号名单击反馈直接公开 `Ctrl+Left/Right jumps edges`，不新增按钮或模式。
- 普通 Left/Right 的固定步进、Home/End、Measure 模式和所有文本输入焦点语义保持不变；相邻边沿导航不创建命令、不触发 autosave，也不改变 Saved 状态。
- 扩展 `wave-wave-edit-autoscroll-smoke` 的 1 µs Saved 夹具，增加 10 ns、50% duty Clock，并验证 Bus 0→50→100→50 ns、无更早边沿原位反馈，以及 Clock 0→5→10→5 ns；同时断言信号目标、水平视图、Scenario、Undo、Saved、选择提示和第四张离屏截图。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 20 ms / Release 4 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 15.12 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.20 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-wave-edit-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 2.23 sec

Automated QA: 选中 data[7:0] 时状态栏先公开 Ctrl+Left/Right；从 0 ps 连续向后到达 50 ns Segment 起点和 100 ns 终点，再向前回到 50 ns；
              再次向前不移动，并明确提示没有更早边沿及 Home。选中 clk 后，从 0 ps 依次到达 5 ns 下降沿、10 ns 上升沿，再返回 5 ns；
              全过程保持 5 级缩放、所选 signal、水平起始视图、Scenario、Undo 和 Saved 状态。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke-next-edge.png
Desktop interaction: none
```

## 持续迭代 72：纵向信号键盘导航

状态：完成

已交付：

- 开发审计确认 Wave Edit 已拦截左右方向键、Home/End 和 Ctrl+Left/Right，但普通 Up/Down 仍落入 `QAbstractScrollArea` 默认滚动。它只能移动视口，不能建立下一条信号目标，后续边沿跳转、拍级写值和 F2 均无法形成连续键盘路径。
- 用户视角在固定时间逐行核对信号：原路径要求每一行都重新移动鼠标并点击，长列表还要手工滚动。用户需要保持当前 tick 和水平缩放，只把目标移动到视觉上的上一条或下一条信号；Group 不是可采样信号，应自动跳过。
- Wave Edit 画布获得焦点时，普通 Up/Down 选择上一条/下一条可见非 Group Lane；无选择时 Up 从末条、Down 从首条进入。导航清理旧的单拍/Segment 选择但不修改模型，保留时间光标和水平视图，并仅在目标行离开视口时调整最少的垂直滚动量。
- 首末边界保持当前目标并说明反向键；成功反馈显示信号名、可见序号/总数，以及 Up/Down 和 Ctrl+Left/Right 的后续路径。信号标题选择反馈也直接公开 Up/Down；`Alt+Up` / `Alt+Down` 的可撤销重排未被拦截。
- 键盘选择明确不设置标题删除状态，因此随后 Delete 不会误删整条信号。拖动期间要求先完成或 Esc；显式范围期间保持范围并提示 Esc，不隐式丢弃用户操作。
- 首次专项运行暴露 QLineEdit 忽略 Up/Down 后向父画布冒泡的问题；现在检测画布内获得焦点的 `QLineEdit` 并在父级消费冒泡事件。End、重命名、快速参数及其他内联文本编辑不再触发信号切换。
- 扩展 `wave-lane-autoscroll-smoke`：20 项夹具含 19 条信号和 1 个 Group，覆盖 End 文本框焦点隔离、无选择 Down→首条、首端 Up 原位、连续 Down 跳过 Group、末条最小滚动、末端 Down 原位、反向 Up、Delete 安全、模型/Undo/Saved/水平视图隔离及第二张离屏截图。`wave-wave-edit-autoscroll-smoke` 另覆盖显式范围期间 Down 无损阻断。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 20 ms / Release 4 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 15.47 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.22 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-(lane-autoscroll|wave-edit-autoscroll)-smoke$" --output-on-failure
2/2 passed
Total Test time: 4.27 sec

Automated QA: End 文本框中的 Down 不改变选择或视口；画布 Down 从 signal_00 开始，Up 在首条原位提示；
              连续 Down 从 signal_04 跳过 group_05 到 signal_06，最终选择 19/19 的 signal_19，目标完整可见但滚动值小于最大值；
              末端 Down 原位提示，Up 返回 signal_18，随后 Delete 不删除 Lane。显式时间范围中的 Down 保持范围、信号、Fit selection、模型、Undo 与 Saved，并提示 Esc。
Offscreen visual QA: build/qtcreator-debug/lane-autoscroll-smoke-keyboard-navigation.png
Desktop interaction: none
```

## 持续迭代 73：所选信号即时采样值

状态：完成

已交付：

- 开发审计确认 `drawLane()` 仅在 Measure 模式通过活动光标为所有 Lane 绘制值；Wave Edit 已有全高编辑光标、上下信号导航和相邻边沿导航，但左侧标题仍只显示名称/类型，导航状态也只说明时间。用户到达目标后必须重新目测波形或区段文字。
- 用户视角的完整键盘路径是“Up/Down 选信号→Ctrl+Left/Right 到边沿→立即读出该信号值”。如果仍需把视线移回波形并判断高低电平，前两轮导航只减少了定位操作，没有消除确认成本；Clock 边沿处的精确前后语义尤其容易误判。
- 抽取统一 `laneValueAt(lane, tick)`：Clock 复用周期与 Gate/Drive X 覆盖采样，Bit 隐式值为 `0`，Bus/Enum 隐式值为 `X`，其余类型保留既有 `?` 语义；Measure 的 `cursorValue()` 继续委托该函数，避免两套取值规则分叉。
- Wave Edit 仅对当前 `selectedLaneIds` 中的非 Group Lane，在标题第一行右端绘制当前 `cursorTick` 值，颜色沿用编辑光标青色并预留固定 78 px；名称继续中间省略，未选行不预留空间。拖动期间不绘制该值，因为预览尚未提交且模型值可能与预览结果不同。
- 信号标题选择、Up/Down 成功选择以及 Ctrl+Left/Right 到达边沿的状态反馈同步包含采样值。没有边沿、范围阻断和文本焦点路径保持原反馈；取值与显示不修改 Scenario、Undo、autosave 或 Saved 状态。
- 扩展 `wave-wave-edit-autoscroll-smoke`，验证 Bus 选择时隐式 `X`、50 ns 起点 `0x35`、100 ns 终点 `X`、返回起点 `0x35`，以及 Clock 5 ns 下降沿 `0`、10 ns 上升沿 `1`、返回下降沿 `0`；最终离屏截图验证左侧 clk 的青色 `0` 标签。`wave-lane-autoscroll-smoke` 验证隐式 Bit `0`；`wave-cursor-mode-smoke` 继续通过，确认 Measure 全 Lane 值显示未回归。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 19 ms / Release 4 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 15.97 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.40 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-(lane-autoscroll|cursor-mode|wave-edit-autoscroll)-smoke$" --output-on-failure
3/3 passed
Total Test time: 4.59 sec

Automated QA: 选中 data[7:0] 时先显示隐式 X，Ctrl+Right 到 50 ns 后显示 0x35，再到 100 ns 显示 X，Ctrl+Left 返回时恢复 0x35；
              选中 clk 后，5 ns/10 ns/5 ns 导航分别显示 0/1/0。长列表选择 signal_00 和 signal_19 时显示隐式 0；Measure 专项保持通过。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke-next-edge.png
                     build/qtcreator-debug/lane-autoscroll-smoke-keyboard-navigation.png
Desktop interaction: none
```

## 持续迭代 74：Bus 键盘一拍编辑

状态：完成

已交付：

- 开发审计确认 Bus 已有非模态一拍编辑条、值校验和命令栈提交，但 Wave Edit 的键盘路径在“Up/Down 选信号→Ctrl+Left/Right 到边沿→读值”后中断；Enter 未处理，用户仍需重新定位鼠标并双击或点击浮层。
- 用户视角需要在核对到某个 Bus 边沿后直接修改该拍：目标信号、时间和当前值均已确定，此时再要求鼠标命中同一位置会增加一次定位和一次精度判断。选中反馈、键盘选信号反馈和 Bus 边沿导航反馈现在均公开 `Enter edits value`。
- Wave Edit 画布获得焦点后，普通 Enter 对当前所选 Bus 打开既有便携编辑条，将当前整数 tick 作为精确目标并预填该处采样值；编辑条内 Enter 提交当前一拍，Esc 取消。提交继续走 `SetLaneRangeCommand`，保持现有值规范化、Event/Relation 一致性、Undo/Redo、autosave 和 Saved 语义。
- 拖动或标题重排期间 Enter 要求先结束当前手势；没有 Bus 目标时说明先选择 Bus；光标位于 Scenario End 时提示 Left/Ctrl+Left，不把 End 钳到最后一拍。显式 Bus 范围已存在时，Enter 聚焦固定范围值输入框，不将范围静默缩成单拍。
- 首次目标 smoke 发现精确位于 50 ns Segment 起点时，入口把 tick 转成像素后再反算为 49,999 tick，导致编辑框误显示空值。`showBusPresetPalette()` 现在接受可选精确 tick，键盘与双击入口不再经像素往返；鼠标点击路径仍按真实指针位置换算。
- 扩展 `wave-wave-edit-autoscroll-smoke`：验证 Bus 选中提示、50 ns 边沿 Enter 打开、`0x35` 精确预填、编辑框可见与获得焦点、上下文/Enter/Esc 反馈、模型/Undo/Saved 零变化、Esc 取消、再次打开并提交 `0x5a`、一拍选择、Unsaved/标题星号/Ctrl+Z 反馈、单步 Undo 精确恢复原 Scenario 与 Saved，以及第五张离屏截图。失败断言保留控件、焦点、值、tick 和状态栏诊断。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 20 ms / Release 4 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 15.58 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.31 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-wave-edit-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 2.26 sec

Automated QA: 选中 data[7:0] 后，状态栏公开 Enter；在 50 ns 边沿按 Enter，便携编辑条保持该精确时间并预填 0x35，模型仍为 Saved 且无 Undo；
              Esc 关闭草稿且零修改。再次 Enter 后输入 0x5a 并提交，只形成一个可撤销写值，显示 Unsaved changes、标题星号和 Ctrl+Z；
              单步 Undo 恢复原始 50–100 ns 的 0x35 Segment、完整 Scenario 与 Saved 状态。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke-keyboard-bus-edit.png
Desktop interaction: none
```

## 持续迭代 75：Enum 键盘一拍编辑

状态：完成

已交付：

- 开发审计确认 Bus 已具备键盘一拍编辑，但 Enum/state 信号仍依赖鼠标命中或整段属性窗口；在 Up/Down 选择信号、Ctrl+Left/Right 到达状态边沿后，键盘路径中断，且用户无法就地发现工程已声明的状态符号。
- 用户视角需要在读到某一拍状态后立即替换该拍，而不是重新定位鼠标或打开完整 Segment 编辑器。Enum 现在与 Bus 一样在选择、纵向导航和边沿导航反馈中公开 `Enter edits value`；现有 Segment 双击继续编辑完整区段，避免改变既有整段工作流。
- 复用既有便携值编辑条支持 Enum：Enter 在当前整数 tick 打开并预填该拍符号，编辑条内 Enter 通过 `SetLaneRangeCommand` 提交一拍，Esc 零修改取消；提交继续保持 Event/Relation、Undo/Redo、autosave 与 Saved 语义。
- Enum 编辑时隐藏仅适用于 Bus 的 `0`、`X`、`Z`、`Don't care` 按钮，输入框的可访问名称、占位文本和提示切换为 Enum 语义；补全列表以稳定字典序展示全部声明符号。非法符号保留原输入、面板和焦点，并在状态栏列出 `DONE, IDLE, WAIT_ACK` 等可用值。再次编辑 Bus 时四个快捷按钮恢复。
- 扩展 `wave-wave-edit-autoscroll-smoke`：加入 `state` Enum lane 和 `IDLE/WAIT_ACK/DONE` 三段状态，验证 50 ns 选中反馈、Enter 精确预填 `WAIT_ACK`、无模态窗口、补全模型、Bus 按钮隐藏、非法值原位纠错、Esc 取消、再次打开提交 `DONE`、Unsaved/Ctrl+Z 反馈，以及单步 Undo 精确恢复原 Scenario 与 Saved。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 21 ms / Release 4 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 16.06 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.71 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-wave-edit-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 1.90 sec

Automated QA: 选中 state 后同时显示 WAIT_ACK 与 Enter 编辑提示；在 50 ns 按 Enter 打开便携编辑条并精确预填 WAIT_ACK，
              补全列表包含 DONE/IDLE/WAIT_ACK，Bus 专用按钮全部隐藏，模型仍为 Saved 且无 Undo；输入 MISSING 后原位列出可用符号，
              Esc 关闭草稿且零修改。再次 Enter 后提交 DONE，只形成一个可撤销的一拍写值；单步 Undo 恢复原始三段 Enum 与 Saved。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke-keyboard-enum-edit.png
Desktop interaction: none
```

## 持续迭代 76：Enum 多拍范围赋值

状态：完成，本轮可用版本终点

已交付：

- 开发审计确认底层 `SetLaneRangesCommand` 与 Enum 值校验已支持范围赋值，但 `explicitRangeKind()` 只接受 Bit/Bus；用户可框选 Enum，却只看到“Mixed/unsupported”以及 Copy/Cut/Clear，必须逐拍修改或返回完整 Segment 窗口。
- 用户视角的目标是把连续多拍状态一次改为 `WAIT_ACK` 或 `DONE`。现在同类型 Enum 范围直接进入既有固定范围栏，目标数量、时间和类型可见；不新增模式或工具栏按钮，现有 Bit/Bus、混合选择与 Segment 双击语义保持不变。
- Enum 范围只显示一个符号输入框，隐藏 `0`、`1`、`X`、`Z`、`Don't care` 等 Bit/Bus 专用预设；输入框提供大小写不敏感的补全、可访问名称、占位文本和完整提示。多条 Enum 同选时补全模型取全部目标声明符号的交集，避免推荐无法原子提交的值。
- 非法或空值保留选区、输入、焦点与 Saved 状态，并继续阻止 Save/Measure 等会结束草稿的动作；状态栏列出共有符号。合法值经单个 `SetLaneRangesCommand` 原子写入全部目标，保持 Event/Relation、Undo/Redo、autosave 和 Saved 语义；数字 `0/1` 可直接赋值，`X/Z` 不再被错误宣传为单字符合法 Enum 值。
- 扩展 `wave-wave-edit-autoscroll-smoke`：加入第二条 `state_next` Enum，声明集合与 `state` 的交集为 `DONE/IDLE`；验证跨两行 50–100 ns 框选、固定栏类型/时间、共同符号补全、五个专用预设隐藏、无模态窗口、`MISSING` 原位纠错及统一草稿门禁、`DONE` 两信号原子提交、边界外值不变、单步 Undo 精确恢复 Scenario 与 Saved、Esc 清除上下文，以及第六张离屏截图。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 22 ms / Release 4 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 16.40 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.73 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-wave-edit-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 1.95 sec

Automated QA: Shift 拖动 state 50 ns 到 state_next 100 ns 后，固定栏显示 2 Enum 与 50–100 ns；补全只包含共同的 DONE/IDLE，
              五个 Bit/Bus 专用预设均隐藏，模型仍为 Saved 且无 Undo。输入 MISSING 后保留草稿、焦点和选区并阻断统一提交；
              输入 DONE 后两条信号仅在目标范围内改变并形成一个 Undo，单步撤销恢复完整 Scenario、选区与 Saved，Esc 再清除上下文。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke-enum-range-edit.png
Desktop interaction: none
```

## 持续迭代 77：键盘时间范围选择

状态：完成，持续迭代中

已交付：

- 开发审计确认第 70–76 轮已形成“Up/Down 选信号、Ctrl+Left/Right 跳边沿、Enter 编辑 Bus/Enum”的键盘路径，但建立多拍范围仍只能 Shift 拖动鼠标。用户在键盘定位到目标信号和时间后必须切回鼠标，属于高频工作流断点。
- Wave Edit 画布获得焦点后，`Shift+Left` / `Shift+Right` 从当前编辑光标建立显式范围，并以既有 10 ns 键盘步长移动活动端点。重复按键可向任一方向扩展或收缩，回到固定锚点时折叠范围；时间轴首尾原位提示且不产生空操作。
- 键盘创建与调整复用既有固定范围栏、范围手柄、Copy/Cut/Paste/Clear 和 Bit/Bus/Enum 批量赋值，不增加模式或按钮。已有多 lane 范围保留完整目标集合；折叠后保留当前信号但清除标题 Delete 武装，避免下一次 Delete 误删整条信号。
- 范围调整只改变选择和视口，不写 Scenario、不进入 Undo、不触发 autosave、不改变 Saved。范围值输入框实际持有焦点时继续由文本编辑消费 Shift 方向键；画布重新获得焦点后才执行时间范围调整，未提交草稿门禁保持有效。
- 扩展 `wave-wave-edit-autoscroll-smoke`：信号选中状态必须公开快捷键；对 `state/state_next` 的 50–100 ns 双 Enum 范围执行 Shift+Left 扩至 40 ns、Shift+Right 收回 50 ns；Esc 后从 Bus/0 ns 用 Shift+Right 创建 0–10 ns 范围，断言固定栏显示 `1 Bus`，保存第七张离屏截图，再用 Shift+Left 折叠并验证信号目标保留。所有阶段精确比较 Scenario、Undo、Saved 和范围栏状态。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 25 ms / Release 4 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 15.39 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.26 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-wave-edit-autoscroll-smoke$" --output-on-failure
1/1 passed; isolated final run 2.30 sec; full Debug run 1.85 sec

Automated QA: 选中信号后状态栏直接公开 Shift+Left/Right；双 Enum 既有范围可从活动端点扩至 40–100 ns 并收回 50–100 ns，
              lane 集合和固定栏保持不变。Esc 清理后，Bus 在 0 ns 通过 Shift+Right 建立 0–10 ns 范围，反向按键折叠范围但保留 Bus 目标；
              全程模型仍为 Saved、无 Undo；折叠后 Delete 也不会误删信号。范围值输入框焦点与画布快捷键互不劫持。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke-keyboard-range.png
Visual result: 固定栏显示 1 Bus 与 0–10 ns，Copy/Cut/Paste/Clear、首拍选区、端点手柄、信号行和网格同时可见，无遮挡或画布跳动。
Desktop interaction: none
Packaging: not run during iteration
```

## 持续迭代 78：键盘多信号范围调整

状态：完成，持续迭代中

已交付：

- 开发审计确认第 77 轮已消除“键盘定位后必须用鼠标建立时间范围”的断点，但范围的纵向目标仍只能通过 Shift 拖动重新框选。用户从一条 Enum/Bus/Bit 开始批量编辑相邻信号时，仍需离开键盘并重新命中时间与信号两个维度。
- 显式范围存在时，`Shift+Up` / `Shift+Down` 以当前活动信号为端点，向相邻可见信号扩展或收缩连续目标块；回到锚点后继续反向按键可自然向另一侧扩展。Group 不进入波形目标，长列表只做保证活动行完整可见的最小纵向滚动。
- 鼠标范围框选现在保留真实释放端作为活动信号，并排除 Group 行，因此可直接接续键盘扩缩。固定范围栏同步刷新目标数量、Bit/Bus/Enum 类型、Enum 共有符号及混合类型的安全操作，不增加新模式或工具栏按钮。
- Bus/Enum 范围值存在未提交草稿时，键盘目标变化被原位阻止并将焦点返回输入框；修正或清除草稿后才允许改变信号集合。选择变化只影响选区与视口，不修改 Scenario、不进入 Undo、不改变 Saved。
- 扩展 `wave-wave-edit-autoscroll-smoke`：从 `state` 的 0–10 ns 单信号键盘范围开始，验证未提交 `D` 草稿阻断 `Shift+Down`；清除草稿后扩展到 `state_next`，固定栏切为 `2 Enum`，补全收敛为 `DONE/IDLE`，再用 `Shift+Up` 收回锚点。全过程断言活动信号、目标顺序、时间、Scenario、Undo、Saved、无模态窗口，并保存第八张离屏截图。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 19 ms / Release 4 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 15.51 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 16.01 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-wave-edit-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 1.99 sec

Automated QA: 在 state/0 ns 仅用键盘建立 0–10 ns 范围；输入未提交 D 后 Shift+Down 保留草稿、焦点和单信号目标。
              清除草稿后 Shift+Down 将活动端移至 state_next，目标按画布顺序变为两条 Enum，固定栏立即显示 2 Enum，
              共有符号仅为 DONE/IDLE；Shift+Up 收回 state，时间范围与光标不变。全程模型保持 Saved 且无 Undo。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke-keyboard-signal-range.png
Visual result: 固定范围栏显示 2 Enum 与 0–10 ns，两条 Enum 首拍均有透明蓝色选区；名称、采样值、波形和网格仍清晰可见，无覆盖、跳动或模态窗口。
Desktop interaction: none
Packaging: not run during iteration
```

## 持续迭代 79：键盘范围直达时间轴边界

状态：完成，持续迭代中

已交付：

- 开发审计确认第 77–78 轮已经能用键盘逐步扩缩时间与信号目标，但长时间轴上从当前锚点选到 0 或 End 仍需重复按 10 ns 步进键，或重新用鼠标跨视口拖动。整段初始化、复制和清除因此仍存在明显等待与定位成本。
- Wave Edit 画布获得焦点后，`Shift+Home` / `Shift+End` 复用既有活动端与固定锚点语义，一次将活动时间端移到 0 或 Scenario End。跨过锚点时范围翻向另一侧，回到锚点时按既有规则折叠并保留信号目标；已有多信号集合、固定范围栏和水平视口均保持同步。
- 信号选中状态、键盘范围状态及固定范围栏提示公开边界快捷键。活动端已经位于目标边界时不改变选区，并明确提示反向快捷键；选择和边界无效果均不写 Scenario、不进入 Undo、不改变 Saved。
- 范围值、End、重命名等 `QLineEdit` 持有焦点时继续自行消费 Shift+Home/End，只改变文本选区，不触发时间轴操作。拖动、标题按压或重排期间仍要求先结束当前手势。
- 扩展 `wave-wave-edit-autoscroll-smoke`：在 Bus 0–10 ns 键盘范围上验证范围值输入框 Shift+Home 只选中文本；画布 Shift+End 精确扩展至 0–1 us 并滚到末端；重复 Shift+End 保持模型和选区不变且给出边界反馈；Shift+Home 折叠至锚点，再次 Shift+Right 可正常重建一拍范围。全过程断言信号目标、固定栏、滚动位置、Scenario、Undo、Saved 与无模态窗口，并保存第九张离屏截图。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 19 ms / Release 4 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 15.70 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.82 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-wave-edit-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 2.05 sec

Automated QA: data[7:0] 的 0–10 ns 范围可由 Shift+End 一次扩展到完整 0–1 us，视口滚至末端且固定栏仍显示 1 Bus；
              再按 Shift+End 保持选区不变并提示已到 End。Shift+Home 回到 0 后折叠范围但保留 Bus 目标，随后可立即重建一拍范围。
              范围值输入框中的 Shift+Home 只选中 0xa5 文本，不移动时间光标。全程模型保持 Saved 且无 Undo。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke-keyboard-boundary-range.png
Visual result: 固定范围栏显示 1 Bus 与 0 ps–1 us；末端视口中整条 Bus 保持透明蓝色选区，End 手柄、采样值、波形和网格均清晰可见，无覆盖、跳动或模态窗口。
Desktop interaction: none
Packaging: not run during iteration
```

## 持续迭代 80：当前信号范围一键全选

状态：完成，持续迭代中

已交付：

- 开发审计确认剪切、复制、粘贴、撤销以及时间/信号范围扩缩均已有标准键盘路径，但缺少用户普遍预期的 `Ctrl+A`。在 1 µs 等长时间轴上，要从当前信号建立完整 0–End 范围仍需先定位到一端再扩展，且已有多信号目标容易在重建过程中丢失。
- Edit 菜单新增 `Select full signal range`，使用平台标准 `QKeySequence::SelectAll`。Wave Edit 中触发后，将当前信号或既有有效多信号目标集合的时间范围直接设为 0–Scenario End；不自动选择其他异类信号，避免一次全选产生不可赋值的混合目标。
- 全选保留当前编辑光标、缩放和水平视口，固定范围栏立即刷新信号数量、类型、时间与 Enum 共有符号；状态栏明确显示完整范围及后续 Copy/Delete/Esc 路径。选择只改变编辑上下文，不写 Scenario、不进入 Undo、不改变 Saved。
- 任何 `QLineEdit` 持有焦点时，Edit 菜单和 `Ctrl+A` 仍委托输入框全选文本；未提交的 Bus/Enum 范围值草稿也会阻止程序化范围变化并恢复输入焦点。拖动或标题重排期间要求先完成或取消当前手势。
- 扩展 `wave-wave-edit-autoscroll-smoke`：验证单 Bus 从无范围直接选中 0–1 us、光标和视口保持、范围值输入框仅选择 `0xa5` 文本；随后重建两条 Enum 的 0–10 ns 目标并以 Ctrl+A 扩展完整时间，断言活动信号、目标顺序、共同符号、固定栏、Scenario、Undo、Saved 与无模态窗口，并保存第十张离屏截图。

开发视角验收：

```text
cmake --build build/qtcreator-debug --target wave-workbench
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 38 ms / Release 5 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 16.47 sec

cmake --build build/qtcreator-release --target wave-workbench
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.45 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-wave-edit-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 2.00 sec

Automated QA: data[7:0] 在 0 ns、无时间范围时按 Ctrl+A，立即得到完整 0–1 us 的 1 Bus 范围，光标和水平视图保持在起点；
              范围值输入框中的 Ctrl+A 只选中 0xa5 文本。state/state_next 的 0–10 ns 两信号目标按 Ctrl+A 后保留活动信号与顺序，
              固定栏更新为 2 Enum 与 0 ps–1 us，共有符号仍为 DONE/IDLE。Esc 清理后模型保持 Saved 且无 Undo。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke-keyboard-select-all-range.png
Visual result: 固定范围栏显示 2 Enum 与 0 ps–1 us；两条 Enum 行的完整透明选区、名称、采样值、波形、Clock 与纵横网格均可辨识，无视口跳动或模态窗口。
Desktop interaction: none
Packaging: not run during iteration
```

## 持续迭代 81：键盘范围直达真实信号边沿

状态：完成，持续迭代中

已交付：

- 开发审计确认 `Ctrl+Left/Right` 已能精确导航真实边沿，`Shift+Left/Right` 已能按 10 ns 固定步进建立范围，但两组修饰键无法组合。对 50 ns Bus Segment、5 ns Clock 半周期或其他非固定步长边界，用户仍需多次按键或切回鼠标命中边沿。
- Wave Edit 新增 `Ctrl+Shift+Left` / `Ctrl+Shift+Right`：以当前范围活动端或当前编辑光标为起点，严格移动到当前活动信号的前一/后一真实边沿，并按既有锚点规则创建、扩展或收缩显式时间范围。Bit/Bus/Enum 复用 Segment 起止边界，Clock 复用周期上升/下降沿及 Gate/Drive X 覆盖边界。
- 已有单/多信号目标集合、活动信号、缩放和最小必要滚动语义均保留；固定范围栏同步更新时间、类型与可用操作。无相邻边沿时范围、光标和视口原位不变，状态栏显示信号名、当前位置及 `Shift+Home/End` 时间轴边界路径。
- 信号标题选中、Up/Down 选择、键盘范围状态及固定范围栏提示公开组合键。画布内 `QLineEdit` 保留 Ctrl+Shift 按词选择，不触发波形范围；拖动或标题重排期间仍需先完成或取消当前手势。选择不写 Scenario、不进入 Undo、不改变 Saved。
- 扩展 `wave-wave-edit-autoscroll-smoke`：Bus 从 0 ns 以组合键建立 0–50 ns，继续扩至 100 ns，再收回 50 ns；无更早边沿时验证原位反馈。范围值输入框验证组合键只选文本；Clock 验证 0–5 ns 首个半周期边沿。全过程断言信号目标、固定栏、视口、Scenario、Undo、Saved、无模态窗口，并保存第十一张离屏截图。

开发视角验收：

```text
cmake --build build/qtcreator-debug --target wave-workbench
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 18 ms / Release 5 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 16.24 sec

cmake --build build/qtcreator-release --target wave-workbench
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.46 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-wave-edit-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 2.08 sec

Automated QA: data[7:0] 在 0 ns 按 Ctrl+Shift+Right 后直接得到 0–50 ns，第二次到 100 ns，Ctrl+Shift+Left 收回 50 ns；
              再向左时范围和光标不变并提示 Shift+Home。范围值输入框中的组合键只选择文本；clk 从 0 ns 一次得到 0–5 ns 半周期范围。
              所有选择只改变编辑上下文，模型保持 Saved 且无 Undo。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke-keyboard-edge-range.png
Visual result: 固定范围栏显示 1 Bus 与 0 ps–50 ns；右端手柄精确对齐 data[7:0] 的 50 ns Segment 边沿，采样值、波形、Clock 与纵横网格均清晰可见，无视口跳动或模态窗口。
Desktop interaction: none
Packaging: not run during iteration
```

## 持续迭代 82：显式范围粘贴落点一致性

状态：完成，持续迭代中

已交付：

- 开发审计确认固定范围栏一直提示 `Paste ... at the selected start`，但实际实现统一使用活动光标。键盘从 0 ps 向右建立 0–10 ns 范围时，固定栏公开起点为 0 ps、活动光标位于 10 ns，旧行为会把内容错误粘贴到 10 ns，形成可见提示与实际写入不一致。
- `pasteAtCursor()` 现在先校验显式范围，并在显式范围存在时严格使用 `selectionRange.first`；没有显式范围时继续使用编辑光标，右键 `Paste copied range here` 的点击定位语义不变。单/多信号目标映射、类型与位宽原子校验、End 自动延长和命令栈语义均保持不变。
- 固定栏按钮提示、`Ctrl+V`、粘贴完成状态、粘贴后的持久范围现在使用同一左端点。活动端点仍可停在右侧用于继续扩缩范围，不再暗中改变粘贴目标；真实修改保持一个 Undo，原位无变化仍不制造空历史。
- 扩展 `wave-wave-edit-autoscroll-smoke`：从 data[7:0] 的 50–100 ns 复制 `0x35`，再用键盘建立 0–10 ns 目标，使活动光标明确停在 10 ns；断言固定栏公开 `selected targets at 0 ps`，`Ctrl+V` 后 5 ns 已为 `0x35`、结果范围为 0–50 ns、状态显示 `at 0 ps` 与 `Ctrl+Z`。单步 Undo 精确恢复原 Scenario 与 Saved，并保存第十二张离屏截图。

开发视角验收：

```text
cmake --build build/qtcreator-debug --target wave-workbench
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 25 ms / Release 7 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 16.29 sec

cmake --build build/qtcreator-release --target wave-workbench
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.53 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-wave-edit-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 2.13 sec

Automated QA: data[7:0] 的 50–100 ns 范围复制后，以 Shift+Right 建立 0–10 ns 目标，活动光标位于 10 ns；
              固定栏 Paste 明确显示目标起点 0 ps。Ctrl+V 后 5 ns 采样为 0x35，结果选区为 0–50 ns，
              状态栏报告 Pasted at 0 ps 与 Ctrl+Z；一次 Undo 恢复原波形、Saved 与空 Undo 栈。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke-keyboard-paste-start.png
Visual result: 固定范围栏显示 1 Bus 与 0 ps–50 ns；蓝色活动光标仍清晰位于 10 ns，data[7:0] 从 0 ps 起显示 0x35，
               50 ns 结果边界、100 ns 后的隐式 X、透明选区、其余波形与网格均可辨识，无模态窗口或视口跳动。
Desktop interaction: none
Packaging: not run during iteration
```

## 持续迭代 83：长信号列表即时查找

状态：完成，持续迭代中

已交付：

- 开发审计确认长列表虽已支持 `Up` / `Down` 逐条选择与最小滚动，但缺少直接查找入口；在 20 条或更多信号中定位末端目标仍需反复按键，完整名称即使可通过悬浮查看也不能直接作为导航条件。
- Edit 菜单新增平台标准 `Ctrl+F` 的 `Find signal…`。触发后只在 Waveform 工具栏临时显示紧凑查找栏；输入时按显示顺序即时匹配当前可见的非 Group 信号，名称和稳定 ID 均支持大小写不敏感的包含匹配。
- 首个结果立即选中并仅做必要垂直滚动，编辑光标、缩放与水平视图保持不变。`Enter` / `Down` 前进，`Shift+Enter` / `Up` 后退；首尾循环、当前序号与总数均可见。无匹配时输入框原位标红并保留原选择，不打开模态窗口。
- Esc 或关闭按钮收起查找栏并保留找到的信号；再次打开保留并全选上次查询。进入 Measure、新建或打开工程会安全收起查找栏。显式时间范围存在时 Ctrl+F 不会静默清除范围，而是提示先按 Esc；查找不写 Scenario、不进入 Undo、不改变 Saved。
- 扩展 `wave-lane-autoscroll-smoke`：20 条长列表中验证空查询、`signal_0` 的 9 个结果、`signal_1` 的 10 个结果、Group 排除、大小写不敏感 ID、前后按钮与 Enter/Shift+Enter、首尾循环、无结果、顶部/底部最小滚动、水平视图保持、显式范围门禁、Esc/关闭/重开、无模态窗口及 Scenario/Undo/Saved 零变化，并保存离屏截图。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 20 ms / Release 5 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 15.77 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.59 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-lane-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 2.53 sec

Automated QA: Ctrl+F 在 20 条信号中以 signal_1 立即定位 signal_10，并以 1/10 显示位置；Enter/Shift+Enter 可顺序与循环导航到 signal_19，Group 不进入结果。名称/ID 不区分大小写，无结果时保留当前信号；Esc/关闭后结果仍被选中，重开保留查询。显式范围先得到无损提示，模型保持 Saved 且无 Undo。
Offscreen visual QA: build/qtcreator-debug/lane-autoscroll-smoke-signal-find.png
Visual result: 查找标签、输入框、1/10 计数、前后与关闭按钮紧凑排列；signal_10 被选中，20 条信号、波形、时间轴和网格同时可见，无裁切、遮挡、模态窗口或水平视图跳动。
Desktop interaction: none
Packaging: not run during iteration
```

## 持续迭代 84：精确时间直接跳转

状态：完成，持续迭代中

已交付：

- 开发审计确认现有 Home/End、方向键与相邻边沿导航适合探索式移动，但缺少“已知目标时间”的直接入口；在长时间轴定位 375 ns 或第 25 周期仍需反复缩放、滚动或换算后点击。
- Edit 菜单新增平台标准 `Ctrl+G` 的 `Go to time…`。触发后只在 Waveform 工具栏临时显示紧凑跳转栏，接受整数 `ps` / `ns` / `us` / `ms` / `tick`，以及 `cycle N`；周期输入优先使用当前所选信号的时钟域，否则仅在项目时钟唯一时自动判定。
- Enter 或 `Go` 将编辑光标准确定位并只做必要的水平滚动，保留当前信号、缩放与垂直位置。成功后输入框规范化为当前时间并全选，便于连续跳转；Esc 或关闭按钮收起工具栏并保留结果，再次打开预填当前光标。
- 非法输入与越界时间在原位标红并公开允许范围，不打开模态窗口、不移动光标。显式时间范围存在时 Ctrl+G 不会静默清除目标，而是提示先按 Esc；未提交草稿仍优先阻止导航，已提交的 Bus/Enum 一拍浮层会在跳转前安全收起。
- 扩展 `wave-wave-edit-autoscroll-smoke`：验证动作、快捷键、工具栏默认隐藏、0 ps 预填、非法与越界输入、375 ns 精确跳转和最小揭示、Bus 选择与缩放保持、周期 25 映射至 250 ns、Esc/关闭/重开、显式范围门禁、Bus 浮层隔离、无模态窗口及 Scenario/Undo/Saved 零变化，并保存离屏截图。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 21 ms / Release 5 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 16.84 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.81 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-wave-edit-autoscroll-smoke$" --output-on-failure
1/1 passed
Total Test time: 2.07 sec

Automated QA: 选中 data[7:0] 后以 Ctrl+G 输入 375 ns，编辑光标准确到达且只做必要水平滚动，信号选择、缩放、Scenario、Undo 与 Saved 均保持；非法值和 1 us 之外的输入原位说明错误，cycle 25 使用唯一 10 ns 时钟到达 250 ns。Esc/关闭保留结果，重开预填当前时间；显式范围先得到无损提示。
Offscreen visual QA: build/qtcreator-debug/wave-edit-autoscroll-smoke-go-to-time.png
Visual result: Go to 标签、375 ns 输入、0 ps–1 us 范围、Go 与关闭按钮紧凑排列；375 ns 光标和所选 Bus 同时可见，无旧 Bus 浮层、裁切、遮挡、模态窗口或垂直视图跳动。
Desktop interaction: none
Packaging: not run during iteration
```

## 持续迭代 85：现有信号一键复制

状态：完成，持续迭代中

已交付：

- 开发审计确认复制现有信号此前需要“新增、重命名、全范围复制、选择目标、粘贴”多个步骤，且 Clock 还需要额外配置时钟域；该流程不适合快速建立相似激励。
- Edit 菜单新增 `Duplicate selected signal`，使用平台标准 `Ctrl+D`；信号标题右键同时提供 `Duplicate signal`。副本立即插入原信号正下方并成为当前选择，名称按 `<原名>_copy`、`_2` 等规则自动保证唯一，状态栏公开结果、`Ctrl+Z` 与 `F2` 入口。
- 副本保留 lane 类型、显示参数、分组、普通时钟关联和完整波形；lane 与 segment stable ID 全部重新生成，颜色从可读调色板重新随机分配且不与源信号相同。Clock 同时获得参数一致但身份独立的新 ClockDomain，后续修改不会联动原时钟。
- Event、Relation 与 trace mapping 不随波形复制，避免产生没有明确语义的关系副本。显式范围存在时操作无损阻断并提示先按 Esc；文本输入框获得焦点时 `Ctrl+D` 保持文本语义；Group 不提供复制入口。
- 新增 `DuplicateLaneCommand` 并扩展 `AddLaneCommand` 的显式插入位置；复制、撤销和重做均维持相邻顺序与稳定身份，整个用户动作只占一个 Undo。
- 扩展 `wave-canvas-add-lane-smoke` 与核心命令测试：覆盖 Edit/右键/画布 `Ctrl+D`、Bus 和 Clock 复制、相邻插入、唯一名称、不同随机颜色、属性与波形一致、lane/segment ID 独立、ClockDomain 独立、Event/Relation 不复制、显式范围门禁、单步 Undo/Redo、状态反馈及无模态窗口，并保存离屏截图。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 19 ms / Release 4 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 16.95 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.80 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-canvas-add-lane-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.93 sec

Automated QA: 右键信号标题复制 bus 后，只新增一个紧邻的 bus_copy；名称、类型、宽度、符号、进制、分组、可见性和波形保持一致，颜色及 lane/segment ID 独立，副本立即选中。Undo 精确移除副本，Redo 恢复同一身份和位置。Ctrl+D 复制 Clock 时生成参数一致的独立时钟域；显式范围保持并得到 Esc 提示，Event/Relation 数量不变。
Offscreen visual QA: build/qtcreator-debug/canvas-signal-management-smoke-duplicate-signal.png
Visual result: bus 与所选 bus_copy 相邻显示，DNT/CA?/X 波形在相同时间位置一致，副本使用不同颜色；状态栏明确显示复制结果、相邻位置、Ctrl+Z 和 F2，无模态窗口、遮挡、裁切或视图跳动。
Desktop interaction: none
Packaging: not run during iteration
```
## 持续迭代 86：可见信号与 Group 一步隐藏

状态：完成，持续迭代中

已交付：

- 开发审计确认隐藏可见信号此前必须右键进入完整属性窗口、定位 `Visible`、取消勾选并确认；纯波形工作区中，一个临时收起操作需要四个步骤并理解属性模型，且容易与删除混淆。
- Edit 菜单新增随选择动态命名的 `Hide selected signal` / `Hide selected group`；信号与 Group 标题右键分别新增 `Hide signal` / `Hide group`。触发后立即隐藏目标，不弹确认或属性窗口，不增加工具栏按钮。
- 新增专用 `HideLaneCommand`：信号与 Group 分别公开 `Hide lane` / `Hide group` Undo 描述，单步 Undo/Redo 始终作用于同一稳定 ID；对已隐藏对象执行时不污染命令历史。
- 隐藏后立即清理不可见标题选择、范围目标和便携面板，状态栏明确显示对象名称、实时 `Show N hidden items`、底部/Edit 恢复位置和 `Ctrl+Z`。显式范围存在时操作无损阻断并提示先按 Esc；既有 `Show hidden items` 继续一次恢复全部隐藏项。
- 扩展核心命令测试、`wave-hidden-lane-smoke` 和 `wave-group-header-smoke`：覆盖动态 Edit 动作、范围门禁、信号 Edit/右键隐藏、Group 右键隐藏、Group 不出现复制入口、无模态窗口、隐藏计数、选择清理、单步 Undo/Redo、恢复全部隐藏项、Saved 基线回归和离屏截图。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 19 ms / Release 4 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 16.07 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.94 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-hidden-lane-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.25 sec

Automated QA: 选中 req 后 Edit 菜单立即显示 Hide selected signal；存在显式范围时隐藏被无损阻断并提示 Esc。清除范围后一步隐藏 req，选择同步清空、Show 2 hidden items 立即出现、状态公开恢复入口与 Ctrl+Z，且没有模态窗口；单步 Undo 回到 Saved。右键信号标题可再次一步隐藏，Show hidden items 恢复信号与既有隐藏 Group，Undo/Redo 和两步回退精确回到示例工程基线。Group 标题右键只提供 Hide group 而不提供 Duplicate signal，实际隐藏及单步 Undo 同样通过。
Offscreen visual QA: build/qtcreator-debug/hidden-lane-smoke-quick-hide.png
Visual result: req 行已消失，clk/reset_n/ack/data/state/transfer 及其波形、时间轴和网格保持原位置；底部 Show 2 hidden items 按钮清晰可见，状态栏同时显示 Hidden signal req、恢复位置、Ctrl+Z 和 Unsaved changes，无弹窗、遮挡、裁切或视图跳动。
Desktop interaction: none
Packaging: not run during iteration
```
## 持续迭代 87：多个隐藏项按名称单独恢复

状态：完成，按用户要求在本轮推送后暂停后续迭代

已交付：

- 用户路径审计确认已有 `Show N hidden items` 只能一次恢复全部；当用户只想找回一个信号时，必须先恢复全部，再逐个重新隐藏其余项目。多个隐藏项时，画布末尾按钮改为分离式入口：主体继续一键恢复全部，箭头菜单按原顺序列出每个隐藏信号或 Group，并保留明确的 `Show all N hidden items`。
- 只剩一个隐藏项时，按钮自动回到无箭头的单击直达模式。菜单动作使用对象名称、类型和稳定 ID，不要求用户记住隐藏顺序或进入完整属性窗口。
- 新增专用 `ShowLaneCommand`。单项恢复保持原顺序与稳定 ID，立即选中并仅做必要的纵向揭示；水平滚动、缩放和编辑光标保持不变。状态栏公开恢复对象、剩余隐藏数量和 `Ctrl+Z`，单步 Undo/Redo 精确作用于同一项目。
- 显式范围存在时，单项恢复无损阻断并提示 Esc；范围、模型、Undo 与 Saved 均不变化。隐藏项专项改用构建目录内的示例工程副本，测试中断产生的 autosave 不再污染源码示例。
- 扩展核心命令测试和 `wave-hidden-lane-smoke`：覆盖信号/Group 单项恢复命令、无效果命令、菜单标签与类型、主体恢复全部、范围门禁、单项恢复后的选择/顺序/剩余计数/水平视图保持、Undo/Redo、单隐藏项直达模式、Saved 基线回归及两张离屏截图。

开发视角验收：

```text
cmake --build build/qtcreator-debug
Result: success

Core test executable: 26/26 passed
Million-transition metric: Debug 24 ms / Release 5 ms

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 16.03 sec

cmake --build build/qtcreator-release
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.80 sec

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-hidden-lane-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.34 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug \
  -R "^wave-group-header-smoke$" --output-on-failure
1/1 passed
Total Test time: 0.29 sec

Automated QA: 两个隐藏项时，按钮主体仍恢复全部，箭头菜单按名称提供 Show group Handshake signals、Show signal req 和 Show all 2 hidden items。显式范围存在时单项恢复保持范围并提示 Esc；清除范围后仅恢复 req，Group 继续隐藏，req 回到原顺序并被选中，水平滚动范围与位置不变，入口自动切换为 Show 1 hidden item。单步 Undo/Redo/Undo 精确恢复单项状态，随后既有全部恢复路径和 Saved 基线回归继续通过。
Offscreen visual QA: build/qtcreator-release/hidden-lane-smoke-quick-hide-restore-menu.png
                     build/qtcreator-release/hidden-lane-smoke-quick-hide-single-restore.png
Visual result: 紧凑菜单完整列出两个隐藏对象和全部恢复；单独恢复 req 后，req 行在原位置以选中态显示，Handshake signals 仍隐藏，Show 1 hidden item、剩余计数、Ctrl+Z 和 Unsaved changes 同时可见。时间轴、波形、网格、缩放与水平视图无跳动，无弹窗、遮挡或裁切。
Desktop interaction: none
Packaging: not run during iteration
```
## 持续迭代 88：Bus 直接编辑、同步/异步时序与 Ctrl 拖动复制

状态：完成，达到本轮可用版本；按用户要求完成验收并推送后结束本轮，不执行打包

已交付：

- Bus 编辑从画布附近浮层收敛为固定顶部编辑栏。单击以一个 Beat 为目标，双击以完整 Segment
  为目标；栏内直接显示目标范围、HEX/BIN/DEC/OCT、值输入、当前信号最近自定义值和
  `0` / `X` / `Z` / `Don't care`。拖动 Segment 或边界超过系统拖动阈值后自动收起编辑栏，
  避免陈旧目标与波形预览同时存在。
- 裸值按所选进制规范化；最近值按 lane 独立维护，快捷预设不挤占最近自定义值。整段自定义值
  只清除 `waveWorkbench.busPreset`，应用预设只替换该扩展，其他 Segment 扩展元数据保持不变。
- `Don't care` 使用灰色纹理和点线边界，Expected/Actual 比较跳过带该语义的 Bus 目标区间；
  普通 `X` 继续按既有比较规则处理。旧 `Reserved` 零值兼容保留。
- 新增默认 `Sync` 编辑。Bit 拍、边沿、Segment 移动/缩放、显式范围边界及 Bus/Enum 目标按
  关联时钟有效边沿和周期量化；无法判定关联时钟时回退 10 ns。工具栏同一动作可切换 `Async`，
  此时允许任意整数 tick，并保留 7 像素轻吸附及 Alt 临时绕过。模式切换取消未完成拖动，
  不提交半成品。
- 非 Bit、非 Clock Segment 支持 `Ctrl` + 拖动复制。预览保留源段、使用虚线并标注 `Copy`；
  释放后由独立 `CopySegmentCommand` 原子提交，支持 Undo/Redo、Event/Relation 重同步和必要的
  依赖清理。复制到完全相同范围为无效果操作，不进入历史且不清除 Redo。
- 更新核心及 offscreen GUI 回归，覆盖固定 Bus 编辑栏、进制与最近值、Don’t-care 比较、
  Sync/Async、Alt、Ctrl 拖动复制预览/提交/撤销/重做、相同范围空历史抑制、元数据保留和
  Qt Creator Debug preset。

开发视角验收：

```text
cmake --build build --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 16.62 sec

cmake --preset qtcreator-debug
cmake --build --preset qtcreator-debug
Result: success

QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-debug
26/26 tests passed
Total Test time: 16.46 sec

Targeted high-risk tests:
wave-core-tests, wave-canvas-add-lane-smoke, wave-wave-edit-smoke
3/3 passed

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
Automated QA: 单击 Bus 后固定栏明确显示 Bus · Beat、0 ps–10 ns、HEX、Value、Recent 和
0/X/Z/Don't care；裸值 2a 提交为 0x2a 并进入该 lane 最近值。双击只编辑完整 Segment。
默认 Sync 下修改以关联时钟一拍为最小单位；切到 Async 后可偏离时钟边沿，Alt 可绕过轻吸附。
Ctrl+拖动 Bus Segment 时原段保持可见，目标使用虚线 Copy 预览；释放后单步 Undo/Redo，
复制到原位不产生虚假历史。

Offscreen visual QA:
build/canvas-signal-management-smoke.png
build/canvas-signal-management-smoke-ctrl-drag-copy-preview.png

Visual result: 固定 Bus 编辑栏不覆盖波形、刻度或信号名，目标范围和快捷值无需猜测；
开始移动、缩放或复制后编辑栏自动收起。复制预览与普通移动具有不同光标、虚线和 Copy 文案，
源段仍可见；Sync/Async 状态在唯一工具栏中可直接确认。无弹窗、裁切、桌面抢焦点或视图跳动。
Desktop interaction: none
Packaging: not run during iteration
```

## 持续迭代 89：Bus/Enum 连续拍录入

状态：完成，达到本轮可用版本；按用户要求完成验收并推送后结束本轮，不执行打包

已交付：

- 固定 Bus/Enum 编辑栏的 Beat 目标新增连续录入：`Tab` 提交当前拍并进入下一拍，
  `Shift+Tab` 提交当前拍并回到上一拍；`Enter` 保留“提交并结束”，Segment 整段编辑不参与
  连续拍导航，不增加新的模式或常驻按钮。
- 当前 Beat 同时由固定栏中的精确时间范围和画布上的透明琥珀边框标明。高亮保持网格、波形和
  隐式 `X` 可见，避免用户在连续录入时猜测下一次写入位置。
- Sync 模式按关联时钟的一拍边界推进；Async 模式从当前范围连续前后移动并保留非同步偏移。
  到达时间轴首尾时，当前值正常提交，编辑器和目标保持原位，状态栏明确给出反向导航按键。
- 非法值不再通过重新定位清除当前选择；草稿、焦点、目标范围和既有模型值均原位保留。
  未修改的相同值可以继续导航，但不新增空 Undo 或清除 Redo；每个真实拍写入保持独立 Undo。
- 扩展 `wave-canvas-add-lane-smoke`，覆盖打开 Beat 目标、Tab/Shift+Tab、Sync/Async、
  非法值、相同值、首尾边界、逐拍 Undo、编辑器焦点和离屏截图。

开发视角验收：

```text
cmake --build build --parallel 4
Result: success

cmake --build build/qtcreator-debug --parallel 4
cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 16.63 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 16.31 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.89 sec

Core test executable: 26/26 passed
Million-transition metric: Debug 20 ms / Release 4 ms

Targeted high-risk test:
wave-canvas-add-lane-smoke 1/1 passed

Git diff --check: passed
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
Automated QA: 选中 Bus 后按 Enter，固定栏和画布共同标明当前 Beat。输入 0x11 并按 Tab 后，
0x11 写入当前拍，焦点留在输入框且目标前进一拍；输入 0x22 并按 Shift+Tab 后，第二拍写入且
目标返回第一拍。确认未修改的 0x11 只导航，不产生空历史；两次 Undo 精确清除两个真实写入。
0x1ff 超过 8 bit 时不跳拍、不覆盖既有 0x11，并保留错误草稿和焦点。

Sync 下目标严格前进一拍；切换 Async 后，从非同步边界开始的拍在 Tab 后保持相同偏移。
末拍按 Tab、首拍按 Shift+Tab 均提交当前值但不越界，状态栏分别提示 End/start 和可用反向键。

Offscreen visual QA:
build/canvas-signal-management-smoke-bus-sequential-entry.png

Visual result: 固定栏清晰显示 `bus · Beat · 110 ns–120 ns` 和 `Value · Tab next`；画布中
110–120 ns 目标以低透明度琥珀框显示，前一拍 0x11、隐式 X、红色虚线、网格和时间刻度均可辨认。
无弹窗、遮挡、裁切、桌面抢焦点或视图跳动。
Desktop interaction: none
Packaging: not run during iteration
```

## 持续迭代 90：隐式 X 拍无写入跳过

状态：完成

已交付：

- Bus/Enum Beat 编辑器中，未修改的隐式 `X` 明确显示为 `X (implicit) · Tab skips`。
- `Tab` / `Shift+Tab` 可直接移动到相邻拍，不把隐式值写成显式 Segment，不改变 Scenario、
  Undo、Redo 或 Saved；首尾仍保留当前目标并给出反向按键。
- `wave-canvas-add-lane-smoke` 覆盖前后跳过、目标高亮、模型与历史零变化。

## 持续迭代 91：Bus 数值草稿键盘步进

状态：完成

已交付：

- Bus 数值草稿可用 `Up` / `Down` 按当前进制加减 1，支持任意位宽二进制运算、signed decimal
  显示及 0/最大值边界钳制。
- 含 `X` / `Z` / 通配位的草稿不被猜测转换，保持原值并提示先输入确定数值；操作只改草稿，
  提交前不修改模型和历史。
- 定向 offscreen smoke 覆盖 HEX/BIN/DEC/OCT、上下界和非确定值拒绝。

## 持续迭代 92：每信号最近值键盘循环

状态：完成

已交付：

- Bus 编辑框内 `Ctrl+Up` / `Ctrl+Down` 按当前 lane 的最近自定义值前后循环；不同信号的历史
  相互隔离，快捷预设不进入最近值列表。
- 循环仅替换可见草稿并保留焦点、目标范围和模型状态，用户确认前不创建 Undo。
- `wave-canvas-add-lane-smoke` 覆盖前后循环、lane 隔离和零模型修改。

## 持续迭代 93：应用并保留当前编辑目标

状态：完成

已交付：

- `Ctrl+Enter` 提交当前 Bus/Enum 值后，固定编辑栏保持同一 Beat 或 Segment 打开，并重新读取
  已提交的规范化值；普通 Enter 继续提交并结束。
- 非法值继续原位纠错；真实写入只产生一个 Undo，相同值不产生空历史。
- 定向 offscreen smoke 覆盖 Beat/Segment 留驻、规范化结果、Undo 与焦点保持。

## 持续迭代 94：画布 Tab 拍级导航

状态：完成

已交付：

- Wave Edit 画布获得焦点且已有信号目标时，`Tab` / `Shift+Tab` 导航下一拍/上一拍，使用拍级
  高亮显示新目标；无需先打开 Bus/Enum 编辑器。
- 导航保留当前信号、缩放和水平视图，只改变编辑上下文，不写值、不创建命令；文本输入框继续
  使用标准 Tab 焦点语义。
- `wave-canvas-add-lane-smoke` 覆盖双向导航、首尾反馈及模型/历史零变化。

## 持续迭代 95：可见时间页导航

状态：完成

已交付：

- Wave Edit 画布上的 `PageDown` / `PageUp` 按当前可见时间跨度向后/向前移动编辑目标，并只做
  必要的水平揭示；不要求改变缩放或拖动滚动条。
- 到达 Scenario 首尾时保持原位并提示反向键；信号目标、Scenario、Undo 和 Saved 均保持。
- 定向 offscreen smoke 覆盖双向翻页、边界、选择与水平视图。

## 持续迭代 96：Clock 周期直接键盘编辑

状态：完成

已交付：

- Wave Edit 选中 Clock 后，`G`、`X`、`R` 分别对当前完整周期执行 Gate、Drive X、Run；状态栏和
  标题选择提示公开快捷键。
- 三个入口复用既有拍级命令与覆盖清理路径，保持周期选中、Event/Relation 同步和单步 Undo；
  重复 Run 不制造空历史。
- `wave-canvas-add-lane-smoke` 覆盖三种结果、周期范围、Undo/Redo 和无效果反馈。

## 持续迭代 97：Segment 拖动精确时序反馈

状态：完成

已交付：

- Segment 主体移动、左右边界缩放和 Ctrl 复制的实时预览直接显示值、目标起点、终点和
  `width`；状态栏同步同一组数据。
- 标签按实际文本宽度绘制并限制在波形视口内；普通静态选择仍保持简洁值标签。
- `wave-canvas-add-lane-smoke` 断言精确宽度并生成
  `canvas-signal-management-smoke-ctrl-drag-copy-preview.png`。

## 持续迭代 98：范围拖动精确宽度反馈

状态：完成

已交付：

- Shift 拖出范围及拖动左右端点时，画布就地显示 `起点–终点 · width`；释放后的状态栏继续显示
  完整范围和信号数量。
- 标签使用高对比背景、受波形视口约束，不遮挡固定范围工具栏；选择过程不修改模型或历史。
- `wave-wave-edit-smoke` 覆盖 20–40 ns 双信号范围、左右端点调整、精确宽度反馈和离屏截图。

## 持续迭代 99：Segment 一步复制到下一段

状态：完成，达到本轮可用版本；按用户要求验收并推送后结束，不执行打包

已交付：

- Bus/Enum Segment 右键菜单新增 `Duplicate segment after`，将完整值和扩展语义复制到紧邻的
  同宽区间，源段保留，结果立即选中。
- 复制通过 `CopySegmentCommand` 原子提交，Event/Relation 继续同步；一次 Undo/Redo 精确恢复。
  目标已相同不产生空历史，目标越过 End 时明确显示目标区间和 End，Scenario 与命令历史零变化。
- 修正同步模式右键命中：编辑光标仍按拍量化，但 Segment 菜单按真实点击时间命中，片段中部右键
  不再误判到下一拍。
- `wave-canvas-add-lane-smoke` 真实选择菜单动作，覆盖相邻复制、语义元数据、选择、单步
  Undo/Redo、End 边界反馈、无历史和同步模式中点命中。

开发视角验收：

```text
cmake --build build --parallel 4
cmake --build build/qtcreator-debug --parallel 4
cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 16.83 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 17.00 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 15.94 sec

Core test executable: Debug 26/26, Release 26/26
Million-transition metric: Debug 20 ms / Release 4 ms
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
Automated QA: 隐式 X 可无写入跳拍；Bus 草稿支持加减、最近值循环和 Ctrl+Enter 留驻；画布
Tab/Shift+Tab 与 PageUp/PageDown 可连续导航；Clock G/X/R 可直接修改一个周期。Segment
移动/缩放/复制及范围拖动均公开精确起止和宽度。Bus Segment 右键一步复制到下一段，保留
值与 Don’t-care 元数据，单步 Undo/Redo；末段复制明确提示 exceeds End 且历史不变。

Offscreen visual QA:
build/qtcreator-debug/canvas-signal-management-smoke-bus-sequential-entry.png
build/qtcreator-debug/canvas-signal-management-smoke-ctrl-drag-copy-preview.png
build/qtcreator-debug/wave-edit-smoke-range-selection.png

Visual result: 固定编辑栏的隐式 X/跳拍说明、当前拍琥珀框、Ctrl 复制虚线目标、110–120 ns 与
width 10 ns 标签、20–40 ns 固定范围栏、透明选区、手柄、波形和网格均可辨认。无模态窗口、
遮挡、裁切、桌面抢焦点或视图跳动。
Desktop interaction: none
Packaging: not run during iteration
```

## 持续迭代 100：Ctrl+D 优先复制已选 Segment

状态：完成

已交付：

- Wave Edit 中存在已选 Bus/Enum Segment 时，`Ctrl+D` 复制该 Segment 到下一等宽区间，不再误复制
  整条信号；没有 Segment 目标时仍保留既有信号复制语义。
- 全局 Duplicate 动作与画布快捷键共用同一路径，结果立即选中并保持单步 Undo。
- `wave-canvas-add-lane-smoke` 通过真实 QAction 验证 lane 数量不变、Segment 值与元数据保留。

## 持续迭代 101：Segment 双向相邻复制

状态：完成

已交付：

- Bus/Enum Segment 右键菜单同时提供 `Duplicate segment before` 与 `Duplicate segment after`；
  `Ctrl+Shift+D` 可一步复制到前一等宽区间。
- 起点或 End 边界不可容纳目标时，状态栏说明阻挡方向且不创建空历史。
- offscreen 回归覆盖前后复制、菜单可发现性、语义元数据、选择与单步 Undo。

## 持续迭代 102：显式 Segment 键盘导航

状态：完成

已交付：

- 已选 Segment 可用 `Ctrl+Tab` / `Ctrl+Shift+Tab` 跳到同一信号的下一/上一显式 Segment。
- 导航同步更新选择范围、编辑光标和最小水平揭示；到达首尾时原位说明，不修改模型或历史。
- `wave-canvas-add-lane-smoke` 覆盖双向导航、边界与 Scenario 零变化。

## 持续迭代 103：按编辑单位微移 Segment

状态：完成

已交付：

- 已选 Segment 可用 `Alt+Left` / `Alt+Right` 前后微移；Sync 使用当前信号的一拍，Async 使用
  一个整数 tick。
- 微移沿用拖动的相邻 Segment 与 Scenario 边界约束，值、扩展语义和 Event/Relation 同步保持，
  每次按键只产生一个可撤销命令。
- offscreen 回归覆盖双向一拍移动、精确范围、元数据、选择和单步 Undo。

## 持续迭代 104：Enum 声明符号直接轮换

状态：完成

已交付：

- Enum 就地值编辑器中，`Down` / `Up` 按声明顺序前后循环符号；未知草稿分别从首项/末项开始。
- 轮换只修改可见草稿，保留焦点并显示当前位置与总数；Enter 前不修改模型、Undo 或 Saved。
- `wave-wave-edit-autoscroll-smoke` 覆盖 `WAIT_ACK → DONE → WAIT_ACK` 及零模型变化。

## 持续迭代 105：Segment 选中精确反馈

状态：完成

已交付：

- 单击非 Bit Segment 后，状态栏显示信号名、当前值、精确起止、宽度，以及移动、双向复制、
  双击编辑和 Delete 入口。
- 单击仍仅改变编辑上下文，不写波形、不创建历史。
- `wave-canvas-add-lane-smoke` 覆盖精确范围、值、宽度、快捷键提示和 Scenario 零变化。

## 持续迭代 106：Segment 精确悬浮提示

状态：完成

已交付：

- Wave Edit 下悬浮 Bus/Enum/Clock Segment 即显示信号名、值、精确起止、宽度和直接操作提示；
  Bus/Enum 同时公开 `Ctrl+drag copies`。
- Bit 继续使用拍级悬浮反馈，不与 Segment 语义混淆。
- offscreen `QHelpEvent` 回归验证提示内容完整且模型不变。

## 持续迭代 107：指针下信号和值

状态：完成

已交付：

- Wave Edit 状态栏在指针停留于波形区时显示 `pointer <signal> · value <value>`。
- 值按实际指针位置采样，与同步模式中吸附后的编辑时间分开；指针位于 Segment 中部时不再误报
  下一边界的隐式值。
- offscreen 回归覆盖 Bus 显式值、同步半拍吸附与 Scenario 零变化。

## 持续迭代 108：可见编辑光标锚定缩放

状态：完成

已交付：

- Zoom in/out 在编辑光标可见时围绕该光标缩放并保持其屏幕位置；光标离屏时回退到视口中心。
- 工具提示和状态栏明确当前缩放锚点；缩放不修改模型、选择或历史。
- `wave-wave-edit-autoscroll-smoke` 以屏幕坐标断言放大和缩小误差不超过 2 px。

## 持续迭代 109：Ctrl+0 上下文 Fit

状态：完成，达到本轮可用版本；按用户要求验收并推送后结束，不执行打包

已交付：

- 单一 Fit 动作增加窗口级 `Ctrl+0`；存在显式范围时适配该范围，否则适配完整 Scenario。
- 快捷键沿用既有 `Fit selection` / `Fit scenario` 动态文本、固定范围栏和无模型修改语义。
- `wave-wave-edit-autoscroll-smoke` 通过真实快捷键路径验证选区满宽与完整概览恢复。

开发视角验收：

```text
cmake --build build --parallel 4
cmake --build build/qtcreator-debug --parallel 4
cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 16.99 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 16.88 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 16.47 sec

Core test executable: Debug 26/26, Release 26/26
Million-transition metric: Debug 20 ms / Release 7 ms
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
Automated QA: Ctrl+D/Shift 变体按当前 Segment 上下文执行双向相邻复制；Ctrl+Tab 双向浏览显式
Segment；Alt+Left/Right 以当前编辑单位微移并保持单步 Undo。Enum 编辑器 Up/Down 直接循环声明
符号。单击、悬浮和指针状态均公开当前 Segment 的值、起止与宽度。Zoom 围绕可见编辑光标保持
屏幕位置，Ctrl+0 在选区与完整场景之间使用同一 Fit 动作。

Offscreen visual QA:
build/qtcreator-release/canvas-signal-management-smoke-ctrl-drag-copy-preview.png
build/qtcreator-release/wave-edit-autoscroll-smoke-keyboard-enum-edit.png
build/qtcreator-release/wave-edit-autoscroll-smoke-fit-selection.png

Visual result: Ctrl 复制源段与虚线目标、精确 110–120 ns/width 10 ns 标签、Enum 固定编辑栏、
透明拍级目标、Fit selection 满宽选区、信号值标签、波形和网格均清晰可辨。无模态窗口、遮挡、
裁切、桌面抢焦点或视图跳动。
Desktop interaction: none
Packaging: not run during iteration
```

## 持续迭代 110：Enter 编辑已选 Segment

状态：完成

已交付：

- Wave Edit 中已选 Bus/Enum Segment 时，`Enter` 打开该完整 Segment 的固定值编辑栏，不再退回
  编辑光标所在 Beat。
- 编辑栏预填当前值并保持精确范围、Segment ID、信号目标和模型不变。
- `wave-canvas-add-lane-smoke` 覆盖固定栏、预填值、精确范围和零历史。

## 持续迭代 111：直接输入编辑已选 Segment

状态：完成

已交付：

- 已选 Bus/Enum Segment 后输入 `0` / `1` / `X` / `Z` 或其他可打印字符，直接以该字符启动
  Segment 草稿，不再修改单拍。
- 草稿提交前仅改变编辑器文本；Esc 可取消且 Scenario、选择和历史保持不变。
- offscreen 回归覆盖直接输入、修改态、精确目标和零模型变化。

## 持续迭代 112：Segment 值连续向后录入

状态：完成

已交付：

- Segment 值编辑栏内按 `Tab` 提交当前值并打开同信号的下一显式 Segment。
- 确认原值会保留 Bus preset 扩展元数据，不产生隐性命令或空历史。
- 固定栏占位、提示与状态反馈公开下一 Segment 路径；offscreen 回归覆盖值、范围、元数据和
  Scenario 零变化。

## 持续迭代 113：Segment 值连续向前录入

状态：完成

已交付：

- Segment 值编辑栏内按 `Shift+Tab` 提交当前值并打开上一显式 Segment。
- 到达首段或末段时，编辑栏保留当前目标，状态栏给出可返回方向，不丢草稿、不改变历史。
- offscreen 回归覆盖双向连续录入、首段边界、目标保持和原值零变化。

## 持续迭代 114：按编辑光标选中 Segment

状态：完成

已交付：

- 当前信号与时间已知时，`Ctrl+Space` 直接选中光标处的 Bus/Enum/Clock Segment，并公开值、
  精确范围和后续操作。
- 位于间隙边界时可选中恰好结束于该位置的左侧 Segment；Bit 继续保持拍级编辑语义。
- 修正无修饰 Space 平移与 `Ctrl+Space` 的快捷键分流；选择过程不修改 Scenario 或历史。

## 持续迭代 115：从光标进入后续 Segment

状态：完成

已交付：

- 只有信号和编辑光标、尚未选中 Segment 时，`Ctrl+Tab` 选择光标处或其后的最近显式 Segment。
- 结果同步更新精确范围、编辑光标和最小视图揭示；无后续段时原位说明。
- offscreen 回归覆盖间隙起点、结果选择、光标位置和 Scenario 零变化。

## 持续迭代 116：从光标进入前序 Segment

状态：完成

已交付：

- 只有信号和编辑光标时，`Ctrl+Shift+Tab` 选择严格位于光标之前的最近显式 Segment。
- 与正向路径共用 Bus/Enum/Clock 类型门禁、选择、视图和边界反馈；Bit 不被误切换为 Segment
  语义。
- offscreen 回归覆盖边界光标、前序结果和模型零变化。

## 持续迭代 117：键盘调整 Segment 左边界

状态：完成

已交付：

- 已选 Segment 可用 `[` 向左扩展、`Shift+[` 向右收缩左边界；Sync 使用一拍，Async 使用
  一个整数 tick。
- 调整沿用相邻 Segment、时间轴起点和最小宽度约束，保持值、扩展元数据与 Event/Relation
  一致。
- 每次按键只产生一个 Undo；offscreen 回归覆盖扩展、收缩、精确范围、元数据和逐步撤销。

## 持续迭代 118：键盘调整 Segment 右边界

状态：完成

已交付：

- 已选 Segment 可用 `]` 向右扩展、`Shift+]` 向左收缩右边界，编辑单位与左边界一致。
- 相邻内容、Scenario End 和最小宽度阻挡时不写模型；成功调整保持当前 Segment 选中。
- offscreen 回归覆盖扩展、收缩、值与元数据保持，以及两个独立原子 Undo。

## 持续迭代 119：退出 Segment 编辑后保留工作位置

状态：完成，达到本轮可用版本；按用户要求验收并推送后结束，不执行打包

已交付：

- 画布中按 Esc 清除 Segment 选择和编辑状态，但保留当前信号与精确编辑光标；随后可继续边沿
  导航或按 `Ctrl+Space` 恢复选段。
- 拖动取消仍恢复起始视图，并在保留信号/时间时明确说明；没有 Segment 上下文时维持既有
  清空语义。
- Segment 选中状态和悬浮提示公开 Enter、边界快捷键及 Esc 保留目标语义。

开发视角验收：

```text
cmake --build build --parallel 4
cmake --build build/qtcreator-debug --parallel 4
cmake --build build/qtcreator-release --parallel 4
Result: success

QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 17.28 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 17.22 sec

QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 16.59 sec

Core test executable: Debug 26/26, Release 26/26
Million-transition metric: Debug 21 ms / Release 4 ms
Desktop interaction: none
Packaging: not run during iteration
```

用户视角验收：

```text
Automated QA: 单击 Bus Segment 后，Enter 和直接输入均打开完整 Segment 草稿；Tab/Shift+Tab
在显式段间连续录入，并在首尾保留当前目标。Ctrl+Space 可按编辑光标选段；未预选时，
Ctrl+Tab/Ctrl+Shift+Tab 从信号和时间位置直接进入后/前一段。[、] 及 Shift 变体按一拍调整
左右边界，值与 preset 元数据保持，每次为一个 Undo。Esc 退出后信号和时间不丢失，并可立即
Ctrl+Space 恢复。

Offscreen visual QA:
build/qtcreator-release/canvas-signal-management-smoke-bus-sequential-entry.png
build/qtcreator-release/canvas-signal-management-smoke.png

Visual result: 固定 Segment/Beat 编辑栏、当前拍透明高亮、Bus 显式值与隐式 X、编辑光标、
信号名称、波形和网格均清晰；新增键盘路径不引入弹窗、遮挡、裁切、桌面抢焦点或视图跳动。
Desktop interaction: none
Packaging: not run during iteration
```

## 持续迭代 120：目标可见性与交互语义收敛

状态：完成，达到可用版本；未打包，未执行桌面交互

已交付：

- 工具栏新增持续可见的当前目标摘要，区分 Signal、Beat、Segment、Range 与 Measure；目标悬浮
  提示承载当前可用操作。Sync 直接显示关联时钟和实际每步周期，Async 显示 `1 tick/step`。
- 状态栏从快捷键手册收敛为本次选择或编辑结果；显式范围显示时隐藏重复目标标签，960 px 最小
  窗口仍保持范围栏全部控件可见和可点击。
- Segment 选择、前后导航改为 Edit > Segment 及 `F6` / `F7` / `F8`；前后复制、前后移动、
  左右边界扩展/收缩均提供明确菜单命令。`Ctrl+Space`、`Ctrl+Tab`、`Alt+Left/Right` 与方括号
  不再被画布占用。
- `Ctrl+D` 始终复制整条已选信号；Segment 是否选中不再改变命令含义。Delete 继续遵循既有
  Signal/Beat/Segment/Range 目标，但当前目标在执行前持续可见，整条信号删除仍保留确认。
- Bus/Enum 编辑器从顶部工具栏移到当前 Beat/Segment 附近的画布浮层，按目标行自动选择上方或
  下方并随滚动重新定位；点击信号标题只选择信号，不再意外弹出 Beat 编辑器。

开发视角验收：

```text
cmake --build build --parallel 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 17.72 sec

cmake --build build/qtcreator-debug --parallel 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 17.37 sec

cmake --build build/qtcreator-release --parallel 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 16.90 sec

Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: 当前目标和实际步长随 Signal/Beat/Segment/Range/Measure 状态更新；F6/F7/F8 与
Edit > Segment 的复制、移动、边界命令保持模型零变化或单步 Undo。Ctrl+D 在 Segment 已选时仍
只复制整条信号；旧冲突快捷键不会改变目标或模型。Bus 编辑器位于目标附近并随目标移动；信号标题
选择不再打开 Beat 编辑器。960 px 范围栏无裁切。

Offscreen visual QA:
build/qtcreator-release/canvas-signal-management-smoke-bus-sequential-entry.png
build/qtcreator-release/canvas-signal-management-smoke.png

Visual result: 目标标签、关联时钟与步长、就近 Bus 编辑器、波形目标、短状态结果和网格均清晰；
未发现模态窗口、裁切、桌面抢焦点或意外视图跳动。
Desktop interaction: none
Packaging: not run
```

## 持续迭代 121–140：编辑目标稳定性与就地操作闭环

状态：完成，达到本轮可用版本；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 121 轮：Wave Edit 的被动鼠标移动不再修改已确认的编辑光标。
2. 第 122 轮：指针下采样与编辑光标采样分离，左侧信号值和 Target 不再随悬浮漂移。
3. 第 123 轮：时间标尺支持单击定位编辑光标，并保持当前信号目标。
4. 第 124 轮：时间标尺支持按住左键连续拖动查样，实时更新精确时间和值反馈。
5. 第 125 轮：标尺拖动可用 Esc 或左键状态丢失安全取消，并恢复起始光标与水平视图。
6. 第 126 轮：在显式范围内部右键不再先清除范围，菜单直接沿用当前目标。
7. 第 127 轮：范围右键菜单集中提供 Copy、Cut、Paste 与 Clear。
8. 第 128 轮：范围右键菜单补充 Fit selected range，并在类型一致时提供 Edit value。
9. 第 129 轮：固定范围栏增加可见关闭按钮，关闭后保留当前信号和编辑光标。
10. 第 130 轮：信号名、标尺和非 Bit 波形支持同一次安全点击关闭旧范围并改选；Bit 仍保留一次防误触门禁。
11. 第 131 轮：Bus/Enum 就近编辑器增加可见关闭按钮，放弃草稿但保留目标。
12. 第 132 轮：Bus/Enum 就近编辑器增加可见 Apply，明确提交并关闭。
13. 第 133 轮：Bus/Enum 就近编辑器增加前一项/后一项按钮，与 Tab/Shift+Tab 语义一致。
14. 第 134 轮：Bus/Enum 就近编辑器增加 Beat/Segment 显式范围切换，并同步画布选区与上下文。
15. 第 135 轮：范围切换和导航前统一处理草稿；有效草稿先提交，无效草稿原位保留并阻止目标变化。
16. 第 136 轮：Bit 悬浮提示公开精确拍范围、当前值及单击/拖动将产生的结果。
17. 第 137 轮：隐式 Bus/Enum 悬浮提示明确显示 `X`、一拍范围与点击结果。
18. 第 138 轮：Bus/Enum/Clock 显式 Segment 悬浮增加透明预选框，并公开值、起止与宽度。
19. 第 139 轮：标尺和空白波形提示公开空格/中键平移、Ctrl+滚轮缩放及 Shift+滚轮横向滚动。
20. 第 140 轮：工具栏模式文案统一为 `Timing: Sync/Async`，并直接显示关联时钟与真实编辑步长。

开发视角验收：

```text
cmake --build build --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 17.03 sec

cmake --build build/qtcreator-debug --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 16.74 sec

cmake --build build/qtcreator-release --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 16.25 sec

Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: 被动悬浮保持编辑光标和模型不变；标尺单击、拖动、释放与 Esc 取消均保持信号目标，
并提供精确采样反馈。显式范围内部右键 Copy/Fit 后目标仍完整保留，可由可见关闭按钮结束；安全
改选不产生意外 Bit 翻转。Bus/Enum 编辑器公开 Beat/Segment、前后导航、Apply 与关闭，无效草稿
不丢失、不改目标、不写模型，有效一拍 Apply 为单个 Undo。

Offscreen visual QA:
build/canvas-signal-management-smoke-bus-sequential-entry.png
build/wave-edit-smoke.png

Visual result: 两行 Bus 编辑器位于目标附近且未越界；拍级琥珀框、显式/隐式 Bus 值、稳定编辑
光标、选中行透明背景、波形与网格同时清晰。Bit 边沿虚线仍只覆盖受影响后段。未发现模态窗口、
裁切、桌面抢焦点或意外视图跳动。
Desktop interaction: none
Packaging: not run
```

## 持续迭代 141–150：Bus/Enum Beat 与 Segment 目标一致性

状态：完成，达到下一阶段可用版本；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 141 轮：审计发现单击显示 Beat 编辑器时仍保留 Segment ID，Delete 与直接输入可能扩大到整段。
2. 第 142 轮：Bus/Enum 单击释放后统一为一拍目标，Target、范围框和编辑器范围一致。
3. 第 143 轮：主体或边界只有在形成真实拖动后才进入 Segment 操作；轻微按压/释放仍为 Beat。
4. 第 144 轮：双击和 F6 保留完整 Segment 入口，F7/F8、移动、复制与边界命令继续只作用于显式段。
5. 第 145 轮：Bus/Enum Beat 的 Delete/Backspace 只清除当前一拍至隐式 `X`，不影响相邻拍。
6. 第 146 轮：Beat 清除保持原拍选中、编辑器与 Target 可见，并以单个 Undo 恢复完整 Scenario。
7. 第 147 轮：一拍 `0/X/Z/Don't care` 预设提交后不再把目标升级为合并后的完整 Segment。
8. 第 148 轮：直接值 Apply 后继续保留精确 Beat；Segment 范围提交仍保持完整段语义。
9. 第 149 轮：Bus/Enum 编辑器增加可见 Clear；Segment Clear 后自动转到被清区间首拍，消除陈旧 Segment。
10. 第 150 轮：960 px 最小窗口专项覆盖 Beat Target、两行编辑器、Clear/Apply/导航/关闭及完整控件边界。

开发视角验收：

```text
cmake --build build --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 16.98 sec

cmake --build build/qtcreator-debug --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 16.83 sec

cmake --build build/qtcreator-release --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 16.52 sec

Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: 单击 data[7:0] 的 110–120 ns 后 Target、编辑器和透明框均为 Beat，Segment 命令禁用；
F6 才切换到完整显式段。Delete 与可见 Clear 仅将 110–120 ns 清为隐式 X，105 ns 和 125 ns
仍为 0x35；单步 Undo 完整恢复。Segment Clear 清除完整段后自动回到首拍，Undo 后可重新进入
Segment。一拍 Apply 与 Don't care 均保持 Beat，不产生陈旧 Segment ID。

Offscreen visual QA:
build/wave-edit-smoke-bus-editor-960.png
build/canvas-signal-management-smoke-bus-sequential-entry.png

Visual result: 960 px 下工具栏明确显示 `Target: beat`，编辑器的 Beat、前后导航、关闭、进制、
直接值、Recent、0/X/Z/Don't care、Clear 与 Apply 均完整可见；目标拍、波形、信号值和网格清晰。
Desktop interaction: none
Packaging: not run
```

## 持续迭代 151–160：合法草稿同次继续与无效草稿原位恢复

状态：阶段完成，长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 151 轮：审计确认快速新增与 Bus/Enum/范围值草稿会吞掉提交后的第一次目标点击。
2. 第 152 轮：快速新增使用默认或已输入参数提交后，同一左键继续选中目标信号；创建仍为单个 Undo。
3. 第 153 轮：Bus/Enum 合法 Beat 草稿提交后，同一左键直接进入另一拍，不再要求第二次点击。
4. 第 154 轮：新建 Bus/Enum 的隐式 `X` 点击保留真实 Beat 选择，Target、编辑器和高亮范围一致。
5. 第 155 轮：Bus/Enum 无效草稿阻止同次目标变化，保留旧 Beat、错误文本、焦点和零部分写入。
6. 第 156 轮：合法 Bus/Enum 草稿提交后，同一双击直接进入目标 Segment。
7. 第 157 轮：合法 Bus/Enum 草稿提交后，同一右键直接打开目标波形菜单。
8. 第 158 轮：合法多 Bus 范围草稿原子提交后，同一左键完成信号改选，Undo 只撤销该次赋值。
9. 第 159 轮：多 Bus 范围任一位宽校验失败时，恢复原范围、lane 集合、草稿与输入焦点。
10. 第 160 轮：Timeline End 单独保留安全点击消费，因为提交会改变坐标映射；完成同次改选视觉验收。

开发视角验收：

```text
cmake --build build --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 17.61 sec

cmake --build build/qtcreator-debug --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 18.38 sec

cmake --build build/qtcreator-release --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 16.62 sec

Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: 新增 Bit 后直接点击既有 Clock，会在一次手势中提交新增并选中 Clock；Bus 草稿
0x3c 在点击下一拍时写入旧拍并立即打开下一 Beat，单步 Undo 只恢复旧拍且保留新目标。无效
0x1ff 不改变目标或模型。合法草稿后双击直接进入目标 Segment，右键直接打开目标菜单；两条不同
位宽 Bus 的范围值 0xa 可原子提交并同次改选，0xa5 被原位拒绝且不产生部分写入。End 修改仍先
安全提交，避免使用旧坐标执行波形动作。

Offscreen visual QA:
build/canvas-signal-management-smoke-same-click-retarget.png

Visual result: 已写入的 50–60 ns `0x3c` 与当前 60–70 ns 隐式 `X` Beat 同时可见；工具栏 Target、
琥珀拍级框、就近编辑器、红色隐式 X 虚线、波形与网格明确区分“已提交结果”和“下一编辑目标”。
Desktop interaction: none
Packaging: not run
```

## 持续迭代 161–170：范围草稿两级退出与选择恢复

状态：阶段完成，长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 161 轮：审计确认范围输入框按 Esc 会同时丢草稿和范围，错误修正后必须重新框选。
2. 第 162 轮：范围值存在未提交修改时，第一次 Esc 只清空草稿并恢复正常输入样式。
3. 第 163 轮：非法多 Bus 范围草稿按 Esc 后保留原时间范围和全部 lane 目标。
4. 第 164 轮：草稿丢弃后输入焦点保留，可立即重新输入而无需点击输入框。
5. 第 165 轮：状态栏明确显示 `selection kept`，并公开再次 Esc 关闭范围。
6. 第 166 轮：范围输入 tooltip 公开“Esc 丢草稿、再次 Esc 关闭”的分层行为。
7. 第 167 轮：未修改状态下第二次 Esc 关闭范围、固定栏和拍级手柄。
8. 第 168 轮：固定栏关闭按钮仍保持一步关闭，不增加额外确认层。
9. 第 169 轮：草稿丢弃与范围关闭均保持 Scenario、Undo、Redo 和 Saved 状态不变。
10. 第 170 轮：960 px 双 Bus 范围专项完成离屏视觉与三套全量回归。

开发视角验收：

```text
cmake --build build --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 17.10 sec

cmake --build build/qtcreator-debug --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 17.01 sec

cmake --build build/qtcreator-release --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 16.85 sec

Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: 两条不同位宽 Bus 的 20–50 ns 范围输入非法 `0xa5` 后，同次点击仍保留原范围、
两条 lane、错误草稿和焦点；第一次 Esc 清空草稿但保留选择、固定栏和零模型变化，第二次 Esc
关闭范围。重新框选后仍可提交合法 `0xa`，原子 Undo/Redo 不受两级退出影响。

Offscreen visual QA:
build/wave-edit-smoke-range-draft-discard.png

Visual result: 960 px 下两条 Bus 的 20–50 ns 蓝色选区、端点手柄、Copy/Cut/Paste/Clear/值/
0/X/Z/Don't care/关闭控件完整可见；状态栏直接显示草稿已丢弃、选择保留和再次 Esc 路径。
Desktop interaction: none
Packaging: not run
```

## 持续迭代 171–180：所选范围紧邻重复

状态：阶段完成，长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 171 轮：审计确认重复协议片段必须执行 Copy、重新定位和 Paste，且容易选错目标时间。
2. 第 172 轮：范围右键新增 `Duplicate selected range after`，一次动作写入紧邻后方。
3. 第 173 轮：Edit 菜单提供同名可发现入口，不新增模式或常驻工具栏按钮。
4. 第 174 轮：多信号按当前选择顺序原子复制，保留隐式空白、Segment 值和扩展语义。
5. 第 175 轮：执行后自动选中新范围并把编辑光标放到结果起点，可立即继续编辑或再次重复。
6. 第 176 轮：重复动作不读写系统剪贴板，既有文本或范围剪贴板内容保持不变。
7. 第 177 轮：历史项使用专用 `Duplicate range` 描述，一次 Undo/Redo 恢复完整 Scenario。
8. 第 178 轮：被复制边沿导致 Relation 清理时公开数量与恢复方式；Undo 同步恢复关系。
9. 第 179 轮：结果已一致时显示 `no values changed`，不新增历史且不清除前一真实 Redo。
10. 第 180 轮：越过 End 继续复用原子延长语义，并完成离屏截图与三套全量回归。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 18.10 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 18.09 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 17.79 sec

Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: 选中 req/ack 的 80–100 ns 后，从范围右键一次执行即可把两条信号重复到
100–120 ns；源范围保持不变，结果范围继续选中，系统剪贴板中的哨兵文本保持不变。该操作清理
1 条引用消失边沿的 Relation 并明确提示，单步 Undo/Redo 同时恢复波形与关系。对已一致的普通
Clock 范围再次执行只移动结果选择，显示 no values changed，不产生历史且保留上一真实 Redo。

Offscreen visual QA:
build/wave-edit-smoke-duplicate-range-after.png
build/edit-menu-smoke.png

Visual result: 两条 Bit 结果范围、端点手柄、固定范围栏、100 ns 编辑光标、波形、刻度和网格
同时清晰；状态栏直接显示目标 100–120 ns、clipboard unchanged、Relation 清理和 Ctrl+Z 路径；
Edit 菜单中的范围重复入口完整可见且未挤占画布工具栏。
Desktop interaction: none
Packaging: not run
```

## 持续迭代 181–190：显式范围整块拖动与复制

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 181 轮：开发审计确认移动已选多信号范围仍需 Cut、重新定位、Paste 三步，且正文单击会先关闭选区。
2. 第 182 轮：选区正文单击改为保持范围，悬浮使用整块移动光标并公开“拖动移动、Ctrl 拖动复制”。
3. 第 183 轮：拖动选区正文可将全部目标信号和完整时间宽度作为一个原子操作水平移动。
4. 第 184 轮：`Ctrl` + 拖动复制所选范围，源区间和系统剪贴板均保持不变。
5. 第 185 轮：源选区保持实线，目标预览使用虚线；画布标签和状态栏实时显示 Move/Copy、起止与宽度。
6. 第 186 轮：范围移动复用 Sync/Async、轻吸附、`Alt` 绕过和水平边缘自动滚动，提交后滚入延长结果。
7. 第 187 轮：复制目标与源区间重叠时显示禁止光标和原因，释放后恢复源选区且不修改模型或历史。
8. 第 188 轮：Esc 与异常失去左键取消拖动，恢复起始选区、编辑光标及自动滚动前视图。
9. 第 189 轮：新增 `TransferRangeCommand`；完整移动的 Segment 尽量保留 stable ID/Event，部分段和覆盖目标按 Relation 规则同步；原位或无效果操作不清除真实 Redo，越过 End 原子延长。
10. 第 190 轮：扩展核心与 `wave-wave-edit-smoke`，覆盖多 lane Move/Copy、预览、剪贴板隔离、重叠拒绝、取消、结果选择、Undo/Redo、身份/关系及 End 边界，并完成三配置 offscreen 视觉验收。

开发视角验收：

```text
cmake --build build --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 17.51 sec

cmake --build build/qtcreator-debug --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 17.22 sec

cmake --build build/qtcreator-release --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 16.68 sec

Targeted: wave-core-tests, wave-wave-edit-smoke, wave-edit-menu-smoke passed
git diff --check: passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: 在 req/ack 的 80–100 ns 双信号选区正文按下并拖到 120–140 ns 时，源选区保持实线，
目标显示虚线 Move 预览，释放后源显式内容清除、结果继续选中且一次 Undo/Redo 精确恢复。
重新选择源范围后 Ctrl+拖动到 140–160 ns，源值保持、目标得到相同波形、系统剪贴板哨兵不变；
重叠复制被原位拒绝，Esc 取消恢复源选区，普通单击只保留选区并显示后续操作提示。

Offscreen visual QA:
build/wave-edit-smoke-range-move-preview.png
build/wave-edit-smoke-range-copy-preview.png
build/wave-edit-smoke-range-copy-result.png

Visual result: 源/目标实虚线、Move/Copy 标签、精确时间与宽度、结果选区、状态反馈、刻度和网格均清晰；
未出现模态窗口、桌面抢焦点、工具栏跳动或选中背景遮挡刻度。
Desktop interaction: none
Packaging: not run
```

## 持续迭代 191–200：撤销目标回显与拖动意图即时切换

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 191 轮：开发审计确认范围或 Segment 操作 Undo 后模型虽已恢复，画布仍可能停在已失效的结果目标。
2. 第 192 轮：命令历史增加方向化选择快照，仅按实际 `from state → to state` 恢复对应源或结果目标。
3. 第 193 轮：范围 Move 的 Undo 恢复源时间、全部信号与编辑光标，Redo 恢复目标范围。
4. 第 194 轮：范围 Copy 同样形成源/结果双向恢复，源保持与剪贴板隔离语义不变。
5. 第 195 轮：`Duplicate selected range after` 的 Undo/Redo 恢复源/紧邻结果选区。
6. 第 196 轮：Segment 移动、复制、键盘微移和边界调整接入相同恢复机制；失效 Segment ID 会安全降级。
7. 第 197 轮：显式目标 Paste 记录粘贴前目标；两拍目标粘贴一拍内容后，Undo 回到原两拍，Redo 回到一拍结果；右键 `Paste here` 继续保留既有结果选择。
8. 第 198 轮：范围与非 Clock Segment 拖动期间按下或释放 `Ctrl` 可即时切换 Copy/Move，指针、虚线预览、状态和鼠标释放时的实际命令一致。
9. 第 199 轮：历史恢复对离屏目标做最小水平/垂直揭示，清理残留拖动、悬浮与吸附状态；无关和无效果历史不触发旧目标跳转。
10. 第 200 轮：扩展 `wave-wave-edit-smoke`、`wave-canvas-add-lane-smoke` 与三配置 offscreen 回归，并完成开发视角和用户视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 17.63 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 17.28 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 16.96 sec

Targeted: wave-canvas-add-lane-smoke, wave-wave-edit-smoke, wave-edit-menu-smoke passed
git diff --check: passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: 将 req/ack 的 80–100 ns 范围移动或复制后，Ctrl+Z 会回到源选区，Ctrl+Y 会回到
结果选区；固定范围栏、编辑光标和信号集合同步恢复，状态栏直接说明恢复位置。两拍显式目标中粘贴
一拍内容后，Undo 恢复原 20–40 ns 目标，Redo 恢复实际 20–30 ns 结果。Segment 相邻复制也在
Undo/Redo 间恢复对应完整 Segment。普通拖动中按下 Ctrl 后释放鼠标实际执行 Copy；Ctrl 拖动中
释放 Ctrl 后释放鼠标实际执行 Move，不需要取消重拖。

Offscreen visual QA:
build/wave-edit-smoke-range-move-undo-source.png
build/wave-edit-smoke-range-live-ctrl-copy-preview.png
build/wave-edit-smoke-multi-target-paste.png

Visual result: Undo 后源选区、Copy 的源实线/目标虚线、Paste 实际结果宽度、精确标签、固定范围栏、
状态反馈、刻度和网格同时清晰；未出现模态窗口、桌面抢焦点、工具栏跳动或选中背景遮挡刻度。
Desktop interaction: none
Packaging: not run
```

长期 Goal 当前保持 active；第 200 轮为已验收的阶段可用节点，后续从第 201 轮继续。

## 持续迭代 201–210：跨信号范围整块转移

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 201 轮：开发审计确认范围正文拖动只接受水平时间变化，纵向移动被静默忽略，用户仍需 Copy、重新选择目标再 Paste。
2. 第 202 轮：`TransferRangeCommand` 将源信号与目标信号独立映射，并统一校验目标存在性、类型、Bus/Enum 位宽及实际值。
3. 第 203 轮：画布从指针所在行推断连续目标信号块，保留按下行在多信号选区中的相对偏移并跳过 Group。
4. 第 204 轮：预览保留源选区实线，以虚线显示目标块，标签和状态栏实时公开单信号或多信号映射、时间范围及宽度。
5. 第 205 轮：`Ctrl` 纵向拖动可在相同时间复制到不相交信号；与源信号集合相交的同时间 Copy 继续拒绝，避免自覆盖。
6. 第 206 轮：跨信号 Move 先读取全部源，再清理全部源/目标并统一写入，避免 `A → B、B → C` 链式污染；跨信号 Segment 采用新身份并同步 Event/Relation。
7. 第 207 轮：目标不足、类型不同、位宽不同或值不兼容均在释放前显示红色虚线、禁止指针和具体原因，不产生部分提交。
8. 第 208 轮：范围纵向拖到上下边缘时持续自动滚动；Esc、失去左键、无效果或非法释放恢复原选择及横纵视图。
9. 第 209 轮：成功 Move/Copy 后选择目标块；Undo 恢复源块，Redo 恢复结果块，并对离屏目标做最小揭示。
10. 第 210 轮：补齐核心命令、单/多信号画布路径、长列表自动滚动、三配置 offscreen 回归和开发/用户双视角验收。

开发视角验收：

```text
cmake --build build --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 18.08 sec

cmake --build build/qtcreator-debug --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 17.95 sec

cmake --build build/qtcreator-release --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 17.56 sec

Targeted: wave-core-tests, wave-wave-edit-smoke, wave-lane-autoscroll-smoke passed
git diff --check: passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: 选中 req 的 80–100 ns 后，可保持时间不变直接 Ctrl 拖到 ack；释放前源范围为实线、
目标范围为虚线并显示 req → ack，释放后 req 保持、ack 写入且目标继续选中。普通拖动执行 Move，
Undo 回到 req，Redo 回到 ack。选择 reset_n/req 两行并从第二行拖到 ack，可得到
reset_n…req → req…ack，证明按下行相对位置与重叠链式移动均正确。拖到 data[7:0] 时在释放前
显示红色 Cannot drop、类型不匹配原因和禁止指针，释放不改变模型、历史或选择。长列表拖到下边缘
可持续滚动并揭示 signal_16，Esc 恢复 signal_00 源范围与原纵向位置。

Offscreen visual QA:
build/wave-edit-smoke-range-cross-lane-copy-preview.png
build/wave-edit-smoke-range-cross-lane-invalid-target.png
build/lane-autoscroll-smoke-range-transfer.png

Visual result: 源实线、目标蓝色虚线、非法目标红色虚线、映射标签、精确时间、自动滚动状态、刻度和
网格同时清晰；未出现模态窗口、桌面抢焦点、工具栏跳动或选中背景遮挡刻度。
Desktop interaction: none
Packaging: not run
```

长期 Goal 当前保持 active；第 210 轮为已验收的阶段可用节点，后续从第 211 轮继续。

## 持续迭代 211–220：范围转移的内容感知预览

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 211 轮：用户视角审计确认跨信号目标仅显示填充矩形，无法在释放前判断实际 0/1、数值、符号或 Clock 覆盖。
2. 第 212 轮：建立源范围到目标时间/信号的只读预览 Lane，不修改 Scenario、命令栈或保存状态。
3. 第 213 轮：Bit 目标按真实隐式 0、显式 1、X/Z 区间绘制高低电平和虚线边沿。
4. 第 214 轮：Bus 目标保留数值、多态 X/Z、Don’t care 斜纹及扩展语义，使用目标信号颜色绘制。
5. 第 215 轮：Enum 目标显示实际符号轮廓；目标声明缺少源符号时释放前按具体值拒绝。
6. 第 216 轮：Clock 只预览真正会转移的 gated/disabled 覆盖，不把目标自身基础周期误画成复制内容。
7. 第 217 轮：Move/Copy 精确标签优先放在范围右侧或左侧，避免遮挡幽灵波形；空间不足时才回退到范围内。
8. 第 218 轮：类型/位宽/值不兼容及 Copy 重叠统一使用红色虚线、`Cannot drop` 标签和禁止指针，并抑制误导性内容预览。
9. 第 219 轮：预览仅为当前可见目标行查找并截取源 Segment，使用 Lane/Layout 索引，避免复制整条目标 Lane 或扫描离屏目标内容。
10. 第 220 轮：补齐 Bit/Bus/Enum/Clock、值级拒绝、重叠拒绝、取消隔离的 offscreen 自动化与三配置双视角验收。

开发视角验收：

```text
cmake --build build --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 18.75 sec

cmake --build build/qtcreator-debug --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 18.43 sec

cmake --build build/qtcreator-release --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 18.15 sec

Targeted: wave-canvas-add-lane-smoke, wave-wave-edit-smoke,
wave-wave-edit-autoscroll-smoke passed
git diff --check: passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: req 的高电平复制到 ack 时，目标行直接显示虚线高电平；16-bit Bus 的 Don’t care 从
80–90 ns 拖到 bus_copy 的 20–30 ns 时，目标显示斜纹和文字而非空矩形；state 的 DONE 从
100–150 ns 拖到 state_next 的 50–100 ns 时显示实际 DONE 轮廓，而 WAIT_ACK 因目标未声明该
符号而在释放前拒绝；clk_2 的 24–36 ns GATED 覆盖复制到 clk_2_copy 时只显示低电平覆盖，不重复
基础时钟。90–110 ns 与源重叠的 Ctrl Copy 显示红色 Cannot drop 且没有伪目标波形。所有预览按
Esc 后均恢复源选区，Scenario、Undo 和 Saved 不变。

Offscreen visual QA:
build/wave-edit-smoke-range-cross-lane-copy-preview.png
build/canvas-signal-management-smoke-range-bus-content-preview.png
build/canvas-signal-management-smoke-range-clock-content-preview.png
build/wave-edit-autoscroll-smoke-range-enum-content-preview.png
build/wave-edit-smoke-range-overlap-copy-rejected.png

Visual result: Bit 高低电平、Bus 语义斜纹、Enum 符号、Clock 覆盖、源/目标边框、范围外标签与红色
拒绝状态均可直接辨识；刻度和网格无遮挡，未出现模态窗口、桌面抢焦点或工具栏跳动。
Desktop interaction: none
Packaging: not run
```

长期 Goal 当前保持 active；第 220 轮为已验收的阶段可用节点，后续从第 221 轮继续。

## 持续迭代 221–230：Clock 范围批量编辑

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 221 轮：用户视角审计确认有效 Clock 范围被标成 `Mixed/unsupported`，用户只能逐周期编辑 G/X/R。
2. 第 222 轮：将同类型 Clock 纳入显式范围类型识别，混合类型仍保持原子拒绝，不扩大到其他不可赋值 Lane。
3. 第 223 轮：固定栏直接显示 `N Clock · start–end`，悬浮说明公开完整目标、Run/Gate/Disable 与 R/G/X。
4. 第 224 轮：复用原 Clear 位置按上下文显示 `Run`，新增 `Gate` 和 `Disable`；Clock 范围不显示无意义的 0/1/Z、Don’t care 或值输入框。
5. 第 225 轮：`R` / `G` / `X` 在纯 Clock 范围内分别执行 Run、Gate、Disable，作用范围与固定栏完全一致。
6. 第 226 轮：`Shift+Up` / `Shift+Down` 扩展到多条连续 Clock 后，一次写入全部目标，不要求逐行重复操作。
7. 第 227 轮：多 Clock Gate/Disable/Run 均使用单个命令；重复 Gate 为无效果操作，不新增历史或清除既有 Redo，Undo/Redo 精确恢复全部目标。
8. 第 228 轮：Run 只清除真正存在的覆盖，并沿用 Event/Relation 安全同步；状态栏区分已正常运行、实际恢复及依赖清理结果。
9. 第 229 轮：Clock/Enum 等混合范围上的 G/X/R 保留原选区并明确拒绝；补齐文本焦点、范围保持、控件显隐和离屏视觉回归。
10. 第 230 轮：补齐核心命令、固定栏按钮、键盘、多 Clock、无效果、混合保护的自动化及三配置开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 18.01 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 18.15 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 17.79 sec

Targeted: wave-core-tests, wave-canvas-add-lane-smoke,
wave-wave-edit-smoke, wave-wave-edit-autoscroll-smoke passed
git diff --check: passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: 选中 clk_2 的 24–36 ns 后，固定栏直接显示 `1 Clock`、精确范围及
Run/Gate/Disable，不再显示 Mixed/unsupported 或无关数值控件。对已经 GATED 的范围再次按 Gate
不会新增 Undo；Disable 将整个范围改为 X，单步 Undo 精确恢复 GATED。Shift+Down 将目标扩展为
clk_2/clk_2_copy 两条 Clock 后，X、R、G 和 Run 均一次作用于两条信号且可单步恢复。把 Enum 与
Clock 混选后按 G 保持模型、历史、Saved 和原选区不变，并明确提示只选择 Clock。

Offscreen visual QA:
build/canvas-signal-management-smoke-range-clock-content-preview.png

Visual result: 固定栏中的 `1 Clock · 24 ns–36 ns`、Run/Gate/Disable、选区手柄、GATED 源覆盖、
目标虚线内容预览、刻度和网格可同时辨识；未出现模态窗口、桌面抢焦点、工具栏裁切或视图跳动。
Desktop interaction: none
Packaging: not run
```

长期 Goal 当前保持 active；第 230 轮为已验收的阶段可用节点，后续从第 231 轮继续。

## 持续迭代 231–240：键盘时间步长与 Sync/Async 一致

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 231 轮：用户视角审计确认工具栏对 clk_2 显示 `12 ns/step`，但 Left/Right 与 Shift+Left/Right 仍固定移动 10 ns，Async 也未按 1 tick 移动。
2. 第 232 轮：建立严格前一/后一键盘时间边界计算；离网格位置按方向进入相邻边界，不使用可能停在当前点的“最近值”取整。
3. 第 233 轮：Wave Edit 的 Left/Right 改为使用当前活动信号的真实 Timing；Sync 按关联 Clock 一拍，未关联信号保留 10 ns 回退。
4. 第 234 轮：Shift+Left/Right 建立、扩展、收缩和折叠范围时复用同一边界计算，固定范围栏与实际端点一致。
5. 第 235 轮：Async 下普通方向键和 Shift 方向键均严格移动 1 tick，不再沿用 10 ns 导航步长。
6. 第 236 轮：Sync 边界保留 Clock period、phase 与 active edge；2 ns phase 的 Rising 及 Falling 边界均有隔离 offscreen 回归。
7. 第 237 轮：0 和 Scenario End 始终作为可达边界；End 不是整拍时先到 End，再次同向按键原位说明，反向返回最后有效 Clock 边界。
8. 第 238 轮：多信号范围保留全部目标并由当前活动信号决定公开步长；12 ns 双 Clock 范围可按一拍扩展与收回。
9. 第 239 轮：Timing 按钮、信号目标和范围悬浮说明直接公开 Left/Right/Shift+Left/Right 的真实步长；文本编辑焦点与 Measure 既有语义保持隔离。
10. 第 240 轮：补齐 12 ns、离网格、phase、Falling、End、多 Clock、Async、无模型修改的自动化及三配置双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 18.13 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 18.39 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 18.33 sec

Targeted: wave-canvas-add-lane-smoke, wave-cursor-mode-smoke,
wave-wave-edit-smoke, wave-wave-edit-autoscroll-smoke passed
git diff --check: passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: 选中周期 12 ns 的 clk_2 并位于 24 ns 时，Right 到 36 ns，Left 返回 24 ns；
Shift+Right 建立 24–36 ns 单拍范围，固定栏和状态栏均公开 `12 ns/step`。从 25 ns 离网格位置
向右进入 36 ns，向左返回 24 ns，不会取整回当前点。2 ns phase 的 Rising Clock 从 15 ns
向右/左到 26/14 ns；切到 Falling active edge 后到 20/8 ns。219 ns 向右先到非整拍 End 220 ns，
再次按键保持并说明边界，向左回到 216 ns。双 Clock 范围按 12 ns 扩展/收回且目标不变；Async
从 25 ns 以 1 tick 建立 25000–25001 tick 范围。全过程 Project、Undo 和保存状态不变。

Offscreen visual QA:
build/canvas-signal-management-smoke-range-clock-content-preview.png

Visual result: `Timing: Sync · clk_2 · 12 ns/step`、`1 Clock · 24 ns–36 ns`、范围手柄、
Run/Gate/Disable、波形、刻度和网格同时清晰；未出现模态窗口、桌面抢焦点、裁切或视图跳动。
Desktop interaction: none
Packaging: not run
```

长期 Goal 当前保持 active；第 240 轮为已验收的阶段可用节点，后续从第 241 轮继续。

## 持续迭代 241–250：显式范围端点精确输入

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 241 轮：用户视角审计确认显式范围只能像素拖动或逐拍按键修正，已知精确时间时仍需反复调整。
2. 第 242 轮：复用现有 `Ctrl+G` 紧凑栏；有显式范围时切换为 `Range edge`，不新增常驻按钮或模态窗口。
3. 第 243 轮：固定范围栏的范围摘要改为可点击入口，并通过指针、下划线和悬浮说明公开精确编辑路径。
4. 第 244 轮：统一推导固定锚点和活动端点；鼠标建立、键盘调整和跨锚点后的端点身份保持确定。
5. 第 245 轮：支持 ps/ns/us/ms/tick 与 `cycle N` 输入；Enter/`Set edge` 只更新选择上下文，不修改波形或历史。
6. 第 246 轮：新增 `Other edge` 就地切换；保留范围和全部信号目标，并允许活动端点自然跨过固定锚点。
7. 第 247 轮：非法、越界及锚点相等输入在原处标红并保留焦点；空范围被拒绝，状态栏使用范围专用反馈。
8. 第 248 轮：多 Enum 目标精确扩缩后仍保留完整 lane 集合；未提交或非法的范围值草稿优先阻止入口切换。
9. 第 249 轮：精确编辑期间临时隐藏原范围控件，关闭后恢复；用户转到其他画布目标时自动关闭过期编辑器，并消除 960 px 工具栏裁切回归。
10. 第 250 轮：补齐单/多信号、周期、跨锚点、无效输入、草稿门禁、入口显隐、零模型修改及三配置双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 18.34 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 18.17 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 17.96 sec

Targeted: wave-wave-edit-smoke, wave-wave-edit-autoscroll-smoke,
wave-user-journey-smoke, wave-edit-menu-smoke, wave-waveform-only-smoke passed
git diff --check: passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: 建立 data[7:0] 的 0–10 ns 范围后，Ctrl+G 直接显示活动端点 10 ns 和固定锚点
0 ps。输入 37 ns、cycle 5 可立即得到 0–37 ns、0–50 ns；`Other edge` 切到 0 ps 后输入
70 ns，范围跨锚点成为 50–70 ns。非法时间与锚点相等值均留在原处纠错。关闭后范围、信号目标和
固定栏保持；点击范围摘要可重开，转到其他信号后编辑器自动关闭。两条 Enum 的 50–100 ns 范围
可精确扩缩并恢复，非法值草稿阻止切换。全过程 Scenario、Undo 和 Saved 不变。

Offscreen visual QA:
build/wave-edit-autoscroll-smoke-range-edge.png

Visual result: `Range edge`、70 ns 输入、`Anchor 50 ns · End 1 us`、`Other edge`、
`Set edge`、50–70 ns 透明选区、Bus 值、Clock、刻度和网格同时清晰；未出现模态窗口、
桌面抢焦点、工具栏裁切或视图跳动。
Desktop interaction: none
Packaging: not run
```

长期 Goal 当前保持 active；第 250 轮为已验收的阶段可用节点，后续从第 251 轮继续。

## 持续迭代 251–260：小数物理时间精确输入

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 251 轮：用户视角审计确认所有时间输入只接受整数，常见 2.5 ns、6.4 ns 必须人工换算为 ps。
2. 第 252 轮：`wavetime` 增加字符串小数到 Tick 的精确换算，按十进制有理数计算，不使用浮点。
3. 第 253 轮：小数位只在能形成整数 ps 且能被项目 timebase 整除时接受；亚 ps、不可整除和溢出均原子拒绝。
4. 第 254 轮：统一时间输入解析器接受小数 ps/ns/us/ms；tick 与 `cycle N` 保持整数语义并提供独立错误提示。
5. 第 255 轮：`formatTick` 使用最多三位精确小数显示更大的合适单位；2.5 ns 不再提交后回显为 2500 ps。
6. 第 256 轮：Go to 与显式范围端点分别覆盖 2.5 ns 和 37.5 ns，保持信号、选区、模型、历史与 Saved。
7. 第 257 轮：Timeline End 接受 `.5 ns` 增量并保持单命令 Undo；Clock 周期接受 `0.012 us` 并回显 12 ns。
8. 第 258 轮：共享解析路径使导出起止范围、PDF 页跨度、Relation delay、trace offset 与比较 tolerance 同步获得小数物理时间能力。
9. 第 259 轮：Go to、Clock、快速新增 Clock 与 End 的占位和悬浮说明公开小数能力；核心覆盖正负值、前导小数点、尾随零、边界与溢出。
10. 第 260 轮：补齐 fractional tick、亚 timebase、无舍入、各 GUI 入口及三配置开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 18.36 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 18.12 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 17.84 sec

Targeted: wave-core-tests, wave-canvas-add-lane-smoke,
wave-wave-edit-autoscroll-smoke, wave-user-journey-smoke passed
git diff --check: passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: 在 1 ps/tick 工程中输入 2.5 ns，编辑光标准确到 2500 tick 且输入框、光标标签和状态
均回显 `2.5 ns`；输入 37.5 ns 后范围端点准确到 37500 tick。Timeline End 以 `.5 ns` 增量提交
并可单步 Undo；Clock 周期输入 `0.012 us` 后准确为 12 ns。导出范围 120.5 ns 与 PDF 跨度
25.5 ns 可继续进入目录选择。`1.5 tick`、`0.0005 ns`、不可整除和溢出值均原位拒绝，不发生舍入。

Offscreen visual QA:
build/wave-edit-autoscroll-smoke-go-to-decimal.png

Visual result: `Go to`、`2.5 ns` 输入、2.5 ns 光标标签、所选 data[7:0] 值、Bus/Enum/Clock 波形、
刻度和网格同时清晰；未出现模态窗口、桌面抢焦点、工具栏裁切或视图跳动。
Desktop interaction: none
Packaging: not run
```

长期 Goal 当前保持 active；第 260 轮为已验收的阶段可用节点，后续从第 261 轮继续。

## 持续迭代 261–270：显式范围精确宽度

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 261 轮：用户视角审计确认精确范围只接受绝对端点，已知宽度时仍需读取锚点并人工加减。
2. 第 262 轮：现有紧凑精确栏的模式标签改为可点击的 `Range edge ▾`，不增加常驻工具栏或模态窗口。
3. 第 263 轮：同一入口增加 `Range width ▾`，预填当前宽度并明确显示固定锚点和延伸方向。
4. 第 264 轮：宽度接受精确小数 ps/ns/us/ms 与整数 tick，继续复用无浮点舍入的时间换算。
5. 第 265 轮：宽度接受 `cycle N` 与 `N cycles`，按当前信号时钟域或唯一项目时钟换算周期数。
6. 第 266 轮：`Other edge` 在宽度模式下保持范围并反转固定边与延伸方向，可从左或右锚点输入宽度。
7. 第 267 轮：零宽、负宽、超出 0/End、无时钟、fractional tick 与溢出均在原输入框纠错并保留选择。
8. 第 268 轮：成功提交保留单/多信号目标、视图及精确栏；模式切换和提交均不修改 Scenario、Undo 或 Saved。
9. 第 269 轮：模式标签、输入辅助、锚点方向、提交和反向按钮补齐可访问名称与悬浮说明；960 像素保持全部控件可见可点击。
10. 第 270 轮：补齐小数、tick、cycle、方向、边界、模式往返、响应式截图及三配置开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
26/26 tests passed
Total Test time: 18.13 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
26/26 tests passed
Total Test time: 18.21 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
26/26 tests passed
Total Test time: 17.88 sec

Targeted: wave-wave-edit-autoscroll-smoke passed
git diff --check: passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Automated QA: 50–70 ns 范围从 Range edge 原位切换为 Range width，输入 25.5 ns 后得到
50–75.5 ns；输入 cycle 3 后按 10 ns 时钟得到 50–80 ns。Other edge 保持范围并把锚点切到
80 ns，输入 12500 ticks 后得到 67.5–80 ns。零宽与向起点超出 80 ns 的宽度均原位拒绝，
方向化提示公开最大允许值；再切回端点模式可继续精确编辑。全过程保持信号目标、模型、历史与 Saved。

Offscreen visual QA:
build/wave-edit-autoscroll-smoke-range-width.png
build/wave-edit-autoscroll-smoke-range-width-960.png

Visual result: `Range width ▾`、30 ns 输入、Anchor 50 ns、to End、Other edge、Set width、
50–80 ns 透明选区、Bus/Enum/Clock 波形、刻度和网格同时清晰；960 像素下输入与全部操作控件
仍可见可点击，未出现模态窗口、桌面抢焦点、工具栏裁切或视图跳动。
Desktop interaction: none
Packaging: not run
```

长期 Goal 当前保持 active；第 270 轮为已验收的阶段可用节点。用户已明确要求从第 271 轮开始
将统一 headless CLI 纳入长期迭代，优先复用现有 `wave-generate`、`wave-compare` 与
`wave-bridge`，补齐机器可读 inspect、validate 和原子波形编辑能力。

## 持续迭代 271–280：可嵌入自动化层与统一 CLI

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 271 轮：审计 `wave-generate`、`wave-compare`、`wave-bridge`，确认其分别处理派生、
   对比和跨应用文件，但缺少统一工程读取、校验和原子编辑入口。
2. 第 272 轮：新增无 QtWidgets 依赖的 `waveautomation` 静态库，向嵌入式 Qt/C++ 宿主公开
   inspect、validate 与 apply API，源 `Project` 始终只读。
3. 第 273 轮：建立 `wave-workbench.cli/v1` 机器可读 inspect，完整返回稳定 ID、timebase、
   ClockDomain、Scenario、Lane/Segment、Event/Relation、Marker、trace、链接资源和 JSON 扩展值。
4. 第 274 轮：建立机器可读 validate，按 Scenario 输出可定位 issue 与
   information/warning/error 汇总；CLI 支持 warning 严格失败。
5. 第 275 轮：建立 `wave-workbench.operations/v1` 原子批次，首批支持 add-signal、
   set-range、clear-range、rename-lane、set-duration，并复用桌面命令与依赖清理规则。
6. 第 276 轮：新增 `wave-cli`，统一 stdout 成功 JSON、stderr 错误 JSON、稳定退出码、
   `--scenario`、`--pretty` 和 operations 文件/stdin 输入。
7. 第 277 轮：补齐 `--dry-run`、显式 `--output`、`--in-place`、QSaveFile 原子替换、
   `--expect-sha256` 和保存前二次源文件冲突检查；无效果原地编辑不重写文件。
8. 第 278 轮：自动生成的 Lane/Clock/Segment/Event 身份与颜色改为稳定伪随机结果，
   新事件重新按 tick/ID 排序；相同源和批次的 dry-run SHA 与实际输出 SHA 保持一致。
9. 第 279 轮：修复 Windows PowerShell 原生管道的 stdin 句柄兼容；补齐核心、独立进程、
   stdin、独立输出、原地替换、冲突、失败无部分结果和 dry-run/实际一致性测试，并公开
   CLI/API 文档及 Qt Creator 目标。
10. 第 280 轮：执行 Codex 用户旅程与默认、Qt Creator Debug、Qt Creator Release 三配置
    全量静默验收；所有 GUI 测试继续使用 offscreen。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
32/32 tests passed
Total Test time: 19.10 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
32/32 tests passed
Total Test time: 19.24 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
32/32 tests passed
Total Test time: 18.55 sec

Targeted: wave-core-tests and all six wave-cli process tests passed
git diff --check: passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 先以 inspect 取得 64 位 sourceSha256 和完整稳定 ID，再将 operations 通过 PowerShell
stdin 送入 apply --dry-run；结果 ok=true、written=false。随后以同一 sourceSha256 写入独立
工程，dry-run 与实际 resultSha256 完全一致。重新 inspect 可见 End=230000 tick、req 已重命名
为 req_cli、新增 cli_data=0xa5；validate 报告 0 error。整个过程不启动窗口、不读取桌面设置、
不修改源示例。过期 SHA、未知 operation、无效目标和批次中途失败均返回结构化错误且不留下部分结果；
原地写入只在 SHA 复核通过后执行原子替换。
```

长期 Goal 当前保持 active；第 280 轮为已验收的阶段可用节点，后续 CLI 迭代可扩展更多领域
operation 与查询过滤，但不改变现有 v1 契约。

## 持续迭代 281–290：CLI 日常定位、采样与信号管理

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 281 轮：从 Codex/脚本的真实路径审计统一 CLI，确认完整 inspect 载荷、必须手算 tick、
   不能按时刻读取值，以及缺少删除、重排和参数修改是当前高频成本。
2. 第 282 轮：新增 `inspect --summary`，保留稳定 ID、类型、宽度、时钟、Scenario 与计数，
   省略 Segment/Event/Relation/Marker 和扩展载荷。
3. 第 283 轮：新增 `sample` 命令与嵌入 API，可按 Scenario 和重复 Lane ID 返回指定时刻的
   Clock/Bit/Bus/Enum 值、显式/隐式状态及 Segment 边界。
4. 第 284 轮：建立共享精确时间解析，支持整数 tick、ps/ns/us/ms 小数物理时间和 `cycle N`；
   不使用浮点数，歧义时钟、亚 timebase 和溢出均明确拒绝。
5. 第 285 轮：`set-range`、`clear-range` 与 `set-duration` 接受物理时间和周期字段，并从目标
   Lane 唯一时钟推断上下文；原 `*Tick` 字段保持兼容且二者冲突时拒绝。
6. 第 286 轮：新增 `delete-signal`，按稳定 Lane ID 删除并复用既有 Event、Relation 和
   imported trace mapping 依赖清理。
7. 第 287 轮：新增 `move-signal`，支持最终索引、before/after 稳定 ID；同位置移动为无效果，
   不污染结果。
8. 第 288 轮：新增 `update-signal`，覆盖名称、颜色、高度、可见性、分组、时钟关联，以及
   Bus/Enum 的宽度、符号和进制；类型不兼容或引用无效时原子拒绝。
9. 第 289 轮：新增 `update-clock`，并使新增 Clock 同样接受精确物理周期/相位；支持名称、
   占空比、活动边沿与 reset relation，复用周期事件重定时和波形区间安全检查；补齐高级
   多操作进程契约。
10. 第 290 轮：补齐时间字段冲突、多时钟周期歧义、非法 Bit 宽度、同位置重排、失败无部分
    结果及 dry-run/实际 SHA 一致性回归；更新 CLI/API 文档并完成三配置双视角静默验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
35/35 tests passed
Total Test time: 19.59 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
35/35 tests passed
Total Test time: 20.22 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
35/35 tests passed
Total Test time: 18.90 sec

Targeted: wave-core-tests and all nine wave-cli process tests passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 先用 inspect --summary 定位 8 条 Lane，无需读取完整波形；sample 80 ns 返回
request=1、data=0x35。随后预演一个包含周期写值、信号参数修改、重排、时钟修改、删除和
End 延长的 6-operation 批次，written=false；写入独立结果后 resultSha256 与预演一致。
结果工程为 7 条 Lane、End=230000 tick，data 已移到 request 前，55 ns 采样 request=1，
validate 为 0 error。物理时间、周期和稳定 ID 均无需调用方手算内部 tick；原示例未被替换。
时间字段冲突、多时钟歧义、非法类型属性和批次中途失败均返回结构化错误且无部分结果。
```

长期 Goal 当前保持 active；第 290 轮为已验收的阶段可用节点。

## 持续迭代 291–300：CLI 局部波形、范围转移与恢复保护

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 291 轮：审计 Codex 的局部修改路径，确认只改一段波形仍需读取完整 Segment/Event/
   Relation，多个 Lane 需要重复 operation，复制/移动需由调用方重建内容，原地写错后缺少文件级恢复。
2. 第 292 轮：新增 `window` CLI 与 `inspectProjectWindowForAutomation`，按时间窗口和 Lane
   过滤，只返回相交 Segment、裁切边界及窗口首尾值。
3. 第 293 轮：窗口结果加入引用的 ClockDomain、局部 Event、相关 Relation 的另一端上下文
   Event 和相交 Marker；支持物理时间、tick、周期和 Lane 时钟推断。
4. 第 294 轮：扩展 `set-range.assignments`，一个 operation 可为多个 Lane 分别赋值，所有
   类型和值先验证，再由同一个领域命令原子提交。
5. 第 295 轮：新增 `transfer-range`，支持 Copy/Move、单 Lane、多 Lane 原位映射及
   `{sourceLaneId,targetLaneId}` 跨兼容 Lane 映射，保留扩展元数据并复用 Event/Relation 同步。
6. 第 296 轮：范围转移默认拒绝覆盖目标显式 Segment；只有 `"overwrite": true` 才允许替换，
   Move 的源区间清理、目标写入和必要 End 延长保持一个原子结果。
7. 第 297 轮：新增 `assert-value` 内容前置条件，可在批次任意位置按物理时间/周期断言
   Clock/Bit/Bus/Enum 当前值；失败 operation 之前的修改也不泄漏。
8. 第 298 轮：为原地写入新增 `--backup`/`--backup=PATH`，保存原始字节并按源 SHA 命名为
   可直接打开的 `.wave.json`；不同内容的既有备份拒绝覆盖。`--output=源文件` 改为显式拒绝，
   替换必须使用 `--in-place`。
9. 第 299 轮：补齐窗口裁切、周期窗口、Relation 端点闭包、多 Lane 写入、跨 Lane Copy/Move、
   源清理、默认覆盖拒绝、显式覆盖、断言回滚、备份复用/冲突和源文件不变的核心及进程契约。
10. 第 300 轮：执行默认、Qt Creator Debug、Qt Creator Release 三配置全量静默回归，
    运行 Codex 局部定位→预演→输出→采样→校验→覆盖拒绝→备份恢复用户旅程并同步文档。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
37/37 tests passed
Total Test time: 20.00 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
37/37 tests passed
Total Test time: 20.97 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
37/37 tests passed
Total Test time: 19.42 sec

Targeted: wave-core-tests and all eleven wave-cli process tests passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 用 window 读取 70–130 ns 的 request/data，仅返回 2 条 Lane、2 个局部 Event、1 个
Relation 端点上下文 Event 和 1 条 Relation，无需读取完整工程。6-operation 批次先断言
request@70ns=0，再同时写 request/ack、建立两个目标信号、复制 data 范围并跨 Lane 移动
request 范围；dry-run written=false，独立输出 SHA 与预演一致。55 ns 得到 request=1/ack=1，
10 ns 得到 data_mirror=0x35/request_moved=1，原 request@90ns=0，validate 为 0 error。
向已有 data 内容复制且未确认 overwrite 时退出码 4、失败 operation=0。临时工程原地修改前
生成 cli-user-journey-inplace.backup-d13f64028f78.wave.json，备份 SHA 与原始源一致；磁盘
示例 SHA 全程不变。
```

长期 Goal 当前保持 active；第 300 轮为已验收的阶段可用节点。

## 持续迭代 301–310：CLI 从零建工程与多信号时序

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 301 轮：审计独立自动化路径，确认 CLI 仍要求先有工程文件，常见多拍、多信号刺激需要
   重复大量 `set-range`，直接阻断 Codex 从零完成任务。
2. 第 302 轮：新增 `createProjectForAutomation`，以名称、timebase、End 和可选显式 ID 创建
   单 Scenario 空白工程，自动身份与序列化结果可重复。
3. 第 303 轮：新增 `wave-cli new`，提供工程名、Scenario 名、End、timebase 和显式 ID 参数；
   缺少扩展名时补全 `.wave.json`，已有目标始终拒绝覆盖。
4. 第 304 轮：`new --operations=FILE|-` 在首次写盘前应用完整原子批次；文件或 stdin 读取、
   解析、operation、校验或写入失败均不留下半成品，dry-run 不创建文件。
5. 第 305 轮：新增 `set-sequence`，支持单 Lane `values` 或多 Lane `sequences`，共享起点和
   步长，一次 operation 生成常见刺激序列。
6. 第 306 轮：序列步长支持整数 tick、精确物理时间或整数时钟周期，支持 `repeat`、唯一时钟
   推断和 End 自动延长；无时钟周期、歧义、溢出与非法值在修改前拒绝。
7. 第 307 轮：允许在序列写入后使用 `assert-value` 作为同批次后置条件；apply/new 结果增加
   Clock、Lane、Scenario 时长及 Segment/Event/Relation/Marker 计数的结构化变更摘要。
8. 第 308 轮：补齐新工程身份/SHA 确定性、dry-run/实际一致、已有输出不变、批次失败零结果、
   无时钟 `stepCycles` 和多 Lane 所有值先验证等安全契约。
9. 第 309 轮：新增空白 new、文件/stdin operations、多信号重复序列、采样、局部窗口、校验、
   重复输出拒绝及非法序列无半成品的核心和进程测试，并同步 CLI/API/Qt Creator 文档。
10. 第 310 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置全量静默回归，并
    执行 Codex 从零预演→创建→采样→窗口→校验→重复创建拒绝→非法批次拒绝用户旅程。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
39/39 tests passed
Total Test time: 20.32 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
39/39 tests passed
Total Test time: 21.13 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
39/39 tests passed
Total Test time: 19.37 sec

Targeted: wave-core-tests and all thirteen wave-cli process tests passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 对不存在的路径先执行 new dry-run，再从同一 6-operation 文件创建工程；预演不写盘，
实际输出 resultSha256 与预演一致。一个批次建立 clk/request/data 并以共享 1-cycle 步长写入
两条重复序列，End 从 60 ns 自动延长至 80 ns；cycle 1 采样 request=1，cycle 2 采样
data=0x34，cycle 0–4 的局部窗口只返回 2 条 Lane，validate 为 0 error。结果明确报告工程/
Scenario 已创建、6 项 operation 和 3 条新增 Lane。再次向同一路径 new 返回 output-exists、
原 SHA 不变；非法 Bit 值在 operation 1 返回 operation-rejected，且不生成文件。
```

长期 Goal 当前保持 active；第 310 轮为已验收的阶段可用节点。

## 持续迭代 311–320：CLI 名称寻址与紧凑信号定位

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 311 轮：从 Codex/脚本真实路径审计 CLI，确认调用方即使已知界面中的信号名，仍需先读
   summary、提取稳定 ID，再拼接查询或 operation；大工程中该步骤增加载荷和错误映射。
2. 第 312 轮：新增统一选择器解析，已有 Scenario、Lane 与 Clock 均接受稳定 ID 或唯一的
   大小写不敏感完整名称；稳定 ID 精确匹配优先，缺失与重名不猜测。
3. 第 313 轮：使 inspect、validate、sample、window 的 Scenario/Lane/Clock 参数共用选择器，
   返回结果统一规范为稳定 ID，名称输入不改变机器可串联输出。
4. 第 314 轮：使批次顶层 Scenario 及单 Lane、Lane 数组、assignment、sequence、范围映射和
   相邻重排字段接受相同选择器，并在每项 operation 前解析。
5. 第 315 轮：将 ClockDomain、Clock 与 Group 引用纳入名称解析；覆盖 add/update signal、
   set-duration 与 update-clock，同批次新建的信号和时钟可由后续 operation 直接按名称引用。
6. 第 316 轮：新增 `signals` 命令及 `findSignalsForAutomation`，按名称/ID 子串、Lane kind、
   精确匹配和 1–1000 条 limit 返回紧凑元数据，不读取 Segment 正文。
7. 第 317 轮：查询结果加入 match/returned/truncated、Lane 总数、规范 Scenario ID、时钟与
   Group 名称；`--exact` 未提供 `--match`、空查询、无效 kind/limit 均返回结构化 usage 错误。
8. 第 318 轮：补齐所有十二类 operation 的名称规范化、大小写变体、同批次生成身份、查询裁切、
   重名歧义、缺失选择器和失败无部分结果的核心回归。
9. 第 319 轮：新增进程级名称用户旅程，覆盖紧凑定位、现有工程名称预演/写入/采样、新工程无
   显式 ID 创建与同批次引用、确定性 SHA、规范 ID 报告和结构化失败；同步 CLI/API/Qt Creator 文档。
10. 第 320 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置全量静默回归，并执行
    Codex 紧凑定位→名称预演→名称写入→名称采样→局部窗口→校验用户旅程。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
41/41 tests passed
Total Test time: 20.77 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
41/41 tests passed
Total Test time: 23.08 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
41/41 tests passed
Total Test time: 20.18 sec

Targeted: wave-core-tests and all fifteen wave-cli process tests passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 以 signals --match=req --kind=bit --exact 直接定位到 lane-request；结果只有一条紧凑
元数据且不含 segments。随后使用 Scenario 名、Clock 名及 REQ/ack/DATA[7:0] 名称预演四操作
批次：先断言 req@cycle8=1，再原子写入 ack=1、data=0xa5，并在同批次后置断言。dry-run
written=false，独立输出 SHA 与预演一致，报告将输入名称规范为 lane-request/lane-ack/lane-data。
名称采样得到 ack=1、data=0xa5；名称窗口按输入顺序返回 request/data；validate accepted=true、
0 error。歧义名称、缺失目标、--exact 无 --match 和非法 limit 均结构化拒绝且不产生部分结果。
```

长期 Goal 当前保持 active；第 320 轮为已验收的阶段可用节点。

## 持续迭代 321–330：CLI Group/Enum 符号刺激建模

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 321 轮：从 Codex 从零建立真实 FPGA 刺激的路径审计 CLI，确认 Clock/Bit/Bus 已可创建，
   但 Group 与 Enum 仍要求调用方手写工程 JSON，符号状态机路径未闭环。
2. 第 322 轮：扩展 `add-signal` 支持 `kind=enum`，要求显式位宽与非空声明映射，并沿用稳定
   Lane ID、确定性可读颜色和唯一名称语义。
3. 第 323 轮：Enum 映射的每个底层值按位宽和有符号属性校验并规范化；空符号、空值、非法格式
   与溢出在任何模型修改前拒绝。
4. 第 324 轮：新增 `add-group`，支持名称、稳定/自动 ID、颜色和插入位置，以领域命令加入
   Scenario，不引入桌面专用状态。
5. 第 325 轮：`add-signal.groupId` 接受已有或同批次先创建 Group 的唯一名称/稳定 ID；Clock、
   Bit、Bus 与 Enum 均可直接归组，报告返回规范 Group ID。
6. 第 326 轮：`update-signal.enumMap` 支持完整替换 Enum 声明映射，并在同一 operation 中按
   修改后的位宽/有符号属性校验，结果报告映射与声明数量。
7. 第 327 轮：映射删除若使现有 Segment/Event 值失效，由 `ChangeLaneCommand` 拒绝整个批次；
   非 Group 归组、非 Enum 使用映射及溢出值同样保持零部分结果。
8. 第 328 轮：补齐 Group/Enum 自动身份、颜色、序列结果、报告字段及 dry-run/实际写入 SHA
   确定性核心回归；相同输入重复执行得到相同序列化工程。
9. 第 329 轮：新增进程级符号刺激契约，覆盖新建、紧凑 Enum 查询、声明采样、映射更新、校验、
   溢出拒绝、无失败输出和源 SHA 不变，并同步 CLI/API/Qt Creator 文档。
10. 第 330 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置全量静默回归，并执行
    Codex Group→Clock/Bit/Enum→符号序列→映射扩展→采样→校验→非法映射拒绝用户旅程。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
42/42 tests passed
Total Test time: 21.01 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
42/42 tests passed
Total Test time: 23.06 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
42/42 tests passed
Total Test time: 20.16 sec

Targeted: wave-core-tests and wave-cli-symbolic-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 对不存在的路径先执行 new dry-run，再用同一 6-operation 文件实际创建工程；预演不写盘，
实际 resultSha256 与预演一致。一个批次建立 Control Group、clk、enable 和 2-bit state Enum，
三条信号均按 Group 名称直接归组，state 关联 clk 并声明 IDLE/BUSY/DONE；紧凑 signals 查询只
返回一条 Enum 元数据和三项映射，不含 segments，cycle 2 采样为 DONE。第二批先断言 BUSY，
将映射扩展为四项并写入 ERROR，预演/写入 SHA 一致，cycle 3 采样为 ERROR，validate 为
0 error。加入超出 2-bit 的 OVERFLOW=4 时退出码 4、失败 operation=0、无输出文件；源工程仍为
三项映射且 sourceSha256 保持 41d2196ab6fb37f1c3e76be4612ae00a5b3973745b84c3c694702e6a96c04e48。
```

长期 Goal 当前保持 active；第 330 轮为已验收的阶段可用节点。

## 持续迭代 331–340：CLI 时序关系与阶段标注

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 331 轮：从 Codex 表达真实时序意图的路径审计 CLI，确认直接开放 Event CRUD 会迫使调用方
   维护随 Segment 编辑重映射的内部身份，决定改用 Lane 选择器与精确波形边沿时刻定位 Relation。
2. 第 332 轮：实现 Relation 源/目标端点的 Lane 名称/稳定 ID 与 tick/物理时间/cycle 寻址；
   只接受恰好一个现有波形 Event 的非 Clock、非 Group 边沿，歧义或缺失不猜测。
3. 第 333 轮：新增 `add-relation`，支持 tick、物理时间或完整时钟周期的最小/最大延迟，以及
   时钟上下文、condition、severity、description、显式或确定性自动 ID。
4. 第 334 轮：新增 `update-relation` 与 `delete-relation`；端点可移到另一现有边沿，延迟与
   描述属性可局部修改，语义重复及无效果修改返回 `changed=false`。
5. 第 335 轮：新增 `add-marker`，以唯一名称建立点或非空区间，支持 point、interval、phase、
   error、note 类型、备注、精确时间及确定性自动 ID。
6. 第 336 轮：新增 `update-marker` 与 `delete-marker`；`markerId` 接受稳定 ID 或唯一的
   大小写不敏感名称，同批次改名后的名称可立即用于后续 operation。
7. 第 337 轮：补齐 Relation/Marker 确定性身份、规范报告、重复关系幂等、更新空效果抑制及
   dry-run/实际写入序列化一致性核心契约。
8. 第 338 轮：补齐不同 ClockDomain、非法延迟范围、点/区间字段混用、空区间、缺失边沿及
   批次中途失败的原子拒绝，确认先前 Marker/Relation 不会形成部分结果。
9. 第 339 轮：新增进程级时序意图契约，覆盖预演、写入、局部窗口依赖闭包、无效果操作、
   名称清理、无效边沿回滚和源 SHA 不变；同步 CLI/API/Qt Creator 文档。
10. 第 340 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置全量静默回归，并执行
    Codex Relation→Marker→端点/区间修改→局部窗口→校验→清理→无效边沿拒绝用户旅程。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
43/43 tests passed
Total Test time: 21.36 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
43/43 tests passed
Total Test time: 23.21 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
43/43 tests passed
Total Test time: 20.88 sec

Targeted: wave-core-tests and wave-cli-intent-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 在符号刺激工程上预演并实际写入 6-operation 时序意图批次；预演 written=false 且不产生
文件，实际 resultSha256 与预演一致。enable@cycle1 到 state@cycle2 的 Relation 使用规范 Lane
身份，延迟为 10000..20000 tick，重复更新 changed=false；Control window Marker 自动生成稳定
ID，范围为 10000..40000 tick，按改名后的名称重复更新同样 changed=false。局部 window 查询
返回 1 Relation、1 Marker，validate 为 0 error；清理后两者计数均为 0。

另一批次先加入 Marker，再引用不存在的 enable@cycle2 边沿；进程以退出码 4 在 operation 1
返回 operation-rejected，不创建失败输出，源 SHA
b6e74567da5cc22bd21bef915327b954eaa546f89f18700f2636abf563d1e36f 保持不变，完整 inspect
确认 Relation/Marker 计数仍为 0。
```

长期 Goal 当前保持 active；第 340 轮为已验收的阶段可用节点。

## 持续迭代 341–350：CLI 能力发现与 Group 生命周期

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 341 轮：从其他应用和 Codex 首次接入路径审计 CLI，确认纯文本 `--help/--version`
   无法用于可靠能力协商，Group 也只有创建与引用、缺少安全维护闭环。
2. 第 342 轮：在 `waveautomation` 新增 `describeAutomationCapabilities()` 与
   `wave-workbench.capabilities/v1`，让嵌入式 Qt/C++ 宿主直接取得能力文档。
3. 第 343 轮：新增无工程依赖的 `wave-cli capabilities [--pretty]`；未知参数以退出码 2 和
   结构化 usage 错误返回，不要求解析自然语言帮助文本。
4. 第 344 轮：能力文档机器列出八个命令、22 类 operation 及类别、schema 版本、选择器寻址、
   精确时间格式、可读/可创建 Lane 类型、Marker/Relation 值域和七项安全特性。
5. 第 345 轮：新增 `update-group`，按稳定 ID 或唯一名称修改名称、颜色、高度和可见性；
   报告返回规范身份、最终属性和成员数，无效果修改不产生历史变化。
6. 第 346 轮：新增 `move-group`，复用索引、before Lane、after Lane 三种位置表达；参照对象
   可为任意 Lane，同批次改名后的 Group 可立即按新名称移动。
7. 第 347 轮：新增 `delete-group`，只删除 Group 并保留、解除归组全部成员；报告直接返回
   `ungroupedSignalCount` 与规范成员 Lane ID，调用方无需重新扫描工程解释影响。
8. 第 348 轮：补齐自动 Group ID、名称大小写、预演/写入 SHA、更新/移动无效果抑制、名称冲突、
   普通信号误作 Group 和中途失败零部分结果的核心契约。
9. 第 349 轮：新增进程级 capabilities/Group 契约，覆盖能力协商、空工程创建、属性更新、
   重排、成员查询、删除与解除归组、结构化错误和源 SHA 不变；同步 CLI/API/Qt Creator 文档。
10. 第 350 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置全量静默回归，并执行
    Codex 能力发现→Group 创建/归组→改名/移动→查询→删除→校验→错误回滚用户旅程。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
44/44 tests passed
Total Test time: 21.57 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
44/44 tests passed
Total Test time: 23.87 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
44/44 tests passed
Total Test time: 20.68 sec

Targeted: wave-core-tests and wave-cli-capabilities-group-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 无需工程先读取 capabilities，确认 schema=wave-workbench.capabilities/v1、8 个命令、
22 类 operation，并直接发现 add/update/move/delete-group。随后从空工程预演并写入 Group
批次：Core 自动身份 group-auto-367ffc9d1aef367e，req 按大小写变体名称归组，Core 原位改名为
Control、设为 #336699/52 px/hidden，再移动到索引 2；重复属性和位置操作均 changed=false。
预演不写文件，实际 resultSha256 与预演一致，结果 SHA 为
f6c7ab41a4a959f92c69ec3bd9e9737c15bd095f7cbda8636da15c3077241a7a。

按 CONTROL 删除 Group 后报告 1 个解除归组成员，req 本身及波形保留、groupId 为空，validate
为 0 error。另一批次先修改 Group，再把 req 当作 Group 删除；进程以退出码 4 在 operation 1
原子拒绝，无 stdout、无失败输出，源 SHA 保持不变。
```

长期 Goal 当前保持 active；第 350 轮为已验收的阶段可用节点。

## 持续迭代 351–360：CLI 整信号安全复制

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 351 轮：审计 Codex 批量搭建相似 Bus/Enum/Clock 的实际步骤，确认 CLI 缺少桌面端已有的
   整信号复制入口，调用方必须重复声明属性、波形与身份并容易误接依赖。
2. 第 352 轮：新增 `duplicate-signal`，按 Lane 稳定 ID 或唯一名称取得源信号，以一个领域命令
   复制完整 Lane 属性、扩展数据和 Segment 内容，Group 明确拒绝。
3. 第 353 轮：默认名称使用 `<源名称>_copy` 并自动递增后缀，默认颜色确定性分配且排除源颜色；
   Lane 与 Segment 均生成独立、可重复的 stable ID，dry-run 与实际写入一致。
4. 第 354 轮：默认紧邻源信号插入；支持 `insertionIndex`、`beforeLaneId`、`afterLaneId`
   至多一个位置覆盖，以及名称/ID/颜色/Group 覆盖和空 Group 解除归组。
5. 第 355 轮：复制全部 Segment 的起止、值与扩展语义，同时确保所有 Segment ID 与源独立；
   Bus/Enum 位宽、有符号、进制、声明映射和原 ClockDomain 关联保持不变。
6. 第 356 轮：Clock 副本创建独立 ClockDomain，复制周期、相位、占空比、有效边沿、复位关系和
   扩展属性；`newClockId` 可固定身份，非 Clock 使用该字段原子拒绝。
7. 第 357 轮：明确不复制 Event、Relation 或 imported trace mapping；operation 报告返回
   source/new Lane、Segment 数、最终位置、颜色/分组、Clock 身份及三个零复制依赖计数。
8. 第 358 轮：补齐同批次按新副本名称继续复制、自动名称冲突后缀、显式名称冲突、Group 误用、
   Clock 字段误用和批次第二项失败零部分结果的核心回归。
9. 第 359 轮：将 `duplicate-signal` 加入 capabilities，新增进程级契约覆盖预演/写入 SHA、
   Bus/Clock 采样等价、独立时钟域、计数不变、校验和失败源 SHA 不变，并同步文档。
10. 第 360 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置全量静默回归，并执行
    Codex 能力发现→Bus 复制→按新名称再复制→Clock 复制→采样→校验→错误回滚用户旅程。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
45/45 tests passed
Total Test time: 22.21 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
45/45 tests passed
Total Test time: 24.50 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
45/45 tests passed
Total Test time: 20.93 sec

Targeted: wave-core-tests and wave-cli-duplicate-signal-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 先从 capabilities 发现第 23 类 operation `duplicate-signal`，再在 handshake 工程上预演
三项批次：data[7:0] 复制为 payload_copy，随后直接按新名称复制为 payload_copy_copy；clk
复制为 clk_shadow，并创建 clock-shadow 独立 ClockDomain。预演不写文件，实际 SHA 与预演
一致，结果为 276389843af01b1416b68c64cca2130817dbc07c4cb85393e79cf88528571b54。

90 ns 时原 Bus 与两个副本均为 0x35，175 ns 时 clk/clk_shadow 取值一致；payload_copy 保留
3 个 Segment、Handshake signals 分组和原 ClockDomain，工程由 8 增至 11 Lane、1 增至
2 ClockDomain，Event 仍为 14、Relation 仍为 1，validate 0 error。另一批次先产生临时副本，
再给 Bus 指定 newClockId；进程以退出码 4 在 operation 1 原子拒绝，无 stdout、无失败输出，
源 SHA d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4 保持不变。
```

长期 Goal 当前保持 active；第 360 轮为已验收的阶段可用节点。

## 持续迭代 361–370：CLI 安全缩短与显式时间轴截断

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 361 轮：审计 CLI 仍标记为“只能延长”的 End 限制，确认常规信号 Segment 会铺到 Scenario
   末端，现有桌面保护逻辑使实际缩短几乎总被拒绝，自动化只能手工逐 Lane 清理后再改 End。
2. 第 362 轮：定义无损缩短与破坏性缩短两级契约；新 End 之后没有内容时直接提交，可能丢失内容
   时默认拒绝，不从调用意图猜测。
3. 第 363 轮：新增 `TruncateScenarioDurationCommand`，以完整 Scenario 前后快照提供单步
   Undo/Redo；非法零值、负值、等长或扩展目标由命令边界拒绝。
4. 第 364 轮：截断时保留跨越新 End 的 Segment 身份、起点、值和扩展数据，只裁短终点；起点位于
   新 End 或之后的 Segment 被删除，所有保留区间严格位于新场景范围内。
5. 第 365 轮：删除位于新 End 或之后的 Event，并同步删除引用这些 Event 的 Relation；仍在范围内
   的 Event、Relation 和波形关联保持不变。
6. 第 366 轮：区间 Marker 跨越新 End 时裁短，起点位于新 End 或之后的区间删除；新 End 上的点
   Marker 保留，之后的点 Marker 删除。
7. 第 367 轮：`set-duration` 新增布尔 `truncate`；无损缩短不需要授权，会损失内容的缩短必须
   显式为 `true`，否则错误直接列出六项裁剪/删除计数，且整个批次原子回滚。
8. 第 368 轮：成功报告新增方向、授权状态、是否实际截断及 Segment/Event/Relation/Marker 六项
   影响计数；相同 End 规范返回 `changed=false`，避免产生假变更。
9. 第 369 轮：capabilities 新增 `scenarioDuration` 机器契约；核心与进程级回归覆盖安全缩短、
   显式截断、错误类型、影响计数、确定性 SHA、源文件不变和边界采样，并同步 CLI 文档。
10. 第 370 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置全量静默回归，并执行
    Codex 保护拒绝→预演→确认截断→边界采样→校验→源 SHA 复核用户旅程。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
46/46 tests passed
Total Test time: 22.35 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
46/46 tests passed
Total Test time: 24.73 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
46/46 tests passed
Total Test time: 21.21 sec

Targeted: wave-core-tests and wave-cli-duration-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 用户把 handshake 的 End 从 220 ns 改为 120 ns。未提供 truncate 的两操作批次在 operation 1
以退出码 4 拒绝，错误明确预告将裁短 6 个 Segment、1 个 Marker，并删除 6 个 Segment 和
4 个 Event；前置重命名未写入，无 stdout、无失败输出。

显式 truncate 预演不创建文件，实际写入与预演 SHA 均为
3eb6d420fd2978c52a51bb75cd756bca6d8a9122f3de073f80d09511ef3aa36a。结果 End 为
120000 tick，119 ns 采样依次为 req=1、ack=1、data=0x35、state=WAIT_ACK，validate 为
0 error；源工程 SHA 前后相同。另从 60 ns 空白工程无授权缩到 40 ns，报告
contentTruncated=false，确认空白尾部不需要破坏性确认。
```

长期 Goal 当前保持 active；第 370 轮为已验收的阶段可用节点。

## 持续迭代 371–380：CLI Relation 就绪边沿查询

状态：阶段完成，达到可用节点；长期 Goal 按用户要求暂停；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 371 轮：审计 Relation 端点定位流程，确认调用方只能读取较大的 `window` 结果，再自行筛选
   waveform-linked Event、Lane、时刻和唯一性，增加传输、理解及错误成本。
2. 第 372 轮：在 `waveautomation` 定义 `AutomationEdgeKind`、
   `AutomationEdgeQueryOptions` 与 `findWaveformEdgesForAutomation()`，供 CLI 与嵌入宿主复用。
3. 第 373 轮：新增只读 `wave-cli edges`，支持 Scenario/Lane 唯一名称或稳定 ID、可选半开范围、
   默认 200/最大 10000 条结果及版本化 JSON stdout/stderr。
4. 第 374 轮：实现 `initial`、Bit `rising`/`falling` 和通用 `change` 分类，返回前值、当前值、
   Event 动作、有效 ClockDomain、说明、格式化时间和十进制字符串 tick。
5. 第 375 轮：按 tick、Lane 顺序和稳定候选顺序确定性输出；同 Lane/tick 多个候选合并为一项，
   用 `candidateCount`、`relationEndpoint` 和 `ambiguousEndpointCount` 明确禁止猜测。
6. 第 376 轮：结果不暴露内部 Event ID 或 linked Segment ID；显式 Clock/Group、重复 Lane、
   越界/空范围、非法边沿过滤和非法 limit 均返回结构化错误。
7. 第 377 轮：CLI 支持 tick、精确物理时间及 `cycle N` 范围；显式 `--clock` 优先，所选 Lane
   具有唯一关联时钟时自动推断，默认范围为 `[0, Scenario End)`。
8. 第 378 轮：capabilities 增加第九个命令、四类 `edgeKinds` 与
   `relationReadyEdgeQuery=true`，嵌入 API 与独立 CLI 保持同一能力文档。
9. 第 379 轮：补齐核心和进程级契约，覆盖初始/上升/下降/Bus 变化、周期范围、结果上限、
   歧义端点、Clock/Group/非法过滤拒绝、不泄漏内部 ID 与源文件只读保持。
10. 第 380 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置全量静默回归，并执行
    Codex 边沿查询→Relation 预演→实际写入→局部窗口确认→校验→源 SHA 复核用户旅程。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
47/47 tests passed
Total Test time: 22.99 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
47/47 tests passed
Total Test time: 25.95 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
47/47 tests passed
Total Test time: 21.61 sec

Targeted: wave-core-tests and wave-cli-edges-contract-smoke passed
git diff --check: clean
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 以 req/ack、70–130 ns 和 rising 作为唯一输入，edges 直接返回两项可用端点：
lane-request@80000（0→1）与 lane-ack@110000（0→1），均为 candidateCount=1、
relationEndpoint=true，结果不含 Event/Segment ID。

调用方直接用这两个 Lane/tick 更新 relation-req-ack；dry-run 与实际写入 SHA 相同，结果工程 SHA
为 0200ba947e510e313682f18ca09fcabee0feee663cbd6972b6d95f9f1944aae1。70–130 ns 局部窗口
仍返回 1 个 Relation，validate 为 0 error，源工程 SHA 前后不变。下降沿 limit=1 明确报告
matchCount=2、returnedCount=1、truncated=true；Bus change 返回 80/150 ns；cycle 8–13
解析为 80000–130000 tick。Clock Lane 和 sideways 过滤分别以退出码 4/2 结构化拒绝。
```

长期 Goal 当前已按用户要求暂停；第 380 轮为已验收的阶段可用节点。

## 持续迭代 381–390：CLI 既有 Relation 紧凑查询

状态：阶段完成，达到可用节点；长期 Goal 由系统恢复为 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 381 轮：审计既有 Relation 的维护路径，确认 `window` 以 source/target Event ID 表达关系，
   调用方必须连接 Relation→Event→Lane 才能获得 `update-relation`/`delete-relation` 所需信息。
2. 第 382 轮：定义 `AutomationRelationQueryOptions` 与
   `findRelationsForAutomation()`，由嵌入式 Qt/C++ 宿主和独立 CLI 共用查询语义。
3. 第 383 轮：新增只读 `wave-cli relations`，支持 Relation 文本、严重性、Lane、半开端点时间
   范围、Scenario、时钟和结果上限组合筛选。
4. 第 384 轮：结果以 Relation ID 及 source/target Lane+tick 为中心，直接返回前后值、边沿类型、
   Event 动作、时钟、条件、说明和严重性，不暴露 source/target Event ID 或 linked Segment ID。
5. 第 385 轮：计算 `observedDelayTick` 与 `timingWithinRange`，让调用方无需自行减法即可确认实际
   端点延迟是否位于声明的最小/最大范围。
6. 第 386 轮：每个端点公开 resolved、eventIdCount、candidateCount、relationEndpoint 和稳定
   issue 代码；缺失 Event/Lane、重复 Event ID、脱离波形、越界时刻及歧义 waveform edge 均可诊断。
7. 第 387 轮：按最早可解析端点时刻和 Relation ID 稳定排序；`readyCount`、
   `endpointIssueCount`、match/returned/truncated 使宿主可直接决定维护或清理路径。
8. 第 388 轮：capabilities 增加第十个命令和 `compactRelationQuery=true`；CLI 支持精确 ID/
   完整文本匹配、物理时间/tick/cycle 范围以及 Lane 唯一名称或稳定 ID。
9. 第 389 轮：补齐核心与进程契约，覆盖健康/损坏/重复/脱离波形端点、延迟越界、各类筛选、
   空结果、上限、非法输入、内部 ID 隔离及源 SHA 只读保证。
10. 第 390 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置全量静默回归，并执行
    Codex Relation 定位→ID 复用→预演→实际更新→重新查询→校验→源 SHA 复核用户旅程。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
48/48 tests passed
Total Test time: 23.76 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
48/48 tests passed
Total Test time: 26.22 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
48/48 tests passed
Total Test time: 22.14 sec

Targeted: wave-core-tests, wave-cli-capabilities-group-contract-smoke,
          wave-cli-edges-contract-smoke and wave-cli-relations-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 只提供 req、within 和 70–130 ns 即定位 relation-req-ack；结果直接显示
lane-request@80000 → lane-ack@110000、observedDelayTick=30000、timingWithinRange=true、
endpointsReady=true，不包含 Event/Segment ID。

调用方直接复用返回的 relationId，将 severity 改为 warning、description 改为
"ack observed within requested latency"。dry-run 与实际写入 SHA 相同，结果 SHA 为
4007aa9366d6733a08a666328a8249439ec5ee0ee7743178527a1d6aed3dada4；重新按精确 ID 查询
确认新值与完整端点，validate 为 0 error，源工程 SHA 前后相同。
```

长期 Goal 当前已按用户要求暂停；第 390 轮为已验收的阶段可用节点。

## 持续迭代 391–400：CLI 既有 Marker 紧凑查询

状态：阶段完成，达到可用节点；用户已明确恢复长期迭代；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 391 轮：审计既有 Marker 维护路径，确认 `inspect` 会返回完整 Scenario，`window` 只能按已知
   时间范围附带 Marker；调用方无法先按名称、备注或用途定位标注。
2. 第 392 轮：定义 `AutomationMarkerQueryOptions` 与 `findMarkersForAutomation()`，由嵌入式
   Qt/C++ 宿主和独立 CLI 共用筛选与诊断语义。
3. 第 393 轮：新增只读 `wave-cli markers`，支持稳定 ID/名称/备注文本、五种 Marker 用途、
   半开时间范围、Scenario、Clock 和结果上限组合筛选。
4. 第 394 轮：点 Marker 按位置落入 `[start,end)`，区间 Marker 按相交语义返回；省略时间筛选时
   保留全部对象，包括需要清理的越界或反向 Marker。
5. 第 395 轮：结果直接返回 Marker ID、名称、用途、备注、精确起止、持续 tick、格式化时间和
   point 标志，不返回扩展载荷或完整工程正文。
6. 第 396 轮：逐项公开 `idCount`、`nameCount`、`addressable`、`valid` 与稳定 issue 代码；
   空身份、重复 ID/名称、越界/反向几何及 Point/范围冲突均可诊断。
7. 第 397 轮：按起点、终点和 Marker ID 稳定排序；match/returned/truncated、valid/addressable/
   issue 计数覆盖全部命中项，不因 limit 截断而静默隐藏异常。
8. 第 398 轮：capabilities 增加第十一个命令和 `compactMarkerQuery=true`；CLI 支持精确匹配及
   tick/物理时间/cycle 范围，单 ClockDomain 工程自动推断周期时钟并返回规范 ID。
9. 第 399 轮：补齐核心与进程契约，覆盖健康/重复/损坏 Marker、文本/类型/时间筛选、半开边界、
   稳定排序、上限、非法输入、扩展隔离及源 SHA 只读保证。
10. 第 400 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置全量静默回归，并执行
    Codex Marker 定位→ID 复用→预演→实际更新→重新查询→校验→源 SHA 复核用户旅程。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
49/49 tests passed
Total Test time: 25.26 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
49/49 tests passed
Total Test time: 28.32 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
49/49 tests passed
Total Test time: 22.98 sec

Targeted: wave-core-tests, wave-cli-capabilities-group-contract-smoke,
          wave-cli-edges-contract-smoke, wave-cli-markers-contract-smoke
          and wave-cli-relations-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 只提供 request、phase 和 70–130 ns，即从完整工程中定位 marker-transfer；结果直接显示
80–150 ns、durationTick=70000、addressable=true、valid=true，不需要读取完整 Marker 数组。

调用方复用返回的 markerId，将名称改为 "Transfer window"、备注改为
"Request acknowledged within expected window"。dry-run 与实际写入 SHA 同为
a87b7d62d80568f221093176864bf836272205a935d386e9bbd743f838ac668e；重新按精确 ID 查询
确认新值及健康状态，validate 为 0 error，源工程 SHA 前后相同。
```

长期 Goal 已按用户明确指令恢复迭代；第 400 轮为已验收的阶段可用节点。

## 持续迭代 401–410：CLI 损坏 Relation 显式端点修复

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 401 轮：审计 `update-relation`，确认其在读取替换字段前即以 `findEvent()` 要求旧源/目标
   Event 存在，导致 `relations` 虽能诊断损坏端点，却只能删除而不能恢复。
2. 第 402 轮：复用 Relation 紧凑查询的端点健康判定，统一识别缺失/重复 Event ID、缺失 Lane、
   越界时刻、Clock/Group、脱离波形及歧义 waveform edge。
3. 第 403 轮：允许损坏端点由显式 Lane 与精确时刻完整替换；无需读取或提交内部 Event ID，
   Relation ID、健康端点及未指定属性保持不变。
4. 第 404 轮：保留健康端点的既有局部修改语义；仅损坏端点强制同时提交 Lane 和时刻，双端损坏
   可在同一 operation 中完整修复。
5. 第 405 轮：operation 报告新增 `repairedSourceEndpoint`/`repairedTargetEndpoint`，
   capabilities 新增 `relationEndpointRepair=true`，宿主可直接确认修复范围。
6. 第 406 轮：收紧 Relation 边沿可寻址性；`edges` 新增 `eventIdCount`，只有位置唯一且 Event
   ID 非空、全局唯一时才返回 `relationEndpoint=true`。
7. 第 407 轮：部分修复、元数据绕过、同位置多 Event、重复或空 Event ID 均结构化拒绝；批次
   失败不返回候选工程，CLI 不写失败输出。
8. 第 408 轮：补齐核心契约，覆盖缺失目标、重复/脱离源端、双端损坏、确定性重复修复、健康端
   保留、部分字段拒绝、歧义位置及重复 Event ID。
9. 第 409 轮：新增 `wave-cli-relation-repair-contract-smoke`，从磁盘示例构造缺失目标工程，
   覆盖诊断→dry-run→写入→重查→validate→失败输出缺失→源 SHA 保持的真实进程链路。
10. 第 410 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置全量 offscreen 回归，
    更新自动化文档、README、PLAN 与 GOAL，并执行开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
50/50 tests passed
Total Test time: 25.04 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
50/50 tests passed
Total Test time: 27.73 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
50/50 tests passed
Total Test time: 22.57 sec

Targeted: wave-core-tests and
          wave-cli-relation-repair-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 先查询一个仍可加载但 relation-req-ack 目标 Event 已丢失的工程；结果直接返回
readyCount=0、endpointIssueCount=1、target.issue=missing-event，无需解析完整 Event 数组。

调用方只提交 relationId、targetLaneId=ack 与 targetAt=110 ns。dry-run 不写文件并报告
repairedSourceEndpoint=false、repairedTargetEndpoint=true；实际写入与预演 SHA 同为
7f69420c38e934b9a29eb77ca5165879db31674263604ff51a33993470082cd1。重新查询得到
lane-request@80000 → lane-ack@110000、endpointsReady=true，validate 为 0 error。
只给 targetLaneId 的尝试以退出码 4 拒绝、无 stdout、无失败输出，损坏源工程 SHA 前后保持
6c22b84180f06429076129b478df9992ddbaa3fd1be85be05ce0357e54f53e10。
```

长期 Goal 保持 active；第 410 轮为已验收的阶段可用节点。

## 持续迭代 411–420：CLI 损坏 Marker 快照引用恢复

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互

逐轮交付：

1. 第 411 轮：审计 `update-marker`，确认唯一 ID 的越界/反向/Point 冲突几何可以显式修正，
   但重复稳定 ID 会被通用选择器静默解析为第一项，空 ID 则无法选择。
2. 第 412 轮：收紧通用命名选择器；直接稳定 ID 重复时拒绝，唯一名称解析出的底层稳定 ID
   仍重复时同样拒绝，不再让显示名绕过身份歧义。
3. 第 413 轮：定义 `markerRef` 为 Scenario、Marker 索引及完整持久内容的 SHA-256 快照引用；
   扩展载荷参与摘要但不在紧凑查询中暴露。
4. 第 414 轮：`markers` 为每条结果返回不透明 `markerRef`；重复 Marker 因索引和完整内容
   获得不同引用，内容或索引变化后旧引用确定失效。
5. 第 415 轮：`update-marker` 支持 `markerId`/`markerRef` 二选一，并以 `newId` 修复空或重复
   稳定身份；名称、几何、用途、备注及未指定扩展保持原有语义。
6. 第 416 轮：`delete-marker` 支持按 `markerRef` 精确删除单项；新增带期望快照的索引修改/删除
   领域命令，保留完整单步 Undo/Redo 和过期引用拒绝。
7. 第 417 轮：operation 报告公开选择方式、旧/新 ID、身份/几何修复结果和新引用；
   capabilities 新增 `markerRepairReference=true`。
8. 第 418 轮：补齐核心契约，覆盖重复/空 ID、唯一名称绕过、改 ID、几何修复、ID 冲突、旧引用、
   引用删除、扩展保留、确定性、失败无候选工程及领域命令 Undo/Redo。
9. 第 419 轮：新增 `wave-cli-marker-repair-contract-smoke`，从磁盘示例构造重复 Marker ID，
   覆盖查询→引用→预演→改 ID→复查→校验→旧引用拒绝→引用删除→源 SHA 保持。
10. 第 420 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置全量 offscreen 回归，
    更新自动化文档、README、PLAN 与 GOAL，并执行开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
51/51 tests passed
Total Test time: 25.35 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
51/51 tests passed
Total Test time: 28.42 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
51/51 tests passed
Total Test time: 22.88 sec

Targeted: wave-core-tests and
          wave-cli-marker-repair-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 查询含两个 marker-transfer 的损坏工程；结果 matchCount=2、addressableCount=0，
两项 markerRef 不同。直接用 markerId 更新以退出码 4 拒绝，不写失败输出。

调用方使用第二项 markerRef，将其 ID 改为 marker-transfer-follow-up。dry-run 报告
selectedByRepairRef=true、repairedIdentity=true，实际写入与预演 SHA 同为
24092a613ca527b9381090dd770b638b55ba5475dd77b8e4c979e4d8fa968914。复查原 ID 与新 ID
各命中一项且均 addressable，validate 为 0 error。修改前引用用于新工程时以退出码 4 拒绝；
同一引用也可在原损坏工程精确删除第二项。损坏源工程 SHA 前后保持
372f121f018e51446876dcb82a9fbafafb69241a4149116249f476506c8bf00a。
```

长期 Goal 保持 active；第 420 轮为已验收的阶段可用节点。

## 持续迭代 421–430：CLI 损坏 Relation 快照引用恢复

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

逐轮交付：

1. 第 421 轮：审计 Relation 恢复链路，确认端点已可显式修复，但空 ID 无法寻址、重复 ID 会被
   `findRelation()` 静默解析为第一项，导致损坏 Relation 无法安全维护。
2. 第 422 轮：收紧稳定 ID 路径；`update-relation` 与 `delete-relation` 只接受唯一非空 ID，
   重复 ID 返回结构化歧义错误，不修改候选工程。
3. 第 423 轮：定义 `relationRef` 为 Scenario、Relation 索引和完整持久内容的 SHA-256
   快照引用；身份、端点 ID、延迟、时钟、条件、严重性、说明和扩展数据均参与摘要。
4. 第 424 轮：`relations` 为每项返回 `relationRef`、`idCount`、`addressable` 与
   `identityIssues`；顶层返回 `addressableCount` 和 `identityIssueCount`，计数不受 limit 截断。
5. 第 425 轮：`update-relation` 支持 `relationId`/`relationRef` 二选一及 `newId`，可将空或
   重复 ID 改为 Scenario 内唯一身份，并报告选择方式、旧 ID、身份修复结果和新引用。
6. 第 426 轮：引用路径复用既有显式端点修复规则，可在同一原子 operation 中同时改 ID 与替换
   损坏端点；健康端点、未指定属性及扩展载荷保持不变。
7. 第 427 轮：`delete-relation` 支持引用精确删除重复项；新增带预期快照的索引修改/删除领域
   命令，旧引用拒绝且整个操作保持单步 Undo/Redo。
8. 第 428 轮：补齐核心契约，覆盖重复/空 ID、不同引用、稳定 ID 歧义、联合修复、冲突 ID、
   旧引用、扩展隔离、精确删除及索引命令 Undo/Redo。
9. 第 429 轮：新增 `wave-cli-relation-identity-repair-contract-smoke`，从磁盘示例构造重复
   Relation ID，覆盖查询→引用→dry-run→写入→复查→validate→旧引用拒绝→引用删除→源 SHA 保持。
10. 第 430 轮：capabilities 新增 `relationRepairReference=true`，完成默认、Qt Creator Debug、
    Qt Creator Release 三配置全量 offscreen 回归，更新自动化文档、README、PLAN 与 GOAL，
    并执行开发视角和用户视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
52/52 tests passed
Total Test time: 23.96 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
52/52 tests passed
Total Test time: 29.15 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
52/52 tests passed
Total Test time: 22.69 sec

Targeted: wave-core-tests and
          wave-cli-relation-identity-repair-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 查询含两个 relation-req-ack 的损坏工程；结果 matchCount=2、readyCount=2、
addressableCount=0，两项 relationRef 不同。直接使用 relationId 更新以退出码 4 原子拒绝。

调用方使用第二项 relationRef，将其 ID 改为 relation-req-ack-follow-up。dry-run 报告
selectedByRepairRef=true、repairedIdentity=true，结果 SHA 为
9839398b496c9eb18b18bfa795e0e2c6f3b4274042e77abddae9d10503f12931。复查原 ID 与新 ID
各命中一项、均 addressable，目标关系 endpointsReady，validate 为 0 error。修改前引用用于
新工程时以退出码 4 拒绝；同一引用可在原损坏工程精确删除第二项。损坏源工程 SHA 保持
b634698c888de4912bb453347b20e938abeee64fe4a46770ca85d5433526abc7。
```

长期 Goal 保持 active；第 430 轮为已验收的阶段可用节点。

## 持续迭代 431–440：CLI 核心结构身份校验

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

逐轮交付：

1. 第 431 轮：以重复 Relation ID 工程复核 `validate`，确认旧实现仅检查行为语义，返回
   `valid=true`、0 error，给调用方错误的安全结论。
2. 第 432 轮：定义 Project、ClockDomain、Scenario、Lane、Segment、Event、Relation 与 Marker
   八类核心稳定身份作用域；不同 Scenario 或不同对象类型的同名 ID 不被误判为冲突。
3. 第 433 轮：为空 ID 返回 `missing-stable-id`，为同作用域重复 ID 的每个受影响对象返回
   `duplicate-stable-id`，不再只报告一个无法定位的汇总错误。
4. 第 434 轮：每项身份问题公开 `objectKind`、数组索引 `path`、`stableId`、`idCount`，并按可用
   上下文附带 Scenario、Lane 和对象索引，调用方无需解析自然语言消息。
5. 第 435 轮：新增 `identitySummary`、`identityIssueCount` 和 `semanticIssueCount`；结构身份问题
   计入 error，使 `valid`、`accepted` 和 CLI 退出码与真实可安全编辑状态一致。
6. 第 436 轮：Project、ClockDomain 与 Scenario 身份始终按工程作用域审计；指定唯一 Scenario
   时只扫描其内部对象，保持局部语义校验边界，同时不会隐藏工程级身份损坏。
7. 第 437 轮：capabilities 新增 `structuralIdentityValidation=true`；自动化文档说明问题代码、
   路径、计数、只读性及 source SHA 修复流程。
8. 第 438 轮：核心契约构造同时包含空 Project/Marker ID 和重复 ClockDomain/Scenario/Lane/
   Segment/Event/Relation ID 的工程，精确断言 14 个问题、八类作用域及健康工程零回归。
9. 第 439 轮：新增 `wave-cli-identity-validation-contract-smoke`，从磁盘示例构造重复 Relation
   ID，覆盖错误退出→结构定位→引用修复 dry-run/写入→重新校验→源 SHA 保持。
10. 第 440 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置全量 offscreen 回归，
    更新 README、PLAN、GOAL、Qt Creator 与自动化文档，并执行开发视角和用户视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
53/53 tests passed
Total Test time: 23.96 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
53/53 tests passed
Total Test time: 28.96 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
53/53 tests passed
Total Test time: 23.12 sec

Targeted: wave-core-tests and
          wave-cli-identity-validation-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 对含两个 relation-req-ack 的工程执行 validate。旧版本会返回 valid=true；当前版本以退出码 4
返回 valid=false、identityIssueCount=2、errors=2，首项为 duplicate-stable-id，
objectKind=relation、path=scenarios[0].relations[0].id、idCount=2。四条既有 Relation 行为结果
仍单独计入 semanticIssueCount，不与结构问题混淆。

调用方按既有 relations 查询取得第二项 relationRef 并改为 relation-req-ack-follow-up；
dry-run 与实际写入结果 SHA 同为
b9dbd835ba98009ac6c2b43960229b68e496408c4680736eb7eb0bd35e775d2d。重新 validate 返回
退出码 0、valid=true、identityIssueCount=0、errors=0。损坏源工程 SHA 保持
b634698c888de4912bb453347b20e938abeee64fe4a46770ca85d5433526abc7。
```

长期 Goal 保持 active；第 440 轮为已验收的阶段可用节点。

## 持续迭代 441–450：CLI 非退化后置校验门禁

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

逐轮交付：

1. 第 441 轮：对含重复 Relation ID 的工程执行无关 Lane 重命名，复现旧 `apply` 返回退出码 0、
   `ok=true`，同时内嵌 `validation.valid=false` 的假成功。
2. 第 442 轮：在公共 `applyAutomationBatch()` 内建立后置校验门禁，失败时不返回候选 Project，
   使嵌入宿主与独立 CLI 共享同一安全边界。
3. 第 443 轮：对完整源工程与候选工程同时校验，而非只检查所选 Scenario；公开源/候选 error
   和身份问题计数，区分健康编辑、完整修复、渐进修复与无效候选。
4. 第 444 轮：为 error 建立由代码、对象类型、稳定 ID、Scenario/Lane、tick、Event 和 Relation
   上下文组成的多重指纹；数组重排不制造假新增，但用一种错误替换另一种错误会被识别。
5. 第 445 轮：允许损坏工程保存无新增 error、身份问题不增加且确实减少错误的渐进修复；操作明确
   报告身份、几何或端点修复时，也可在 error 集合完全不退化的前提下分步恢复。
6. 第 446 轮：新增 `validationGuard`，公开 accepted、reason、source/candidate valid、
   新增/消除 error 数和显式修复标志；非改进候选返回 `post-validation-failed`。
7. 第 447 轮：CLI 将核心结构化拒绝报告写入 stderr、返回退出码 4，并明确 `written=false`；
   stdout 为空、无候选工程、无输出文件，源 SHA 保持不变。
8. 第 448 轮：门禁捕获旧验收中 40 ns 源端点到 110 ns 目标端点违反 10–40 ns 约束的隐藏问题；
   修复用户旅程为 80 ns 的有效边沿，并增加“显式修复产生新错误仍拒绝”的核心断言。
9. 第 449 轮：新增 `wave-cli-post-validation-guard-contract-smoke`，覆盖双重身份损坏、无关编辑
   拒绝、Relation 渐进修复、剩余 Marker 定位、最终修复、SHA 一致及源文件只读。
10. 第 450 轮：capabilities 新增 `nonRegressiveApplyValidation=true`，更新自动化文档、
    README、PLAN 和 GOAL，完成三配置全量 offscreen 回归及开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
54/54 tests passed
Total Test time: 24.49 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
54/54 tests passed
Total Test time: 29.84 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
54/54 tests passed
Total Test time: 23.66 sec

Targeted: wave-core-tests,
          wave-cli-relation-repair-contract-smoke,
          wave-cli-relation-identity-repair-contract-smoke,
          wave-cli-marker-repair-contract-smoke,
          wave-cli-identity-validation-contract-smoke and
          wave-cli-post-validation-guard-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 对同时含两个 relation-req-ack 和两个 marker-transfer 的工程提交无关 reset 重命名。
旧版返回退出码 0、ok=true 且候选 validation.valid=false；当前版本返回退出码 4、
error.code=post-validation-failed、reason=invalid-candidate、错误数 4→4、newErrorCount=0，
stdout 为空、written=false 且不生成输出。

调用方查询第二项 relationRef 并修复身份。门禁报告 reason=progressive-repair、错误数 4→2、
newErrorCount=0，允许保存恢复进度；dry-run 与文件 SHA 同为
584183fda4c07b4484d97f498cd7f53ff2475fbab3db9230b59a56561e8dbe78。validate 精确确认
剩余问题仅为 Marker 身份。随后查询第二项 markerRef 并改为 marker-transfer-imported，报告
reason=valid-candidate、错误数 2→0，dry-run 与文件 SHA 同为
69fe8be3cdc148246b9fe9799df2c6e3f463e8b4711ea65b38064e369d8ca3b2；最终 validate 返回
退出码 0、valid=true、0 error。损坏源工程 SHA 始终为
e9a263322e8a4cac0baed4c8d0f9eeb776bf73b0fcd0586fef880f81b1e47291。
```

长期 Goal 保持 active；第 450 轮为已验收的阶段可用节点。

## 持续迭代 451–460：CLI Marker 完整性校验

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

逐轮交付：

1. 第 451 轮：以两个大小写等价 Marker 名称和一个越界 Point 构造损坏工程，复现 `markers`
   报告 5 项问题而旧 `validate` 仍返回退出码 0、`valid=true` 的假绿。
2. 第 452 轮：提取统一 Marker 完整性规则，名称按 trim 后大小写折叠统计；几何统一检查
   Scenario 起止、反向区间和 Point 起止一致性。
3. 第 453 轮：将空名称、重复名称、起点越界、反向终点、终点越界和 Point/范围冲突映射为
   六个稳定问题码，避免查询与校验各自维护判断。
4. 第 454 轮：为每项问题加入 `objectKind=marker`、Scenario/Marker 索引、精确属性 `path`、
   Marker ID、名称计数、起止 tick 和用途类型，调用方无需解析自然语言。
5. 第 455 轮：`validate` 新增 `markerIssueCount` 和 `markerSummary`，区分受影响 Marker、
   名称问题与几何问题；全部 Marker 问题计入 semantic issue 和 error。
6. 第 456 轮：`capabilities` 新增 `markerIntegrityValidation=true`，嵌入 API 与独立 CLI
   共享同一能力声明和校验结果。
7. 第 457 轮：将 Marker 完整性接入工程级 apply 后置门禁；无关编辑保留既有损坏时退出 4、
   不返回候选工程、不写文件，普通名称修复也可按 error 下降作为渐进恢复。
8. 第 458 轮：新增核心契约，以七个唯一 ID Marker 覆盖 7 项名称/几何问题、精确路径、
   查询/校验计数一致、无关编辑阻断及 7→5 名称修复。
9. 第 459 轮：新增 `wave-cli-marker-validation-contract-smoke`，覆盖能力发现、5 项磁盘损坏、
   只读查询/校验、5→2→0 两阶段修复、dry-run/写入 SHA 一致和源文件不变。
10. 第 460 轮：更新 README、PLAN、GOAL 与 CLI 文档，完成默认、Qt Creator Debug/Release
    三配置全量 offscreen 回归及开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
55/55 tests passed
Total Test time: 25.12 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
55/55 tests passed
Total Test time: 30.13 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
55/55 tests passed
Total Test time: 23.65 sec

Core: 32/32 tests passed
Targeted: wave-core-tests,
          wave-cli-markers-contract-smoke,
          wave-cli-marker-repair-contract-smoke,
          wave-cli-marker-validation-contract-smoke,
          wave-cli-identity-validation-contract-smoke and
          wave-cli-post-validation-guard-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 查询含两个大小写等价名称和一个越界 Point 的工程。markers 返回 issueCount=5、
validCount=0；validate 同步返回退出码 4、markerIssueCount=5，而不再错误报告 valid=true。

对同一损坏工程提交无关 Lane 重命名，门禁返回退出码 4、reason=invalid-candidate、
错误数 5→5、written=false，stdout 为空且没有输出文件。查询 marker-broken 的 markerRef，
把它恢复为 120 ns 点后，错误数 5→2、reason=progressive-repair；dry-run 与文件 SHA 同为
65b376b37adf12722a44099fa6179e446619c5302dfe50e47574b4019e6912a9。

随后查询 marker-duplicate-name 的 markerRef 并改为唯一名称，错误数 2→0、
reason=valid-candidate；dry-run 与文件 SHA 同为
16f5f3c5973c4b130d283df02b83ae34afe6f54cc9131662f33d3cf4f3bfe932。最终 validate 返回
退出码 0、markerIssueCount=0。示例源工程 SHA 始终为
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 始终为
5ccea02cb613245c91f5f050f63ed5f9ebfad057a0529ad7c623006cb8754e6e。
```

长期 Goal 保持 active；第 460 轮为已验收的阶段可用节点。

## 持续迭代 461–470：CLI 可操作校验引用

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

逐轮交付：

1. 第 461 轮：从第 460 轮真实恢复链路反向审计，确认 `validate` 已给出 Marker 精确数组路径，
   调用方仍必须再执行 `markers` 才能取得 `markerRef`，增加一次往返和快照变化风险。
2. 第 462 轮：为 Marker 名称、几何和稳定身份问题直接加入与当前完整快照绑定的 `markerRef`，
   同一 Marker 的多项诊断共享同一引用。
3. 第 463 轮：为 Relation 空/重复稳定身份问题按精确数组索引生成 `relationRef`；同 ID 的两个
   Relation 得到不同引用，不要求调用方以歧义 ID 猜测目标。
4. 第 464 轮：唯一 Relation 的缺失端点、延迟违例、时钟和 condition 等语义问题也直接返回
   `relationRef`，允许从一次 validate 进入端点修复。
5. 第 465 轮：每个可操作问题加入 `repairOperations`，明确 Marker/Relation 可使用的 update
   与 delete operation，不要求宿主从对象类型推断命令。
6. 第 466 轮：排除 `relation-satisfied` 和 `relation-not-applicable` 信息项，避免健康或条件
   未触发状态被误呈现为待修复任务。
7. 第 467 轮：顶层新增 `repairableIssueCount` 和按引用去重的 `repairTargetCount`；多问题同
   对象保留完整诊断，但不会夸大用户必须处理的对象数。
8. 第 468 轮：capabilities 新增 `actionableValidationReferences=true`；核心契约覆盖健康零目标、
   重复 Relation、Marker 多问题、缺失 Relation 端点、信息项隔离和直接写回。
9. 第 469 轮：新增 `wave-cli-actionable-validation-contract-smoke`；磁盘用户旅程只使用一次
   validate 取得三个引用，免 `relations`/`markers` 查询完成 5→0 联合修复，并校验 SHA/只读。
10. 第 470 轮：更新 README、PLAN、GOAL 与 CLI 文档，完成默认、Qt Creator Debug/Release
    三配置全量 offscreen 回归及开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
56/56 tests passed
Total Test time: 25.46 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
56/56 tests passed
Total Test time: 30.84 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
56/56 tests passed
Total Test time: 23.98 sec

Core: 33/33 tests passed
Targeted: wave-core-tests,
          wave-cli-relations-contract-smoke,
          wave-cli-relation-repair-contract-smoke,
          wave-cli-relation-identity-repair-contract-smoke,
          wave-cli-markers-contract-smoke,
          wave-cli-marker-repair-contract-smoke,
          wave-cli-marker-validation-contract-smoke,
          wave-cli-identity-validation-contract-smoke,
          wave-cli-post-validation-guard-contract-smoke and
          wave-cli-actionable-validation-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 对含两个同 ID Relation 和一个越界 Point 的工程只执行一次 validate。命令返回退出码 4、
identityIssueCount=2、markerIssueCount=3、repairableIssueCount=5、repairTargetCount=3；
两个 Relation 问题携带不同 relationRef，三个 Marker 几何问题共享同一 markerRef。

调用方直接把第二个 relationRef 和 markerRef 写入同一 operations 批次，不执行 relations 或
markers 查询。dry-run 报告错误数 5→0、reason=valid-candidate，未生成输出文件；实际写入 SHA
与预演同为 7977e09a583e3af88a197143d4b069b453d2f533539f2e008ce90f604d1a7857。
最终 validate 返回退出码 0、repairableIssueCount=0、repairTargetCount=0。示例源工程 SHA
始终为 d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA
始终为 84c7568bdd03e32ba0b00696bc2b2d9df466ede60488cf7412f12749e0e4283e。
```

长期 Goal 保持 active；第 470 轮为已验收的阶段可用节点。

## 持续迭代 471–480：CLI 结构化 Relation 语义诊断

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

逐轮交付：

1. 第 471 轮：审计缺失目标端的 `validate` 输出，确认虽已有 `relationRef`，仍无对象索引和属性
   路径，通用 lane/tick/event 字段实际描述源端，容易被误认为损坏目标。
2. 第 472 轮：提取统一 `relationAutomationObject`；`relations` 查询与 validate 诊断共用同一
   紧凑快照，避免端点状态、实际延迟和健康判定发生漂移。
3. 第 473 轮：Relation 语义问题补齐 `objectKind=relation`、Scenario/Relation 索引，以及精确
   首要 `path` 和全部相关 `paths`。
4. 第 474 轮：新增 `repairProperties`，以 stable-id、source/target-endpoint、delay-range、
   clock、condition 和 scenario-duration 明确用户应修改的逻辑属性。
5. 第 475 轮：内嵌 `relationContext`，公开两端 Lane/名称/精确时间/边沿/值/候选与问题码、
   声明和实际延迟、时序结果、时钟、condition、严重性及 endpointsReady。
6. 第 476 轮：快照保持紧凑边界，不返回 Relation 内部 Event ID 或扩展载荷；目标缺失时仍完整
   公开健康源端，而不是用源端通用字段替代目标诊断。
7. 第 477 轮：capabilities 新增 `structuredRelationValidation=true`；身份诊断也复用同一对象
   上下文和路径契约。
8. 第 478 轮：核心类型矩阵覆盖缺失目标、实际延迟越界、非法 condition、时钟不匹配、观察窗口
   不足，并断言 validate 的 relationContext 与 relations 查询项完全一致。
9. 第 479 轮：新增 `wave-cli-relation-validation-context-contract-smoke`，从磁盘损坏工程仅用
   validate 上下文和引用恢复目标端，验证 dry-run/写入 SHA、最终校验及源文件只读。
10. 第 480 轮：更新 README、PLAN、GOAL 与 CLI 文档，完成默认、Qt Creator Debug/Release
    三配置全量 offscreen 回归及开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
57/57 tests passed
Total Test time: 25.20 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
57/57 tests passed
Total Test time: 30.81 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
57/57 tests passed
Total Test time: 24.31 sec

Core: 34/34 tests passed
Targeted: wave-core-tests,
          wave-cli-relations-contract-smoke,
          wave-cli-relation-repair-contract-smoke,
          wave-cli-relation-identity-repair-contract-smoke,
          wave-cli-identity-validation-contract-smoke,
          wave-cli-post-validation-guard-contract-smoke,
          wave-cli-actionable-validation-contract-smoke and
          wave-cli-relation-validation-context-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 对 relation-req-ack 目标 Event 已丢失的工程只执行一次 validate。命令返回退出码 4，
问题精确指向 scenarios[0].relations[0].targetEventId，repairProperties 为 target-endpoint；
relationContext 同时说明源端 req@80 ns 是 rising、值为 1 且可用，目标 issue=missing-event，
声明延迟为 10000..40000 tick、实际延迟为空。

调用方直接使用问题中的 relationRef，把目标替换为 ack@110 ns，不执行 relations 查询。
dry-run 报告错误数 1→0、reason=valid-candidate，未生成输出文件；实际写入 SHA 与预演同为
2391211fe2b5e75d77f267bdcc223d05391bc868d944ac4d986ebe159962affc。最终 validate 返回
退出码 0、0 error。示例源工程 SHA 始终为
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 始终为
4f55c0524b17099927fd8b8c74d13a613aaca11b44e9ced4eab100dfe0c397b9。
```

长期 Goal 保持 active；第 480 轮为已验收的阶段可用节点。

## 第 481–490 轮：结构化波形校验与直接恢复

目标：让外部生成或导入工程中的非法 Bus/Enum 值和未定义区间，仅凭一次 `validate` 就能定位
并调用现有波形 operation 修复；不再要求调用方解析英文 message 或额外查询完整波形。

逐轮交付：

1. 第 481 轮：审计 `invalid-bus-value` 与 `undefined-region`，确认旧输出只有 Lane、起点和
   message，缺少 Segment、结束位置及属性路径。
2. 第 482 轮：按唯一 Lane 身份和校验 tick 解析真实 Lane/Segment 索引；歧义身份不生成可能
   误改对象的修复入口。
3. 第 483 轮：非法值补齐 `objectKind=segment`、Scenario/Lane/Segment 索引及精确 `.value`
   路径；未定义区间补齐 Lane 索引与 `.segments` 路径。
4. 第 484 轮：新增紧凑 `waveformContext`，公开 Lane 名称/类型/位宽/signedness/radix、
   Segment ID/原值和格式化起止时间，不复制整条波形、Enum 映射或扩展载荷。
5. 第 485 轮：新增半开 `repairRange`、`repairProperties` 与 `repairOperations`；非法值可
   `set-range`/`clear-range`，空洞可直接 `set-range`。
6. 第 486 轮：修复磁盘用户路径的前置阻塞；读取器保留非法值原文交给语义校验，同时继续拒绝
   负时间、空区间和 Segment 重叠等结构损坏。
7. 第 487 轮：capabilities 新增 `structuredWaveformValidation=true`；修复目标计数按
   Lane+半开范围去重，并与既有 Marker/Relation 引用共存。
8. 第 488 轮：核心矩阵覆盖 8-bit Bus 溢出、Enum 未知符号、内部空洞及仅使用返回范围的直接
   修复，断言源 Project 保持只读。
9. 第 489 轮：新增 `wave-cli-waveform-validation-context-contract-smoke`，从磁盘构造非法
   Bus 工程，覆盖读取、定位、dry-run、写入、最终校验、SHA 一致和源文件只读。
10. 第 490 轮：更新 README、PLAN、GOAL 与 CLI 文档，完成默认、Qt Creator Debug/Release
    三配置全量 offscreen 回归及开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
58/58 tests passed
Total Test time: 25.91 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
58/58 tests passed
Total Test time: 31.45 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
58/58 tests passed
Total Test time: 24.37 sec

Core: 35/35 tests passed
Targeted: wave-core-tests,
          wave-cli-actionable-validation-contract-smoke,
          wave-cli-relation-validation-context-contract-smoke and
          wave-cli-waveform-validation-context-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 对外部工程中的 data[7:0]=0x1ff 只执行一次 validate。命令返回退出码 4，问题精确指向
scenarios[0].lanes[5].segments[1].value；waveformContext 说明目标是 8-bit Bus 的
segment-data-payload，原值为 0x1ff，半开范围为 80–150 ns，允许 set-range/clear-range。

调用方不执行 sample 或 window 查询，直接把 repairRange 原样用于 set-range 并恢复 0x35。
dry-run 报告错误数 1→0、reason=valid-candidate，未生成输出文件；实际写入 SHA 与预演同为
8d98d67b0d9b45d01f6885c95df41c8daed26d4004027412989d839998c5f9ac。最终 validate 返回
退出码 0、valid=true、0 error。示例源工程 SHA 始终为
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 始终为
177c0a67e2c394ab94a84e1694dbedc4c638c1ca429576a12de6c3a1ec382133。
```

长期 Goal 保持 active；第 490 轮为已验收的阶段可用节点。

## 第 491–500 轮：结构化 Event 校验与受限恢复

目标：让外部生成或导入工程中的 Event Lane 丢失和时间越界，仅凭一次 `validate` 就能定位并
选择安全恢复；删除能力仅服务于明确的校验问题，不成为绕过波形编辑语义的通用 Event 删除入口。

逐轮交付：

1. 第 491 轮：审计 `missing-lane` 与 `event-outside-scenario`，确认旧输出只有 Event ID、
   Lane 和 tick，缺少对象索引、精确路径及 Event 行为上下文。
2. 第 492 轮：统一 Event 有效时间为 Scenario 半开区间 `[0, End)`；位于 End 的 Event 不再被
   错误接受，与新增 Event 和 Segment 的时间语义一致。
3. 第 493 轮：按唯一 Event 稳定 ID 解析真实数组索引；重复或空 ID 不生成可能误改对象的恢复入口。
4. 第 494 轮：新增紧凑 `eventContext`，公开 Lane/Clock 解析状态、格式化时刻、动作、值、预期
   结果及波形关联健康度，但不暴露易失的 Segment ID 或扩展载荷。
5. 第 495 轮：补齐 `objectKind=event`、Scenario/Event/Lane 索引、精确 `.laneId`/`.timeTick`
   路径、`repairProperties` 和 `repairOperations`；可表示的正向越界返回最小 End。
6. 第 496 轮：最大 tick 和负时间只提供删除，不生成溢出或无效的 End 建议；Scenario End 边界
   可直接复用返回的 `minimumDurationTick` 修复。
7. 第 497 轮：新增 `delete-event`，复用领域命令同步清理 Relation 与有效波形依赖，并报告
   Event/Relation/Segment 删除及 Event 重建数量。
8. 第 498 轮：将删除收紧为 validation-directed operation；健康 Event 即使 ID 已知也原子拒绝，
   重复 ID 继续以歧义错误拒绝；capabilities 发布 `structuredEventValidation` 与 `eventDeletion`。
9. 第 499 轮：核心矩阵扩展为 36 项，并新增
   `wave-cli-event-validation-context-contract-smoke`，覆盖磁盘损坏、直接恢复、健康/歧义拒绝、
   dry-run/写入 SHA 一致和源文件只读。
10. 第 500 轮：更新 README、PLAN、GOAL、CLI 与 Qt Creator 文档，完成默认、Qt Creator
    Debug/Release 三配置全量 offscreen 回归及开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
59/59 tests passed
Total Test time: 28.03 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
59/59 tests passed
Total Test time: 34.01 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
59/59 tests passed
Total Test time: 25.37 sec

Core: 36/36 tests passed
Capabilities: 11 commands, 24 operations,
              structuredEventValidation=true, eventDeletion=true
Targeted: wave-core-tests,
          wave-cli-capabilities-group-contract-smoke,
          wave-cli-duplicate-signal-contract-smoke,
          wave-cli-actionable-validation-contract-smoke,
          wave-cli-relation-validation-context-contract-smoke,
          wave-cli-waveform-validation-context-contract-smoke and
          wave-cli-event-validation-context-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 将磁盘示例中的 event-ack-high Lane 改为 lane-missing 后只执行一次 validate。命令返回
退出码 4，问题精确指向 scenarios[0].events[9].laneId；eventContext 说明目标位于 110 ns，
action=expect、value=1、Lane 未解析、波形关联 Segment 未解析，并直接给出 delete-event。

调用方不查询内部 Event 或 Segment 数组，直接使用返回的 eventId。dry-run 报告错误数 1→0、
reason=valid-candidate，删除 1 个 Event、1 个 Relation、0 个 Segment，且不生成输出文件；
实际写入 SHA 与预演同为
151a4608d11f4578b88132f97edb352a0d74ac35b96ba810aa33c9a700d6449b。最终 validate 返回
退出码 0、valid=true、0 error，并保留示例工程原有 2 个可修复 warning。示例源工程 SHA
始终为 d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA
始终为 dad522496d48fd204f24ae1407765dede0843a7e414c7062f0d73ad701afd6e9。

对健康 event-ack-high 使用同一删除批次，以及对重复 Event ID 使用该批次，均返回退出码 4、
stdout 为空且不生成输出；源文件保持不变。
```

长期 Goal 保持 active；第 500 轮为已验收的阶段可用节点。

## 第 501–510 轮：Event–Segment 关联一致性与原位恢复

目标：消除 Event 仍指向既有 Segment、但 Lane、时刻或值已与其波形目标脱节时的校验假绿；让
调用方通过一次 `validate` 直接、安全地恢复关联，同时保留 Event 稳定身份、Relation 意图与波形。

逐轮交付：

1. 第 501 轮：审计 waveform-linked Event，复现 `linkedSegmentId` 仍有效但 Lane、tick、value
   已漂移时旧校验返回 valid 的假绿，以及删除 Event 会丢失 Relation 意图的问题。
2. 第 502 轮：新增 `event-waveform-mismatch`；仅在 Lane、Segment、Event 身份作用域稳定时执行
   语义关联检查，结构身份损坏继续要求先修复身份。
3. 第 503 轮：统一解析唯一关联 Segment、实际所属 Lane 和同 Segment Event 数；拒绝缺失目标、
   重复关联、Clock/Group 目标及不支持的 Event action。
4. 第 504 轮：`eventContext` 增加关联目标是否解析、是否一致、是否可恢复、Lane 类型、Segment
   索引、半开范围和值；不暴露内部 `linkedSegmentId`。
5. 第 505 轮：为 Lane、时刻、值和关联属性返回精确 `paths` 与对应 `repairProperties`；问题只
   涉及哪些字段，就只公开哪些字段。
6. 第 506 轮：新增领域命令 `RepairWaveformEventLinkCommand`，从唯一有效 Segment 原位恢复
   Event 的 Lane/tick/value，保留 ID、action、metadata、Relation 与波形，并支持精确 Undo/Redo。
7. 第 507 轮：新增 validation-directed `repair-event-link`；健康 Event 不能作为通用重写入口，
   同时报告更新/新增/删除的 Event、Relation、Segment 数量。
8. 第 508 轮：Lane 丢失但关联 Segment 可唯一定位时优先提供非破坏修复，仍保留显式删除作为备选；
   关联不唯一或目标非法时不提供不安全修复。
9. 第 509 轮：核心矩阵扩展为 37 项，并新增
   `wave-cli-event-link-repair-contract-smoke`，覆盖磁盘误配、Relation 保留、dry-run/写入 SHA
   一致、健康对象拒绝、重复 Event ID 拒绝及失败无输出。
10. 第 510 轮：capabilities 发布 `eventLinkRepair=true`，operation 总数更新为 25；更新 README、
    PLAN、GOAL、CLI 与 Qt Creator 文档，完成三配置全量 offscreen 回归及开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
60/60 tests passed
Total Test time: 27.83 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
60/60 tests passed
Total Test time: 35.03 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
60/60 tests passed
Total Test time: 25.20 sec

Core: 37/37 tests passed
Capabilities: 11 commands, 25 operations,
              eventLinkRepair=true
Targeted: wave-core-tests,
          wave-cli-event-validation-context-contract-smoke and
          wave-cli-event-link-repair-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 将磁盘示例中的 event-ack-high 改为 reset Lane、111 ns、值 0，同时保留它原有的关联
Segment。一次 validate 以退出码 4 返回
scenarios[0].events[9].laneId、scenarios[0].events[9].timeTick 和
scenarios[0].events[9].value；eventContext 说明真实目标为 ack 的 110–150 ns、值 1 Segment，
并直接给出 repair-event-link。

调用方不查询内部 Segment ID，直接按原 Event ID 执行恢复。门禁报告错误数 1→0、更新 1 个
Event，新增或删除 Event、Relation、Segment 均为 0；原 Relation 继续引用同一 Event。
dry-run 不写文件，结果 SHA 与实际文件同为
2391211fe2b5e75d77f267bdcc223d05391bc868d944ac4d986ebe159962affc。最终 validate 返回
退出码 0、valid=true、0 error。示例源 SHA 始终为
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 始终为
d433922a8f57bdb6397c11c2e96a44ba13c79c77d19f21458ba5cf6e4f16bb85。

对健康 event-ack-high 及重复 Event ID 工程执行同一修复批次，均返回退出码 4、stdout 为空、
不生成输出且保持源文件不变；缺失关联 Segment 的问题也不宣称可原位恢复。
```

长期 Goal 保持 active；第 510 轮为已验收的阶段可用节点。

## 第 511–520 轮：Event cycle 一致性与非破坏恢复

目标：消除 Event 的逻辑 cycle 与当前时刻、Clock 活动边沿脱节时仍被判定有效的风险；恢复只
移除陈旧周期约束，不移动当前波形、不删除 Relation，也不把 validation-directed operation
扩展成通用 Event 编辑入口。

逐轮交付：

1. 第 511 轮：复现 110 ns Event 携带 `cycle=8`、10 ns Clock 时旧 `validate` 仍返回 valid，
   后续修改 Clock 会把 Event 和波形意外重定时到陈旧周期的问题。
2. 第 512 轮：新增 `event-cycle-mismatch`，仅对显式携带 cycle 的 Event 执行语义校验；无 cycle
   Event 保持现有固定时刻语义。
3. 第 513 轮：cycle 必须非负，并能通过 Event 显式 ClockDomain 或 Lane 推断 ClockDomain 唯一
   解析；时钟缺失、歧义和整数溢出均成为可诊断错误。
4. 第 514 轮：以 Clock 的 active edge 精确计算预期 tick；实际 `timeTick` 必须完全一致，不做
   近似吸附或浮点换算。
5. 第 515 轮：`eventContext` 增加 cycle 是否存在、时钟来源、有效 ClockDomain、匹配数、预期
   tick/格式化时间、当前一致性及可恢复状态。
6. 第 516 轮：问题返回精确 `.cycle` 路径；与当前时刻冲突时同时返回 `.timeTick`，但恢复属性
   只声明 `event-cycle`，不默认提供破坏性的 Event 删除。
7. 第 517 轮：新增 `ClearEventCycleCommand` 与 validation-directed `clear-event-cycle`，仅清除
   cycle，并报告 Event 更新数；稳定 ID、时刻、ClockDomain、Relation、Segment 与扩展载荷不变。
8. 第 518 轮：`RepairWaveformEventLinkCommand` 接入 Clock 语义；目标时刻改变后保留仍正确的
   cycle，清除已陈旧的 cycle，避免关联修复被后置校验拒绝或埋下下一次时钟跳变。
9. 第 519 轮：核心矩阵扩展为 38 项，新增
   `wave-cli-event-cycle-repair-contract-smoke`，覆盖磁盘损坏、预期时刻、健康/歧义拒绝、
   Relation 保留、dry-run/写入 SHA 一致和源文件只读。
10. 第 520 轮：capabilities 发布 `eventCycleRepair=true`，operation 总数更新为 26；更新
    README、PLAN、GOAL、CLI 与 Qt Creator 文档，完成三配置全量 offscreen 回归及双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
61/61 tests passed
Total Test time: 28.10 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
61/61 tests passed
Total Test time: 34.31 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
61/61 tests passed
Total Test time: 25.63 sec

Core: 38/38 tests passed
Capabilities: 11 commands, 26 operations,
              eventCycleRepair=true
Targeted: wave-core-tests,
          wave-cli-event-link-repair-contract-smoke and
          wave-cli-event-cycle-repair-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 给磁盘示例中当前位于 110 ns 的 event-ack-high 写入 cycle 8。一次 validate 以退出码 4
返回 scenarios[0].events[9].cycle 与 scenarios[0].events[9].timeTick；eventContext 说明
Clock 来源为 Event 的 clock-main、cycle 8 预期 80 ns、实际为 110 ns，且当前波形关联仍健康。

调用方不查询 Clock 或 Relation 数组，直接按同一 Event ID 执行 clear-event-cycle。门禁报告
错误数 1→0、更新 1 个 Event，新增/删除 Event、Relation、Segment 均为 0；Relation 查询仍得到
ack@110 ns=1。dry-run 不写文件，结果 SHA 与实际文件同为
2391211fe2b5e75d77f267bdcc223d05391bc868d944ac4d986ebe159962affc。最终 validate 返回
退出码 0、valid=true、0 error。示例源 SHA 始终为
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 始终为
d62ff724c4c63e101be03dec4f9a1be2fafa92eea9baefe7c4ab292a69be68ff。

对无 cycle 的健康 Event 和重复 Event ID 工程执行同一 operation，均返回退出码 4、stdout
为空、不生成输出且保持源文件不变。Event–Segment 关联修复的专项用例另确认：若目标时刻仍
符合 cycle 则保留；若不再符合则清除，不会把新修复替换成另一项隐藏错误。
```

长期 Goal 保持 active；第 520 轮为已验收的阶段可用节点。

## 第 521–530 轮：Event ClockDomain 引用完整性与原位恢复

目标：让 Event 的非空显式 ClockDomain 必须真实、唯一地存在；引用缺失时优先采用目标 Lane 的
唯一有效时钟，否则只在 Lane 无时钟或 Lane 不存在时清除 Event 自身的无效引用，同时保持当前
波形、时刻和 Relation 意图。

逐轮交付：

1. 第 521 轮：复现未参与 Relation、或 Relation 各端使用同一不存在 ClockDomain 时旧
   `validate` 仍可能返回 valid，后续 cycle 解释和 Clock 重定时不可预测的问题。
2. 第 522 轮：新增 `event-clock-domain-invalid`；Event 的非空 `clockDomainId` 必须在工程中
   恰好匹配一个 ClockDomain，缺失与歧义均成为 error。
3. 第 523 轮：`eventContext` 增加当前 ClockDomain 匹配数、严格有效状态、Lane ClockDomain、
   Lane 时钟匹配数和是否可安全恢复。
4. 第 524 轮：推导明确 `clockRepairAction`：Lane 时钟唯一有效时为 `use-lane-clock`；Lane 无
   时钟或 Lane 不存在时为 `clear-event-clock`；Lane 时钟也悬空或 Lane 身份歧义时为 `none`。
5. 第 525 轮：问题返回精确 `.clockDomainId` 路径、`event-clock` 恢复属性和唯一
   `repair-event-clock`；不可安全恢复的问题仍保留上下文，但不计为 repairable。
6. 第 526 轮：新增 `RepairEventClockReferenceCommand`，原位替换或清除 Event 显式 ClockDomain，
   保留 Event ID、Lane、tick、value、action、Segment、Relation 与扩展载荷，并支持精确 Undo/Redo。
7. 第 527 轮：替换后按有效时钟复核 cycle；仍落在 active edge 的 cycle 保留，陈旧、负值或无法
   解析的 cycle 清除，避免修复一个引用后留下另一项隐藏错误。
8. 第 528 轮：operation 收紧为 validation-directed；健康 Event、当前 ClockDomain 歧义、Lane
   时钟无效和 Event ID 歧义均原子拒绝，不产生部分结果。
9. 第 529 轮：核心矩阵扩展为 39 项，新增
   `wave-cli-event-clock-repair-contract-smoke`，覆盖磁盘损坏、Lane 回退、Relation 冲突同步消除、
   healthy/ambiguous 拒绝、dry-run/写入 SHA 一致和源文件只读。
10. 第 530 轮：capabilities 发布 `eventClockRepair=true`，operation 总数更新为 27；更新
    README、PLAN、GOAL、CLI 与 Qt Creator 文档，完成三配置全量 offscreen 回归及双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
62/62 tests passed
Total Test time: 29.14 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
62/62 tests passed
Total Test time: 34.34 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
62/62 tests passed
Total Test time: 25.80 sec

Core: 39/39 tests passed
Capabilities: 11 commands, 27 operations,
              eventClockRepair=true
Targeted: wave-core-tests,
          wave-cli-event-cycle-repair-contract-smoke and
          wave-cli-event-clock-repair-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 将磁盘示例中 event-ack-high 的 ClockDomain 改为 clock-missing。一次 validate 以退出码 4
返回 scenarios[0].events[9].clockDomainId；eventContext 说明当前匹配数为 0、Lane ack 的
clock-main 匹配数为 1，恢复动作是 use-lane-clock，替换目标为 clock-main。

调用方不查询 Clock 或 Relation 数组，直接按同一 Event ID 执行 repair-event-clock。门禁同时
消除 Event 引用错误和它引发的 Relation 时钟冲突，错误数 2→0；只更新 1 个 Event，新增/删除
Event、Relation、Segment 均为 0，Relation 查询仍得到 ack@110 ns=1。dry-run 不写文件，结果
SHA 与实际文件同为
2391211fe2b5e75d77f267bdcc223d05391bc868d944ac4d986ebe159962affc。最终 validate 返回
退出码 0、valid=true、0 error。示例源 SHA 始终为
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 始终为
27b0640ab61948d6205d460ec1c3d3cffdc5f9b7dbbca4e3cd6f644683c99bb9。

对健康 Event 和重复 Event ID 工程执行同一 operation，均返回退出码 4、stdout 为空、不生成
输出且保持源文件不变。Lane 的 ClockDomain 也悬空时，validate 仍提供完整上下文，但不宣称可
安全恢复。
```

长期 Goal 保持 active；第 530 轮为已验收的阶段可用节点。

## 第 531–540 轮：Lane ClockDomain 引用完整性与安全恢复

目标：让 Lane 自身的 ClockDomain 引用在进入采样、同步编辑和自动化流程前即可被准确诊断；
普通 Lane 的悬空引用可无损清除，Clock Lane 只在替换目标唯一时自动恢复，不在多时钟工程中
猜测用户意图。

逐轮交付：

1. 第 531 轮：复现普通 Lane 引用已删除 ClockDomain 时，波形仍可显示但 Relation、cycle 与同步
   编辑对其时钟解释不一致的问题。
2. 第 532 轮：新增 `lane-clock-domain-invalid`；Clock Lane 必须引用唯一存在的 ClockDomain，
   普通 Lane 的非空引用必须唯一有效，Group 不得携带 ClockDomain。
3. 第 533 轮：校验问题返回 Scenario/Lane 索引、精确 `.clockDomainId` 路径、`lane-clock`
   修复属性和结构化 `laneContext`。
4. 第 534 轮：上下文公开 Lane 身份/类型、当前时钟匹配数、工程 ClockDomain 数、明确
   `clear-lane-clock`/`use-only-project-clock` 动作、替换值和是否可安全恢复。
5. 第 535 轮：恢复策略限定为普通 Lane 清除缺失引用、Group 清除非法引用；Clock Lane 仅在
   工程恰有一个非空稳定 ID 的 ClockDomain 时采用它。
6. 第 536 轮：新增 `RepairLaneClockReferenceCommand`，保留 Lane 波形、Event、Relation、
   Segment 与扩展载荷，并支持精确 Undo/Redo。
7. 第 537 轮：继承 Lane 时钟的 cycle Event 在修复后重新核对；仍一致的 cycle 保留，已无法解释
   的 cycle 清除，并单独报告更新数量。
8. 第 538 轮：新增 validation-directed `repair-lane-clock`；健康 Lane、多时钟 Clock Lane、
   歧义 ClockDomain 和重复 Lane ID 均原子拒绝，不产生部分结果。
9. 第 539 轮：核心矩阵扩展为 40 项，新增
   `wave-cli-lane-clock-repair-contract-smoke`，覆盖磁盘损坏、精确路径、健康/歧义拒绝、
   Relation 端点保留、dry-run/写入 SHA 一致和源文件只读。
10. 第 540 轮：capabilities 发布 `laneClockRepair=true`，operation 总数更新为 28；更新
    README、PLAN、GOAL、CLI 与 Qt Creator 文档，完成三配置全量 offscreen 回归及双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
63/63 tests passed
Total Test time: 29.20 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
63/63 tests passed
Total Test time: 35.55 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
63/63 tests passed
Total Test time: 26.53 sec

Core: 40/40 tests passed
Capabilities: 11 commands, 28 operations,
              laneClockRepair=true
Targeted: wave-core-tests and
          wave-cli-lane-clock-repair-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 将磁盘示例中 Lane ack 的 ClockDomain 改为 clock-missing，而 Event、Relation 和波形保持
原状。一次 validate 以退出码 4 返回 scenarios[0].lanes[4].clockDomainId；laneContext 说明
当前匹配数为 0、恢复动作是 clear-lane-clock，并只提供 repair-lane-clock。

调用方不查询内部 Clock/Event/Relation 数组，直接按 Lane ID 执行恢复。门禁报告错误数 1→0；
只更新 1 个 Lane，不更新或删除 Event，不删除 Relation/Segment，Relation 查询仍得到
ack@110 ns=1。dry-run 不写文件，结果 SHA 与实际文件同为
b531034fc14e8066fade31a9807ea3e8f38476d2dec9eeda4c60f6e7b05f62c0。最终 validate 返回
退出码 0、valid=true、0 error。示例源 SHA 始终为
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 始终为
d2763dd0ea7bb5b7bf1667668df3303dc5293bc454156d77f768c0f4194d400b。

对健康 Lane 和重复 Lane ID 工程执行同一 operation，均返回退出码 4、stdout 为空、不生成
输出且保持源文件不变。Clock Lane 缺失引用时，仅在工程只有一个 ClockDomain 时采用它；存在
两个候选时仍返回完整诊断，但不宣称可安全恢复。继承悬空 Lane 时钟的陈旧 cycle 会清除并计数，
当前波形时刻不移动。
```

长期 Goal 保持 active；第 540 轮为已验收的阶段可用节点。

## 第 541–550 轮：Lane Group 引用完整性与无损解除

目标：消除工程可加载、但信号被放入不存在或错误 Group 后只能显示“未解析分组”的状态；恢复
只移除无效组织元数据，不改变波形、时钟、Event 或 Relation，也不猜测替代 Group。

逐轮交付：

1. 第 541 轮：复现 Lane `groupId` 缺失、指向普通 Lane、Group 嵌套/自引用及目标 ID 歧义时，
   旧 `validate` 仍返回有效、画布与查询只能被动显示未解析分组的问题。
2. 第 542 轮：新增 `lane-group-reference-invalid`；非空 `groupId` 必须唯一指向同一 Scenario
   中另一条 Group Lane，Group 本身不得携带 `groupId`。
3. 第 543 轮：问题返回 Scenario/Lane 索引、精确 `.groupId` 路径、`lane-group` 修复属性和
   结构化 `laneContext`。
4. 第 544 轮：上下文公开当前 Group ID、目标 ID 匹配数、其中 Group Lane 数、唯一目标的
   索引/名称/类型、自引用状态和引用有效性。
5. 第 545 轮：恢复动作固定为 `clear-lane-group`；缺失、类型错误、嵌套、自引用或目标歧义均
   解除无效分组，不按名称、类型或数组顺序选择替代目标。
6. 第 546 轮：新增 `RepairLaneGroupReferenceCommand`，只清空待修 Lane 的 `groupId`，保持其
   Segment、显示属性、ClockDomain、Event、Relation 与扩展载荷，并支持精确 Undo/Redo。
7. 第 547 轮：新增 validation-directed `repair-lane-group`；报告修复前后紧凑上下文和 Lane
   更新数，Event、Relation、Segment 变更数固定为零。
8. 第 548 轮：健康 Lane 与重复待修 Lane ID 原子拒绝；重复 Group 目标允许渐进式解除分组，
   但不会选择任一歧义目标。
9. 第 549 轮：核心矩阵扩展为 41 项，新增
   `wave-cli-lane-group-repair-contract-smoke`，覆盖磁盘损坏、精确路径、健康/歧义拒绝、
   Relation 端点保留、dry-run/写入 SHA 一致和源文件只读。
10. 第 550 轮：capabilities 发布 `laneGroupRepair=true`，operation 总数更新为 29；更新
    README、PLAN、GOAL、CLI 与 Qt Creator 文档，完成三配置全量 offscreen 回归及双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
64/64 tests passed
Total Test time: 29.11 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
64/64 tests passed
Total Test time: 35.71 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
64/64 tests passed
Total Test time: 26.68 sec

Core: 41/41 tests passed
Capabilities: 11 commands, 29 operations,
              laneGroupRepair=true
Targeted: wave-core-tests and
          wave-cli-lane-group-repair-contract-smoke passed
Desktop interaction: none
Packaging: not run
```

用户视角验收：

```text
Codex 将磁盘示例中 Lane ack 的 groupId 改为 group-missing，而 Event、Relation、ClockDomain
和波形保持原状。一次 validate 以退出码 4 返回 scenarios[0].lanes[4].groupId；
laneContext 说明目标匹配数和 Group Lane 匹配数均为 0，恢复动作是 clear-lane-group，并只提供
repair-lane-group。

调用方不查询 Group/Lane/Event/Relation 数组，直接按 Lane ID 执行恢复。门禁报告错误数 1→0；
只更新 1 个 Lane，不更新或删除 Event，不删除 Relation/Segment，Relation 查询仍得到
ack@110 ns=1。dry-run 不写文件，结果 SHA 与实际文件同为
5e9c033153daaa608d18502a5dadf647d624eb9241ecde6fc9ffb8c1f5e9f39f。最终 validate 返回
退出码 0、valid=true、0 error。示例源 SHA 始终为
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 始终为
d74c0aba9272d823c5eef80b700eb2a20287d492c1a17ede5b85dc41c2e4f7eb。

对健康 Lane 和重复 Lane ID 工程执行同一 operation，均返回退出码 4、stdout 为空、不生成
输出且保持源文件不变。目标 Group ID 重复时，操作只解除当前 Lane 的分组并报告
progressive-repair，不会选择任何一个重复 Group；指向普通 Lane、Group 嵌套和自引用也采用
相同的可预测恢复结果。
```

长期 Goal 保持 active；第 550 轮为已验收的阶段可用节点。

## 第 551–560 轮：Relation ClockDomain 引用完整性与端点派生恢复

目标：消除 Relation 显式引用已删除 ClockDomain、但无时钟端点使旧校验误判为有效的状态；
只在两端能够唯一证明共同有效时钟，或两端均明确无时钟时恢复，不修改端点、波形或时序范围，
也不在冲突或歧义数据中猜测。

逐轮交付：

1. 第 551 轮：复现 Relation 的非空 `clockDomainId` 不存在、且端点无时钟时旧 `validate`
   仍返回有效的问题；确认该悬空引用会延迟到后续 Relation 修改或周期解释阶段才暴露。
2. 第 552 轮：新增 `relation-clock-domain-invalid`；Relation 的非空 ClockDomain 必须在工程中
   恰好匹配一项，缺失与歧义均为 error，并与端点时钟冲突诊断保持独立。
3. 第 553 轮：诊断返回 Scenario/Relation 索引、精确 `.clockDomainId` 路径、`relation-clock`
   修复属性、现有 `relationContext` 和新增 `relationClockContext`。
4. 第 554 轮：上下文公开 Relation 时钟匹配数、两端 Event/Lane 匹配数、有效时钟与匹配数、
   端点时钟是否可判定、是否冲突、恢复动作和替换值。
5. 第 555 轮：恢复仅接受当前引用缺失且端点上下文均可判定；两端唯一共同有效时钟时采用
   `use-endpoint-clock`，两端均无时钟时采用 `clear-relation-clock`。
6. 第 556 轮：新增 `RepairRelationClockReferenceCommand`，只修改 Relation 的
   `clockDomainId`，保留 Relation ID、端点、延迟、condition、严重性、描述、扩展载荷及完整
   Scenario 波形，并支持精确 Undo/Redo。
7. 第 557 轮：新增 validation-directed `repair-relation-clock`；报告修复前后 Relation 与
   Clock 上下文，只计 1 个 Relation 更新，Event、Segment 和依赖删除数固定为零。
8. 第 558 轮：健康 Relation、重复 Relation ID、当前 ClockDomain 歧义、端点 ClockDomain
   悬空或两端时钟冲突均拒绝自动恢复；显式 `update-relation` 仍可供调用方人工指定意图。
9. 第 559 轮：核心矩阵扩展为 42 项，新增
   `wave-cli-relation-clock-repair-contract-smoke`，覆盖磁盘损坏、精确路径、双错误同时消除、
   健康/重复身份拒绝、端点保持、dry-run/写入 SHA 一致和源文件只读。
10. 第 560 轮：capabilities 发布 `relationClockRepair=true`，operation 总数更新为 30；更新
    README、PLAN、GOAL、CLI 与 Qt Creator 文档，完成三配置全量 offscreen 回归及双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
65/65 tests passed
Total Test time: 30.14 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
65/65 tests passed
Total Test time: 37.01 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
65/65 tests passed
Total Test time: 27.14 sec

Core: 42/42 tests passed
Capabilities: 11 commands, 30 operations,
              relationClockRepair=true
Targeted: wave-core-tests and
          wave-cli-relation-clock-repair-contract-smoke passed
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

用户视角验收：

```text
Codex 将磁盘示例 Relation relation-req-ack 的 clockDomainId 改为 clock-missing，而源/目标
Event、Lane、波形、延迟和描述保持原状。一次 validate 以退出码 4 返回
scenarios[0].relations[0].clockDomainId；relationClockContext 说明当前匹配数为 0，两端有效
时钟均为唯一 clock-main，恢复动作是 use-endpoint-clock，并只提供
repair-relation-clock。

调用方不查询内部 Clock/Event 数组，直接按 Relation ID 执行恢复。门禁同时消除悬空引用与
它引发的端点时钟冲突，错误数 2→0；只更新 1 个 Relation，Event、Segment 和 Relation 删除数
均为 0，查询仍得到 req@80 ns=1 → ack@110 ns=1。dry-run 不写文件，结果 SHA 与实际文件同为
2391211fe2b5e75d77f267bdcc223d05391bc868d944ac4d986ebe159962affc。最终 validate 返回
退出码 0、valid=true、0 error。示例源 SHA 始终为
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 始终为
8b8e76da72927363a74a41b29fe2c9d2f0d5f04b0761ffcc8ecf24dbdc850714。

对健康 Relation 和重复 Relation ID 工程执行同一 operation，均返回退出码 4、stdout 为空、
不生成输出且保持源文件不变。两端时钟冲突、上游时钟悬空或当前引用歧义时只返回诊断，不暴露
自动恢复入口；两端均无时钟时则只清除无效 Relation 时钟，不替端点创建 ClockDomain。
```

长期 Goal 保持 active；第 560 轮为已验收的阶段可用节点。

## 第 561–570 轮：Imported Trace 映射完整性与精确清理

目标：让已删除 Lane 遗留映射、空 Actual signal ID 等本地可证明的 Trace 配置损坏在 Compare
前即可被发现并恢复；只删除确定无效的映射，不读取、改写或猜测外部 VCD/CSV 信号。

逐轮交付：

1. 第 561 轮：复现 `signalMapping` 键指向已删除 Lane 或值为空时工程仍可加载，直到 Compare
   才统一退化为“未映射信号”的问题。
2. 第 562 轮：新增工程级 Trace mapping 审计；映射键必须指向任一 Scenario 中存在的非 Group
   Lane，Actual signal ID 必须非空，问题代码统一为 `trace-mapping-invalid`。
3. 第 563 轮：诊断返回 Trace/映射索引、精确映射路径、Expected Lane、Actual signal、
   问题码、`signal-mapping` 修复属性和 `repair-trace-mapping`。
4. 第 564 轮：新增 `traceContext`/`traceMappingContext`，公开 Trace ID 匹配数、Lane 匹配数、
   Actual ID 是否存在、外部信号未校验边界、恢复动作与可恢复性；顶层增加独立统计摘要。
5. 第 565 轮：完整 `inspect` 公开 Trace 索引、快照绑定 `traceRef` 和映射表；`--summary`
   继续只返回映射计数，避免常规定位读取整表。
6. 第 566 轮：新增 `RemoveTraceMappingAtIndexCommand`，只删除指定映射项，保留 Trace 路径、
   格式、偏移、其他映射与扩展载荷，并支持精确 Undo/Redo。
7. 第 567 轮：新增 validation-directed `repair-trace-mapping`；可按唯一 `traceId` 或快照
   `traceRef` 寻址，报告修复前上下文、删除数和剩余映射数。
8. 第 568 轮：健康映射、缺失映射、歧义 Trace ID 和已变化快照均原子拒绝；重复 Trace ID
   可由 `traceRef` 精确区分，但不会对外部 trace 中的新目标作任何推断。
9. 第 569 轮：核心矩阵扩展为 43 项，新增
   `wave-cli-trace-mapping-repair-contract-smoke`，覆盖磁盘损坏、完整/精简 inspect、
   healthy/stale/ambiguous 拒绝、dry-run/写入 SHA 一致和源文件只读。
10. 第 570 轮：capabilities 发布 `traceMappingValidation`、`traceMappingRepair` 与
    `traceRepairReference`，operation 总数更新为 31；更新 README、PLAN、GOAL、工程格式、
    CLI 与 Qt Creator 文档，完成三配置全量 offscreen 回归及双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
66/66 tests passed
Total Test time: 30.62 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
66/66 tests passed
Total Test time: 36.81 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
66/66 tests passed
Total Test time: 27.31 sec

Core: 43/43 tests passed
Capabilities: 11 commands, 31 operations,
              traceMappingValidation=true,
              traceMappingRepair=true,
              traceRepairReference=true
Targeted: wave-core-tests and
          wave-cli-trace-mapping-repair-contract-smoke passed
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

用户视角验收：

```text
Codex 在磁盘示例的 Imported Trace 中加入 lane-stale → tb.dut.removed，并把 lane-data 的
Actual signal ID 置空。一次 validate 以退出码 4 返回两项 trace-mapping-invalid；
traceMappingSummary 说明 1 个 Trace 中有 2 条无效映射，分别属于 Lane 缺失和 Actual ID 为空。
每项均给出精确映射键、Trace 快照、问题码和 remove-invalid-mapping 动作，同时明确外部信号
verification=not-performed。

调用方先按 traceRef 删除陈旧 Lane 映射，再按唯一 traceId 删除空 Actual ID 映射。门禁报告
错误数 2→0、映射数 7→5；lane-request 等 5 条有效映射、Trace 路径/格式/偏移和工程波形均保持
不变。dry-run 不写文件，结果 SHA 与实际文件同为
594e45027b2155a4832df60731978605bc7c2f295a0836fc8dc26d64fb256da4。最终 validate 返回退出码
0、valid=true、0 error。示例源 SHA 始终为
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 始终为
493dd28ddc30ab43f1487483d3439b9d5fc195527ba392cc3c1f2991abee9337。

对健康映射执行同一 operation 会以退出码 4 原子拒绝、stdout 为空且不生成输出。重复 Trace ID
使用 traceId 时明确拒绝，使用校验返回的 traceRef 可精确定位；工程在快照后变化时旧 ref 失效，
整个批次不留下第一步的部分删除。工具不会因文件系统当前状态自动重映射 VCD/CSV。
```

长期 Goal 保持 active；第 570 轮为已验收的阶段可用节点。

## 第 571–580 轮：Imported Trace 身份恢复与 Compare 显式选择

目标：避免多个 Imported Trace 或重复稳定 ID 时静默选错外部文件，并在外部 Trace 已加载后
直接区分“没有映射”与“映射目标已消失”；身份修复只修改可证明选中的快照，不改写 Trace
内容或其他配置。

逐轮交付：

1. 第 571 轮：复现 `wave-compare` 在未指定 Trace 时总取数组第一项、重复 ID 时也取首个命中，
   桌面端 `activeTraceReference` 同样按首项返回的问题。
2. 第 572 轮：把 Imported Trace 纳入工程级稳定身份审计；空/重复 ID 分别返回
   `missing-stable-id`/`duplicate-stable-id` 和精确 `importedTraces[N].id` 路径。
3. 第 573 轮：为每个身份问题附加 Trace 索引、ID 匹配数、完整上下文和内容绑定 `traceRef`；
   重复 ID 的不同数组项得到不同引用，不再依赖 ID 猜测。
4. 第 574 轮：新增 `ChangeTraceIdentityAtIndexCommand`；提交前核对索引快照与新 ID 唯一性，
   只更新目标 ID，并提供精确 Undo/Redo 与过期快照拒绝。
5. 第 575 轮：新增 validation-directed `repair-trace-identity`；必须使用 `traceRef`，可接收
   显式 `newId`，省略时由 Trace 快照确定性生成未占用的 `trace-auto-*`。
6. 第 576 轮：桌面端活动 Trace 查找改为只接受唯一 ID；首项 ID 为空或重复时不再启动后台
   解析，而是在 Trace 摘要中明确指出身份损坏。
7. 第 577 轮：`wave-compare` 在工程包含多条 Trace 引用时要求显式
   `--trace-id=<stable-id>`；不存在、空或重复 ID 均在解析外部文件前拒绝。
8. 第 578 轮：Compare 新增 `unmapped-signal`，专门表示 Lane 未配置映射；已有映射指向外部
   Trace 中不存在的信号时保留 `missing-signal`，并在 `traceSignalId` 和消息中返回目标 ID。
9. 第 579 轮：核心矩阵扩展为 44 项；新增
   `wave-cli-trace-identity-compare-contract-smoke`，覆盖磁盘身份修复、多 Trace 选择、
   两类 Compare 诊断、确定性 dry-run/写入和源文件只读。
10. 第 580 轮：capabilities 发布 `traceIdentityValidation`、`traceIdentityRepair` 和
    `repair-trace-identity`，operation 总数更新为 32；更新 README、PLAN、GOAL、工程格式、
    CLI 与 Qt Creator 文档，完成三配置全量 offscreen 回归及双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
67/67 tests passed
Total Test time: 29.46 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
67/67 tests passed
Total Test time: 37.41 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
67/67 tests passed
Total Test time: 27.14 sec

Core: 44/44 tests passed
Capabilities: 11 commands, 32 operations,
              traceIdentityValidation=true,
              traceIdentityRepair=true,
              traceMappingValidation=true,
              traceMappingRepair=true
Targeted: wave-core-tests,
          wave-compare-smoke,
          wave-cli-trace-mapping-repair-contract-smoke and
          wave-cli-trace-identity-compare-contract-smoke passed
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

用户视角验收：

```text
Codex 把磁盘示例的同一 Imported Trace 复制为第二条，保留相同
trace-handshake-actual ID。一次 validate 以退出码 4 返回两项 duplicate-stable-id，路径分别为
importedTraces[0].id 与 importedTraces[1].id；两项 traceContext 均报告 ID 匹配数 2，但
traceRef 因数组索引和完整内容快照不同而可精确区分。

调用方将第二项 traceRef 直接交给 repair-trace-identity。门禁报告错误数 2→0，只更新 1 条
Trace；生成 ID 为 trace-auto-7b07b7dde44a16d2，首条 Trace、两条路径、格式、偏移和映射均
保持不变。dry-run 未写文件且与实际写入产生相同 ID。重复夹具 SHA 为
c4dd187064b688bfd9e69e534a4050431a3ef158b65688ac34993aa332ead52f，恢复结果 SHA 为
8401e107f4d109a3e750a66a8666c9a48130903f7707d9a36da170a3cdb152d8；示例源 SHA 始终为
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4。

多 Trace 工程未给 --trace-id 时明确要求选择；重复 ID 即使显式给出也报告 ambiguous，不读取
或比较任意一条。修复后两个唯一 ID 均可分别完成 Compare。把 lane-request 映射改为
tb.dut.removed_request 后，报告返回 missing-signal 并保留该目标 ID；删除映射后则返回
unmapped-signal 且目标 ID 为空。用户无需再同时反查工程映射表与外部 Trace 才能判断下一步。
健康身份和变化后的旧 traceRef 均以退出码 4 原子拒绝。
```

长期 Goal 保持 active；第 580 轮为已验收的阶段可用节点。

## 第 581–590 轮：Imported Trace 源引用完整性与显式修复

目标：让空 Trace 路径和未知格式在读取外部文件前成为可定位、可恢复的工程问题；恢复必须由
调用方明确给出来源信息，不检查当前文件系统、不根据扩展名猜测，也不改变 Trace 的身份、映射
或验证模型。

逐轮交付：

1. 第 581 轮：复现工程加载器接受空 `path` 和任意 `format`、空路径可能先解析为工程目录、
   未知格式仅返回笼统解析失败的问题。
2. 第 582 轮：定义本地引用约束：路径去除首尾空白后非空；当前格式集合已扩展为大小写不敏感的 VCD/FST/CSV；
   工程校验不检查文件存在性，也不从文件扩展名推断格式。
3. 第 583 轮：新增按 Trace 聚合的 `trace-reference-invalid`；同时返回精确 `paths`、
   `empty-path`/`unsupported-format` 和独立计数，避免同一对象要求两轮校验。
4. 第 584 轮：新增 `traceReferenceContext`，公开路径/格式状态、规范化格式、支持格式、
   `filesystemVerification=not-performed`、恢复动作与快照绑定 `traceRef`。
5. 第 585 轮：新增 `ChangeTraceSourceAtIndexCommand`；提交前核对完整 Trace 快照，只替换
   路径/格式，并提供精确 Undo/Redo 与过期快照拒绝。
6. 第 586 轮：新增 validation-directed `repair-trace-reference`；必须回传 `traceRef` 并显式
   提供路径/格式至少一项，只有最终引用整体有效时才提交，显式格式规范为小写。
7. 第 587 轮：操作报告公开修复前后引用、路径、格式、上下文与 `changedProperties`；ID、偏移、
   signalMapping、扩展载荷和工程波形保持不变。
8. 第 588 轮：`wave-compare` 在解析前分别拒绝空路径、未知格式和目标不是文件；保留的导入代码
   路径也增加具体错误文本，但不恢复此前已删除的 Trace/Compare 面板。
9. 第 589 轮：核心矩阵扩展为 45 项；新增
   `wave-cli-trace-reference-repair-contract-smoke`，覆盖磁盘诊断、dry-run/写入 SHA 一致、
   Compare 门禁、健康/部分/陈旧修复拒绝和源文件只读。
10. 第 590 轮：capabilities 发布 `traceReferenceValidation`、`traceReferenceRepair` 和
    `repair-trace-reference`，operation 总数更新为 33；更新 README、PLAN、GOAL、工程格式、
    CLI 与 Qt Creator 文档，完成三配置全量 offscreen 回归及双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --parallel 4
68/68 tests passed
Total Test time: 8.19 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure --parallel 4
68/68 tests passed
Total Test time: 10.43 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure --parallel 4
68/68 tests passed
Total Test time: 7.02 sec

Core: 45/45 tests passed
Capabilities: 11 commands, 33 operations,
              traceReferenceValidation=true,
              traceReferenceRepair=true,
              traceRepairReference=true
Targeted: wave-core-tests,
          wave-compare-smoke and
          wave-cli-trace-reference-repair-contract-smoke passed
Desktop interaction: none
Legacy Trace panel restored: no
Packaging: not run
Commit/push: not run
```

用户视角验收：

```text
Codex 把磁盘示例首条 Imported Trace 的 path 清空并把 format 改为 fst。一次 validate 以退出码
4 返回一项 trace-reference-invalid；同一问题的 paths 同时列出 importedTraces[0].path 与
importedTraces[0].format，problems 为 empty-path/unsupported-format，且明确文件系统未校验。

调用方不需要尝试解析文件，只需把问题中的 traceRef、明确路径和 VCD 格式交给
repair-trace-reference。门禁报告结构错误数 1→0，只更新 1 条 Trace；ID、offsetTick、六条
signalMapping 和工程波形保持不变。dry-run 不写文件，结果 SHA 与实际文件同为
5d3d5a8f2e88a3fe67450cd56cc463bd22b52a55a9136c344e12ac02da3fec6a。最终 validate 返回
退出码 0，修复后的工程可直接完成 wave-compare。示例源 SHA 始终为
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 始终为
a45b427274c1643c8d887c86b21c54f64c565ee216e81183cbe5706058a0be25。

空路径和未知格式在 Compare 读取文件前分别得到具体原因；只修复路径、继续提交 fst、对健康
引用重复修复，或在快照变化后使用旧 traceRef，均以退出码 4 原子拒绝、不生成输出且不改写
源工程。纯波形桌面工作区未重新引入已删除的 Trace/Compare 面板。
```

长期 Goal 保持 active；第 590 轮为已验收的阶段可用节点。

## 第 591–600 轮：多 Scenario CLI 显式选择与误操作门禁

目标：消除独立 Generate、Compare 与 Bridge 工具在多 Scenario 工程中静默采用数组首项的
行为，使交互式 CLI 与嵌入自动化共享可预测的目标选择规则；单 Scenario 工程继续保持无需额外
参数，已有脚本不因本轮升级失效。

逐轮交付：

1. 第 591 轮：审计 `wave-generate`、`wave-compare`、`wave-bridge` 与统一 `wave-cli`，
   确认 Generate/Compare/Bridge 的 Scenario 级操作存在首项默认，且重复 ID 可落到首个命中。
2. 第 592 轮：定义统一规则：零 Scenario 明确拒绝；单 Scenario 可省略；多 Scenario 必须显式
   选择；稳定 ID 精确匹配优先，名称使用大小写不敏感的唯一完整匹配。
3. 第 593 轮：新增共享无界面选择器；空白、不存在、重名、重复稳定 ID 和所选对象空 ID 均返回
   具体原因，不依赖数组顺序。
4. 第 594 轮：`wave-compare` 新增 `--scenario=SELECTOR`，在选择 Trace、访问外部文件和创建报告
   目录前完成 Scenario 门禁；成功结果回显名称与 ID。
5. 第 595 轮：`wave-generate` 新增主入口 `--scenario=SELECTOR`，保留第三个位置 Scenario ID
   作为兼容别名；两种写法同时出现或重复指定时拒绝。
6. 第 596 轮：`wave-bridge import-signals` 支持尾随 `--scenario`，选择失败发生在读取信号清单
   和写出工程之前；导入只修改目标 Scenario。
7. 第 597 轮：`wave-bridge pinloom-entry` 使用相同选择规则，生成文件与 URI 均绑定明确的
   Scenario ID；工程级 `describe` 与 `link-frame` 保持原语义，workspace manifest 的
   Generate/Compare 模板同步公开选择器。
8. 第 598 轮：统一 `wave-cli` 的 Scenario 解析到同一共享门禁，补齐重复稳定 ID 和空身份拒绝，
   保留原有结构化 `selector-invalid` 错误及规范 ID 输出。
9. 第 599 轮：新增 `wave-cli-scenario-selection-contract-smoke`，从磁盘构造双 Scenario、
   重复 ID 与重名夹具，覆盖四个入口、失败无输出、Generate 旧参数和 Bridge 目标隔离。
10. 第 600 轮：更新 README、PLAN、GOAL、自动化、工程格式与 Qt Creator 文档；完成默认、
    Qt Creator Debug、Qt Creator Release 三配置全量 offscreen 回归和开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --parallel 4
69/69 tests passed
Total Test time: 8.60 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure --parallel 4
69/69 tests passed
Total Test time: 10.45 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure --parallel 4
69/69 tests passed
Total Test time: 7.53 sec

Core: 45/45 tests passed
Capabilities: 11 commands, 33 operations (unchanged)
Targeted: wave-cli-scenario-selection-contract-smoke passed in all three configurations
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

用户视角验收：

```text
Codex 将磁盘示例复制为包含 Request / acknowledge 与 Alternative timing 的双 Scenario 工程。
不提供选择器时，wave-cli、wave-generate、wave-compare、bridge import-signals 与
bridge pinloom-entry 全部在读取外部输入或创建输出前拒绝，并明确指出工程含 2 个 Scenario
以及需要 --scenario。

使用 scenario-alternative 或大小写变体 alternative TIMING 后，各入口均回显
Alternative timing (scenario-alternative)。Generate 写出七件目标产物，Compare 只生成
Alternative_timing 报告；旧位置参数 scenario-alternative 仍产生相同目标。信号导入使第二个
Scenario 的 Lane 数由 8 变为 9，首个 Scenario 保持 8，新增 zeroslack-ready 只存在于所选
Scenario；Pinloom 文件中的 scenarioId 也为 scenario-alternative。

不存在的目标、重名 Scenario 和重复 scenario-handshake ID 均拒绝且不留下输出。双 Scenario
夹具 SHA 为 6071a17b92575bf90aca750d7d766a48ed0ab08d4545dc33e46e4ce1466da38b，
示例源 SHA 始终为
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4。
```

长期 Goal 保持 active；第 600 轮为已验收的阶段可用节点。

## 第 601–610 轮：桌面多波形显式选择与可见恢复

问题：CLI 已拒绝在多 Scenario 工程中隐式使用数组首项，但桌面端仍固定编辑
`scenarios.front()`；没有 Scenario Dock 后，用户无法确认或切换当前波形。URI 为选择目标还会
旋转数组，导致会话选择污染模型顺序。简单拆分每个 Scenario 的 Undo 栈也不可行，因为
ClockDomain 是工程级对象，一次时钟修改可能同时影响多个波形。

1. 第 601 轮：从打开、定位、编辑和撤销路径审计多 Scenario 桌面行为，确认隐式首项是可导致
   误编辑的阻断级问题。
2. 第 602 轮：确定最小界面契约：仅多波形工程显示工具栏 `Waveform` 选择器；单波形工程不增加
   任何常驻控件，不恢复已删除的 Scenario 窗口。
3. 第 603 轮：主窗口改用显式活动索引；URI 将唯一稳定 ID 解析为初始索引，不再
   `std::rotate` Scenario 数组；不存在和重复 ID 均拒绝。
4. 第 604 轮：选择器公开名称、序号、稳定 ID tooltip；重名时在下拉项、标题和状态反馈中附带
   ID，避免同名波形再次依赖数组位置。
5. 第 605 轮：切换接入未提交就地草稿门禁，成功切换不执行命令、不修改模型、不改变 Saved，
   并刷新当前目标、时长、Trace 映射视图和窗口标题。
6. 第 606 轮：WaveCanvas 为每个 Scenario 保留信号选择、时间光标、显式范围、缩放、水平/
   垂直滚动及历史选择转换；新建/打开工程前清除旧指针上下文。
7. 第 607 轮：每次真实命令状态记录其活动 Scenario 索引；直接非命令修改仍沿用工程级 revision，
   保存点和 autosave 语义不变。
8. 第 608 轮：Undo/Redo 保持单一工程级时间序，在命令属于另一波形时自动切换并恢复其上下文；
   不拆分工程级 ClockDomain 命令，也不撤销不可见内容。
9. 第 609 轮：新增 `wave-scenario-switch-smoke`，覆盖初始第二项不重排、切换零副作用、双波形
   编辑隔离、上下文恢复、跨波形 Undo/Redo、单波形隐藏和离屏截图。首次全量回归发现隐藏控件
   后分隔线仍占宽度，修正为整组同步显隐并使最小窗口回归恢复通过。
10. 第 610 轮：更新 README、PLAN、GOAL、工程格式、集成与 Qt Creator 文档；完成默认、
    Qt Creator Debug、Qt Creator Release 三配置全量 offscreen 回归和开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 9.41 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 10.84 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 8.83 sec

Core: 45/45 tests passed
Targeted: wave-scenario-switch-smoke and wave-uri-smoke passed in all three configurations
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

用户视角验收：

```text
打开两个波形的工程后，工具栏左侧直接显示 Waveform 和当前名称；用户不需要寻找已删除的
Scenario 面板，也不会在看不见当前目标时开始修改。选择另一项后，工程内容、Undo、Saved 和
数组顺序均不变化；返回原波形时，信号、20 ns 光标、拍范围和缩放位置仍在原处。

先修改 Request / acknowledge，再修改 Response path，然后从第一个波形按 Ctrl+Z：应用自动
显示 Response path 并撤回最新修改；再次 Ctrl+Z 自动回到 Request / acknowledge，两个波形
均回到干净基线且标题星号消失。两次 Ctrl+Y 按相同顺序恢复并显示实际目标。只有一个波形的
工程不显示标签、下拉框或遗留分隔线，原工具栏宽度和操作路径不变。
```

长期 Goal 保持 active；第 610 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 611–620 轮：多波形连续工作记忆与键盘切换

问题：桌面端已经能显式切换多个波形，但每次重新打开已保存工程仍回到数组第一项，用户需要
重复定位；连续键盘编辑时还必须离开画布点击下拉框。直接记忆数组索引会在 Scenario 重排后
恢复错误目标，记忆损坏身份也不能静默落到第一个同名或同 ID 项。

1. 第 611 轮：从重新打开、连续编辑、就地草稿和 URI 定位路径审计多波形连续使用成本，确认
   “重复定位”和“离开画布点下拉框”是本阶段应消除的高频步骤。
2. 第 612 轮：确定最小契约：不增加常驻按钮；使用 `Ctrl+PageUp` / `Ctrl+PageDown` 前后
   切换；首尾不循环并明确提示方向；所有既有就地草稿门禁继续优先。
3. 第 613 轮：以规范化工程路径和 Project 稳定 ID 的 SHA-256 建立编辑器偏好键，避免路径
   文本直接进入设置层级，也避免不同工程复用同一 Scenario ID 时互相覆盖。
4. 第 614 轮：偏好只保存唯一、非空的 Scenario 稳定 ID；重新打开时按身份恢复而非按数组位置，
   Scenario 数组重排后仍选中同一波形。
5. 第 615 轮：普通下拉切换、键盘切换、URI 显式启动和 Save As 成功后同步当前选择；偏好只写
   本地 `QSettings`，不执行命令、不修改工程 JSON、不改变 Undo 或 Saved。
6. 第 616 轮：新增窗口级前后波形动作及快捷键；多波形工程启用，单波形工程禁用且继续完全隐藏
   选择器；下拉 tooltip 和成功状态直接公开快捷键。
7. 第 617 轮：键盘切换复用统一 `commitPendingEdits` 门禁；非法 End 等草稿保留文本、错误样式
   与焦点并阻止离开当前波形，修正后同一次切换动作继续完成。
8. 第 618 轮：定义恢复优先级与损坏边界：URI/调用方显式初始索引优先；已删除目标、空 ID 或
   重复 ID 不猜测，清除失效偏好并回到第一项；重名/重 ID 下拉项附加序号消歧。
9. 第 619 轮：扩展 `wave-scenario-switch-smoke`，以隔离设置和临时磁盘工程覆盖快捷键、首尾
   反馈、非法草稿门禁、重开恢复、数组重排、删除/重复身份回退、显式目标优先及单波形禁用。
10. 第 620 轮：复核 1440×900 offscreen 截图，修正本阶段状态文本中的错误分隔符；更新
    README、PLAN、GOAL、工程格式、集成和 Qt Creator 文档，执行三配置全量回归及开发/用户
    双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 8.45 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 11.59 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 8.36 sec

Core: 45/45 tests passed
Targeted: wave-scenario-switch-smoke, wave-wave-edit-smoke,
          wave-new-project-smoke, wave-user-journey-smoke and wave-uri-smoke passed
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：选择偏好不属于工程事实源，不进入命令栈或 autosave；工程路径与 Project ID
共同隔离设置，Scenario ID 唯一性门禁避免恢复歧义；显式启动目标在构造阶段优先于偏好，URI
契约不受本地历史影响。导航复用已有切换、草稿提交、画布上下文和工程级历史路径，没有产生
第二套状态；单波形时动作禁用，首尾边界不制造假切换或假 Undo。

用户视角验收：

```text
用户在 Response path 工作后关闭并重新打开已保存的双波形工程，界面直接回到 Response path，
不需要重新寻找下拉项。即使工程文件中的两个 Scenario 调换顺序，仍恢复同一稳定目标，而不是
原来的数组位置。

在画布内按 Ctrl+PageUp 返回 Request / acknowledge，按 Ctrl+PageDown 回到 Response path；
到达首尾时保持原位并说明反向快捷键。End 中保留非法文本时按切换键不会丢失输入或跳到另一
波形，修正后可直接完成切换。选择、Undo、Saved 和工程内容全程不因导航变化。

若先前波形已从工程删除或稳定 ID 重复，应用不猜测目标，回到第一项并显示可区分的下拉文本。
外部 URI 明确指定 Request / acknowledge 时，该显式目标覆盖本机此前记住的 Response path。
单波形工程仍没有选择器或可触发的前后导航。
```

长期 Goal 保持 active；第 620 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 621–630 轮：按波形恢复最后安全编辑位置

问题：第 611–620 轮已让已保存工程回到最后波形，但重开后仍丢失正在查看的信号和时刻。用户
仍需重新查找 lane、输入时间或滚动定位。直接持久化完整选择会在重开后恢复 Beat、Segment、
范围或标题删除目标，使 Delete 等操作具有意外破坏性；持久化裸数组索引也无法承受 Lane 重排。

1. 第 621 轮：从 Save As→New→Open、关闭重开和多波形首次进入路径复现“波形正确但位置丢失”，
   确认重复信号查找和时间定位是本阶段应消除的高频步骤。
2. 第 622 轮：确定安全最小状态仅为唯一可见非 Group Lane 的稳定 ID 与整数 tick；明确不恢复
   Beat、Segment、显式范围、标题删除武装或未提交编辑。
3. 第 623 轮：以规范化工程路径、Project ID 和 Scenario ID 的 SHA-256 建立逐波形位置键；
   设置不包含明文路径，不进入工程 schema、内容哈希、命令栈或 autosave。
4. 第 624 轮：正常关闭、波形切换、New/Open 替换前、保存/Save As 及显式外部定位后更新位置；
   未保存空白工程不产生跨工程偏好。
5. 第 625 轮：恢复时重新验证 Scenario 唯一性和 Lane 唯一、可见、非 Group 条件；Lane 隐藏、
   删除或身份重复时清除陈旧 lane，只恢复 tick；tick 越界时钳到当前时间轴并更新设置。
6. 第 626 轮：WaveCanvas 公开只读的 Scenario 会话上下文存在性；首次进入才读取持久位置，
   同一会话返回时优先恢复范围、Segment、缩放和滚动等更完整的内存上下文。
7. 第 627 轮：启动 URI/外部调用的显式 lane/tick 在构造期恢复之后执行，因此确定性覆盖本地
   位置；成功定位同步成为下一次普通打开的安全位置。
8. 第 628 轮：启动、Open 和首次跨波形恢复反馈直接显示信号与格式化时间，并明确
   `no edit range restored`；恢复后 Delete 验证为零模型、零历史变化。
9. 第 629 轮：扩展 `wave-scenario-switch-smoke` 与 `wave-user-journey-smoke`，覆盖
   Save As→New→Open、双波形位置、首次/再次进入优先级、隐藏 lane 降级、Scenario 重排、
   显式 lane/tick 覆盖及 1440×900 恢复位置截图。
10. 第 630 轮：更新 README、PLAN、GOAL、工程格式、集成和 Qt Creator 文档；完成默认、
    Qt Creator Debug、Qt Creator Release 三配置全量 offscreen 回归及开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 9.13 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 11.81 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 8.59 sec

Core: 45/45 tests passed
Targeted: wave-scenario-switch-smoke, wave-user-journey-smoke and wave-uri-smoke passed
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：位置键同时隔离路径、Project 和 Scenario；Lane 每次恢复均按稳定 ID 重新验证，
不保存或读取模型指针与数组位置。持久位置只在没有当前 Scenario 内存上下文时应用，避免覆盖
已有范围和视口。恢复函数先建立无范围的光标位置，再选择安全 Lane；失败分支不会留下陈旧
Segment 或范围。位置偏好不调用 command、不标记 dirty，也不改变 autosave generation。

用户视角验收：

```text
用户在新工程的 data Bus 约 260 ns 完成编辑并首次 Save As，随后 New，再从 Open 选择刚保存的
工程。界面直接回到同一 data 与精确 tick，状态显示 Opened、resumed 和 no edit range restored；
Saved 保持不变，不需要 Ctrl+F 或 Ctrl+G。

双波形工程在 Response path 的 ack@120 ns 关闭后重开，Waveform、左侧 ack 高亮、120 ns 光标、
Target 采样值和 Saved 同时可见。此时没有 Beat、Segment 或范围，按 Delete 不修改波形。首次
切到 Request / acknowledge 自动恢复此前的 data@180 ns；返回 Response path 时，当前会话中
刚建立的完整范围仍在，没有被磁盘上的 ack@120 ns 覆盖。

若 ack 已隐藏，应用保留 120 ns 但不选择其他相似信号，并明确 saved signal unavailable。
Scenario 数组重排后仍恢复相同波形和位置；显式 URI 定位到 req@40 ns 时覆盖旧 data@180 ns，
下一次普通打开也从 req@40 ns 继续。
```

长期 Goal 保持 active；第 630 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 631–640 轮：恢复安全时间细节尺度

问题：第 621–630 轮已经恢复波形、信号和精确 tick，但离屏用户截图仍显示重开后回到完整
220 ns 时间轴。用户在查看边沿或局部协议窗口时，每次重开都要重复点击 Zoom in。直接保存
`pixelsPerTick` 或滚动条像素会依赖窗口尺寸和滚动条布局，也可能把旧的编辑范围误当成恢复目标。

1. 第 631 轮：从恢复位置截图复现“ack@120 ns 正确、时间细节尺度丢失”，将重复缩放确定为本
   阶段需要消除的剩余高频步骤。
2. 第 632 轮：定义只保存整数可见 tick 跨度的语义契约；不保存像素宽度、滚动条值、Beat、
   Segment、范围或任何可被 Delete 修改的目标。
3. 第 633 轮：WaveCanvas 提供当前语义跨度读取及按跨度/锚点恢复；跨度和锚点分别钳到当前
   Scenario duration，视图以恢复光标为中心。
4. 第 634 轮：逐 Scenario 安全位置新增可选 `visibleSpanTick`；正常关闭、切换、保存、
   Save As、New/Open 前及显式定位沿用原位置写入路径，设置仍不进入工程或命令历史。
5. 第 635 轮：恢复请求在首次 Show、viewport resize 和滚动条布局后重放一次，消除构造期尺寸
   与最终窗口尺寸不同造成的跨度漂移；用户主动 Zoom/Fit 会取消待恢复请求。
6. 第 636 轮：显式 lane/tick 在待恢复布局期间更新尺度锚点，因此 URI/外部调用保留局部尺度，
   但确定性地以显式 tick 居中并写回新安全位置。
7. 第 637 轮：同一会话已有 WaveCanvas 上下文继续优先，返回波形时保留真实缩放、滚动、范围和
   Segment，不被磁盘语义跨度覆盖。
8. 第 638 轮：旧设置缺少跨度时保持原默认视图；非法跨度单独清除，超长跨度钳到当前 End；
   Lane 隐藏、删除或重复仍恢复时间与有效尺度，不猜测替代信号。
9. 第 639 轮：扩展 `wave-scenario-switch-smoke` 和 `wave-user-journey-smoke`，覆盖双波形不同
   尺度、关闭重开、首次切换、会话优先、隐藏 Lane、Scenario 重排、Save As→New→Open 及
   显式 lane/tick 重锚；1120×760 保存到 1440×900 重开验证跨窗口尺寸语义，最终截图复核
   局部时间窗、光标和安全反馈。
10. 第 640 轮：同步 README、PLAN、GOAL、工程格式、集成及 Qt Creator 文档，并完成默认、
    Qt Creator Debug、Qt Creator Release 三配置全量 offscreen 回归及开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 8.85 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 11.18 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 8.47 sec

Core: 45/45 tests passed
Targeted: wave-scenario-switch-smoke, wave-user-journey-smoke, wave-uri-smoke,
          wave-wave-edit-smoke, wave-wave-edit-autoscroll-smoke and
          wave-edit-menu-smoke passed
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：持久值是与窗口像素无关的整数时间跨度；应用尺度的内部路径不创建命令、不改变
选择、Saved 或 autosave generation。构造期先应用以避免明显闪回，首次布局稳定后再按最终
viewport 重放；待恢复状态在用户主动 Zoom/Fit、文档替换或上下文清理时取消。状态反馈和设置
写回使用目标语义跨度，不会把滚动条出现前的临时像素结果误记为下一次尺度。显式位置只改锚点，
会话上下文仍按原优先级恢复。

用户视角验收：

```text
用户在 Response path 放大后停在 ack@120 ns，关闭再打开工程。界面直接显示约 176 ns 的局部
时间窗，120 ns 位于视图中央附近；不再退回完整 220 ns，也不需要再次点击 Zoom in。左侧 ack、
Target 值、Saved 和 no edit range restored 同时可见，按 Delete 不会修改波形。

首次切到 Request / acknowledge 时恢复该波形此前更细的约 141 ns 时间窗和 data@180 ns；
返回 Response path 后，本次会话建立的完整范围与实际视口优先，不被磁盘尺度覆盖。隐藏 ack 或
交换 Scenario 数组顺序仍保持 120 ns 与相同尺度，不选择相似信号。

显式定位 req@40 ns 时保留 Request / acknowledge 的细节尺度，但将中心改到 40 ns；下一次普通
打开继续使用该位置。Save As→New→Open 同样恢复 Bus、精确 tick 和保存时尺度，工程始终保持
Saved，未产生额外 Undo。
```

长期 Goal 保持 active；第 640 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 641–650 轮：异常退出时保留最后安全视图

问题：工程数据已能通过 autosave 在异常退出后恢复，但安全 Lane、tick 和时间细节尺度此前主要在
Save、波形切换及正常关闭等生命周期边界写入。用户在一次较长的浏览或编辑过程中定位到局部波形，
若进程异常结束，数据可以回来，界面却可能回到更早的位置。直接在每个鼠标或滚动事件中同步写设置
会造成高频磁盘写入；保存当前 Beat、Segment 或范围又会让重启后的 Delete 带有破坏性目标。

1. 第 641 轮：复核恢复快照与逐 Scenario 位置生命周期，确认“数据较新、位置较旧”是异常退出
   路径中剩余的实际断点。
2. 第 642 轮：保持最小安全契约，只记唯一可见非 Group Lane、整数 tick 和语义
   `visibleSpanTick`；继续排除 Beat、Segment、显式范围、标题删除状态及所有草稿。
3. 第 643 轮：为已有正式路径的工程增加 400 ms 单次去抖计时器，连续导航只在最后一次变化
   稳定后写入，不修改模型、命令栈、Saved 或 autosave generation。
4. 第 644 轮：以选择/光标状态反馈以及水平滚动条 value/range 变化触发调度，覆盖信号定位、
   键盘光标、Zoom、Fit、水平浏览和窗口布局后的最终尺度；首次构造恢复阶段不产生陈旧写入。
5. 第 645 轮：Save、Save As、切换波形、New/Open 替换前和正常关闭继续同步记忆，并先取消
   待执行计时器，保证生命周期边界不会被旧的延迟任务覆盖。
6. 第 646 轮：Untitled 工程不创建持久路径身份；快速新增信号仍在就地补齐名称/参数时，新的
   调度和已经在途的计时器均不得写入临时 Lane，Esc 取消后恢复原工程状态。
7. 第 647 轮：较新 `.autosave` 按去除后缀的正式工程路径复用安全位置；恢复状态栏同时报告
   `Recovery snapshot loaded`、`resumed` 与 `no edit range restored`。
8. 第 648 轮：扩展 `wave-autosave-smoke`，在不 Save、不切换、不正常关闭的条件下快速执行
   req@40 ns→data@170 ns 及两次缩放，等待去抖后验证只有最终位置和局部尺度写入；随后建立
   待提交快速 Bit，并验证既有计时器到期也不会污染位置。
9. 第 649 轮：重开人为更新的恢复快照，验证数据 duration、data@170 ns、约 142 ns 尺度、
   Save required、零范围、零 Segment、零 Undo 共同恢复；按 Delete 保持工程不变。扩展
   `wave-new-project-smoke`，确认 Untitled 恢复后没有任何 `waveforms/lastLocation` 键；
   保存并复核 `autosave-smoke-resumed-location.png`。
10. 第 650 轮：同步 README、PLAN、GOAL、工程格式、集成及 Qt Creator 文档，执行默认、
    Qt Creator Debug、Qt Creator Release 三配置全量 offscreen 回归和开发/用户双视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 9.85 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 11.88 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 8.16 sec

Core: 45/45 tests passed
Targeted: wave-autosave-smoke, wave-new-project-smoke, wave-canvas-add-lane-smoke,
          wave-scenario-switch-smoke and wave-user-journey-smoke passed
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：计时器只为正式工程调度，且所有同步写入入口先停止待执行任务；快速新增事务在开始
前已形成的计时器到期时也会再次检查临时 Lane 门禁。设置写入仍按规范化路径、Project ID 和
Scenario ID 隔离，并同步落盘以服务异常终止；去抖只合并重复写入，不把视图状态纳入 JSON 或
autosave 数据。恢复快照使用正式目标路径查找设置，恢复选择不包含范围或 Segment。专项测试在
检查恢复前不调用 MainWindow 的保存、切换或关闭入口，避免将生命周期写入误当作去抖证据。

用户视角验收：

```text
用户在已保存工程中先查看 req@40 ns，再放大并定位到 data[7:0]@170 ns。停止操作约 400 ms 后，
应用仍显示 Saved，也没有新增 Undo；不需要执行 Save 或关闭窗口。

用户随后误点 + BIT，临时信号进入就地命名，但异常恢复位置仍保持 data[7:0]@170 ns。按 Esc
取消后信号数和 Saved 状态恢复，临时名称不会成为下次启动目标。

模拟进程异常结束并让较新的恢复快照重开后，更新过的数据、data[7:0]@170 ns 和约 142 ns 的
局部时间窗同时出现。状态明确显示 Recovery loaded、Save required、resumed 和
no edit range restored；没有选中 Beat/Segment/范围，按 Delete 不改变工程。首次保存前的
Untitled 恢复继续只恢复数据，不建立与任意正式工程混用的位置身份。
```

长期 Goal 保持 active；第 650 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 651–660 轮：将 Group 收敛为可折叠波形层级

问题：已有 Group 能保存成员引用，也支持选择、改名、重排、隐藏和删除，但画布只把它绘制成
一条空白行，成员与普通信号没有层级差异。用户无法从长波形列表中快速收起一组信号，也无法确认
哪些信号属于该组。若直接复用 Lane `visible` 实现折叠，会污染工程、Saved 和 Undo，并可能把
当前 Beat、Segment 或范围编辑目标留在不可见成员上。

1. 第 651 轮：审计 Group 模型、标题命中、Lane 布局、选择清理、隐藏/删除、逐 Scenario 画布
   上下文及现有 smoke，确定折叠必须是纯会话视图状态。
2. 第 652 轮：为可见 Group 增加标准 disclosure 箭头；箭头、普通 `Left` / `Right` 和标题右键
   `Collapse group` / `Expand group` 使用同一入口。
3. 第 653 轮：Lane 布局只跳过已折叠 Group 的可见成员，保留成员 `visible`、`groupId`、波形、
   Event/Relation 和原数组顺序；折叠不创建 command。
4. 第 654 轮：成员标题增加缩进与连续层级引导线，Group 标题使用独立背景并直接显示成员数量及
   Expanded/Collapsed；名称列自动适配同时计入层级缩进和 Group 状态说明。
5. 第 655 轮：折叠当前成员时清除 Beat、Segment、显式范围、浮动编辑器和拖动预览，选择安全
   指向 Group，但不武装标题 Delete；状态栏与 Target 明确显示数据未改变及反向操作。
6. 第 656 轮：每个 Scenario 的会话上下文分别保存折叠 Group 集合；切换波形后恢复本次会话状态，
   不把折叠写入工程文件或持久位置设置。
7. 第 657 轮：查找、URI 及其他显式 `revealLane` / `revealLocation` 定位成员时自动展开其 Group，
   避免定位成功但目标仍不可见。
8. 第 658 轮：Group 隐藏、删除、类型变化或不再拥有可见成员时清除失效折叠状态；隐藏折叠 Group
   后仍可见成员立即恢复显示，Undo 恢复 Group 时使用安全展开默认值。
9. 第 659 轮：扩展 `wave-group-header-smoke`，覆盖 disclosure、Left/Right、右键状态、成员
   Segment 安全清理、Delete 防误触、逐 Scenario 隔离、显式定位、隐藏恢复、模型/Undo/Saved
   零变化，并保存折叠与展开两张 offscreen 截图。
10. 第 660 轮：复核截图，回归 Lane 隐藏/重排/自动滚动、信号标题、Wave Edit 与 Scenario
    切换；同步 README、PLAN、GOAL、工程格式、集成和 Qt Creator 文档，执行三配置全量验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 9.29 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 12.37 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure --parallel 4
70/70 tests passed
Total Test time: 9.13 sec

Core: 45/45 tests passed
Targeted: wave-group-header-smoke, wave-hidden-lane-smoke, wave-lane-reorder-smoke,
          wave-lane-autoscroll-smoke, wave-signal-header-smoke, wave-wave-edit-smoke,
          wave-wave-edit-autoscroll-smoke and wave-scenario-switch-smoke passed
Screenshots: build/group-collapse-smoke.png,
             build/group-collapse-smoke-expanded.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：折叠集合只存在于 `WaveCanvas` 及其逐 Scenario `DocumentContext`，布局、命中、
吸附和键盘信号导航均以实际显示 Lane 为准。模型层 `visible`、成员引用和命令栈完全不变；Group
被隐藏或删除后，布局重建先清理失效折叠身份。成员选择被折叠时统一经过现有波形编辑状态清理，
因此不会保留可由 Delete 修改的不可见范围；通过 disclosure 产生的安全 Group 目标也不会直接
获得删除权限。显式揭示路径先展开再定位，避免滚动到不存在的布局项。

用户视角验收：

```text
用户打开含 Handshake signals 的波形后，立即从向下箭头和成员缩进看出 req、ack、data、state、
transfer 属于同一组。点击箭头后五行一次收起，时钟、复位、时间刻度与波形位置保持不变；状态栏
显示 5 signal(s) hidden from view、data unchanged 和 Right expands。

用户原本选中了 data 的一个 Segment。收起组后该 Segment 和范围不再是隐藏的删除目标，按 Delete
不会修改工程；Target 显示 group、collapsed。按 Right 后五条成员原位返回，Saved 和 Undo 与
折叠前完全相同。选中组名后也可用 Left/Right，右键菜单显示与当前状态一致的 Collapse/Expand。

从查找或外部定位跳到 data 时，已折叠组自动展开并直接显示目标。若用户在折叠状态隐藏整个 Group，
成员立即以普通可见信号返回，不会因一个不可见组头而消失。切换到另一 Waveform 不继承当前折叠，
返回时恢复各自会话状态。两张离屏截图确认箭头、层级线、成员数量、Target、刻度与结果反馈同时可见。
```

长期 Goal 保持 active；第 660 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 661–670 轮：将 Group 成员维护收敛为一次直接操作

本轮只处理信号归组、解除归组及其拖放反馈。现有 Group 折叠、普通 Lane 重排、波形编辑、
属性窗口和工程格式保持兼容，不新增面板。

1. 第 661 轮：审计 Add group、Lane 属性、信号右键、标题拖动和 Group 折叠路径，确认高频阻力
   是每条信号必须进入完整属性窗口设置 Group，之后还可能再次重排。
2. 第 662 轮：增加单一 `SetLaneGroupCommand`；归组同时修改 `groupId` 并把信号移到目标 Group
   最后一个既有成员之后，解除归组保持当前顺序，二者均只占一个 Undo 状态。
3. 第 663 轮：在信号标题右键菜单加入 `Move to group` 子菜单，直接列出当前可见 Group，
   不再经过完整 Lane 配置页。
4. 第 664 轮：子菜单显示准确当前勾选状态；`No group` 一次解除归属，当前无 Group 时禁用，
   没有可见 Group 时给出明确恢复/创建入口提示。
5. 第 665 轮：扩展标题拖动命中；非 Group 信号落在 Group 行时解释为归组，Group 自身拖动及
   其他位置仍沿用原插入线重排。
6. 第 666 轮：归组拖动改用整行蓝色轮廓、半透明背景和 `Move <signal> into <group>` 文案，
   与普通重排的单条插入线明确区分。
7. 第 667 轮：允许折叠 Group 接收拖放；提交后自动展开、滚动到并选中新成员，使结果立即可见。
8. 第 668 轮：补齐零变化、无效/歧义目标、非 Group 目标、Group 嵌套和 Undo/Redo 边界；
   Event、Relation、Segment、ClockDomain 与波形数据均不得变化。
9. 第 669 轮：增加核心命令测试和独立 `wave-group-membership-smoke`，覆盖右键归组、拖放预览、
   折叠组提交、解除归组、精确回退及 Saved 基线，并保存 offscreen 截图。
10. 第 670 轮：回归 Group 折叠、Lane 重排/自动滚动、信号标题和全量测试；同步
    README、PLAN、GOAL，执行默认、Qt Creator Debug、Qt Creator Release 三配置验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --parallel 4
71/71 tests passed
Total Test time: 9.41 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure --parallel 4
71/71 tests passed
Total Test time: 9.14 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure --parallel 4
71/71 tests passed
Total Test time: 8.67 sec

Core: 46/46 tests passed
Targeted: wave-group-membership-smoke, wave-group-header-smoke,
          wave-lane-reorder-smoke, wave-lane-autoscroll-smoke,
          wave-signal-header-smoke and wave-core-tests passed
Screenshot: build/group-membership-smoke.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：命令快照只替换当前 Scenario 的 Lane 列表；归组前后 Lane 本体、Segment、
Event 和 Relation 数据保持相等，仅目标信号的 `groupId` 与显示位置变化。目标 Group 必须是
同一 Scenario 中唯一存在的 Group，源必须是非 Group Lane；重复归入当前 Group 的
`hasEffect()` 为 false，不写入命令历史。即使历史工程的成员被其他信号隔开，新成员仍放在
最后一个既有成员之后。解除归组只清空引用，不产生隐式重排。

用户视角验收：

```text
用户要把 reset_n 放入 Handshake signals。右键信号名后，Move to group 直接显示
Handshake signals；点击一次即完成归属和位置整理，状态栏说明放在五个既有成员之后并提示
Ctrl+Z。无需打开属性窗口，也无需第二次拖动重排。一次 Undo 同时恢复未分组和原位置。

用户也可以收起 Handshake signals 后直接拖动 reset_n 到该标题。目标行出现蓝色轮廓和
Move reset_n into Handshake signals，普通排序插入线消失，因此释放前即可判断动作含义。
释放后 Group 自动展开，reset_n 作为第六个成员显示并保持选中。

再次右键 reset_n 时当前 Group 已勾选，No group 可一次解除成员关系且不移动该行。连续 Undo
依次恢复归组、撤销归组，最终回到 Group 隐藏、reset_n 未分组的 Saved 基线。离屏截图确认
目标反馈、折叠状态、时间刻度和源波形同时可见。
```

长期 Goal 保持 active；第 670 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 671–680 轮：将首次建组收敛为名称和当前信号

本轮只减少 Group 首次创建及放入首成员的步骤。画布底部继续保持 `+ CLK`、`+ BIT`、`+ BUS`
三项，不增加常驻 Group 按钮；既有 Group 高级属性和成员拖放保持不变。

1. 第 671 轮：审计 `Edit > Add group…`、完整 Lane 属性页与创建后归组路径，确认用户必须先处理
   无关结构字段，再重新找到信号完成第二次操作。
2. 第 672 轮：建立名称-only Group 对话框，只保留一项当前任务信息；默认名称按全 Scenario
   信号/Group 名称自动唯一。
3. 第 673 轮：在同一短对话框即时验证空名和大小写不敏感重名，Create 按钮原位禁用并显示原因，
   无需关闭警告后重新开始。
4. 第 674 轮：`Edit > Add group…` 改用短对话框；合法名称创建可见空 Group，状态栏直接说明
   拖入信号、`Move to group` 和 Ctrl+Z，高级颜色/高度仍由标题右键承担。
5. 第 675 轮：信号右键 Group 子菜单加入 `New group with this signal…`；即使当前没有可见 Group，
   该动作仍保持可见可用。
6. 第 676 轮：新增 `CreateGroupWithLaneCommand`，在信号正上方插入 Group 并将其设为首成员，
   新建、归属和顺序只形成一个命令状态。
7. 第 677 轮：若源信号已有 Group，提示其将离开旧 Group；提交只迁移该信号，旧 Group 其他成员、
   Event、Relation、Segment、ClockDomain 与波形保持不变。
8. 第 678 轮：补齐空/重复 ID、空名、非 Group 数据、Group 嵌套、Undo/Redo 和 Saved 边界，
   提交后自动显示并选中首成员。
9. 第 679 轮：增加核心命令测试和独立 `wave-group-creation-smoke`，覆盖空组短对话框、即时名称
   校验、无可见 Group 时的发现入口、首成员原子创建及精确回退，并保存 offscreen 截图。
10. 第 680 轮：回归 Group 属性、折叠、成员维护、Lane 对话框/重排和信号标题；同步
    README、PLAN、GOAL，执行默认、Qt Creator Debug、Qt Creator Release 三配置验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --parallel 4
72/72 tests passed
Total Test time: 9.55 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure --parallel 4
72/72 tests passed
Total Test time: 8.93 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure --parallel 4
72/72 tests passed
Total Test time: 8.65 sec

Core: 47/47 tests passed
Targeted: wave-group-creation-smoke, wave-group-membership-smoke,
          wave-group-header-smoke, wave-lane-dialog-smoke,
          wave-lane-reorder-smoke and wave-signal-header-smoke passed
Screenshot: build/group-creation-smoke.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：名称对话框只读取当前 Scenario 的 Lane 名称，空值和重名不会触发命令。
`CreateGroupWithLaneCommand` 先验证唯一源 Lane、全新 Group ID、Group 结构与非 Group 源，再以
完整 Lane 列表快照生成前后状态；Group 插入源位置，源顺延一位并引用新 Group。Undo/Redo 因此
同时恢复 Group 存在性、源归属和顺序，不需要组合两条命令，也不存在只撤销一半的中间状态。

用户视角验收：

```text
用户从 Edit 选择 Add group，只看到 Group name。输入空白时 Create 立即禁用并显示 Enter a group
name；输入已有 Handshake signals 时显示名称已使用；改成 Control signals 后一次创建。画布直接
选中新空 Group，状态栏说明如何拖入信号，Undo 后完整回到 Saved。

用户真正要从 reset_n 开始整理时，无需先创建空组。在 reset_n 右键的 Move to group 子菜单选择
New group with this signal，输入 Reset controls 后，Group 标题立即位于 reset_n 上方，reset_n
缩进为唯一首成员且保持选中。状态栏同时给出 Ctrl+Z 和继续拖入更多信号。

一次 Undo 同时删除 Reset controls、清除 reset_n 归属并恢复原顺序；Redo 完整重建同一结果，
再次 Undo 回到无星号 Saved 基线。截图确认用户无需查看属性页即可识别结果已经生效。
```

长期 Goal 保持 active；第 680 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 681–690 轮：将多信号归组收敛为一次选择、一次命令

本轮只减少整理多条相关信号时的重复选择和重复 Undo。多选直接复用信号名及既有右键 Group
入口，不增加面板、复选框列表或常驻工具栏；普通单击拖动重排和波形范围选择保持原语义。

1. 第 681 轮：审计 `selectedLaneIds`、标题命中、范围选择、右键菜单与 Group 命令，确认画布
   已能保存并绘制多 Lane 选择，但标题单击和右键始终将其缩回单条，批量整理入口不可达。
2. 第 682 轮：信号名加入 `Ctrl+单击` 增减选择；最后一条不能被切换为空，选择按当前可见显示
   顺序稳定排列。带修饰键的点击不启动标题拖动。
3. 第 683 轮：信号名加入 `Shift+单击`，从稳定锚点连续选择可见非 Group 信号；自动跳过
   Group 标题和折叠后不可见成员。普通单击继续恢复单信号选择及原拖动重排。
4. 第 684 轮：Target、状态栏和提示明确显示所选数量及活动信号；右键任一已选名称保持整个
   选择，右键未选名称则安全收敛为单条。Esc 只保留活动信号。
5. 第 685 轮：新增 `SetLanesGroupCommand`。批量归组只迁移尚未位于目标 Group 的信号，保持
   所选显示顺序并放在既有成员之后；批量解除只清空成员引用，不改变当前顺序，二者均为一次 Undo。
6. 第 686 轮：新增 `CreateGroupWithLanesCommand`。新 Group 插入最早所选信号位置，全部所选
   信号按原显示顺序连续成为成员；新建、旧组迁出、位置整理只形成一个历史状态。
7. 第 687 轮：右键 Group 子菜单在多选时改为 `Move selected signals to group`，统一提供目标
   Group、`No group` 和 `New group with selected signals…`；勾选状态按全部所选信号计算。
   参数、重命名、复制、隐藏和删除仍限定单条并原位说明原因，避免隐式批量破坏。
8. 第 688 轮：命令提交后自动展开必要 Group、恢复全部标题高亮及活动信号，状态栏区分实际迁移
   数与已经在目标中的数量；零变化不写入历史，取消对话框保持选择和工程不变。
9. 第 689 轮：新增核心批量命令测试和独立 `wave-group-batch-smoke`，覆盖 Ctrl 增减、Shift
   连选、单信号动作门禁、右键保持、批量建组、混合成员归组、精确 Undo/Redo 和 Saved 基线，
   并保存 offscreen 截图。
10. 第 690 轮：回归单信号建组/归组、Group 折叠、Lane 重排/自动滚动和信号标题；同步
    README、PLAN、GOAL，执行默认、Qt Creator Debug、Qt Creator Release 三配置验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --parallel 4
73/73 tests passed
Total Test time: 9.95 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure --parallel 4
73/73 tests passed
Total Test time: 9.38 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure --parallel 4
73/73 tests passed
Total Test time: 8.84 sec

Core: 48/48 tests passed
Targeted: wave-group-batch-smoke, wave-group-creation-smoke,
          wave-group-membership-smoke, wave-group-header-smoke,
          wave-lane-reorder-smoke, wave-lane-autoscroll-smoke,
          wave-signal-header-smoke and wave-core-tests passed
Screenshot: build/group-batch-smoke.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：两个批量命令在构造时验证非空、唯一、非 Group 的源身份以及唯一有效的目标
Group，再生成完整 Lane 列表前后快照。归入已有 Group 时，已经属于目标的所选成员原位保留，
只有其余成员按原显示顺序迁移到最后一个既有成员之后；解除归组完全不重排。新建 Group 时按
Scenario 原顺序收集所选成员，因此调用方传入顺序不会改变画布顺序。Undo/Redo 只替换当前
Scenario 的 Lane 列表，Event、Relation、Segment、ClockDomain 和波形数据保持相等；重复成员
身份、Group 嵌套、无效目标和零变化不会产生部分结果或无意义历史。

用户视角验收：

```text
用户依次单击 reset_n、Ctrl+单击 req，可看到两行同时高亮；再次 Ctrl+单击 req 后只保留
reset_n。随后 Shift+单击 ack，reset_n、req、ack 三条连续可见信号一次选中，Target 显示
3 signals 和活动信号。此时误按 F2 不会弹出重命名，状态栏说明该动作要求单信号。

右键任一已选名称，选择不会缩回一条；菜单直接显示 Move selected signals to group。
选择 New group with selected signals，只输入 Control bundle，三条信号按原顺序立即位于新
Group 下方并保持共同高亮。状态栏说明来自一个旧 Group、只占一个 Undo；一次 Undo 删除新
Group 并精确恢复三条信号的原归属和顺序。

恢复 Handshake signals 后再次选择同三条信号并归入该组，其中 req、ack 已是成员，只有
reset_n 实际迁移。状态栏明确显示 3 selected、1 moved、2 already there；一次 Undo 只撤销
这次批量结果。离屏截图确认多行高亮、Group 层级、刻度、波形与结果反馈同时可见。
```

长期 Goal 保持 active；第 690 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 691–700 轮：让多信号选择形成可恢复的清理闭环

本轮只处理标题多选后必然出现的批量隐藏和批量删除，不扩展波形编辑语义。删除仍先确认真实
依赖影响，隐藏仍由底部 `Show N hidden items` 恢复；重命名和复制继续限定单信号。

1. 第 691 轮：审计单信号隐藏、删除、事件/Relation/trace mapping 清理、确认策略与历史选择
   恢复，确认现有多选必须退回逐条操作，并且外部结构命令没有记录标题选择。
2. 第 692 轮：新增 `HideLanesCommand`，严格验证非空、唯一、非 Group 的源身份；一次隐藏
   全部所选信号，Undo 恢复每条信号原可见状态。
3. 第 693 轮：新增 `RemoveLanesCommand`，从完整 Scenario 快照原子删除所选 Lane、其全部
   Event 以及引用这些 Event 的 Relation；重复或歧义身份、Group 源在执行前拒绝。
4. 第 694 轮：批量删除同步清理不再被其他 Scenario 使用的 imported trace mapping；Undo
   按 trace 身份合并恢复映射，不替换整个 Trace 列表，因此命令执行后新导入的 Trace 保留。
5. 第 695 轮：信号标题右键加入 `Hide selected signals` 和
   `Remove selected signals…`；Edit 菜单同步显示数量。隐藏直接提交，删除确认框先汇总
   信号、Event、Relation 和 trace mapping 数量，Cancel 保持模型与选择不变。
6. 第 696 轮：多选标题上的 Delete/Backspace 改为调用同一批量删除路径；F2 和 Ctrl+D 仍
   原位说明只接受单信号，避免重命名或复制语义变得不确定。
7. 第 697 轮：标题选择进入命令历史过渡快照。Undo 恢复原多选、活动信号和编辑 tick；Redo
   在 Lane 已隐藏或删除时主动清除失效目标，并通过 Target 与状态栏给出明确反馈。
8. 第 698 轮：新增核心批量清理用例，覆盖一条历史状态、精确前后模型、依赖并集清理、共享
   mapping 保留、后来导入 Trace 保留、Undo/Redo、重复身份和 Group 源拒绝。
9. 第 699 轮：扩展 `wave-group-batch-smoke`，offscreen 覆盖右键入口、三信号隐藏、Undo
   选择恢复、Redo 清除、删除预览取消、Delete 确认、依赖清理和 Saved 基线。
10. 第 700 轮：复核离屏截图并回归既有 Group、单信号隐藏/删除、标题、重排及波形编辑；
    同步 README、PLAN、GOAL，执行三配置全量验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --parallel 4
73/73 tests passed
Total Test time: 10.39 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure --parallel 4
73/73 tests passed
Total Test time: 9.60 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure --parallel 4
73/73 tests passed
Total Test time: 9.90 sec

Core: 49/49 tests passed
Targeted: wave-group-batch-smoke, wave-lane-removal-smoke,
          wave-hidden-lane-smoke, wave-group-creation-smoke,
          wave-group-membership-smoke, wave-group-header-smoke,
          wave-signal-header-smoke and wave-core-tests passed
Screenshot: build/group-batch-smoke.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：两个批量命令都在构造阶段完成全量验证，再生成确定的结果。删除关系按被删
Event 身份的并集计算，同一 Relation 只删除一次；trace mapping 逐 Lane 检查其他 Scenario，
仍有使用者时原位保留。Undo 恢复 Scenario 快照，并只向仍存在的目标 Trace 合并原映射，不会
删除后来导入的 Trace。画布在命令前后记录标题选择过渡：恢复时重新验证所有 Lane 可见且非
Group，空的目标快照则清理 Segment、范围、编辑面板和标题状态，不留下悬空选择。

用户视角验收：

```text
用户从 reset_n Shift 连选到 ack，右键任一已选名称可直接看到 Hide selected signals 与
Remove selected signals…。选择 Hide 后三条信号同时消失，底部 Show hidden items 数量立即
更新；一次 Undo 恢复三条波形及原三行高亮，Redo 再次隐藏并清除失效 Target。

恢复后只选择 req 和 ack，按 Delete。确认框先说明将删除两个信号以及关联 Event、Relation，
默认按钮为 Cancel；取消后模型、Saved 和双行选择均不变。再次 Delete 并确认，两个信号及
依赖一次移除，reset_n 和其他波形保持不变。一次 Undo 精确回到 Saved，req/ack 双行高亮、
活动 ack 和编辑 tick 同时恢复；Redo 清除目标，再次 Undo 仍完整恢复。

离屏截图显示 Target: 2 signals · active ack、req/ack 双行高亮、时间刻度与网格、Saved，
状态栏明确报告 Undid Remove selected signals、Ctrl+Y 与 target restored。
```

长期 Goal 保持 active；第 700 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 701–710 轮：让标题多选可以直接作为整体拖动

本轮只补齐标题多选与既有拖动重排之间的断点。已选信号作为一个整体重排或归组，单信号拖动、
自动滚动、Group 层级、波形编辑和键盘排序保持原语义；仅单击已选名称仍收敛为单信号。

1. 第 701 轮：审计标题按下、拖动阈值、插入槽、Group 命中、自动滚动和 `MoveLaneCommand`，
   确认普通按下会过早清空多选，后续只能移动活动信号。
2. 第 702 轮：在无修饰键按下已选名称时暂时保留全部标题选择；超过拖动阈值才进入批量拖动，
   未拖动释放则只保留所点击信号，避免改变普通选择直觉。
3. 第 703 轮：拖动开始前按当前 Scenario 顺序归一化选择，不采用 Ctrl 点击先后或 Undo 后
   遗留的列表顺序；活动信号仍是实际抓取的名称。
4. 第 704 轮：新增 `MoveLanesCommand`，一次验证全部唯一非 Group 信号并生成完整 Lane 前后
   快照；所选信号按原画布顺序组成连续块，只形成一个 Undo 状态。
5. 第 705 轮：把可见插入线映射为原模型插入槽，正确扣除插入槽前已选成员；支持非连续选择、
   首尾投放和选择块内部的零变化投放，Group 归属保持不变。
6. 第 706 轮：拖到 Group 标题时复用 `SetLanesGroupCommand`，全部选择一次归入目标；反馈区分
   实际迁移与已在目标中的成员，零变化不写入历史。
7. 第 707 轮：普通投放在线旁显示 `Move N selected signals`，Group 投放整行显示
   `Move N selected signals into <group>`；关闭手势与 Group 手势继续使用不同光标。
8. 第 708 轮：Esc 取消批量拖动时恢复原垂直视图并保留多选、模型和 Redo；提交后多选及活动
   信号保持，单击不拖动仍明确清除其余选择。
9. 第 709 轮：新增核心批量重排契约并扩展 `wave-group-batch-smoke`，覆盖无序输入、相对顺序、
   Group 归属、零变化、异常输入、普通投放、Group 投放、Esc、Undo/Redo 与 Saved 基线。
10. 第 710 轮：复核普通插入线和 Group 目标两张 offscreen 预览，回归单信号重排、自动滚动、
    Group、隐藏/删除及 Wave Edit；同步 README、PLAN、GOAL 并完成三配置全量验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --parallel 4
73/73 tests passed
Total Test time: 10.59 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure --parallel 4
73/73 tests passed
Total Test time: 9.24 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure --parallel 4
73/73 tests passed
Total Test time: 9.40 sec

Core: 50/50 tests passed
Targeted: wave-group-batch-smoke, wave-lane-reorder-smoke,
          wave-lane-autoscroll-smoke, wave-group-membership-smoke,
          wave-group-header-smoke, wave-hidden-lane-smoke,
          wave-lane-removal-smoke, wave-wave-edit-smoke and
          wave-core-tests passed
Screenshots: build/group-batch-drag-preview.png
             build/group-batch-group-drop-preview.png
             build/group-batch-smoke.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：`MoveLanesCommand` 不信任调用方选择顺序，而是从原 Scenario 收集成员；删除成员后
按“原插入槽减去槽前已选成员数”定位连续块，因此向前、向后和首尾移动使用同一确定算法。命令
保留每条 Lane 的完整内容及 `groupId`，Event、Relation、Segment、ClockDomain 和工程格式不变；
重复、缺失、歧义、Group 源和越界插入在执行前拒绝。零变化由 `hasEffect()` 阻止，不截断 Redo。

画布只在无修饰键按下“当前多选中的信号名称”时进入待定批量拖动；Ctrl/Shift 仍只修改选择，
未选名称和 Group 仍走既有单项路径。普通投放显示最终连续块位置，Group 投放不暴露普通插入线；
两者提交后重新按实际画布顺序选择全部成员。Esc 清空暂态目标和自动滚动但不清除标题选择。

用户视角验收：

```text
用户从 reset_n Shift 连选到 ack 后，抓住其中 ack 名称向 state 上方拖动。三行选择没有缩回
单行，插入线旁直接显示 Move 3 selected signals；释放后三条信号按 reset_n、req、ack 的原
顺序连续位于 state 前，原有 Group 归属不变。一次 Undo 回到 Saved，Redo 恢复整个连续块。

再次开始同一拖动并按 Esc，三条信号、选择、Saved 和已有 Redo 都不变；只在名称上单击而不拖动，
则明确只保留所点击信号。用户不需要猜测一次动作会移动一条还是三条。

显示 Handshake signals 后，把同三条选择拖到 Group 标题。标题整行蓝色反馈直接写明
Move 3 selected signals into Handshake signals；释放后 reset_n 迁入，req、ack 保持原成员，
状态栏显示 1 moved、2 already there。一次 Undo 精确恢复，Event、Relation 与波形内容未变。
```

长期 Goal 保持 active；第 710 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 711–720 轮：让多选信号可以精确键盘步移

本轮补齐整组拖动之后的精确位置调整。`Alt+↑/↓` 对标题多选执行一次可见行步移，不要求重新
瞄准插入线；隐藏行不会消耗按键，单信号和 Group 的既有快捷键语义不变。

1. 第 711 轮：审计 Edit 菜单、快捷键、标题多选和重排调用链，确认多选分支会提前返回并禁用
   两个动作，实际执行路径只能构造单信号 `MoveLaneCommand`。
2. 第 712 轮：定义“一个位置”为相邻未选的可见画布行；隐藏 Lane、隐藏 Group 和折叠成员不
   产生无视觉变化的步移，所选信号仍按 Scenario 顺序组成连续块。
3. 第 713 轮：新增批量步移目标计算，验证全部选择唯一、可见、非 Group 且仍显示；向上取首个
   所选信号之前的相邻可见行，向下取最后所选信号之后的相邻可见行。
4. 第 714 轮：多选 `Alt+↑/↓` 改用 `MoveLanesCommand` 原子提交，保留相对顺序、Lane 内容和
   各自 Group 归属；活动信号与全部标题高亮在提交后继续保留。
5. 第 715 轮：Edit 菜单动作动态显示 `Move N selected signals up/down`，保留原
   `Alt+↑/↓` 快捷键，并在 Target 提示中直接公开“移动一行”，无需记忆隐藏功能。
6. 第 716 轮：到达可见顶部或底部时对应动作立即禁用；直接调用也只给出明确边界反馈，不新增
   Undo 状态、不截断历史。
7. 第 717 轮：键盘步移登记命令前后标题选择快照；Undo/Redo 同时恢复模型、选择、活动信号和
   Saved 状态，不退化为单信号。
8. 第 718 轮：扩展 `wave-group-batch-smoke`，使用真实 Alt+方向键覆盖隐藏 Group 跳过、上下
   步移、动态菜单、顶部边界、Undo/Redo 和事件/Relation/Group 归属保持，并保存离屏截图。
9. 第 719 轮：回归单信号 Alt+方向键、鼠标整组拖动、自动滚动、Group 折叠/归组、批量清理和
   Wave Edit，确认新分支只在有效标题多选时生效。
10. 第 720 轮：执行默认、Qt Creator Debug、Qt Creator Release 三配置全量验收，复核截图并
    同步 README、PLAN、GOAL。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --parallel 4
73/73 tests passed
Total Test time: 9.84 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure --parallel 4
73/73 tests passed
Total Test time: 9.00 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure --parallel 4
73/73 tests passed
Total Test time: 9.24 sec

Core: 50/50 tests passed
Targeted: wave-group-batch-smoke, wave-lane-reorder-smoke,
          wave-lane-autoscroll-smoke, wave-group-header-smoke and
          wave-core-tests passed
Screenshot: build/group-batch-keyboard-step.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：批量步移先按当前 Scenario 验证并归一化选择，再以画布 `isLaneDisplayed` 结果
寻找相邻目标。因而隐藏 Group 位于模型索引之间时不会出现“按了一次但画面没动”；边界也按用户
实际看到的首末行计算。最终仍由已验证的 `MoveLanesCommand` 生成完整 Lane 快照，Event、
Relation、Segment、ClockDomain、波形数据和 `groupId` 不变。动作禁用和执行使用同一目标计算，
不会出现菜单声称可用但提交失败的分叉。

用户视角验收：

```text
用户选择 reset_n、req、ack 后，Edit 菜单直接显示 Move 3 selected signals up/down，Target
提示 Alt+Up/Down moves one row。按 Alt+Down，隐藏的 Handshake Group 不消耗按键，画面中
data[7:0] 一次移动到选择块上方，三条信号保持 reset_n、req、ack 顺序和共同高亮；状态栏说明
只移动一个可见行且 Group memberships kept。

一次 Undo 同时恢复 Saved、原顺序和活动 ack，Redo 精确恢复，再次 Undo 回到基线。按 Alt+Up
后选择块越过 clk 到达可见顶部，Move up 立即禁用；再次请求只显示 already at the visible top
boundary，模型和 Undo 数量不变。原单信号 Alt+方向键专项仍按原位置和文案通过。
```

长期 Goal 保持 active；第 720 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 721–730 轮：让多选信号可以一次复制

本轮消除复制相似接口信号时逐条执行 `Ctrl+D`、再重新多选的重复步骤。标题多选可直接复制为
一个连续副本块；身份、时钟和依赖语义保持明确，整个结果只占一个 Undo 状态。

1. 第 721 轮：审计单信号复制对名称、颜色、Lane/Segment 身份、ClockDomain、Event、
   Relation、Trace 映射和插入位置的实际语义。
2. 第 722 轮：固定批量契约：按 Scenario 顺序复制，在最后一个源信号后插入连续块；非连续
   点击顺序不会改变副本顺序。
3. 第 723 轮：新增 `DuplicateLanesCommand`，同时保存 Lane 列表和 ClockDomain 列表的前后
   快照，将全部副本作为一个命令提交。
4. 第 724 轮：每个副本生成唯一名称、新 Lane ID 和新 Segment ID，并基于当前画布重新分配
   可读颜色；波形、常用属性、扩展字段和 Group 归属保持。
5. 第 725 轮：Clock 副本必须携带唯一且有效的独立 ClockDomain；普通信号继续引用原有关联
   时钟，不因同批次包含 Clock 而被隐式重连。
6. 第 726 轮：标题多选的 `Ctrl+D`、Edit 菜单和右键菜单统一启用，动作文字显示选择数量；
   F2 仍明确限定单信号，不再错误宣称复制也只能单信号。
7. 第 727 轮：提交后全部副本立即成为标题多选，活动副本对应原活动信号；命令前后选择快照
   进入历史，Undo/Redo 恢复源/副本选择、活动信号和 Saved。
8. 第 728 轮：明确不复制 Event、Relation 和 Imported Trace 映射，避免生成没有用户确认的
   新依赖；重复/既有身份、Group 源、无效归组、缺失 ClockDomain 和越界插入在提交前拒绝。
9. 第 729 轮：新增核心批量复制测试并扩展 `wave-group-batch-smoke`，真实覆盖快捷键、菜单、
   连续块、颜色、身份、波形、归组、依赖边界、Undo/Redo 和离屏截图。
10. 第 730 轮：执行默认、Qt Creator Debug、Qt Creator Release 三配置全量验收，复核截图并
    同步 README、PLAN、GOAL。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 36.91 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 35.30 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 33.98 sec

Core: 51/51 tests passed
Targeted: wave-group-batch-smoke, wave-signal-header-smoke,
          wave-edit-menu-smoke and wave-core-tests passed
Screenshot: build/group-batch-duplicate.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：`DuplicateLanesCommand` 要求 Scenario 属于当前 Project，并在修改前验证所有新
Lane、Segment 和 ClockDomain 身份唯一。每个 Clock Lane 必须恰好对应一个新增且有效的独立
时钟域，额外或缺失时钟均拒绝；普通信号的非空时钟引用必须仍存在。命令只替换 Scenario 的
Lane 列表和 Project 的 ClockDomain 列表，Event、Relation、Marker、Trace 与其他 Scenario
不参与推断或改写。副本规划先在临时 Scenario 中逐条登记，因此同批次名称和随机颜色也能看到
前面已规划的副本。

用户视角验收：

```text
用户选择 reset_n、req、ack 后，Edit 菜单直接显示 Duplicate 3 selected signals，按 Ctrl+D
即可在 ack 下方得到 reset_n_copy、req_copy、ack_copy 连续三行，不需要重复三次复制或重新排序。
副本颜色与源信号不同，但每条波形、属性和原 Group 归属保持；Target 显示 3 signals，活动信号
对应 ack_copy。状态栏明确说明 Event、Relation 和 Trace links not copied。

一次 Undo 删除整个副本块、恢复原三条标题多选并回到 Saved；Redo 使用相同稳定身份恢复副本块
和副本多选，再次 Undo 精确回到基线。右键菜单同时提供 Duplicate selected signals，F2 仍只
提示 rename requires one signal。离屏截图确认连续布局、共同高亮、随机颜色、刻度和波形可读。
```

长期 Goal 保持 active；第 730 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 731–740 轮：让 Ctrl+D 始终跟随可见编辑目标

本轮消除“整信号复制”和“范围重复”同时存在却使用不同入口的目标冲突。用户只需要确认画布上
当前高亮的是时间范围还是信号标题，`Ctrl+D` 和 Edit 菜单即执行对应复制，不再要求记住第二个
动作名称或先退出范围。

1. 第 731 轮：审计整信号、标题多选、Segment 与显式多信号范围的复制入口，确认同一个
   `Ctrl+D` 过去会在可见范围存在时仍尝试复制整信号。
2. 第 732 轮：固定目标优先级为“显式时间范围 > 标题单选/多选 > 当前 Segment 所属信号”；
   文本编辑焦点继续阻止快捷键修改模型。
3. 第 733 轮：Edit 菜单删除独立的 `DuplicateRangeAfterAction`，只保留一个
   `DuplicateLaneAction`；其文字、可用状态和说明随可见目标动态切换。
4. 第 734 轮：主窗口复制分发先检查显式范围，范围存在时直接调用既有原子范围重复；否则继续
   使用单信号或批量信号复制路径。
5. 第 735 轮：画布按键分发采用同一优先级，确保焦点位于画布时真实 `Ctrl+D` 不会绕过动态
   Edit 动作或要求先按 Esc。
6. 第 736 轮：保持既有范围重复契约：多信号顺序、隐式空白、Segment 元数据、End 自动延长、
   Relation 清理和结果范围选择均不退化，系统剪贴板不改变。
7. 第 737 轮：零变化重复只把范围推进到结果位置并给出 `no values changed`，不新增 Undo、
   不清除 Redo；有效修改仍只形成一个 `Duplicate range` 历史状态。
8. 第 738 轮：统一范围工具提示、固定编辑栏和状态栏中的 `Ctrl+D repeats` 反馈；信号标题右键
   直接请求整信号复制时仍保留范围并说明按 Esc。
9. 第 739 轮：扩展 `wave-wave-edit-smoke`、`wave-canvas-add-lane-smoke` 和 Edit 菜单契约，
   使用真实快捷键覆盖动作唯一性、目标优先级、剪贴板隔离、零变化及 Undo/Redo；全量回归发现
   隐藏动作在范围状态下静默禁用后，恢复为可触发的无损说明并由 `wave-hidden-lane-smoke` 固化。
10. 第 740 轮：执行默认、Qt Creator Debug、Qt Creator Release 三配置全量 offscreen 验收，
    复核范围重复截图并同步 README、PLAN、GOAL。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 39.09 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 36.57 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 35.26 sec

Core: 51/51 tests passed
Targeted: wave-wave-edit-smoke, wave-canvas-add-lane-smoke,
          wave-edit-menu-smoke, wave-hidden-lane-smoke,
          wave-group-batch-smoke and wave-signal-header-smoke passed
Screenshot: build/wave-edit-smoke-duplicate-range-after.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：动态 Edit 动作只有一个创建点和一个快捷键连接点；显式范围分支在标题多选之前
返回，因此不会同时触发两类命令。范围重复继续复用经过验证的完整选择快照和原子命令路径，
未引入第二套复制算法。直接标题右键入口仍保留整信号语义，其范围门禁不会被快捷键分发绕过。
隐藏整项在范围存在时保持动作可发现，但只显示“Esc clears”恢复说明，不清除范围、不修改可见性
或历史。旧动作对象名只保留在测试的“不存在”断言中。

用户视角验收：

```text
用户在 req 和 ack 上选中同一段协议波形后直接按 Ctrl+D，两个信号的完整片段立即出现在选区
后方，结果范围继续高亮；无需先 Copy、移动光标或寻找另一条菜单命令。状态栏说明复制数量、
时间范围、剪贴板未改变、Relation 清理数量和 Ctrl+Z。

一次 Undo 同时恢复原波形、原范围、时间轴和 Relation，Redo 恢复结果范围。后一段已经相同时，
Ctrl+D 只推进选择并明确显示 no values changed，已有 Redo 保持。清除显式范围后，同一个
Ctrl+D 恢复为复制已选单条或多条信号；Edit 菜单文字同步变化，不需要猜测当前目标。

范围存在时请求隐藏整个信号不会静默失效或丢失选择，而是保持画布不变并说明先按 Esc。
离屏截图确认两条结果波形、100–120 ns 结果范围、Target、时间刻度和反馈同时可见。
```

长期 Goal 保持 active；第 740 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 741–750 轮：让范围重复成为可见的一步操作

本轮不增加新的波形语义，而是调整固定范围栏的主次关系。范围重复已经是绘制周期性协议片段的
直接路径，但用户只能记住 `Ctrl+D` 或进入菜单；同时低频且会清除源内容的 Cut 长期占据主栏。
固定栏现用 `Repeat` 取代 Cut，按钮总数不增加，Cut 的标准入口继续保留。

1. 第 741 轮：从当前离屏截图与固定范围栏验收审计主操作顺序，确认 Copy/Cut/Paste/Clear
   全部可见，但“紧邻重复且不改剪贴板”没有可见入口。
2. 第 742 轮：固定交互契约：范围栏显示 Copy/Repeat/Paste/Clear；Cut 不再占主栏，但
   `Ctrl+X`、Edit 菜单和选区右键仍完整可用。
3. 第 743 轮：删除 `RangeEditCutButton` 成员和创建路径，新增带明确文字的
   `RangeEditRepeatButton`，避免使用可能与 Redo 混淆的纯图标。
4. 第 744 轮：Repeat 直接调用既有 `duplicateSelectionAfter()`，不建立第二套复制算法，
   因而继续继承多信号、隐式空白、End 延长、Relation 清理和历史选择语义。
5. 第 745 轮：按钮提示公开“immediately after”“clipboard unchanged”和 `Ctrl+D`，
   accessible name 明确为 Repeat selected range after；混合类型摘要改为
   Copy/Repeat/Paste/Clear。
6. 第 746 轮：扩展 Wave Edit 流程，确认旧 Cut 按钮不存在，Repeat 可见、启用且可命中；
   实际多信号重复由按钮触发，零变化分支继续使用真实 `Ctrl+D`。
7. 第 747 轮：原固定栏 Cut 用例改从 Edit 动作执行，并保留文本焦点 Cut、选区右键 Cut、
   原子源清理、剪贴板内容及 Undo 恢复覆盖，证明功能只是退出主栏而非被删除。
8. 第 748 轮：首次 960 像素专项准确捕获 Repeat 文字增加造成的裁切；保持文字入口，
   仅把范围按钮横向留白从 8 px 调为 7 px、控件间距从 5 px 调为 4 px，恢复全部中心命中。
9. 第 749 轮：执行 Wave Edit、长时间轴自动滚动、用户旅程、Edit 菜单和波形专注布局专项，
   并复核 `build/wave-edit-smoke-duplicate-range-after.png` 中 Repeat、结果范围和波形。
10. 第 750 轮：执行默认、Qt Creator Debug、Qt Creator Release 三配置全量 offscreen 验收，
    同步 README、PLAN、GOAL。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 37.93 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 35.74 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 34.95 sec

Core: 51/51 tests passed
Targeted: wave-wave-edit-smoke, wave-wave-edit-autoscroll-smoke,
          wave-user-journey-smoke, wave-edit-menu-smoke and
          wave-waveform-only-smoke passed
Screenshot: build/wave-edit-smoke-duplicate-range-after.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：`RangeEditRepeatButton` 只有一个创建点，并直接连接经过第 731–740 轮验证的
`duplicateSelectionAfter()`；范围栏没有保留隐藏 Cut 控件或重复动作状态。旧对象名只存在于
“确认其不存在”的 GUI 断言中。Cut 的主窗口 QAction、`Ctrl+X` 文本焦点分发和范围右键动作
均未删除；实际范围 Cut 仍产生原命令、原剪贴板快照和单步 Undo。紧凑样式仅作用于范围浮栏，
没有改变画布、其他工具栏或点击高度。

用户视角验收：

```text
用户框选 req 与 ack 的协议片段后，顶部直接看到 Copy、Repeat、Paste、Clear。点击 Repeat，
两条波形一次复制到紧邻后方，结果范围继续高亮；系统剪贴板保持原文本，状态栏说明范围、
Relation 清理和 Ctrl+Z。无需先复制、移动光标、粘贴，也无需记忆快捷键。

再次点击 Repeat 可从当前结果继续重复；目标已经一致时只推进选择，不制造空历史。需要移动而非
复制时仍直接拖动范围；确实需要 Cut 时使用 Ctrl+X、Edit 或右键，源清理和 Undo 行为不变。

在 960 像素窗口下，Bus/混合范围的摘要、Copy、Repeat、Paste、Clear、值输入和全部有效预设
仍完整位于固定栏内，按钮中心可实际命中。离屏截图确认文字入口、选区、刻度、波形和结果反馈
同时可见。
```

长期 Goal 保持 active；第 750 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 751–760 轮：在点击 Paste 前识别不兼容目标

本轮继续收敛固定范围栏的错误预防。此前 Paste 按钮只检查剪贴板是否包含 Lane，数量、类型、
Bus/Enum 位宽或值不兼容时仍显示为可点击；用户必须执行一次失败操作后才能知道当前目标不可用。
固定栏现复用完整粘贴契约进行只读预检，并把修正条件直接放入禁用提示；菜单和快捷键仍保留
命令执行时的原子门禁。

1. 第 751 轮：审计固定范围栏、`Ctrl+V`、Edit 和右键四个 Paste 入口，确认固定按钮此前仅按
   clipboard lane 数启用，未验证显式目标。
2. 第 752 轮：确定点击前契约，覆盖 schema、正复制宽度、目标时间溢出、目标数量、Lane 存在性、
   类型、Bus/Enum 位宽、Segment 几何和值域；预检保持只读。
3. 第 753 轮：新增统一 `rangePasteAvailability()`，返回启用状态和提示；固定按钮不再复制一套
   零散判断。
4. 第 754 轮：同时支持自描述 schema 2 与源信号仍存在时的 schema 1，损坏或过期内容直接说明
   不可用原因。
5. 第 755 轮：按复制与目标的可见顺序逐项验证，数量、Bit→Bus、Bus 位宽和值域失败均在按钮上
   指出具体源→目标配对，不修改范围、模型或历史。
6. 第 756 轮：有效提示提前公开复制信号数、完整复制宽度、固定选区左端点、逐信号映射、当前
   选区宽度差异和必要的 End 延长，点击前即可确认实际结果。
7. 第 757 轮：扩展 GUI 回归，固定按钮在 2→1 数量不匹配时可见但禁用；直接触发 Edit 动作仍由
   运行时门禁拒绝，证明按钮预检不能替代命令层保护。
8. 第 758 轮：增加 Bit→Bus、8-bit→4-bit Bus 和非法 Bit 值剪贴板用例，验证零写入、明确提示及
   旧 schema 1 兼容路径。
9. 第 759 轮：执行 Wave Edit、自动滚动、添加信号、用户旅程、Edit 菜单与纯波形布局专项，并
   复核 `build/wave-edit-smoke-multi-target-paste.png` 的双目标粘贴结果和固定栏布局。
10. 第 760 轮：执行默认、Qt Creator Debug、Qt Creator Release 三配置全量 offscreen 验收，
    同步 README、PLAN、GOAL。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 37.57 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 35.08 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 33.78 sec

Core: 51/51 tests passed
Targeted: wave-canvas-add-lane-smoke, wave-wave-edit-smoke,
          wave-wave-edit-autoscroll-smoke, wave-user-journey-smoke,
          wave-edit-menu-smoke and wave-waveform-only-smoke passed
Screenshot: build/wave-edit-smoke-multi-target-paste.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：预检 helper 只读取当前范围、剪贴板和 Scenario，不创建命令或修改选择；固定按钮
只有一个启用状态来源。实际 `pasteAtCursor()`、Edit 动作、`Ctrl+V` 和右键粘贴仍执行完整
运行时校验，因此剪贴板在提示后变化、目标被删除或调用方绕过按钮时仍保持原子拒绝。schema 1
只在原源信号仍可解析时通过；schema 2 使用快照元数据，不重新依赖已删除源。End 延长与零变化
历史抑制语义未改变。

用户视角验收：

```text
用户复制 req 与 ack 两条范围后只选中一个目标，Paste 仍在固定栏原位可见，但处于禁用状态；
提示直接说明复制了 2 条、当前只选 1 条，并要求选择相同数量，不需要执行一次失败操作。

选中两个兼容 Bit 目标后，按钮立即启用，提示提前显示 10 ns 完整宽度、20 ns 固定落点、
req/ack 的逐项映射，以及当前两拍选区宽于复制内容。点击后两条结果一次写入，目标范围继续
高亮，状态栏显示 Pasted、目标数量、时间、宽度和 Ctrl+Z。

把同一 Bit 内容指向 Bus 时，提示明确显示 req (bit) → data[7:0] (bus)；把 8-bit 内容指向
4-bit Bus 或使用非法 Bit 值时，也分别说明确切位宽和值域原因。按钮不会先写入再回滚，
原选择、波形、Saved 和 Undo 历史保持不变。离屏截图确认固定栏、双目标结果、刻度、网格和
反馈同时可辨认。
```

长期 Goal 保持 active；第 760 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 761–770 轮：标题多选直接成为 Paste 目标

本轮消除多信号 Paste 中一个由旧交互造成的额外步骤。标题区域已经支持单选、`Ctrl` 增减和
`Shift` 连选，但没有显式时间范围时，Paste 仍忽略这些目标并回写复制时的原信号集合。用户必须
重新跨目标信号框出任意宽度的时间矩形，虽然 Paste 随后只采用其左端点并忽略所框宽度。标题选择
现直接成为编辑光标处的目标，不增加模式、按钮或第二套粘贴命令。

1. 第 761 轮：审计 `pasteAtCursor()` 的目标分派，确认显式范围支持多目标、单复制支持当前单信号，
   但多标题选择未进入任何目标分支。
2. 第 762 轮：固定目标优先级为显式时间范围、标题选择、单信号目标、无显式目标时的原复制信号
   集合，避免标题选择与既有同源重复粘贴互相覆盖语义。
3. 第 763 轮：标题选择数量必须与复制信号数完全相同，并按当前画布可见顺序一一映射；单标题面对
   多信号复制时明确拒绝，不再静默修改原信号。
4. 第 764 轮：把第 751–760 轮的 schema、宽度、类型、Bus/Enum 位宽和数值预检抽为目标参数化
   helper，固定范围栏与标题提示共用同一契约。
5. 第 765 轮：标题选择状态栏在 clipboard 可用时直接显示 Ctrl+V、目标数量和格式化光标落点；
   数量或类型不兼容时显示具体源→目标原因。
6. 第 766 轮：`Target` 摘要为单/多标题补充光标时间，工具提示说明 `Ctrl+G` 定位、
   `Ctrl+V` 按可见顺序粘贴；现有多选移动、归组、复制和清理入口保持。
7. 第 767 轮：成功 Paste 将标题集合转换为完整结果时间范围，清除陈旧 Segment/悬浮状态并保持
   既有单命令、End 延长、Relation 清理和零变化历史抑制。
8. 第 768 轮：选择历史记录标题集合与光标；Undo 恢复原标题多选，Redo 恢复粘贴结果范围。
   多标题复制、隐藏和删除的恢复反馈同步改为明确数量，不再退化为单活动目标描述。
9. 第 769 轮：扩展 `wave-wave-edit-smoke`，真实覆盖 2→1 拒绝、Bit→Bus 配对拒绝、精确定位保持、
   两标题有效粘贴及 Undo/Redo；复核 `build/wave-edit-smoke-header-target-paste.png`。
10. 第 770 轮：执行标题批量、添加信号、自动滚动、用户旅程、Edit 菜单专项及默认、
    Qt Creator Debug、Qt Creator Release 三配置全量 offscreen 验收，同步 README、PLAN、GOAL。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 37.65 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 35.24 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 33.41 sec

Core: 51/51 tests passed
Targeted: wave-wave-edit-smoke, wave-group-batch-smoke,
          wave-canvas-add-lane-smoke, wave-wave-edit-autoscroll-smoke,
          wave-user-journey-smoke and wave-edit-menu-smoke passed
Screenshot: build/wave-edit-smoke-header-target-paste.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：目标参数化预检只读取 clipboard、Scenario、目标集合和落点；固定范围栏仍是
同一调用方。`pasteAtCursor()` 只在 `laneHeaderSelectionActive_` 明确成立时采用标题目标，
因此普通单信号波形点击与无目标的原集合重复路径不变。标题数量、类型、位宽和值失败发生在
命令构造前；执行层继续逐项校验。成功路径仍使用原 `PasteRangeCommand`，没有第二套 Segment
写入算法。历史快照已有标题多选字段，本轮只让 Paste 记录该既有状态并在结果中关闭标题模式。

用户视角验收：

```text
用户复制 req 与 ack 的 10 ns 协议片段后，单击 reset_n 标题会立即看到“复制 2 条、当前只选
1 条”的 Ctrl+V 不可用提示；此时执行 Paste 不会把内容回写 req/ack。再选择 data[7:0] 时，
状态栏按顺序指出 ack (bit) → data[7:0] (bus) 类型不匹配，波形和 Undo 保持不变。

用户移除 data[7:0]，保留 reset_n，使用精确时间定位到 0 ps 后再 Ctrl 选择 req。Target 显示
2 signals、active req 和 0 ps，状态栏直接说明 Ctrl+V。执行后复制的两条内容按可见顺序一次
写入 reset_n/req 的 0–10 ns，结果范围继续高亮，可立即 Repeat、Clear 或批量改值。

一次 Undo 恢复 reset_n/req 标题多选和 0 ps 光标，Redo 恢复 0–10 ns 结果范围；不需要重新框选
一个最终会被忽略宽度的目标矩形。离屏截图确认两条结果波形、固定栏、时间刻度、网格、信号名和
Pasted/Ctrl+Z 反馈同时可见。
```

长期 Goal 保持 active；第 770 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 771–780 轮：标题右键 Paste 与逐项映射确认

本轮解决标题目标 Paste 的可发现性与提交前确认问题。第 761–770 轮已经允许标题选择后按
`Ctrl+V`，但用户必须记住快捷键，且有效状态提示没有完整列出每一项源→目标配对。现在只要
剪贴板包含 Wave 范围，既有标题右键菜单即提供 Paste；目标、时间、宽度、映射与失败原因均在
提交前可见，不增加新模式、顶层按钮或第二套粘贴实现。

1. 第 771 轮：审计标题目标 Paste 的入口，确认运行路径完整，但右键菜单没有动作，主要依赖
   用户记忆 `Ctrl+V`；有效状态也只给出数量和落点，无法核对逐项映射。
2. 第 772 轮：固定交互契约为复用现有信号标题右键菜单；Wave 剪贴板不存在时不增加无关菜单项，
   存在时将 Paste 置于菜单顶部，不创建新工具栏按钮或编辑模式。
3. 第 773 轮：公开标题选择专用的 Paste availability 查询，并复用目标参数化预检；状态、菜单、
   快捷键执行层由同一 schema、数量、类型、位宽和值规则驱动。
4. 第 774 轮：菜单动作文字直接显示单个目标名称或多目标数量及格式化落点，例如
   `Paste copied range into 2 selected signals at 0 ps`，并保留标准 `Ctrl+V` 快捷键标识。
5. 第 775 轮：复制数量与标题数量不一致时不隐藏动作；动作保持可见但禁用，工具提示准确说明
   复制数量、目标数量和所需修正，不允许退回原复制信号集合。
6. 第 776 轮：按可见顺序逐项检查类型；Bit→Bus 等错误直接显示
   `ack (bit) → data[7:0] (bus)` 配对及失败原因，零部分写入。
7. 第 777 轮：有效动作的工具提示和状态栏增加复制宽度、精确落点与全部
   `source → target` 映射；多目标顺序无需执行后再从波形反推。
8. 第 778 轮：菜单动作触发既有 `pasteAtCursor()`，继续使用 `PasteRangeCommand`、End 原子延长、
   Relation 清理、零变化抑制和选择感知 Undo/Redo；菜单打开后执行层仍重新校验。
9. 第 779 轮：扩展 `wave-wave-edit-smoke`，真实创建并检查标题右键菜单的有效与禁用动作，
   触发有效动作并复核 `build/wave-edit-smoke-header-paste-menu.png`；
   Wave Edit、标题批量、添加信号、自动滚动、用户旅程和 Edit 菜单六项专项通过。
10. 第 780 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen 回归，同步 README、PLAN、GOAL，并执行开发视角与用户视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 36.20 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 33.85 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 32.99 sec

Core: 51/51 tests passed
Targeted: wave-wave-edit-smoke, wave-group-batch-smoke,
          wave-canvas-add-lane-smoke, wave-wave-edit-autoscroll-smoke,
          wave-user-journey-smoke and wave-edit-menu-smoke passed
Screenshots: build/wave-edit-smoke-header-paste-menu.png
             build/wave-edit-smoke-header-target-paste.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：`selectedLanePasteAvailability()` 只在标题选择有效时调用通用
`pasteAvailabilityForTargets()`，没有复制 schema、兼容性规则或写入算法。菜单动作只在系统
剪贴板含 `application/x-wave-workbench-range+json` 时出现，失效动作仍保留完整原因；
`pasteAtCursor()` 在触发时再次读取当前剪贴板和目标，因此菜单打开后状态变化不会绕过校验。
成功路径仍进入既有单命令，Undo 恢复标题集合与光标，Redo 恢复结果范围。

用户视角验收：

```text
用户复制 req 与 ack 后，在左侧只选择 reset_n 并右键，无需记忆快捷键即可在菜单顶部看到
“Paste copied range into reset_n at 0 ps”；动作禁用，提示明确指出复制 2 条、当前目标 1 条。

用户再选择 reset_n 与 data[7:0] 后右键，动作仍在原位置但不可执行，提示精确指出
ack (bit) → data[7:0] (bus) 类型不匹配。错误不会关闭选择、改写原信号或新增 Undo。

用户改选 reset_n 与 req 后右键，动作文字显示 2 个目标与 0 ps；工具提示在提交前显示
10 ns 复制宽度、req → reset_n、ack → req。单击一次完成 0–10 ns 粘贴，结果范围保持选中，
可继续 Repeat、Clear 或批量赋值；一次 Undo/Redo 在原标题集合与结果范围之间往返。

离屏菜单截图确认 Paste 位于既有信号操作之前，文字和快捷键无裁切；结果截图确认目标波形、
固定范围栏、刻度、网格和 Pasted/Ctrl+Z 反馈同时可见。
```

长期 Goal 保持 active；第 780 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 781–790 轮：标尺定位保持编辑目标与取消恢复

本轮消除鼠标时间定位与标题 Paste 目标之间的冲突。标题单选/多选已经能够作为粘贴目标，但
Wave Edit 中最直觉的标尺单击或拖动会无条件关闭标题选择；用户只能记住 `Ctrl+G` 才能保留目标。
现在标尺只改变时间，标题目标、顺序和 Paste 预检保持；误拖取消同时恢复此前被临时关闭的选择。

1. 第 781 轮：审计标尺按下路径，确认 `laneHeaderSelectionActive_ = false` 在移动光标前无条件执行，
   导致“复制→选目标→点标尺→Paste”在第三步退化为无目标粘贴。
2. 第 782 轮：固定交互契约为标尺定位不改变标题目标；显式时间范围正常释放时仍按既有规则关闭，
   不把两种选择语义混合，也不增加定位模式。
3. 第 783 轮：移除标尺对标题选择的清理；单击与连续拖动均保留活动信号、完整标题集合及可见顺序。
4. 第 784 轮：标尺状态反馈在拖动与释放时显示保留的目标数量，并复用 `laneHeaderPasteHint()`，
   使复制宽度、当前落点和每项源→目标映射随时间实时更新。
5. 第 785 轮：标尺悬浮提示在标题目标存在时明确说明目标不会丢失，并公开右键所选名称或
   `Ctrl+V` 在当前时间 Paste；平移、缩放和横向滚动说明继续保留。
6. 第 786 轮：在任何显式范围清理之前保存完整标尺交互快照，覆盖活动信号、标题集合、Segment、
   时间范围、编辑光标及选择类型，而非只保存一个 tick。
7. 第 787 轮：统一 Esc 与异常失去左键的取消路径；恢复原光标、水平视图、标题目标，若按下前为
   显式范围则恢复原范围、全部信号及固定范围栏，零模型和零命令变化。
8. 第 788 轮：将标题 Paste 自动化改为真实鼠标流程：复制 req/ack，选择 reset_n/req，
   标尺定位 40 ns，再从标题右键菜单粘贴为 40–50 ns；Undo/Redo 继续恢复对应目标。
9. 第 789 轮：扩展 `wave-wave-edit-smoke` 覆盖目标保持、Esc 恢复、显式范围恢复、动态提示和真实
   Paste；复核 `build/wave-edit-smoke-header-ruler-target.png` 与结果截图，并通过七项关联专项。
10. 第 790 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen 回归，同步 README、PLAN、GOAL，并执行开发视角与用户视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 37.46 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 33.63 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 32.85 sec

Core: 51/51 tests passed
Targeted: wave-wave-edit-smoke, wave-wave-edit-autoscroll-smoke,
          wave-group-batch-smoke, wave-signal-header-smoke,
          wave-cursor-mode-smoke, wave-user-journey-smoke
          and wave-edit-menu-smoke passed
Screenshots: build/wave-edit-smoke-header-ruler-target.png
             build/wave-edit-smoke-header-target-paste.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：标尺交互快照只存在于按下到释放/取消之间，不进入工程、命令栈、autosave 或文档
上下文。`updateRulerScrub()` 继续清理不再适用的 Segment/范围暂态并移动编辑光标，但不再清除标题
选择；状态中的 Paste 信息调用已有目标参数化预检。正常释放丢弃快照，成功 Paste 仍进入既有
`PasteRangeCommand`。取消统一恢复快照和原水平滚动量，并按恢复后的选择类型重新显示或隐藏固定
范围栏；没有新的波形写入路径。

用户视角验收：

```text
用户复制 req 与 ack 的 10 ns 片段，在左侧选择 reset_n 与 req。直接单击标尺 40 ns 后，两条标题
仍保持高亮，Target 显示 active req · 40 ns，状态栏立即显示 req → reset_n、ack → req。

用户先把标尺拖到 60 ns，发现位置不对后按 Esc；两条目标、原光标和水平视图一次恢复，既不需要
重新选信号，也不产生 Undo。标尺提示明确说明选中目标会保留及当前时间可 Paste。

用户再次单击 40 ns，右键任一所选名称并选择顶部 Paste，一次完成 40–50 ns 双信号写入。结果范围
保持选中，可继续 Repeat、Clear 或批量赋值；Undo 恢复两标题与 40 ns 光标，Redo 恢复结果范围。

若用户从一个显式范围开始误拖标尺并按 Esc，原时间范围、信号集合和固定范围栏完整返回。离屏
截图确认双标题高亮、40 ns 光标、Target、映射状态及最终 40–50 ns 波形均清晰可辨。
```

长期 Goal 保持 active；第 790 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 791–800 轮：结果范围重定位与同目标连续 Paste

本轮消除第一次 Paste 后选择非相邻落点时必须重新选择信号的问题。粘贴结果此前保持为显式范围，
但从该范围单击标尺会在定位前清空全部目标；第二次 Paste 因而可能退回源信号。现在只有标尺重定位
会把结果范围转换为相同顺序的标题目标，新时间、目标和映射同步刷新，可直接连续 Paste。

1. 第 791 轮：审计显式范围的标尺按下路径，确认 `clearExplicitRangeSelection()` 默认同时清空
   `selectedLaneIds_`，使粘贴结果无法复用为下一次目标。
2. 第 792 轮：固定交互契约为仅标尺正常重定位执行“范围→标题目标”；标题、波形正文和其他范围
   清理路径继续沿用原语义，不扩大持久选择范围。
3. 第 793 轮：标尺重定位关闭时间范围与固定栏，但保留可见顺序的全部 Lane、活动 Lane，并重新
   激活标题选择，防止 Paste 回退到复制源集合。
4. 第 794 轮：`Target`、状态栏和标题 Paste 预检立即使用新标尺时间，明确显示保留的目标数量与
   逐项源→目标映射；该转换不修改 Scenario、Saved 或命令栈。
5. 第 795 轮：第二次 Paste 复用既有 clipboard、目标参数化预检和 `PasteRangeCommand`，不新增
   剪贴板格式、波形写入算法或特殊重复命令。
6. 第 796 轮：offscreen 自动化复制 req/ack、选择 reset_n/req，在 40 ns 完成第一次 Paste，
   再从结果范围真实单击标尺 60 ns，断言范围关闭而两个标题目标保持。
7. 第 797 轮：不重新选择信号直接执行第二次 Paste，断言新增 60–70 ns 结果且原 40–50 ns 结果
   保留，未匹配的 ack 不变，也未发生源信号回写。
8. 第 798 轮：扩展选择感知历史链，连续两次 Undo 分别恢复 60 ns、40 ns 的标题目标，连续两次
   Redo 分别恢复两段结果；最终两次 Undo 回到基线且目标仍可继续使用。
9. 第 799 轮：复核范围重定位与连续 Paste 离屏截图，并通过 Wave Edit、自动滚动、标题批量、
   信号标题、光标模式、用户旅程和 Edit 菜单七项关联专项。
10. 第 800 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen 回归，同步 README、PLAN、GOAL，并执行开发视角与用户视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 36.15 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 34.22 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 33.34 sec

Core: 51/51 tests passed
Targeted: wave-wave-edit-smoke, wave-wave-edit-autoscroll-smoke,
          wave-group-batch-smoke, wave-signal-header-smoke,
          wave-cursor-mode-smoke, wave-user-journey-smoke
          and wave-edit-menu-smoke passed
Screenshots: build/wave-edit-smoke-range-ruler-retarget.png
             build/wave-edit-smoke-header-repeated-paste.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：改动只调整显式范围被标尺重定位时的选择清理参数，并恢复已有标题选择标志。
正常定位关闭范围、范围值草稿和固定栏，但不改模型；Esc 或异常失去左键仍通过第 781–790 轮的
完整快照恢复原范围。连续 Paste 继续调用同一预检和 `PasteRangeCommand`，每个真实写入保持一个
命令，空结果与不兼容目标仍在提交前拒绝。

用户视角验收：

```text
用户只复制一次 req/ack，并只选择一次 reset_n/req。在标尺 40 ns Paste 后，40–50 ns 结果范围
保持可见。用户直接单击标尺 60 ns，范围栏关闭，但两条信号名仍高亮，Target 显示 2 signals、
active reset_n 和 60 ns，状态栏说明 range cleared、2 selected signal targets kept。

此时无需重新选择信号，直接再次 Paste 即写入 60–70 ns；先前 40–50 ns 内容仍在，映射继续为
req → reset_n、ack → req，不会退回复制源。两次 Undo 按 60 ns、40 ns 顺序撤销，两次 Redo
按相同顺序恢复，用户能从选区和状态反馈确认每一步结果。

若用户从结果范围开始拖动标尺后按 Esc，原结果范围、固定栏、信号集合和视图完整恢复，既不修改
波形，也不产生 Undo。
```

长期 Goal 保持 active；第 800 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 801–810 轮：多信号 Paste 提交前波形预览

本轮消除批量 Paste 的最后一个盲操作阶段。此前系统会在状态栏和菜单中列出源→目标映射、宽度与
时间，但目标波形直到提交后才出现；用户仍需先覆盖、再检查、必要时 Undo。现在兼容标题目标会直接
显示真实复制内容的虚线预览，用户可在提交前核对形状、值、范围和落点。

1. 第 801 轮：从连续 Paste 用户路径审计提交前反馈，确认文字映射不能证明两条目标波形的具体
   高低电平或值，错误只能在覆盖后发现。
2. 第 802 轮：固定契约为仅标题目标、Wave Edit、完整预检通过时显示 Paste 预览；显式范围和
   不兼容目标继续保留原行为，不增加模式、按钮或持久状态。
3. 第 803 轮：按 clipboard 有序 Lane 解析相对 Segment、值和扩展元数据，在目标 Lane 副本中
   生成当前标尺时间的预览内容；原 Scenario 与稳定 ID 不参与修改。
4. 第 804 轮：目标区间使用半透明遮罩、虚线波形和虚线边界，Clock、Bit、Bus、Enum 复用各自
   既有绘制语义；标签明确显示 `Paste preview`、目标数量、起止时间和 Ctrl+V。
5. 第 805 轮：左侧标题在预览期间显示 `当前值→预览值`，相同值显示 `=值`，避免标题采样仍显示
   原值而与目标虚线互相矛盾。
6. 第 806 轮：标尺单击与按住拖动实时更新预览范围；自动化断言拖到 60 ns 时预览为 60–70 ns，
   Scenario、Saved 和命令栈保持不变。
7. 第 807 轮：Esc 恢复原光标、目标和预览；数量或类型不匹配时范围与目标访问器均为空；Paste
   成功后预览随标题模式关闭而消失，Undo 恢复对应时间的预览。
8. 第 808 轮：将源夹具改为可区分的 req=0、ack=1，断言有序目标、提交后真实值、连续 Paste、
   两步 Undo/Redo 与既有关系/选择语义一致。
9. 第 809 轮：通过 Wave Edit、自动滚动、标题批量、信号标题、光标模式、用户旅程和 Edit 菜单
   七项关联 offscreen 专项，并复核差异化预览截图。
10. 第 810 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen 回归，同步 README、PLAN、GOAL，并执行开发视角与用户视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 36.42 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 34.73 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 33.56 sec

Core: 51/51 tests passed
Targeted: wave-wave-edit-smoke, wave-wave-edit-autoscroll-smoke,
          wave-group-batch-smoke, wave-signal-header-smoke,
          wave-cursor-mode-smoke, wave-user-journey-smoke
          and wave-edit-menu-smoke passed
Screenshot: build/wave-edit-smoke-header-paste-preview.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：预览首先调用标题目标的既有目标参数化预检；只有 schema、数量、类型、Bus/Enum
位宽、每段值和目标时间全部有效时才解析绘制数据。Segment 只写入临时 Lane 副本，重叠片段按
Paste 的后写覆盖顺序在内存中归一化；绘制路径不调用 CommandStack、模型写接口、稳定 ID 生成、
autosave 或工程格式。clipboard 改变、选择改变和标尺移动均触发重绘，不保存第二份持久状态。

用户视角验收：

```text
用户复制 req=0、ack=1 的 10 ns 片段，选择 reset_n 与 req 并定位 60 ns。提交前已经能看到
60–70 ns 两条虚线波形：reset_n 将从 1 变为 0，req 将从 0 变为 1；左侧同时显示 1→0、0→1，
标签公开两条信号、精确范围和 Ctrl+V。原实线波形和刻度仍可辨认。

用户按住标尺拖动时，虚线结果实时跟随；发现位置不正确后按 Esc，原落点、两条目标和预览一次
恢复，没有 Undo，也没有工程修改。数量或类型选错时只显示明确的不兼容反馈，不显示可能被误认
为有效结果的虚线。

按 Ctrl+V 后虚线变为真实结果范围并停止显示；Undo 回到同一标题目标和同一虚线预览，用户可直接
修正时间后重试。离屏截图确认预览与真实波形、网格、标题值、目标摘要和状态反馈无裁切。
```

长期 Goal 保持 active；第 810 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 811–820 轮：Paste 提交前 Relation 清理预警

本轮消除 Paste 预览与真实副作用之间的信息缺口。此前用户能在提交前看到目标波形，却只能在
提交后从状态栏得知某条 Relation 因引用边沿消失而被清理。现在预览会按真实覆盖结果预测将消失的
波形 Event 及受影响 Relation；有风险的范围在提交前直接改为琥珀色，并公开准确数量与恢复方式。

1. 第 811 轮：审计 Paste 的提交后依赖清理路径，确认预览只证明波形结果，未证明关联时序关系
   是否保留，用户仍需提交后检查。
2. 第 812 轮：固定预检契约为复用真实目标映射和覆盖语义，按提交命令相同的 Event 复用顺序计算
   影响；预检不得修改 Scenario、历史、保存状态、autosave 或稳定 ID。
3. 第 813 轮：在目标 Lane 副本中执行纯内存 clear/set/normalize，构造覆盖后的完整 Segment
   拓扑；临时 Segment ID 避让工程内全部既有 ID。
4. 第 814 轮：将 Segment extensions 的应用顺序对齐 `PasteRangeCommand`，先写入和归一化，
   再更新覆盖段扩展，避免临界相邻段合并与真实提交分叉。
5. 第 815 轮：按 `linkedSegmentId`、`tick+value`、`tick` 的既有优先级模拟波形 Event 复用，
   仅将最终未复用的旧 Event 视为删除候选。
6. 第 816 轮：统计引用删除候选 Event 的 Relation；标题 Paste 可用性、右键菜单和状态栏在提交前
   显示 `removes N relation(s)` 及 `Ctrl+Z` 可恢复波形和关系。
7. 第 817 轮：无影响预览保持蓝色；有依赖清理时范围遮罩、虚线波形、边界、标签和标题差异值统一
   使用琥珀色，原 Relation 连线继续可见。
8. 第 818 轮：自动化同时覆盖安全落点预测 0 且真实保留 Relation，以及风险落点预测 1、真实清理
   1、Undo 精确恢复 Relation、标题目标、光标和同一预警。
9. 第 819 轮：通过 Wave Edit、自动滚动、标题批量、信号标题、光标模式、用户旅程和 Edit 菜单
   七项关联 offscreen 专项，并复核风险预览截图。
10. 第 820 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen 回归，同步 README、PLAN、GOAL，并执行开发视角与用户视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 36.60 sec

cmake --build --preset qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-debug
73/73 tests passed
Total Test time: 34.35 sec

cmake --build --preset qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-release
73/73 tests passed
Total Test time: 33.15 sec

Core: 51/51 tests passed
Targeted: wave-wave-edit-smoke, wave-wave-edit-autoscroll-smoke,
          wave-group-batch-smoke, wave-signal-header-smoke,
          wave-cursor-mode-smoke, wave-user-journey-smoke
          and wave-edit-menu-smoke passed
Screenshot: build/wave-edit-smoke-header-paste-relation-warning.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：预检使用与真实 Paste 相同的目标顺序、范围清除、后写覆盖、Segment 归一化和
extensions 更新顺序。Event 预测复现模型同步时的三段复用优先级，并按 Relation 而非端点计数，
同一 Relation 的两个端点同时消失也只提示一次。全部投影对象位于临时 Lane 副本中；预检未调用
CommandStack、模型写接口、全局 ID 生成器、autosave 或工程序列化。安全预览与真实提交均保留
`relation-req-ack`；风险预览报告 1，真实提交清理 1，Undo 恢复 1，形成可验证闭环。

用户视角验收：

```text
用户复制 req=0、ack=1 的 10 ns 片段，选择 req 与 ack，并在标尺定位 80 ns。提交前目标区间直接
显示为琥珀色，标签写明 “Paste preview · removes 1 relation(s) · 2 signal(s) · 80–90 ns”；
左侧标题同时显示 1→0、0→1，原红色 Relation 连线仍在，因此无需先覆盖再判断依赖影响。

标题右键 Paste 和状态栏也给出相同数量，并明确 Ctrl+Z 会恢复波形和关系。按 Ctrl+V 后系统实际
只清理这一条 Relation，结果范围为 80–90 ns；一次 Undo 同时恢复原波形、Relation、req/ack
标题目标、80 ns 落点和同一琥珀色预警。

在不会删除引用边沿的 20 ns、60 ns 落点，预览继续使用蓝色并报告 0；真实 Paste 后 Relation
保持。离屏截图确认风险标签、差异值、原 Relation、波形、刻度和网格同时可辨认，无桌面交互。
```

长期 Goal 保持 active；第 820 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 821–830 轮：Paste 风险 Relation 直接定位

本轮将 Relation 清理预警从“知道数量”推进为“直接知道具体对象”。此前琥珀色预览能阻止无提示
删除，但当场景中存在多条时序关系时，用户仍需沿连线逐条猜测。现在预检同时生成关系描述、源端点、
目标端点和精确时间，并只强调将被真实 Paste 清理的连线。

1. 第 821 轮：从 811–820 轮截图审计风险反馈，确认数量和原严重级别连线无法建立一一对应，
   `removes 1 relation` 仍要求用户猜测具体对象。
2. 第 822 轮：固定契约为 Relation 身份、描述和端点均来自同一纯预检结果；不增加模式、按钮、
   选择状态或工程字段。
3. 第 823 轮：为每条受影响 Relation 生成“描述 · 源信号 @ 时间 → 目标信号 @ 时间”；缺失 Lane
   或 Event 时提供稳定回退文本，不静默丢失影响项。
4. 第 824 轮：增加只读影响摘要访问器；安全预览返回空列表，风险预览列表数量与清理计数严格一致，
   提交后清空，Undo 后恢复。
5. 第 825 轮：每次绘制仅构造一次 Paste 投影，并将同一结果同时交给波形预览和 Scenario overlay，
   避免两次剪贴板解析产生帧内分叉。
6. 第 826 轮：受影响 Relation 使用琥珀色光晕、2.5 px 虚线、双端点环和中点删除标记；未受影响
   Relation 继续使用原 Error/Warning/Info 样式。
7. 第 827 轮：风险标签直接显示首条源/目标端点，多条时追加剩余高亮数量；标题右键 Paste 和
   状态栏列出全部关系描述、端点及画布高亮说明。
8. 第 828 轮：自动化断言 `relation-req-ack` 的完整描述与
   `req @ 80 ns → ack @ 110 ns`，并覆盖安全落点、真实提交、预览消失和 Undo 恢复。
9. 第 829 轮：通过 Wave Edit、自动滚动、标题批量、信号标题、光标模式、用户旅程和 Edit 菜单
   七项关联 offscreen 专项，生成独立 Relation 身份定位截图并完成视觉复核。
10. 第 830 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen 回归，同步 README、PLAN、GOAL，并执行开发视角与用户视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 36.88 sec

cmake --build --preset qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-debug
73/73 tests passed
Total Test time: 34.06 sec

cmake --build --preset qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-release
73/73 tests passed
Total Test time: 32.83 sec

Core: 51/51 tests passed
Targeted: wave-wave-edit-smoke, wave-wave-edit-autoscroll-smoke,
          wave-group-batch-smoke, wave-signal-header-smoke,
          wave-cursor-mode-smoke, wave-user-journey-smoke
          and wave-edit-menu-smoke passed
Screenshot: build/wave-edit-smoke-header-paste-relation-identity.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：受影响 Relation ID 与摘要在 Event 删除候选确定后按 Scenario 顺序一次生成，计数
直接取身份列表长度。摘要只读取既有 Relation、Event、Lane 和 timebase；绘制阶段不再次推导关系
影响，也不调用模型写接口。`paintEvent()` 为同一帧只生成一份 Paste 投影，波形、标签与 Relation
overlay 使用同一指针。预览离开后没有缓存状态，因此普通 Relation 绘制立即恢复原样。

用户视角验收：

```text
用户在 req/ack 的 80–90 ns 风险落点停下时，标签不再只说会删除一条关系，而是直接写出
req @ 80 ns → ack @ 110 ns。原红色细实线同时变为带光晕的琥珀色虚线，两个事件端点出现圆环，
连线中点出现 ×，无需查看内部 ID 或沿画布猜测。

标题右键 Paste 的说明进一步显示
“ack must rise within 1..4 cycles after req · req @ 80 ns → ack @ 110 ns”，并明确画布中的琥珀色
连线就是受影响对象。安全的 20 ns、60 ns 落点没有摘要或额外强调。

提交后预览与强调同时消失，系统实际只删除该 Relation；Undo 后描述、端点标签、连线强调、标题
目标和 80 ns 落点一起恢复。离屏截图确认提示、虚线、光晕、端点环、删除标记、波形、刻度和网格
同时可辨认。
```

长期 Goal 保持 active；第 830 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 831–840 轮：显式范围 Paste 提交前投影

本轮消除固定范围栏 Paste 的视觉盲区。此前标题目标能看到完整波形与 Relation 风险投影，显式
时间范围却只有按钮 tooltip；范围右键甚至只检查剪贴板是否非空，数量或类型已知不兼容时仍显示为
可点击。现在两种目标共享同一投影、兼容性门禁和风险反馈。

1. 第 831 轮：审计显式范围 Paste，确认固定按钮仅有文字预检、画布无结果投影，范围右键的启用
   条件弱于固定栏，提交后才知道实际复制宽度和依赖影响。
2. 第 832 轮：将投影构造器参数化为有序目标 Lane 与 paste start；标题目标和显式范围不再维护
   两套解析、覆盖或 Relation 预测逻辑。
3. 第 833 轮：增加统一的活动 Paste 投影访问器；显式范围优先，关闭后自动回到标题目标或空状态，
   同一帧继续只构造一次投影。
4. 第 834 轮：显式选区保留原选择边界，虚线波形、边框和标签使用 clipboard 的真实宽度；40–60 ns
   选区可在提交前明确看到只覆盖 40–50 ns。
5. 第 835 轮：固定 Paste 按钮和范围右键均复用同一数量、类型、Bus/Enum 位宽、值、目标时间及
   Relation 预检；已知无效右键动作禁用并显示具体修正方式。
6. 第 836 轮：按 Segment 起止、值和 extensions 判断投影是否已与真实波形一致，忽略临时/真实
   Segment ID 差异；提交结果不再保留看似待执行的陈旧虚线。
7. 第 837 轮：显式范围获得与标题目标一致的 Relation 数量、描述、端点摘要及琥珀色连线强调；
   固定按钮和右键 tooltip 同步公开风险。
8. 第 838 轮：自动化覆盖 2→1 数量不匹配的按钮/右键双禁用、兼容多目标宽度差异、风险范围、
   提交后预览消失、Undo 恢复与零预检模型修改。
9. 第 839 轮：通过 Wave Edit、自动滚动、标题批量、信号标题、光标模式、用户旅程和 Edit 菜单
   七项关联 offscreen 专项，并复核普通与 Relation 风险两张显式范围截图。
10. 第 840 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen 回归，同步 README、PLAN、GOAL，并执行开发视角与用户视角验收。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 36.27 sec

cmake --build --preset qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-debug
73/73 tests passed
Total Test time: 33.71 sec

cmake --build --preset qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-release
73/73 tests passed
Total Test time: 32.83 sec

Core: 51/51 tests passed
Targeted: wave-wave-edit-smoke, wave-wave-edit-autoscroll-smoke,
          wave-group-batch-smoke, wave-signal-header-smoke,
          wave-cursor-mode-smoke, wave-user-journey-smoke
          and wave-edit-menu-smoke passed
Screenshots: build/wave-edit-smoke-explicit-range-paste-preview.png
             build/wave-edit-smoke-explicit-range-paste-relation-warning.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：标题与显式范围在进入投影前均先通过同一 `pasteAvailabilityForTargets()`；之后使用
同一目标顺序、paste start、clipboard 数据、Segment 覆盖和 Event/Relation 预测。显式范围只决定
目标起点和当前选择宽度，不改变 copied duration。等值判定仅比较用户可观察的起止、值和扩展
元数据，不把内部 ID 重建误报为变化；预检未写模型、命令栈、autosave 或工程格式。

用户视角验收：

```text
用户复制 req/ack 的 10 ns 内容，再框选 reset_n/req 的 40–60 ns。固定栏仍明确显示选择宽度 20 ns，
画布中的虚线边界和标签则显示 Paste 实际为 40–50 ns；无需提交即可看出后半段不会覆盖。左侧标题
同步显示目标值变化。

只框选一个信号时，固定 Paste 按钮和右键 Paste 都直接禁用，并说明“复制 2 条、当前选择 1 条”；
不会先执行再报错。框选 req/ack 的 80–100 ns 时，投影显示实际 80–90 ns，Relation 端点、
琥珀色光晕虚线、端点环和删除标记与标题目标路径一致。

提交后真实结果范围变为 copied width，等值投影立即消失；Undo 回到 20 ns 原目标范围时，10 ns
预览恢复。两张离屏截图确认选区、实际覆盖范围、固定栏、关系风险、波形、刻度和网格均可辨认。
```

长期 Goal 保持 active；第 840 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 841–850 轮：无效果 Paste 提交前收口

本轮消除已知等值目标仍允许点击 Paste 的无效动作。此前语义投影只控制提交后的预览清理；固定
按钮、范围菜单、波形菜单和标题菜单仍可能让用户执行一次没有模型结果的操作。现在各入口在提交前
共享同一“是否会产生用户可见模型变化”判定，并保留运行时兜底。

1. 第 841 轮：审计固定范围栏、范围右键、波形右键、标题状态、标题右键和直接 `Ctrl+V` 六条
   Paste 路径，确认无效果反馈此前仅在点击后出现。
2. 第 842 轮：为 Paste 投影增加显式 `modelChanges` 契约，避免界面入口从预览图形或临时 ID
   反推可用性。
3. 第 843 轮：变化判定统一包含波形语义差异、Relation 清理和 Scenario End 延长；波形比较只看
   Segment 起止、值和 extensions，忽略内部 Segment ID。
4. 第 844 轮：固定 Paste 按钮、范围右键、标题状态和标题右键在目标已匹配时统一禁用，并明确
   说明无需波形、Relation 或 End 变化。
5. 第 845 轮：波形右键接入完整预检；单信号 clipboard 使用点击信号，多信号 clipboard 保持
   原复制信号集合及其可见顺序，避免改变既有批量 Paste 语义。
6. 第 846 轮：显式选区宽于 copied duration 且内容已匹配时，仍以中性灰虚线显示实际复制宽度和
   `no Paste needed`；等宽目标及标题目标不残留陈旧预览。
7. 第 847 轮：`pasteAtCursor()` 增加最终语义守卫；即使直接分发 `Ctrl+V`，也只报告
   `Paste skipped`，不修改模型、选择、历史、Saved 或既有 Redo。
8. 第 848 轮：扩展 `wave-wave-edit-smoke`，覆盖六条入口、宽度中性预览、无陈旧标题预览、
   运行时跳过、Redo 保留，以及类型不兼容的波形右键提交前禁用。
9. 第 849 轮：通过 Group 批量、信号标题、光标模式、Wave Edit、Wave Edit 自动滚动、用户旅程和
   Edit 菜单七项关联 offscreen 专项，并复核普通显式投影与无效果预检截图。
10. 第 850 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 37.06 sec

cmake --build --preset qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-debug
73/73 tests passed
Total Test time: 34.34 sec

cmake --build --preset qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-release
73/73 tests passed
Total Test time: 32.49 sec

Related offscreen smoke: 7/7 passed, 6.34 sec
Core entry: 51/51 passed
Screenshots: build/wave-edit-smoke-explicit-range-paste-preview.png
             build/wave-edit-smoke-paste-no-effect-preflight.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：`modelChanges` 只由最终波形语义、Relation 删除数和 End 延长决定；临时 Segment
身份不参与比较。所有可见 Paste 入口读取相同可用性结果，波形右键仅根据 clipboard 信号数量选择
单点击目标或原多信号目标，执行层仍进行最终原子校验。等值目标不会创建命令，也不会清除选区或
覆盖已有 Redo；真正会删除 Relation 或延长 End 的 Paste 继续可用。

用户视角验收：

```text
用户复制 Clock 的 20–30 ns，再选择同一 Clock 的 40–60 ns。固定栏显示原选区 20 ns 宽度，画布以
中性虚线说明 copied width 实际为 40–50 ns；由于内容已一致，固定 Paste 和范围右键均直接禁用并
说明 no Paste needed。

即使通过主 Edit 动作直接分发 Ctrl+V，状态栏也只显示 Paste skipped / already matches copied
range / no values changed。40–60 ns 选区、40–50 ns 宽度说明、模型和先前 Redo 均保持。

退出范围后，在 40 ns 波形右键或选择 Clock 标题时，Paste 同样在点击前禁用；标题目标不显示陈旧
虚线。普通会改值的双信号 Paste、Relation 风险 Paste 和 End 延长仍保持可执行及单步 Undo。
```

两张离屏截图确认普通 40–60 ns 选区中的 40–50 ns 实际投影，以及等值 Clock 场景中的禁用按钮、
中性虚线、网格、波形和无变化状态均清晰可辨。

长期 Goal 保持 active；第 850 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 851–860 轮：显式范围 Clear/Run 提交前影响

本轮消除范围 Clear/Run 只能在执行后才知道无效果或会删除 Relation 的反馈延迟。固定范围栏、
范围右键和 Edit > Cut 现在共享一份纯投影结果，在点击前区分安全、风险与无效果；运行时仍保留
原子校验和无损兜底。

逐轮交付：

1. 第 851 轮：审计固定范围栏、范围右键、Cut、Delete 和 Clock `R` 路径，确认 Clear/Run 原先
   始终可用，Relation 影响与无效果原因只能在提交后得知。
2. 第 852 轮：定义三态交互契约：安全动作公开影响范围且保证不删除 Relation；风险动作在按钮上
   直接显示数量；无效果动作提交前禁用并说明原因。
3. 第 853 轮：实现确定性的范围清除投影，复现清除、拆分、归一化和 Event 复用语义；临时 Segment
   使用隔离身份，不消耗工程稳定 ID。
4. 第 854 轮：提取并复用既有 Paste 的 Relation 影响预测，统一返回受影响 ID、Relation 描述和
   精确源/目标端点，避免两条语义路径分叉。
5. 第 855 轮：安全 Clear/Run 在固定栏和右键公开受影响信号数、精确时间范围以及
   `No Relation will be removed`。
6. 第 856 轮：无效果 Clear/Run 在固定栏和右键直接禁用；Delete 与 Clock `R` 的执行层兜底只报告
   原因，保持选择、模型、Undo/Redo、Saved 和 autosave。
7. 第 857 轮：风险按钮改为琥珀色 `Clear ⚠N`，悬浮、状态和无障碍名称列出准确 Relation 描述及
   端点；实际提交后的影响摘要与预检逐项对应。
8. 第 858 轮：范围右键 Clear/Cut 与 Edit > Cut 接入同一预检；源范围已是隐式值时 Cut 仍可复制，
   但明确说明不会移除源值。
9. 第 859 轮：扩展 `wave-wave-edit-smoke` 覆盖安全、风险、无效果、真实清理、Undo 恢复、
   Cut 区分和 Redo 保留；七项关联 offscreen 专项通过并复核两张新增截图。
10. 第 860 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 36.09 sec

cmake --build --preset qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-debug
73/73 tests passed
Total Test time: 34.28 sec

cmake --build --preset qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-release
73/73 tests passed
Total Test time: 33.52 sec

Related offscreen smoke: 7/7 passed, 6.30 sec
Core entry: 51/51 passed
Screenshots: build/wave-edit-smoke-range-clear-relation-warning.png
             build/wave-edit-smoke-range-clear-no-effect-preflight.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：清除投影在临时 Lane 副本中复现真实命令语义，不修改 Scenario、命令栈、Saved、
autosave 或稳定 ID。Relation 预测由 Paste 与 Clear 共用，风险预检、实际删除数和 Undo 恢复一致。
固定栏、范围右键和 Edit Cut 读取同一可用性结果；执行层仍处理快捷键或过期界面的最终原子校验。

用户视角验收：

```text
用户选择 req/ack 的 80–100 ns 后，固定栏在点击前显示琥珀色 Clear ⚠1；悬浮可直接读到
ack must rise within 1..4 cycles after req · req @ 80 ns → ack @ 110 ns。执行 Clear 后恰好删除
该 Relation，选区保持；一次 Ctrl+Z 同时恢复波形、Relation、选区和同一预警。

用户选择 Clock 的 40–60 ns 正常运行范围时，Run 在点击前禁用并说明该范围没有覆盖或 Relation。
Delete/R 也只报告 no values changed，不增加历史、不清除既有 Redo。Cut 仍可复制这段时间范围，
并明确说明源范围不会移除值。
```

两张离屏截图确认风险范围的琥珀色按钮、受影响 Relation、精确端点，以及无效果 Clock 范围中的
禁用 Run、可用 Cut、网格、波形和结果反馈均清晰可辨。

长期 Goal 保持 active；第 860 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 861–870 轮：范围写值提交前影响

本轮消除范围预设、Clock Gate/Disable 和 Bus/Enum 文本值只能在提交后才知道无效果、位宽错误或
Relation 清理的反馈延迟。固定范围栏保持原有控件数量，但每个候选值现在使用与 Clear/Paste 一致的
纯投影，在点击或 Enter 前区分安全、风险、无效果和非法输入。

逐轮交付：

1. 第 861 轮：审计 `0/1/X/Z/Don't care`、Clock Gate/Disable、Bus/Enum Enter 与键盘入口，确认
   按钮原先始终可用，实际变化数和 Relation 影响只能在提交后看到。
2. 第 862 轮：定义统一契约：只统计真正变化的信号；安全动作保证不删除 Relation；风险动作显示
   数量和端点；等值动作提交前禁用；文本草稿在 Enter 前完成同一判断。
3. 第 863 轮：实现确定性的范围写值投影，复现 `rangeAlreadyEquals`、覆盖拆分、插入、归一化及
   extensions 语义；临时 Segment 使用隔离身份，不消耗工程稳定 ID。
4. 第 864 轮：复用统一 Event/Relation 影响预测，对实际变化 Lane 返回准确 Relation ID、描述和
   源/目标端点；部分 Lane 等值时只报告真实变化数。
5. 第 865 轮：预设和 Clock 按钮接入三态反馈；风险文本为 `值 ⚠N`，等值按钮禁用，悬浮和无障碍
   说明保留 `0/1/X/Z/G` 快捷键及完整影响。
6. 第 866 轮：Bus/Enum 文本草稿实时显示安全、非法、等值或风险状态；位宽错误直接定位具体信号，
   有效草稿显示影响数和 Relation 保证，不新增 Apply 按钮或模态确认。
7. 第 867 轮：`0/1/X/Z/G` 和 Enter 执行层增加最终无效果守卫，只报告 `Set skipped`，保持选区、
   模型、Undo/Redo、Saved 和 autosave。
8. 第 868 轮：实际写值反馈改为“变化数/目标数”，Relation 删除数与预检一致时附加准确描述和
   端点；单步 Undo 恢复波形、关系、选区和风险按钮。
9. 第 869 轮：扩展 `wave-wave-edit-smoke` 与 `wave-canvas-add-lane-smoke`，覆盖 Bit 风险写值、
   Bus 草稿、Clock 等值入口、实际提交/Undo 和 Redo 保留；八项关联 offscreen 专项通过并复核
   两张新增截图。
10. 第 870 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 36.63 sec

cmake --build --preset qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-debug
73/73 tests passed
Total Test time: 34.05 sec

cmake --build --preset qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-release
73/73 tests passed
Total Test time: 32.75 sec

Related offscreen smoke: 8/8 passed, 7.72 sec
Core entry: 51/51 passed
Screenshots: build/wave-edit-smoke-range-value-relation-warning.png
             build/wave-edit-smoke-range-value-no-effect-preflight.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：值投影在临时 Lane 副本中复现真实命令的等值判断、覆盖拆分、归一化、extensions 和
Event 复用，不修改 Scenario、命令栈、Saved、autosave 或稳定 ID。按钮、文本草稿和执行层读取同一
候选语义；实际 Relation 删除数与预检一致时回显同一描述和端点。等值快捷键不创建命令，也不覆盖
已有 Redo。

用户视角验收：

```text
用户选择 req/ack 的 80–100 ns 时，0 按钮在点击前显示 0 ⚠1，并说明只有 1/2 条信号会变化以及
req @ 80 ns → ack @ 110 ns 会受影响。点击后恰好修改 req 并删除该 Relation；再次按 0 只显示
Set skipped，一次 Ctrl+Z 即恢复波形、Relation、选区和同一 0 ⚠1 预警。

用户选择两个不同位宽 Bus 的 20–50 ns 后，输入 0xa5 会在 Enter 前定位 data_small 位宽错误；
改为 0xa 后显示 2/2 条信号安全写入。再次输入 0xa 会在提交前显示等值状态，Enter 不新增历史。

用户选择 Clock 已为 DISABLED 的 200–210 ns 后，Disable 直接禁用，Run/Gate 保持可用；按 X
只报告 no values changed，原选区和既有 Redo 保持。
```

两张离屏截图确认风险 Bit 值按钮、受影响 Relation 和精确端点，以及等值 Clock 范围中的禁用
Disable、可用 Run/Gate、时间刻度、波形和无变化反馈均清晰可辨。

长期 Goal 保持 active；第 870 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 871–880 轮：范围 Repeat 提交前目标与风险

本轮消除 Repeat 只能在提交后才知道目标内容、Relation 清理或无变化的反馈延迟。固定范围栏保持
原有控件数量；Repeat、范围右键、Edit 菜单与 `Ctrl+D` 统一读取目标投影，在动作发生前区分安全、
风险、无效果和 End 延长。

逐轮交付：

1. 第 871 轮：审计固定 Repeat、范围右键、共享 Edit 动作和 `Ctrl+D`，确认旧路径始终显示可用，
   无变化时仍把选区移动到后续范围，造成虚假成功反馈。
2. 第 872 轮：定义统一合同：目标固定为源范围右端；剪贴板始终隔离；无变化保持源选区和历史；
   Relation 风险与 End 延长必须在提交前可见。
3. 第 873 轮：实现确定性的 Repeat 投影，复现范围清除、Segment 插入、归一化、extensions、
   Event 复用和 Relation 清理；临时 Segment 使用隔离身份。
4. 第 874 轮：增加 `RangeRepeatAvailability`，公开源/目标、宽度、End 延长和完整 Relation 影响；
   波形等值但 End 延长仍视为真实变化。
5. 第 875 轮：固定按钮接入三态反馈；风险文本为 `Repeat ⚠N`，无效果按钮禁用，安全提示保证
   `No Relation will be removed`。
6. 第 876 轮：范围右键和共享 Edit 动作复用同一可用性、警告文本与准确 Relation 描述；禁用入口
   保持可见，直接说明为什么无需执行。
7. 第 877 轮：悬浮 Repeat 在后续范围绘制虚线波形、边界、时间和 `Ctrl+D` 提示；风险投影改用
   琥珀色并强调受影响 Relation，且优先于剪贴板 Paste 预览。
8. 第 878 轮：执行层增加最终无效果守卫；直接 `Ctrl+D` 只报告 `Repeat skipped`，不移动选区、
   不创建命令、不清除 Redo；成功提交回显准确 Relation 描述。
9. 第 879 轮：扩展 `wave-wave-edit-smoke` 与 `wave-canvas-add-lane-smoke`，覆盖风险、无效果、
   End 延长、右键/Edit 一致性、悬浮预览、剪贴板隔离及 Undo/Redo；复核两张新增离屏截图。
10. 第 880 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 36.91 sec

cmake --build --preset qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-debug
73/73 tests passed
Total Test time: 36.03 sec

cmake --build --preset qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-release
73/73 tests passed
Total Test time: 34.09 sec

Related offscreen smoke: 8/8 passed, 7.86 sec
Core entry: 51/51 passed
Screenshots: build/wave-edit-smoke-repeat-relation-warning.png
             build/wave-edit-smoke-repeat-no-effect-preflight.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：Repeat 投影只读捕获源范围，在临时 Lane 副本中生成目标波形，不修改 Scenario、
命令栈、Saved、autosave、剪贴板或稳定 ID。固定按钮、右键、Edit 和执行守卫读取同一投影语义；
实际 Relation 删除数及描述与预检一致。无效果路径在命令创建前返回，源选区、活动边、视图和既有
Redo 均保持。

用户视角验收：

```text
用户选择 req/ack 的 80–100 ns 时，Repeat 在点击前显示 Repeat ⚠1；悬浮后 100–120 ns 目标以
琥珀虚线显示，req @ 80 ns → ack @ 110 ns 连线同步强调。点击后恰好删除这一 Relation，剪贴板
不变；一次 Ctrl+Z 恢复波形、Relation、80–100 ns 源选区和同一预警。

用户选择 Clock 的 20–30 ns 时，Repeat 直接禁用；悬浮仍显示 30–40 ns 中性目标并说明已经匹配。
按 Ctrl+D 只报告 Repeat skipped，选区不跳到后方，已有 Redo 不丢失。

用户选择靠近末尾的 Clock 210–220 ns 时，Repeat 因会延长时间轴而保持可用，并在点击前显示
Extends End to 230 ns；执行后 End 和结果选区同时更新，一次 Undo 全部恢复。
```

两张离屏截图确认风险目标波形、Relation 光晕、准确端点和琥珀色按钮，以及无效果 Clock 目标中的
禁用 Repeat、中性虚线、时间刻度和无需提交反馈均清晰可辨。

长期 Goal 保持 active；第 880 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 881–890 轮：范围 Move/Copy 释放前结果与风险

本轮消除范围 Move 与 Ctrl+拖动 Copy 只能看到搬运波形、无法在释放前确认 Relation 清理、无效果
或 End 延长的问题，并修复无变化命令仍把选区移动到目标位置的虚假成功反馈。交互不增加按钮或
模式，继续使用既有范围主体拖动和动态 Ctrl 切换。

逐轮交付：

1. 第 881 轮：审计同信号、跨信号、重叠、类型不兼容、动态 Ctrl 与自动滚动路径，确认旧预览
   不公开 Relation/End 影响，命令无效果时仍移动选区。
2. 第 882 轮：定义统一契约：安全、风险、无效果、非法和 End 延长必须在释放前可区分；无效果
   保持源选区、光标、模型、历史、Redo 和剪贴板。
3. 第 883 轮：实现无副作用的范围转移投影，严格复现真实命令的“Move 先清全部源、再清全部目标、
   最后按映射写入”顺序，以及 Segment 拆分、插入、归一化和 extensions。
4. 第 884 轮：将最终受影响 Lane 拓扑接入统一 Event 复用与 Relation 清理预测，返回稳定 Relation
   ID、完整描述和源/目标信号时间；投影身份与工程稳定 ID 隔离。
5. 第 885 轮：画布接入三态预览：安全目标为蓝色虚线，风险目标为琥珀色 `Move/Copy ⚠N`，
   无效果目标为中性 `no Move/Copy needed`；受影响 Relation 连线同步强调。
6. 第 886 轮：状态栏、Ctrl 按下切换 Copy、Ctrl 释放切回 Move 与鼠标连续移动统一读取同一投影，
   显示源保留/清除、目标映射、Relation 保证或准确风险，以及 `End →`。
7. 第 887 轮：提交层在创建 `TransferRangeCommand` 前执行最终守卫；无效果释放恢复源目标且不触碰
   命令栈，成功提交才切换选区，并回显实际删除的 Relation 描述和端点。
8. 第 888 轮：验证跨信号单 Lane 和多 Lane 映射、Copy 重叠拒绝、类型不兼容、纯 End 延长及
   Ctrl 在拖动中的双向切换，保持一次选择感知 Undo/Redo。
9. 第 889 轮：扩展 `wave-wave-edit-smoke`，覆盖安全 Copy、风险 Move、无效果 Move/Copy、End
   延长、同/跨信号映射、精确 Relation 结果和 Redo/剪贴板保持；复核三张新增离屏截图。
10. 第 890 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 38.59 sec

cmake --build --preset qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-debug
73/73 tests passed
Total Test time: 35.97 sec

cmake --build --preset qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-release
73/73 tests passed
Total Test time: 33.72 sec

Related offscreen smoke: 8/8 passed, 8.51 sec
Core entry: 51/51 passed
Screenshots: build/wave-edit-smoke-range-move-preview.png
             build/wave-edit-smoke-range-copy-no-effect-preflight.png
             build/wave-edit-smoke-range-copy-end-extension-preflight.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：投影只复制受影响 Lane，使用隔离 Segment ID 复现命令顺序，并以 Segment 起止、
值和 extensions 判定可见波形变化；Relation 预测沿用真实 Event 的 ID、tick/value 与 tick
三级复用顺序。预览不修改 Scenario、稳定 ID 生成器、命令栈、Saved、autosave 或剪贴板。执行层
只在 `modelChanges` 为真时创建命令；实际 Relation 删除数和完整描述与预检一致。

用户视角验收：

```text
用户把 req/ack 的 80–100 ns 移到 120–140 ns 时，在释放前看到琥珀色目标、Move ⚠1，以及
ack must rise within 1..4 cycles after req · req @ 80 ns → ack @ 110 ns；对应连线同时带删除标记。
释放后恰好删除该 Relation，一次 Ctrl+Z 恢复波形、关系和 80–100 ns 源选区。

用户按 Ctrl 把同一范围复制到 140–160 ns 时，预览明确显示 source remains 与
No Relation will be removed；释放后源和目标都保留，Undo/Redo 精确往返。

用户把已匹配的 req/ack 20–40 ns 复制到 160–180 ns，或移动无覆盖的 Clock 20–40 ns 时，
释放前看到 no Copy/Move needed；释放后源选区、光标、已有 Redo 和剪贴板均不变化。

用户把末尾 210–220 ns 复制到 220–230 ns 时，即使波形为空也会看到 End → 230 ns；释放后
End 和结果选区原子更新，一次 Undo 全部恢复。
```

三张离屏截图确认风险 Move 的琥珀目标与 Relation 定向强调、无效果 Copy 的中性目标和无需提交
反馈，以及纯 End 延长目标的边界、刻度和安全保证均清晰可辨。

长期 Goal 保持 active；第 890 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 891–900 轮：Segment 释放前最终波形与关系风险

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

本轮消除 Segment 主体移动、Ctrl+拖动复制和边界调整只能看到范围框、必须释放后才能确认最终
波形与 Relation 影响的问题，并修复复制到等值目标时仍改变目标选择的虚假成功反馈。

逐轮交付：

1. 第 891 轮：审计 Move/Copy/Resize 的按下、连续移动、动态 Ctrl、释放、命令历史和选择恢复路径。
2. 第 892 轮：定义安全、风险和无变化三态合同；无变化必须保留源 Segment、Redo 与剪贴板。
3. 第 893 轮：实现纯 Segment 投影，复现相邻 Segment 边界联动和 Move/Resize 归一化。
4. 第 894 轮：补齐 Copy 覆盖、拆分、extensions 和等值区间判断，隔离预览 Segment 身份。
5. 第 895 轮：将投影接入 Event 复用与 Relation 清理预测，返回准确描述和端点。
6. 第 896 轮：在受影响时段绘制最终虚线波形；安全、风险、无变化使用蓝、琥珀、中性三色。
7. 第 897 轮：统一鼠标移动、Ctrl 按下/释放、画布标签和状态栏反馈。
8. 第 898 轮：提交前增加无变化守卫，并使真实 Relation 删除回显与预检逐项一致。
9. 第 899 轮：扩展核心命令和 offscreen smoke，覆盖安全 Move/Resize、无变化 Copy、风险 Copy、
   Undo/Redo、剪贴板不变量及三张视觉证据。
10. 第 900 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 37.35 sec

cmake --build --preset qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-debug
73/73 tests passed
Total Test time: 34.62 sec

cmake --build --preset qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-release
73/73 tests passed
Total Test time: 33.57 sec

Related offscreen smoke: 8/8 passed, 8.82 sec
Core entry: 51/51 passed
Screenshots: build/wave-edit-smoke-segment-move-preview.png
             build/wave-edit-smoke-segment-copy-no-effect-preflight.png
             build/wave-edit-smoke-segment-copy-relation-warning.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：Segment 投影不修改模型，且与 `EditSegmentCommand` / `CopySegmentCommand` 的相邻
边界、覆盖、归一化、metadata 和同步语义一致。画布仅重绘受影响时间并保留刻度；关系风险同时
驱动范围颜色、连线强调、端点说明和提交回显。无变化在命令创建前返回，命令层仍按完整 Scenario
前后状态提供最后一道历史过滤。

用户视角验收：

```text
用户拖动 data[7:0] 的 Segment 主体时，不再只看到一个空范围框；受影响部分直接显示释放后的
蓝色虚线波形、起止时间、宽度和“No Relation will be removed”。

用户按 Ctrl 拖动，但同步吸附后仍落在原拍，预览立即改为中性并显示 no Copy needed；释放后
源 Segment 仍选中，既有 Redo 可继续执行，剪贴板内容不变。

用户把 Segment 复制到会吞掉被引用边沿的位置时，目标改为琥珀色，并在释放前列出准确 Relation
描述以及 data[7:0] @ 100 ns → ack @ 110 ns。释放后恰好删除这一条关系，一次 Ctrl+Z 同时
恢复波形、关系和源 Segment。
```

三张离屏截图确认安全 Move 的局部最终波形、无变化 Copy 的中性反馈和风险 Copy 的 Relation
端点强调均可直接辨识。

长期 Goal 保持 active；第 900 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 901–910 轮：Segment 菜单操作点击前预检

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

本轮消除 Edit > Segment 精确命令和 Bus/Enum 右键相邻复制必须点击后才能知道是否可执行、
是否没有变化以及会删除哪些 Relation 的盲操作；同时避免把菜单投影计算放入高频指针状态路径。

逐轮交付：

1. 第 901 轮：审计八项 Segment 菜单命令、右键相邻复制、命令提交和 Undo 的候选范围差异。
2. 第 902 轮：定义不适用、不可用、无变化、安全与 Relation 风险状态合同及入口禁用规则。
3. 第 903 轮：将拖动专用 Segment 投影抽为可复用纯投影，继续复现覆盖、拆分、归一化与依赖清理。
4. 第 904 轮：按 Sync 拍或 Async tick 计算八项准确候选范围、相邻约束、最小宽度和时间轴边界。
5. 第 905 轮：Edit 菜单与右键复制动态公开 `unavailable`、`no change`、`⚠N`、摘要和目标范围。
6. 第 906 轮：动作悬浮绘制安全蓝色或风险琥珀色最终虚线波形，并同步强调受影响 Relation。
7. 第 907 轮：提交前复用同一评估；无效果不创建命令，真实清理及一次 Undo 与预检逐项一致。
8. 第 908 轮：把八项评估从每次鼠标状态更新移到选择、模型、Timing 和菜单上下文变化点。
9. 第 909 轮：扩展 offscreen smoke，覆盖八项动作状态、安全/风险悬浮、无效果、边界禁用、
   右键复制、真实提交与 Undo，并保存三张视觉证据。
10. 第 910 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 38.38 sec

cmake --build --preset qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-debug
73/73 tests passed
Total Test time: 35.61 sec

cmake --build --preset qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --preset qtcreator-release
73/73 tests passed
Total Test time: 34.37 sec

Related offscreen smoke: 8/8 passed, 9.04 sec
Core entry: 51/51 passed
Million-transition query metric: 21 ms
Final menu-state fixes:
  wave-canvas-add-lane-smoke + wave-wave-edit-smoke: 2/2 passed, 3.81 sec
Screenshots: build/wave-edit-smoke-segment-menu-safe-preview.png
             build/wave-edit-smoke-segment-menu-relation-warning.png
             build/wave-edit-smoke-segment-menu-no-effect-preflight.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：八项动作与右键复制使用相同候选计算、相邻约束、最终波形投影和命令前守卫。
预览不修改 Scenario、稳定 ID、命令历史、Saved、autosave 或剪贴板；提交后的范围、Relation
清理数量与描述均和预检一致。动作状态不在每次指针移动时重复计算；菜单移向导航、值编辑或
分隔项时立即清除旧投影。

用户视角验收：

```text
用户选中 data[7:0] 的 100–170 ns Segment，悬浮 Duplicate Segment before 时，不需要点击就能
看到 30–100 ns 蓝色最终虚线波形、70 ns 宽度和“No Relation will be removed”。

同一操作会删除临时 Relation 时，菜单在点击前显示 ⚠1，波形和受影响连线变为琥珀色，并列出
准确描述和端点。点击后恰好删除这一条关系，一次 Ctrl+Z 同时恢复波形、关系和源 Segment。

相邻目标已经与源值一致时，动作显示 no change 并禁用；末拍向后复制显示 unavailable 和超过
End 的原因。两种情况都不会改变选择、Redo、剪贴板或模型。
```

三张离屏截图确认安全预览、Relation 风险定位和无效果/边界禁用在点击前均可直接辨识。

长期 Goal 保持 active；第 910 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 911–920 轮：Bus/Enum 就地编辑提交前预演

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

本轮消除 Bus/Enum Beat/Segment 编辑必须按 Enter 或点击预设后才能知道输入是否有效、是否实际
改变波形，以及 Clear 是否会清理 Relation 的盲操作。编辑器现在使用与真实提交一致的候选投影，
但不引入新模式或额外确认步骤。

逐轮交付：

1. 第 911 轮：审计文本草稿、四个 Bus 预设、Clear、前后导航及 Beat/Segment 提交路径。
2. 第 912 轮：定义不可用、非法、无变化、安全和 Relation 风险五类编辑状态及视觉合同。
3. 第 913 轮：按真实 Set/Edit/Clear 的拆分、归一化、extensions 与 Event 复用建立纯候选投影。
4. 第 914 轮：文本输入、进制切换、Enum 符号循环、数值步进和最近值切换即时刷新候选状态。
5. 第 915 轮：0/X/Z/Don't care/Clear/Apply 悬浮复用同一投影，离开后恢复当前文本草稿预演。
6. 第 916 轮：无变化显示中性色与 `Done`，非法输入变红并禁用 Apply/前后导航，安全修改显示蓝色。
7. 第 917 轮：Relation 风险显示琥珀色 `⚠N`、受影响连线、准确描述、端点和恢复说明。
8. 第 918 轮：提交层复用预检并增加失效目标守卫；真实依赖清理、状态回显和一次 Undo 对应预演。
9. 第 919 轮：扩展 offscreen smoke，覆盖等值、越位宽、安全、预设悬浮、危险 Clear、零模型预演、
   精确提交与 Undo，并保存四张视觉证据。
10. 第 920 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 38.08 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 37.35 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 36.26 sec

Core entry: 51/51 passed
Million-transition query metric: 25 ms
Screenshots: build/wave-edit-smoke-bus-editor-safe-preview.png
             build/wave-edit-smoke-bus-editor-no-effect-preflight.png
             build/wave-edit-smoke-bus-editor-invalid-preflight.png
             build/wave-edit-smoke-bus-editor-clear-relation-warning.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：所有预演均在 Scenario 副本上计算，不修改模型、稳定 ID、命令历史、Saved、
autosave 或剪贴板。Segment 修改保留既有 Segment 身份及非预设扩展；Beat 写入和 Clear 复现真实
覆盖、拆分与归一化。危险操作的 Relation 数量、描述和端点与实际提交一致；无效果路径不创建命令，
失效范围不会落到其他目标。Enum 上下键、Bus 数值步进与最近值循环在刷新预演后仍保留各自操作反馈；
无效草稿阻断目标切换时明确说明目标保持。

用户视角验收：

```text
用户双击 data[7:0] 的 100–170 ns Segment 并输入 0x2a，无需按 Enter 即看到该范围的蓝色最终
虚线波形和“No Relation will be removed”；悬浮 X 时临时看到 X 结果，移开后自动恢复 0x2a 草稿。

输入当前值 0x35 时，编辑器显示 no change 和 Done；按 Enter 只关闭编辑器，不新增 Undo。
输入超出 8-bit 的 0x1ff 时，输入框变红，Apply 与前后导航立即禁用，波形和历史不变。

Clear 会删除临时 Relation 时，按钮在点击前显示 Clear ⚠1，预演改为琥珀色 implicit X，并列出
temporary Bus editor clear relation · data[7:0] @ 100 ns → ack @ 110 ns。点击后恰好删除该
关系，一次 Ctrl+Z 同时恢复波形与 Relation。
```

四张离屏截图确认安全、无变化、非法和 Relation 风险四种状态在提交前可直接辨识，固定编辑器、
刻度、网格和目标波形均保持可见。

长期 Goal 保持 active；第 920 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 921–930 轮：Bus/Enum 连续多拍录入

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

本轮消除连续填写 Bus/Enum 时每拍重复输入、提交和产生一条 Undo 的成本。既有 Beat 编辑框现在
直接识别值列表并投影全部目标，不新增模式、面板或确认页；单值、预设、Clear 和 Segment 编辑
保持原路径。

逐轮交付：

1. 第 921 轮：审计 Beat/Segment 文本提交、Timing 步长、End、Relation 与历史选择恢复语义。
2. 第 922 轮：定义逗号、分号、空白、Tab、换行列表及最多 1024 个实时值的输入合同。
3. 第 923 轮：实现逐值位宽/Enum 符号校验、具体错误序号、整宽 `X/Z` 与既有进制规则。
4. 第 924 轮：按当前同步拍建立整段候选 Lane，显示数量、范围、规范化值和完整虚线波形。
5. 第 925 轮：把 Relation Event 复用、无变化判断和 `End →` 延长纳入同一提交前投影。
6. 第 926 轮：新增 `SetLaneSequenceCommand`，一次写入、Event 同步、End 延长和一次 Undo 原子完成。
7. 第 927 轮：提交后选择完整结果范围；等值列表不创建命令，Preset/Clear 悬浮离开后恢复列表。
8. 第 928 轮：列表期间禁用歧义的前后导航与 Beat→Segment 切换；Segment 列表可切回 Beat 且保留草稿。
9. 第 929 轮：扩展核心与 `wave-wave-edit-smoke`，覆盖 Bus/Enum、安全/非法/等值/Relation/End、
   换行输入、Segment 恢复、一次 Undo，并保存七张视觉证据。
10. 第 930 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 37.16 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 35.08 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 33.75 sec

Core entry: 52/52 passed
Million-transition query metric: 20 ms
Screenshots: build/wave-edit-smoke-bus-sequence-safe-preview.png
             build/wave-edit-smoke-enum-sequence-safe-preview.png
             build/wave-edit-smoke-bus-sequence-invalid-preflight.png
             build/wave-edit-smoke-bus-sequence-no-effect-preflight.png
             build/wave-edit-smoke-bus-sequence-end-extension-preview.png
             build/wave-edit-smoke-bus-sequence-relation-warning.png
             build/wave-edit-smoke-bus-sequence-segment-blocked.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：列表解析只作用于现有编辑器；Enum 先尝试完整符号，避免含空白符号被误拆。
候选 Lane 使用隔离 ID，不修改 Scenario、稳定 ID、历史、Saved、autosave 或剪贴板。命令验证
目标存在、值有效、区间连续且不溢出；提交只同步一次 Event/Relation，并以完整 Scenario 快照
保证波形、End 和依赖一起 Undo/Redo。无变化列表不执行命令；非法、超长或失效目标原位阻断。

用户视角验收：

```text
用户在 data[7:0] 的 110 ns Beat 中粘贴 0x12, 0x34, X, 0x56，不需要逐拍 Tab，即看到
110–150 ns 四拍最终波形；X 自动成为完整 8-bit 未知值，Apply 显示 Apply 4。

列表中的第二项越过 8-bit 时，输入框立即变红并指出 value 2，模型和历史不变。三项均为当前值时，
按钮显示 Done；提交只选中完整三拍结果，不新增 Undo。

从 210 ns 粘贴三项时，提交前显示 End → 240 ns；点击后一次 Ctrl+Z 同时恢复原 End 和波形。
七拍写入会清理临时 Relation 时，点击前显示 Apply 7 ⚠1、关系描述及端点，真实提交与一次 Undo
逐项一致。

在 Segment 编辑器粘贴两项时不会误改完整 Segment；状态直接说明不适用，点击 Segment 切到 Beat，
原列表仍在并立即得到两拍预览，无需重新粘贴。Enum 的 IDLE, WAIT_ACK, DONE 使用同一路径。
```

七张离屏截图确认安全、Enum、非法、无变化、End、Relation 风险和 Segment 恢复入口均可在提交前
辨识；刻度、目标波形和状态栏保持可见。

长期 Goal 保持 active；第 930 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 931–940 轮：Bit 精确模式批量录入

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

本轮消除已知 Bit 模式必须“写一个值、移动一次光标”逐拍重复的成本。单个 Bit 信号的既有显式
范围栏现在直接接收模式并预演整个结果，不新增模式、面板或确认页；单拍点击翻转、拖动多拍翻转、
0/1/X/Z 快捷键和多信号常量赋值保持原路径。

逐轮交付：

1. 第 931 轮：审计 Bit 单拍点击、键盘赋值、范围常量工具与 Bus/Enum 序列录入的操作成本。
2. 第 932 轮：定义“一个符号对应一拍、短模式仅整除时重复、目标范围不隐式扩张”的输入合同。
3. 第 933 轮：实现紧凑及空格、逗号、分号、下划线、竖线分隔的 `0/1/X/Z` 解析与 `0b` 前缀。
4. 第 934 轮：按当前 Bit 拍边界生成最多 1024 个连续目标；拍数不整除、超长、非法字符和非完整拍原位阻断。
5. 第 935 轮：输入时显示全部目标拍的蓝色虚线波形、模式长度、重复次数、范围和状态栏反馈。
6. 第 936 轮：复用 `SetLaneSequenceCommand` 原子写入整段模式，一次 Undo/Redo 同步波形与 Event。
7. 第 937 轮：等值模式显示无效果且不进入历史；非法提交保留草稿、全选纠错目标和原选区，Esc 只弃稿。
8. 第 938 轮：提交前按最终边沿投影 Relation 清理；风险字段、波形、连线和准确依赖摘要统一变为琥珀色。
9. 第 939 轮：扩展 `wave-wave-edit-smoke`，覆盖重复、分隔 X/Z、非法、等值、Relation、提交、
   一次 Undo/Redo、选择保持和四张视觉证据。
10. 第 940 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 39.67 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 36.71 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 36.15 sec

Targeted wave-wave-edit-smoke: 1/1 passed, 3.50 sec
Core entry: 52/52 passed
Million-transition query metric: 22 ms
Screenshots: build/wave-edit-smoke-bit-pattern-safe-preview.png
             build/wave-edit-smoke-bit-pattern-invalid-preflight.png
             build/wave-edit-smoke-bit-pattern-no-effect-preflight.png
             build/wave-edit-smoke-bit-pattern-relation-warning.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：模式解析和候选 Lane 只存在于当前范围编辑上下文；预演使用隔离 Segment 身份，
不修改 Scenario、稳定 ID、命令历史、Saved、autosave 或剪贴板。提交前与提交后使用相同的
连续拍范围和值序列；无变化不创建命令，非法输入不离开原目标。成功提交复用已验收的序列命令，
Event 同步、Relation 清理、选择恢复和一次 Undo/Redo 与预演逐项一致。

用户视角验收：

```text
用户框选 req 的 20–60 ns 四拍，输入 0b01 后立即看到 0101 的完整蓝色虚线波形，状态栏明确显示
2 个符号重复 2 次、覆盖 4 拍及一次 Undo；按 Enter 后结果与预演一致。

同一四拍输入 010 时，字段立即变红并说明“4 selected beats are not an exact multiple of the
3-symbol pattern”；按 Enter 不改变波形，草稿仍全选可修正，Esc 只丢弃草稿并保留范围。

再次输入当前模式 01 时显示 no command will run，提交不增加历史。输入会删除临时 Relation 的
常量模式 0 时，目标波形、依赖连线和字段在提交前均为琥珀色，并列出准确 Relation 描述和端点；
提交恰好删除该关系，一次 Ctrl+Z 同时恢复波形、关系和选区。
```

四张离屏截图确认安全、非法、无变化和 Relation 风险四种状态均能在提交前辨识；模式字段、刻度、
选区、最终波形和状态栏保持同时可见。

长期 Goal 保持 active；第 940 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 941–950 轮：多 Bit 同模式原子录入

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

本轮消除多个同类 Bit 信号必须重复框选、重复输入同一激励模式的成本。既有显式范围栏现在对共享
拍网格的多 Bit 选择直接应用同一模式，不新增模式、面板或确认步骤；单信号模式、范围常量赋值、
点击翻转和拖动多拍翻转保持原路径。

逐轮交付：

1. 第 941 轮：审计多 Bit 范围常量赋值与单信号模式录入之间的重复输入和错误恢复成本。
2. 第 942 轮：定义“同一输入应用到全部目标、目标必须共享准确拍网格、一个提交对应一次 Undo”的合同。
3. 第 943 轮：新增 `SetLaneSequencesCommand`，以一个 Scenario 前后快照原子提交多条连续序列。
4. 第 944 轮：将 Bit 模式投影扩展为有序多 Lane 投影，并限制为最多 1024 拍、4096 signal-beats。
5. 第 945 轮：输入时为每条目标信号绘制最终虚线波形，状态栏公开模式、重复次数、拍数和变化信号数。
6. 第 946 轮：等值批次不创建历史；不同拍网格、非完整拍、非法模式和非整除长度在提交前原位阻断。
7. 第 947 轮：联合评估全部目标的 Event/Relation 影响，风险预演与一次 Undo 同时覆盖所有信号。
8. 第 948 轮：视觉验收发现并修复多行选区画刷泄漏；第二行不再被不透明填充遮挡。
9. 第 949 轮：扩展核心与 `wave-wave-edit-smoke`，覆盖安全批次、等值历史、Relation、拍网格冲突、
   选择保持、一次 Undo/Redo 和三张视觉证据。
10. 第 950 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 40.42 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 37.15 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 36.54 sec

Targeted wave-wave-edit-smoke: 1/1 passed, 3.17 sec
Core entry: 53/53 passed
Million-transition query metric: 24 ms
Screenshots: build/wave-edit-smoke-bit-pattern-multi-safe-preview.png
             build/wave-edit-smoke-bit-pattern-multi-relation-warning.png
             build/wave-edit-smoke-bit-pattern-multi-grid-mismatch.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：批量命令在修改前校验目标存在、ID 唯一、值合法、序列连续且起点位于 Scenario 内；
任一目标失败时不留下部分写入。预演与提交共用相同的 Lane 序列集合，联合 Event 同步、Relation
清理和无变化判断；等值批次不清除既有 Redo。投影不修改 Scenario、稳定 ID、Saved、autosave 或
剪贴板。离屏截图复核确认选区透明填充不会再因跨行绘制状态泄漏而覆盖后续波形。

用户视角验收：

```text
用户一次框选 req 与 ack 的 20–60 ns 四拍，输入 01 后，两条信号都立即显示 0101 虚线结果；
状态栏说明“4 beats on 2 Bit signals”“changes 2”和“一次 Undo”。按 Enter 后两条波形同时生效，
一次 Ctrl+Z 同时恢复两条波形、Event、Relation 和原选区。

再次输入 01 时显示全部 8 个 signal-beat 已匹配且 no command will run；提交不增加历史。
输入会删除临时 Relation 的常量 0 时，字段、两条最终波形和受影响连线在提交前统一显示风险，
并列出准确关系描述；真实提交与一次 Undo 逐项一致。

当 ack 临时使用 20 ns 拍而 req 使用 10 ns 拍时，同一选择输入 01 会立即变红并说明
different beat grids；按 Enter 不改变任何数据，输入仍全选可修正。第二条信号的透明选区不再
遮住原波形、刻度或虚线模式。
```

三张离屏截图确认安全批次、Relation 风险和拍网格冲突均能在提交前辨识；两条目标波形、范围边界、
刻度、输入字段和状态栏保持同时可见。

长期 Goal 保持 active；第 950 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 951–960 轮：多 Bit 逐信号模式录入

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

本轮消除多个 Bit 信号需要不同激励时仍须逐条框选、逐条输入和逐条撤销的成本。既有单行范围字段
继续把不含 `/` 的模式广播到全部目标；多 Bit 选择中可用 `/` 分隔模式，并按画布从上到下映射，
例如 `01 / 0011`。没有新增模式、面板、表格或确认页。

逐轮交付：

1. 第 951 轮：审计同模式广播完成后，多 Bit 不同激励仍需重复选择和提交的剩余成本。
2. 第 952 轮：定义“不含 `/` 继续广播，含 `/` 时按可见顺序一对一映射”的兼容输入合同。
3. 第 953 轮：在既有模式解析前划分逐信号模式，每条继续支持 `0b`、空格、逗号和下划线。
4. 第 954 轮：每条模式独立校验长度与整除关系，并可使用不同长度在同一目标拍数内重复。
5. 第 955 轮：联合投影每条最终波形，状态栏直接显示 `req=01×2; ack=0011×1`。
6. 第 956 轮：模式数量错误、空模式和非整除模式原位阻断；能够点名具体信号而非只报告全局失败。
7. 第 957 轮：复用 `SetLaneSequencesCommand` 一次提交不同序列；等值映射不进入历史或清除 Redo。
8. 第 958 轮：更新占位文本、范围帮助和选择反馈，公开 `/` 的从上到下映射及广播兼容语义。
9. 第 959 轮：扩展核心与 `wave-wave-edit-smoke`，覆盖不同序列、广播兼容、无效果、数量错误、
   指定信号非整除、选择保持及一次 Undo/Redo，并保存两张视觉证据。
10. 第 960 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 40.93 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 37.80 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 36.96 sec

Targeted wave-wave-edit-smoke: 1/1 passed, 3.08 sec
Core entry: 53/53 passed
Million-transition query metric: 21 ms
Screenshots: build/wave-edit-smoke-bit-pattern-per-signal-safe-preview.png
             build/wave-edit-smoke-bit-pattern-per-signal-count-mismatch.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：`/` 只在 Bit 模式评估层解释，不改变既有单模式解析规则。模式数量必须为一条或与
所选 Bit 信号数完全一致；逐信号模式分别归一化、扩展到同一拍范围并生成有序命令参数。预演和
提交使用同一参数集合，任一模式失败时不创建命令，不修改 Scenario、稳定 ID、Saved、autosave、
剪贴板或选择。不同模式的核心用例确认一条命令可写入两种序列并精确 Undo/Redo。

用户视角验收：

```text
用户一次框选 req 与 ack 的 20–60 ns 四拍，输入 01 / 0011 后立即看到 req=0101、ack=0011；
字段旁无需再选择目标，状态栏直接显示 req=01×2; ack=0011×1、变化信号数和一次 Undo。
按 Enter 后两条结果同时生效，一次 Ctrl+Z 同时恢复波形、Event、Relation 和选区。

再次输入 01 / 0011 时显示 no command will run，提交不增加历史。输入 01 / 0011 / 0 时字段
立即变红，明确说明“3 patterns for 2 selected Bit signals”和从上到下映射规则；Enter 不改变数据，
草稿仍全选可修正。

输入 01 / 010 时错误直接点名 ack，并说明四拍不能整除三符号模式。只输入 01 时仍沿用广播语义，
两条信号都得到 0101，原有单信号及多信号路径不变。
```

两张离屏截图确认逐信号安全预演和模式数量错误均能在提交前辨识；输入字段、映射结果、两条目标
波形、选区、刻度和状态栏保持同时可见。

长期 Goal 保持 active；第 960 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 961–970 轮：Bit 长电平游程录入

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

本轮消除 reset、enable、valid 等 Bit 信号需要连续保持数十拍时重复输入同一字符的成本。既有
Bit 模式字段支持 `symbol*N`，例如 `0*8 1*4`；语法可直接用于单模式广播，也可写入 `/` 分隔的
逐信号模式。没有新增模式、面板、表格或确认页。

逐轮交付：

1. 第 961 轮：审计 Bit 模式完成后长连续电平仍需逐字符输入的剩余成本。
2. 第 962 轮：定义 `symbol*N` 只重复紧邻的一个 `0/1/X/Z`，计数必须为正十进制且不可留空。
3. 第 963 轮：在既有模式解析器中展开游程，保留 `0b`、空格、逗号、下划线等原有输入兼容性。
4. 第 964 轮：将同一展开逻辑复用于共享广播和 `/` 逐信号映射，不增加第二条提交路径。
5. 第 965 轮：预演继续使用展开后的逐拍值，虚线波形、映射摘要、变化数和 Relation 影响保持一致。
6. 第 966 轮：`0*0` 和 `0*` 原位阻断，错误说明计数必须大于零或给出 `0*8` 修正示例。
7. 第 967 轮：单模式展开继续受 1024 符号限制，既有 1024 拍与 4096 signal-beat 门禁保持。
8. 第 968 轮：更新占位文本、范围帮助、键盘反馈和状态摘要，公开游程写法而不占用新按钮。
9. 第 969 轮：扩展 `wave-wave-edit-smoke`，覆盖共享/逐信号、安全预演、提交、无效果、
   一次 Undo/Redo、零计数、缺失计数、超限和两张视觉证据。
10. 第 970 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 41.29 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 39.14 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 35.22 sec

Targeted wave-core-tests + wave-wave-edit-smoke: 2/2 passed, 3.98 sec
Core entry: 53/53 passed
Million-transition query metric: 24 ms
Screenshots: build/wave-edit-smoke-bit-pattern-run-length-safe-preview.png
             build/wave-edit-smoke-bit-pattern-run-length-invalid-count.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：游程只在既有 Bit 模式解析层展开为规范化逐拍值；后续拍网格、整除关系、联合
Lane 投影、Event/Relation 影响和 `SetLaneSequencesCommand` 均继续使用同一数据流。共享模式
`0*2 1*2` 和逐信号模式 `0*2 1*2 / 1*2 0*2` 都由同一个评估器产生最终赋值。任一计数无效或
展开超限时不创建命令，不修改 Scenario、稳定 ID、Saved、autosave、剪贴板或选择；等值提交
不进入历史或清除 Redo。

用户视角验收：

```text
用户一次框选 req 与 ack 的 20–60 ns 四拍，输入 0*2 1*2 / 1*2 0*2 后立即看到
req=0011、ack=1100 的虚线最终结果；无需数同字符或逐条编辑，状态栏同时说明四拍和一次 Undo。
按 Enter 后两条结果同时生效，一次 Ctrl+Z 同时恢复波形、Event、Relation 和选区。

只输入 0*2 1*2 时仍广播到两条信号并得到 0011，原广播习惯不变。再次输入相同逐信号游程时
显示 no command will run，不增加历史。

输入 0*0 / 1 时字段立即变红并点名 req，说明计数必须大于零；输入 0* / 1 时给出 0*8 示例，
输入 0*1025 / 1 时说明超过 1024-symbol limit。三种输入按 Enter 均不改变数据，草稿保持全选
可直接修正。
```

两张离屏截图确认安全游程预演与非法计数均能在提交前辨识；紧凑输入、展开后的两条波形、透明
选区、刻度和状态栏保持同时可见。

长期 Goal 保持 active；第 970 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 971–980 轮：Bus/Enum 序列游程录入

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

本轮消除 Bus 数据阶段和 Enum 状态阶段包含长保持区间时重复粘贴同一值的成本。既有 Beat 编辑器
支持 `value*N`，例如 `0x00*8 0xff*2` 或 `IDLE*4 WAIT_ACK*2`；展开后继续使用原有逐拍投影、
Relation 预检和原子序列命令。没有新增模式、面板、表格或确认页。

逐轮交付：

1. 第 971 轮：审计 Bit 游程完成后，Bus/Enum 混合序列中的长连续值仍需重复录入的剩余成本。
2. 第 972 轮：定义 `value*N` 为紧邻值的正十进制重复；完整合法 Enum 符号优先，避免破坏现有名称。
3. 第 973 轮：在既有 Bus/Enum 列表解析中展开游程，保留逗号、分号、空格、Tab 和换行输入。
4. 第 974 轮：零计数、缺失计数、多重星号与超限展开原位阻断；展开总数继续限制为 1024。
5. 第 975 轮：为编辑状态显式记录 sequence 语义，使单项游程和非法游程不被误判为普通单值。
6. 第 976 轮：展开结果继续驱动逐拍虚线、End、Event/Relation、等值抑制及一次 Undo/Redo。
7. 第 977 轮：占位文本和帮助公开 `value*N`；无效计数不再显示没有意义的伪展开数量或 Apply 数。
8. 第 978 轮：输入字段由 105–150 px 扩至 160–240 px，并保留隐式 X 提示及 960 px 控件包含性。
9. 第 979 轮：扩展 `wave-wave-edit-smoke`，覆盖 Bus 安全提交、Undo/Redo、无效果 Redo 保留、
   三类非法计数、Enum 实际提交/Undo 和三张视觉证据。
10. 第 980 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 39.46 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 37.07 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 35.48 sec

Targeted wave-canvas-add-lane-smoke + wave-wave-edit-smoke: 2/2 passed, 4.42 sec
Core entry: 53/53 passed
Million-transition query metric: 20 ms
Screenshots: build/wave-edit-smoke-bus-sequence-run-length-safe-preview.png
             build/wave-edit-smoke-bus-sequence-run-length-invalid-count.png
             build/wave-edit-smoke-enum-sequence-run-length-safe-preview.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：解析器先尝试完整 Enum 符号，再在普通值校验失败且 token 含 `*` 时解释尾随计数；
因此合法旧符号不改变语义。展开值进入与普通列表完全相同的 `BusEditProjection` 和
`SetLaneSequenceCommand`，不引入第二套波形写入。任一 token、计数或展开上限失败时不创建命令，
不修改 Scenario、稳定 ID、Saved、autosave、剪贴板、选择或已有 Redo。等值游程提交只更新结果
选区，不进入历史。

全量测试首次捕获占位文本替换使旧隐式 X 断言失效；最终文本收敛为
`Value/list · X (implicit) · *N`，同时保留原目标语义和游程发现性。相关新增信号旅程与完整
Wave Edit 旅程复测 2/2 后，三配置全量均通过。

用户视角验收：

```text
用户在 data[7:0] 的 110 ns 拍输入 0x12*2 0x34*2，立即看到 110–150 ns 四拍分别为
0x12、0x12、0x34、0x34；紧凑输入和展开波形同时可见。Apply 后一次生效，一次 Ctrl+Z 恢复，
Redo 后再次精确得到同一结果。

基线三拍本来都是 0x35 时输入 0x35*3，界面显示 no command will run；提交不增加历史，也不清除
上一真实命令的 Redo。输入 0x00*0、0x00* 或 0x00*1025 时，字段分别说明计数必须大于零、
需要正十进制计数和超过 1024 值上限；Enter 不修改数据，草稿保持全选可修正。

在 state 的 110 ns 拍输入 IDLE*2 WAIT_ACK DONE*2，立即看到五拍
IDLE、IDLE、WAIT_ACK、DONE、DONE；Apply 和 Ctrl+Z 各一步完成。Enum 浮层扩宽后完整表达式、
五拍预演、刻度和状态栏可同时校对。
```

三张离屏截图确认 Bus/Enum 安全游程与非法计数均能在提交前辨识；输入字段、展开波形、目标范围、
刻度和状态栏无遮挡，960 px 既有浮层包含性回归继续通过。

长期 Goal 保持 active；第 980 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 981–990 轮：显式范围内的 Bus/Enum 序列编辑

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

本轮消除用户已经框选时间范围后，仍需退出范围栏、重新定位单拍并打开 Bus/Enum 浮层才能粘贴
序列的割裂。既有范围字段现在根据输入自动区分整段单值和逐拍序列，不新增编辑模式、面板或确认页。

逐轮交付：

1. 第 981 轮：审计显式范围栏、单拍序列浮层、拍网格、预览、Relation 预检和提交路径。
2. 第 982 轮：定义兼容契约：无序列标记时保持整段单值；列表、`value*N` 或 `/` 映射进入序列语义。
3. 第 983 轮：将 Bus/Enum 列表解析抽成可接收指定文本的共用入口，范围栏沿用每条 Lane 的进制和
   Enum 映射；完整合法 Enum 符号仍优先于 `*N` 解释。
4. 第 984 轮：范围序列只覆盖完整拍；短序列仅在整除选中拍数时重复，多信号要求相同拍网格；
   继续使用 1024 拍和 4096 signal-beat 实时门禁。
5. 第 985 轮：按最终逐拍值建立所有目标 Lane 的隔离投影、Relation 联合影响和准确变化数；
   输入阶段绘制最终虚线波形，不修改 Scenario、稳定 ID、Saved、autosave 或历史。
6. 第 986 轮：范围占位和帮助公开 `Value/list · *N`、`Symbol/list · *N` 及 `/` 从上到下映射；
   非整除、数量不匹配、值非法和拍网格不一致均在原字段内点名原因。
7. 第 987 轮：提交复用 `SetLaneSequencesCommand`，一次 Undo/Redo 原子恢复全部波形、Event、
   Relation 和选区；等值序列显示 `no command will run` 且保留已有 Redo。
8. 第 988 轮：扩展 `wave-wave-edit-smoke`，覆盖单 Bus 四拍共享序列、安全预览、实际提交、
   无效果、Undo/Redo、非法长度和两张视觉证据。
9. 第 989 轮：扩展 `wave-wave-edit-autoscroll-smoke`，覆盖两个 Enum 信号使用 `/` 和 `*N`
   一次录入不同五拍序列、逐行预览、实际结果、单步 Undo 及视觉证据；同时修复首次加宽造成的
   最小窗口工具栏裁切，保持 960 px 包含性。
10. 第 990 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 40.36 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 36.76 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 35.71 sec

Targeted wave-wave-edit-smoke + wave-wave-edit-autoscroll-smoke: 2/2 passed, 5.80 sec
Core entry: 53/53 passed
Million-transition query metric: 21 ms
Screenshots: build/wave-edit-smoke-bus-range-sequence-safe-preview.png
             build/wave-edit-smoke-bus-range-sequence-invalid-length.png
             build/wave-edit-autoscroll-smoke-enum-range-sequence-mapped-preview.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：范围栏只有在输入包含实际序列语义时才走逐拍投影；`0x22`、`IDLE` 等单值继续使用
原整段赋值路径。共享序列对每条目标 Lane 独立规范化和校验；`/` 映射数量必须等于目标信号数，
顺序与画布从上到下一致。所有目标先完成拍网格、长度、值和影响预检，再由一个命令提交，因此任一
失败不会产生部分写入。预览和提交使用相同 assignments，Relation 风险与真实清理结果一致。

首次 targeted 回归捕获范围输入框统一加宽后最小窗口工具栏裁切；最终保留 Bus/Enum 的 105 px
最小宽度，仅在空间充足时扩展到 200 px，序列仍可通过字段水平滚动完整编辑。第二次回归发现
offscreen 环境不保证未激活窗口拥有键盘焦点，验收改为断言非法草稿保留、全文选中、选区保留和
零模型/历史变化；真实交互的字段聚焦逻辑保持。

用户视角验收：

```text
用户先框选 data[7:0] 的 20–60 ns 四拍，直接在已出现的范围栏输入 0x12 0x34；无需退出选区或
重新点击某一拍，画布立即显示 0x12、0x34、0x12、0x34。Enter 一次生效，一次 Ctrl+Z 恢复，
Redo 后得到同一结果。再次输入同一序列显示 no command will run，不新增历史。

同一四拍输入 0x01 0x02 0x03 时，字段立即说明四拍不能整除三个值；原波形、选区和历史不变，
Enter 后全文保持选中可直接修正，第一次 Esc 只丢弃草稿，第二次 Esc 才关闭范围。

用户框选 state 与 state_next 的 50–100 ns 五拍，输入
IDLE DONE*2 IDLE*2 / DONE IDLE*2 DONE*2；两行分别预演
IDLE、DONE、DONE、IDLE、IDLE 和 DONE、IDLE、IDLE、DONE、DONE。Enter 一次完成两行，
一次 Ctrl+Z 同时恢复两条信号和原选区。
```

三张离屏截图确认单 Bus 安全/非法路径与双 Enum 映射均能在提交前辨识；输入字段、透明选区、
逐拍虚线、刻度和状态栏同时可见，没有桌面窗口或其他应用交互。

长期 Goal 保持 active；第 990 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 991–1000 轮：载入既有 Bus/Enum 拍值并就地修改

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

本轮消除用户修改既有 Bus/Enum 范围中的一两拍时，必须先从画布逐项抄写完整序列的成本。载入入口
内嵌于既有值字段，不增加编辑模式、面板或确认页；载入本身是只读准备动作，实际改动继续复用上一轮
的逐拍预演和一次提交。

逐轮交付：

1. 第 991 轮：审计 Bus/Enum 当前值、隐式值、扩展元数据、拍网格和范围序列字段的往返语义。
2. 第 992 轮：在既有范围值字段末尾加入现有拍值载入动作；保持字段默认空白，不增加首次操作步骤。
3. 第 993 轮：只允许完整显式拍、拍内恒定值和多信号相同拍网格进入载入，避免隐式 X 被物化。
4. 第 994 轮：对预设扩展元数据、非法存量值、`/` 冲突、规模超限和不可无损往返值给出禁用原因。
5. 第 995 轮：16 拍以内展开全部 token；较长序列将三个及以上连续同值压缩为 `value*N`。
6. 第 996 轮：多信号按画布从上到下生成 `/` 映射，并用每条 Lane 的既有解析器完成往返校验。
7. 第 997 轮：载入保持 `modified=false`、模型/历史/Saved 不变；虚线字段样式与状态栏明确区分
   “当前值已载入”与“草稿已修改”，并优先显示第一条信号和第一拍。
8. 第 998 轮：编辑任一 token 自动进入既有最终波形预演；第一次 Esc 仅隐藏载入值，第二次关闭
   范围；程序清空字段后显式恢复未修改状态，保证 Undo 后入口重新可用。
9. 第 999 轮：扩展两个 Wave Edit offscreen 旅程，覆盖单 Bus 载入/隐藏/单拍修改/Undo、双 Enum
   从上到下载入/双行单拍修改/Undo，以及隐式 X 禁用原因和三张视觉证据。
10. 第 1000 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 39.54 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 37.27 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 36.56 sec

Targeted wave-wave-edit-smoke + wave-wave-edit-autoscroll-smoke: 2/2 passed, 6.25 sec
Core entry: 53/53 passed
Million-transition query metric: 20 ms
Screenshots: build/wave-edit-smoke-bus-range-current-values-loaded.png
             build/wave-edit-smoke-bus-range-current-values-one-beat-edit-preview.png
             build/wave-edit-autoscroll-smoke-enum-range-current-values-loaded.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：载入逻辑不直接调用任何写命令。每个候选拍先确认存在显式 Segment、没有扩展元数据、
完整覆盖同一拍且值可规范化；序列文本生成后再通过既有解析器验证可逆。载入只设置字段文本、只读
状态属性、焦点和选择；直到用户实际编辑并按 Enter，才建立最终逐拍投影并走原子序列命令。任一
目标不安全时整批禁用，不产生部分文本、模型变更或历史。

定向测试首次发现 QLineEdit 的程序清空会保留 modified 状态，导致第一次 Esc 后及标量赋值 Undo
后的载入入口不能恢复；最终在两个程序清空路径显式设置 `modified=false`。第二次视觉复核发现
多 Enum 长文本全选时优先显示尾部，最终改为反向全文选择，使画布从上到下的第一条信号与第一拍
优先可见，同时仍可一次替换全文。

用户视角验收：

```text
用户框选 data[7:0] 的 20–60 ns 四拍，点击字段末尾载入图标，立即得到
0x00 0x00 0x00 0x00；此时模型和 Saved 不变。只把第二项改成 0x2a，画布仅在对应一拍显示
最终虚线；Enter 一次生效，Ctrl+Z 一次恢复。若不想编辑，Esc 只隐藏文本并保留原范围。

用户框选 state 与 state_next 的 50–100 ns 五拍，一次载入即按从上到下显示两组当前值；分别
只修改各自中间一拍，预演和提交都保持两行对应关系，一次 Ctrl+Z 同时恢复。

用户框选没有显式 Segment 的 Bus 拍时，载入图标保持禁用并说明该拍是 implicit X，避免只为查看
当前值就改变工程数据；用户仍可直接输入新值执行有意写入。
```

三张离屏截图确认字段内载入文本、从上到下顺序、透明范围选区、时间刻度和单拍最终预演均可辨识；
所有 GUI 路径均使用 offscreen，没有操作桌面或其他同步开发中的应用。

长期 Goal 保持 active；第 1000 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 1001–1010 轮：载入序列 token 与画布拍位联动

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

本轮解决载入当前值后，用户仍需在长文本和画布之间人工计算“这个 token 是哪条信号、哪一拍”的
问题。联动直接附着于既有文本光标和整体选区，不新增面板、模式、编号栏或确认步骤。

逐轮交付：

1. 第 1001 轮：审计范围字段光标、载入序列化、拍网格、画布范围覆盖层和状态反馈路径。
2. 第 1002 轮：在安全载入序列化时同步记录 token 字符范围、Lane ID、物理范围和拍序号。
3. 第 1003 轮：16 拍以内的展开 token 精确映射单拍，不以显示值反查，重复值仍无歧义。
4. 第 1004 轮：较长序列的 `value*N` token 映射整个连续游程，并记录首拍、拍数和总拍数。
5. 第 1005 轮：多信号 `/` 映射保留画布从上到下的 Lane 身份，相同值也能定位到正确信号。
6. 第 1006 轮：文本光标/选择变化只自动露出目标 Lane 和起始时间，不改变编辑光标或显式范围。
7. 第 1007 轮：画布使用青色虚线局部框；状态栏显示信号、拍序号、物理范围和值。
8. 第 1008 轮：全文选择不假定 token；编辑、Esc、关闭范围、切换文档或失去载入状态立即清理
   字符映射和局部覆盖，避免陈旧定位。
9. 第 1009 轮：扩展两个 Wave Edit offscreen 旅程，覆盖 Bus 第二拍、双 Enum 第二 Lane 第三拍、
   百拍压缩游程、自动露出、状态文本、生命周期清理及三张视觉证据。
10. 第 1010 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 40.46 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 37.98 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 36.24 sec

Targeted wave-wave-edit-smoke + wave-wave-edit-autoscroll-smoke: 2/2 passed, 6.54 sec
Core entry: 53/53 passed
Million-transition query metric: 20 ms
Screenshots: build/wave-edit-smoke-bus-range-current-values-token-target.png
             build/wave-edit-autoscroll-smoke-enum-range-current-values-token-target.png
             build/wave-edit-autoscroll-smoke-enum-range-current-values-compressed-run-target.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：映射只由通过上一轮安全门禁且完成解析器往返校验的载入文本生成。每个 token 的
字符位置与其展开值在同一循环产生，`value*N` 使用对应的首拍和连续拍数；多 Lane 在拼接 `/`
分隔符时同步加入文本偏移。光标定位仅查询这些不可变 span，不解析用户正在修改的草稿。用户一旦
开始输入，现有草稿预演接管，载入 span 和局部定位立即失效。

局部框复用既有范围绘制和裁剪路径，保持刻度、原波形与整体选区可见。定位会调用既有只读
`ensureLaneVisible` / `ensureCursorVisible`，但不写 `cursorTick_`、`selectionRange_` 或命令栈。
公开的只读测试接口返回当前目标 Lane 和物理范围，用于断言视觉状态与实际 token 一致。

用户视角验收：

```text
用户载入 data[7:0] 的 20–60 ns 四拍，点入第二个 0x00；无需数网格，画布只框出 30–40 ns，
状态栏显示 data[7:0]、beat 2 of 4 和当前值。随后输入新值时，定位框让位给真实最终波形预演。

用户载入 state 与 state_next 的五拍，点入第二组第三个 IDLE；画布自动定位 state_next 的
70–80 ns，不会因第一组也含相同符号而选错 Lane。

用户选择两条 Enum 的完整 1 us，载入文本自动压缩；点入 WAIT_ACK*5 后，一次看到 state 上
50–100 ns 的整个五拍游程和 beats 6–10 of 100。Esc 只隐藏文本并清除局部框，完整范围仍可继续
复制、清除或重新载入。
```

三张离屏截图确认短 token、多 Lane 重复值和压缩游程均能与画布准确对应，局部框没有遮挡原波形、
时间刻度或整体选区；没有操作桌面或其他同步开发中的应用。

长期 Goal 保持 active；第 1010 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 1011–1020 轮：有效序列草稿的连续 token 定位

状态：阶段完成，达到可用节点；长期 Goal 保持 active；未打包、未提交、未推送，未执行桌面交互。

本轮解决载入当前值后第一次输入就丢失 token 与画布对应关系，导致修改第二、第三拍时再次人工
数拍的问题。定位继续附着于既有范围字段、最终波形预演和局部覆盖层，不增加编辑模式、面板或
确认步骤。

逐轮交付：

1. 第 1011 轮：审计 Bus/Enum 列表、`value*N`、`/` 分组、完整拍投影和文本选择生命周期。
2. 第 1012 轮：解析器为每个合法 token 保留原字段字符起点与长度，不再依赖分隔后文本反查。
3. 第 1013 轮：解析器同步记录展开值偏移和数量，使 `value*N` 与普通 token 使用同一映射路径。
4. 第 1014 轮：范围序列评估将 token 映射到真实拍数组，不使用等宽拍假设推算物理范围。
5. 第 1015 轮：短模式重复时，一个 token 保留全部重复命中；局部虚线框同时绘制所有目标拍。
6. 第 1016 轮：共享序列 token 保留所有目标 Lane；逐信号 `/` 序列保留各自文本偏移与 Lane 身份。
7. 第 1017 轮：有效修改态复用同一定位覆盖层，状态栏显示目标出现次数、信号数和紧凑拍号。
8. 第 1018 轮：文本选择必须完整落在一个 token 内才定位；跨 token 选择和非法草稿立即清除目标。
9. 第 1019 轮：扩展两个 Wave Edit offscreen 旅程，覆盖重复 Bus 模式、共享/逐 Lane Enum、
   连续第二 token、压缩游程、跨 token 选择和三张视觉证据。
10. 第 1020 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 40.16 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 38.75 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 37.32 sec

Targeted wave-wave-edit-smoke + wave-wave-edit-autoscroll-smoke: 2/2 passed, 6.66 sec
Core entry: 53/53 passed
Million-transition query metric: 33 ms
Screenshots: build/wave-edit-smoke-bus-range-sequence-edited-token-target.png
             build/wave-edit-autoscroll-smoke-enum-range-edited-values-next-token-target.png
             build/wave-edit-autoscroll-smoke-enum-range-edited-compressed-run-target.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：序列解析结果在通过值校验和 1024 值展开门禁后保存 token 字符范围、展开偏移与
展开数量；范围评估再使用已经验证的真实拍数组构造目标，不改变逐拍赋值或 Relation 评估。
共享序列为同一文本 span 追加所有 Lane 和重复命中，逐信号序列则把 `/` 前的原始字符偏移加入
各 Lane span。无效评估不发布映射，因而错误草稿不会显示陈旧目标。

定位状态只在未修改的安全载入文本或存在有效 `BusRangeSequenceProjection` 的修改草稿中存活。
绘制遍历当前 token 的全部目标范围；只读测试接口继续返回首目标，并新增目标数量用于验证重复与
共享语义。状态反馈不会覆盖字段 tooltip 中的完整预检说明。提交仍复用
`SetLaneSequencesCommand`，没有新增模型字段、命令或保存格式。

用户视角验收：

```text
用户在 data[7:0] 的 20–60 ns 四拍输入 0x12 0x34。点第一个 token 时同时框出第 1、3 拍，
点第二个 token 时立即改为第 2、4 拍；状态显示 2 target occurrences 和 beats 2, 4 of 4，
不需要重新数拍，Scenario 与 Saved 在 Enter 前保持不变。

用户修改 state / state_next 的五拍文本后，先点第一组 DONE，再点第二组 DONE；目标从 state
70–80 ns 准确切换到 state_next 70–80 ns。拖选跨越两组 token 时定位消失，不会误认为只修改
第一项。共享序列中的同一 DONE 会同时框出两条信号。

用户在完整 1 us 范围输入 IDLE*5 DONE*5 DONE*90 / IDLE*100，选中 DONE*5 后只框出 state 的
50–100 ns，并显示 beats 6–10 of 100。Esc 丢弃草稿后局部框消失，原范围和模型保持不变。
```

三张离屏截图确认重复目标、逐 Lane 连续定位和压缩游程与最终虚线波形、透明整体选区、时间刻度、
字段选择及状态反馈同时可辨识；没有操作桌面或其他同步开发中的应用。

长期 Goal 保持 active；第 1020 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 第 1021–1030 轮：从预演拍反向选择序列 token

状态：阶段完成，达到可用节点；用户要求在本可用版本后结束长期 Goal；未打包、未提交、未推送，
未执行桌面交互。

本轮解决用户已经看到需要修改的波形拍，却仍需回到长文本中人工寻找对应 token 的问题。反向定位
直接复用上一轮的字符 span 和拍目标，不增加模式、面板、编号栏或确认步骤。

逐轮交付：

1. 第 1021 轮：审计画布点击、显式范围拖动、字段焦点离开提交及 token 映射生命周期。
2. 第 1022 轮：确认有效修改草稿点击画布会先提交，定义无修饰拍点击的安全优先级。
3. 第 1023 轮：实现 Lane/物理 tick 到唯一文本 span 的反向查询，外部范围边界不参与拦截。
4. 第 1024 轮：单击映射拍直接选中 QLineEdit token，并保持显式范围、编辑时间和草稿状态。
5. 第 1025 轮：被点击的重复 occurrence 调整为首目标，避免长范围自动跳回第一次出现位置。
6. 第 1026 轮：共享 token 从任一 Lane 点击均可选中，并保留全部 Lane/拍影响框。
7. 第 1027 轮：逐信号 `/` 序列按实际 Lane 选择对应分组，不以相同显示值猜测。
8. 第 1028 轮：悬浮映射拍显示手形指针与 token/目标数量提示；离开或边界操作恢复原指针。
9. 第 1029 轮：扩展两个 Wave Edit offscreen 旅程，覆盖载入态、修改态、重复、共享、逐 Lane、
   焦点释放、零提前提交及四张视觉证据。
10. 第 1030 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、开发视角验收、用户视角验收和文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 40.90 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure
73/73 tests passed
Total Test time: 40.63 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure
73/73 tests passed
Total Test time: 39.02 sec

Targeted wave-wave-edit-smoke + wave-wave-edit-autoscroll-smoke: 2/2 passed, 6.50 sec
Core entry: 53/53 passed
Million-transition query metric: 23 ms
Screenshots: build/wave-edit-smoke-bus-range-current-values-waveform-token-pick.png
             build/wave-edit-smoke-bus-range-sequence-waveform-token-pick.png
             build/wave-edit-autoscroll-smoke-enum-range-waveform-token-pick.png
             build/wave-edit-autoscroll-smoke-enum-range-shared-waveform-token-pick.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：鼠标按下在通用 `hasPendingValueEdit()` 提交之前查询有效 token 映射；仅无修饰键、
范围内部、非外边界且命中 Bus/Enum 目标拍时消费点击。命中 span 内的目标按实际 Lane/时间旋转到
首位，使现有文本→画布定位、自动露出和只读测试接口都指向用户刚点击的位置；其他 occurrence
继续绘制。释放事件只恢复字段焦点，不进入范围移动或 Segment 编辑。

悬浮查询只读取当前有效映射并显示手形指针与 token/目标数量；未修改载入态要求
`loadedExisting`，修改态要求有效 `BusRangeSequenceProjection`。无效草稿、标量输入、Esc、
提交或范围关闭均无映射，因此自动回到原有交互。Ctrl/Shift/Alt 与范围外边界继续走既有路径，
没有改变提交命令、保存格式或模型。

用户视角验收：

```text
用户载入 data[7:0] 的四拍当前值，看到第三拍需要修改，直接点 40–50 ns；字段立即选中第三个
0x00，画布仍保留原范围和局部框，未生成命令。无需从四个相同值中人工判断位置。

用户输入 0x12 0x34 形成四拍重复预演，点第 3 拍会选中 0x12，点第 4 拍会选中 0x34；
两次点击均保持两处实际影响可见，Scenario 和 Saved 在 Enter 前不变。

用户编辑 state / state_next 的逐 Lane 序列，直接点 state_next 第三拍即选中第二组 DONE。
共享序列中点击 state_next 第四拍仍选择唯一共享 DONE，同时框出 state 与 state_next 两个目标；
状态显示 2 target occurrences on 2 signals。
```

四张离屏截图确认字段 token 选择、被点击目标、重复/共享影响框、透明整体选区、原波形、刻度和
状态反馈同时可辨识；没有操作桌面或其他同步开发中的应用。

第 1030 轮为已验收的阶段可用节点。按用户要求，长期 Goal 在本轮完成后结束。
本轮未打包、未提交、未推送。

## 第 1031–1040 轮：序列草稿的安全撤销与连续定位

状态：阶段完成，达到可用节点；全部 GUI 验收使用 offscreen，未操作桌面，未打包、未提交、
未推送。

本轮不增加新面板或模式，集中修复 Bus/Enum 范围序列在真实连续操作中的四个断点：全局
Undo/Redo 可能绕过活动文本草稿而操作模型；载入当前值后首次修改无法恢复安全基线；点击所得
主 Lane/拍在逐字符输入后会跳回规范首目标；有效 token 映射会抢占既有范围拖动。

逐轮交付：

1. 第 1031 轮：固化文本 Undo/Redo、载入基线、Tab 导航及映射态拖动的失败合同和零模型变化
   断言。
2. 第 1032 轮：Edit 菜单及 Ctrl+Z 在活动可编辑 QLineEdit 有文本历史时优先撤销文本，不再
   误撤销波形 CommandStack。
3. 第 1033 轮：补齐文本 Redo、QAction enabled/text/tooltip 与焦点、文本变化和菜单展开的同步；
   隐藏、禁用或只读字段不截获模型历史。
4. 第 1034 轮：载入当前值时保存 Scenario、Lane 顺序、范围、命令状态、原文本和 token spans
   组成的安全基线；上下文或命令状态变化时立即清空旧载入文本、映射与锚点，不能继续显示为
   新模型的当前值。
5. 第 1035 轮：逐字符文本 Undo 回到原文时恢复 `loadedExisting`、原 spans 和未修改样式，保持
   Scenario、Saved、autosave、范围、光标和命令历史不变。
6. 第 1036 轮：源自载入值的修改草稿第一次 Esc 只恢复安全基线，第二次才隐藏载入文本；普通
   丢弃使用程序化清空以终止陈旧文本 Undo 链。
7. 第 1037 轮：Tab/Shift+Tab 按画布 Lane 和时间顺序在映射目标间前后移动，不提交、不循环；
   重复 occurrence 可逐个到达，`value*N` 游程仍作为一个原子 token；Ctrl+Shift+Tab 不被劫持。
8. 第 1038 轮：token 点击延迟到释放且未超过拖动阈值后生效；未修改载入态恢复原有范围拖动，
   修改态拖动原位阻断并保留草稿，不隐式提交，释放后恢复正常指针与字段焦点。
9. 第 1039 轮：保持点击锚点跨逐字符输入及短暂非法中间态；主目标使用亮色实线框，其他关联
   目标使用弱化虚线框，状态栏增加主 Lane、拍号、物理范围和关联数量；扩展两个 offscreen 旅程。
10. 第 1040 轮：完成默认、Qt Creator Debug、Qt Creator Release 三配置构建与 73 项全量
    offscreen CTest、核心性能入口、开发视角验收、用户视角验收、视觉复核及文档同步。

开发视角验收：

```text
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
73/73 tests passed
Total Test time: 44.78 sec

cmake --build build/qtcreator-debug
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-debug --output-on-failure -j 1
73/73 tests passed
Total Test time: 41.69 sec

cmake --build build/qtcreator-release
QT_QPA_PLATFORM=offscreen ctest --test-dir build/qtcreator-release --output-on-failure -j 1
73/73 tests passed
Total Test time: 38.83 sec

Final targeted wave-wave-edit-smoke: 1/1 passed, 4.28 sec
Core entry: 53/53 passed
1000 lanes / 1,000,000 transitions / 100,000 queries: 23 ms
Screenshots: build/wave-edit-smoke-bus-range-token-replace-anchor.png
             build/wave-edit-smoke-bus-range-token-keyboard-next.png
             build/wave-edit-smoke-bus-range-current-values-text-undo-restored.png
             build/wave-edit-autoscroll-smoke-enum-range-shared-token-replace-anchor.png
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

实现审计确认：MainWindow 仅在当前焦点是可见、启用、非只读且具有对应本地历史的 QLineEdit 时
路由文本 Undo/Redo，否则继续使用 CommandStack。已有字段在菜单创建时统一订阅 textChanged，
动态字段继续由 focusChanged 接入，因而模型栈为空时文本撤销仍可用，文本栈为空时模型撤销不被
隐藏编辑器拦截。

载入基线只在同一 Scenario、Lane 顺序、显式范围和命令状态下有效。QLineEdit 文本回到基线时
恢复未修改载入态；波形命令 Undo/Redo 改变状态时，即使字段仍聚焦，也会先清空旧文本、spans、
锚点与本地文本历史。Esc 恢复使用 setText 清除已丢弃分支的旧文本历史。点击锚点保存实际 Lane
和 tick，映射重建后在当前 span 内把包含该锚点的 target 提升为主目标；非法中间态只暂时隐藏
映射，不清除锚点。Tab 导航对所有 span/target 按 Lane 与时间排序，不改变文本或模型。

点击/拖动区分在 QApplication 的标准拖动距离上完成。短按释放才选择 token；越过阈值的未修改
载入态进入原有 MoveRange 路径并保留取消恢复，修改草稿则明确要求先应用或丢弃，并在释放后恢复
正常指针和字段焦点。Backtab 仅在修饰键恰为 Shift 时导航，保留 Ctrl+Shift+Tab。没有新增命令、
模型字段、保存格式、工具按钮或模式。

用户视角验收：

```text
用户在 data[7:0] 的四拍范围输入 0x12 0x34，点击第 3 拍后真实逐字符替换为 0x56。
输入经过临时的 0、0x、0x5 状态，最终主目标仍为第 3 拍 40–50 ns；第 1 拍作为关联目标继续
显示。Edit > Undo 只把文本恢复为 0x12，Redo 只恢复 0x56，Scenario 与 Saved 均不变化。

用户按 Tab 后直接到第 4 拍的 0x34，Shift+Tab 可依次回到第 3、2 拍；字段焦点和草稿保留，
首尾不循环。主拍使用亮色实线，其他重复拍使用弱化虚线，不再猜测当前输入针对哪一处。

用户载入四拍当前值并修改第 2 拍，文本 Undo 恢复完整载入原文和 loadedExisting 状态；Redo
恢复单拍草稿；Esc 再恢复载入原文，下一次才隐藏。字段聚焦但没有本地文本历史时执行模型 Undo，
旧载入文本和拍映射立即消失；模型 Redo 也不会复活旧映射。整个恢复过程不创建额外波形命令。

用户在未修改载入态从映射拍拖动，仍能看到原有整段 Move 预演并可 Esc 取消；在修改草稿时
同样拖动会明确阻断，草稿和选区均不丢失。共享 Enum 从 state_next 第 4 拍替换 token 后，
主目标仍停留在 state_next 80–90 ns，state 的关联目标保持可见，Saved 状态不变。
```

四张离屏截图确认字段选择、主/关联目标视觉层级、原波形、透明范围、刻度和状态反馈同时可辨识；
自动化另外断言每次点击、输入、文本 Undo/Redo、Tab、Esc 及取消拖动均没有提前改变 Scenario。

第 1040 轮为已验收的阶段可用节点。本轮未打包、未提交、未推送。

## 后续可用性收敛与自动化交付

状态：实现与最终三配置静默验收均已完成。本轮 GUI 验收全部使用 offscreen，
未操作桌面或其他同步开发中的应用。

本轮收敛范围：

- 时序状态不再把未关联 Clock 的信号伪装为 Sync；关联时钟时显示 Sync，未关联时显示
  Grid 和 10 ns 固定步长，Async 继续以 1 tick 编辑。
- 指针采样与操作结果分离：被动悬浮使用独立常驻区域，不再覆盖保存、删除、Undo 等结果。
- Tab/Shift+Tab 仅在存在可导航波形目标时消费；否则回到 Qt 标准焦点链。活动可编辑
  QLineEdit 即使无本地文本历史，Ctrl+Z/Ctrl+Y 也不穿透到波形 CommandStack。
- Timeline End 的有效草稿先更新坐标映射，同一次画布点击随后继续；非法草稿仍保留焦点并阻断动作。
- Bus 单击只选中一拍而不弹出编辑器；Enter/双击进入就地编辑。Beat 和 Range 增加
  Reserved 预设，使用显式全零波形并保留独立预设语义；相关按钮进入标准键盘焦点链。
- Bus/Enum 范围当前值载入改为虚拟基线：Bus 隐式 X、预设扩展元数据和拍内已有变化可被载入；
  未改 token 不实体化或重写，仅被修改的 token 以完整编辑拍写入。`SetLaneSequencesCommand` 按
  `preserveExisting` 保留未改拍，继续以一步 Undo/Redo 恢复有效修改。
- Clock 标题右键轻量入口补齐 period/frequency、phase、duty 分子/分母、active edge 和
  reset/disable condition；Bus 快速新增与完整属性入口统一支持 1–65536 bit。
- Relation 常规连线默认隐藏，Edit 菜单提供 `Show relation constraints`；会被当前编辑清理的
  Relation 风险连线即使在隐藏模式下仍显示。工具栏命名收敛为 `Measure / Markers`。
- GUI 正式保存记录已读取文件的 SHA-256；覆盖前检测外部变更，并提供 Reload、Save As、
  Overwrite 和 Cancel，避免桌面编辑与 CLI/Codex 同时写入时静默丢失一方结果。
- `wave-cli` 增加 generate/compare/bridge 兼容适配入口，原样转发参数、文本输出和旧工具退出码；
  capabilities 显式标记三者为非结构化兼容命令。
- 自动化 v1 新增 Draft 2020-12 capabilities/report/operation-batch Schema；33 类 operation 能力信息包含
  Schema 引用、必填/可选字段、幂等性、危险性和是否修改工程。
- CMake `Portable` install 组件收集桌面程序、主 CLI、三个兼容工具、Schema、CLI 文档、最小
  operations 示例和 handshake 工程；源码仓库中的 Windows 脚本负责 windeployqt、ZIP 和 SHA-256。

最终收敛同时确认：Timeline End 失焦提交保留用户的新焦点目标，重缩放后同一次点击仍落在
提交前所指时间；Clock duty 分子/分母使用完整正 `qint64`，不再把合法大分数静默截断；Reserved
在画布和结果反馈中与普通零值可辨识；单信号与批量序列命令统一遵守 `preserveExisting`，纯保留
步骤不改波形、不扩 End、不同步 Event，也不创建历史；超宽 Bus 当前值载入在物化前执行文本预算。

最终验收：

```text
Default build / offscreen CTest: 76/76 passed, 41.41 sec
Qt Creator Debug build / offscreen CTest: 76/76 passed, 38.61 sec
Qt Creator Release build / offscreen CTest: 76/76 passed, 37.16 sec
Focused desktop and CLI contract tests: 6/6 passed
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

遗留与阻塞：

- 多 Waveform/Scenario 已有项可切换，但新建、复制、删除和重排尚未交付。历史命令仍持有
  `Scenario*`，直接修改 `Project::scenarios` 容器可使旧历史失效；在改为稳定身份绑定前不冒险开放该操作。
- 浅色背景/主题切换尚未实现。
- 长信号列表的常驻/粘性添加入口尚未实现；当前仍使用列表末尾的三个快速添加按钮。

本轮已完成三套全量构建与 offscreen CTest，并通过 `git diff --check`；未打包、未提交、未推送。

## 横向工作

- 每个阶段结束后同步更新 `README.md`、`PLAN.md`、`GOAL.md`。
- 每项完成状态必须对应自动化测试或可复现手工验证。
- 保持生成物为派生产物，不将其反向解析成场景事实源。
- 在导入和 compare 阶段建立 1000 lane、百万 transition 的只读浏览基准。
- 长期 Goal 已在第 1030 轮可用版本后结束；用户随后追加的第 1031–1040 轮也已完成。仅在用户
  另行明确要求时打包、提交或推送。
