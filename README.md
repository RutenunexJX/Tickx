# Wave Workbench

Wave Workbench 是用于 FPGA 数字时序设计与验证场景编排的独立桌面应用。工程中的
`Project`、`Scenario`、`Lane`、`Segment`、`Event`、`Relation` 是唯一事实源；画布、
步骤表和后续生成文件均为该模型的视图或派生产物。

当前版本已完成阶段 1 至阶段 6 及最终加固。它提供可运行的 Qt 6 桌面工作台、整数 tick
时间模型、clock/bit/bus/enum/transaction/event/group lane、自绘可滚动画布、光标锚定
缩放、索引化吸附、多 lane 框选、区间绘制、时钟局部门控/禁用、时钟参数编辑与周期
事件重定时、Lane/Group 创建、属性编辑、显示重排与事务化删除、复制/粘贴、撤销/重做、
Event 步骤表双向同步、Marker、Relation、
带确定性条件求值的 Validation、版本化 JSON 工程安全保存与迁移、后台恢复快照，
以及由共享行为语义生成的 SystemVerilog、SVA、cocotb、SVG、PNG、PDF 和 WaveDrom
JSON。应用可在后台导入标准 VCD/CSV、显示 Actual Trace、按可配置规则比较
Expected/Actual，并通过版本化 JSON、CLI 和 URI 与其他应用协作。验收证据见
[PLAN.md](PLAN.md)。

## 构建

要求：

- Qt 6.5 或更高版本，包含 Core、Gui、Widgets、Svg、Concurrent。
- CMake 3.24 或更高版本。
- 支持 C++20 的编译器。
- Ninja。

当前验证环境为 Qt Creator 18.0.2、Qt 6.10.2、G++ 13.1.0、GDB 11.2、
CMake 3.30.5、Ninja 1.12.1。

在 PowerShell 中配置、构建和测试：

```powershell
$env:PATH='E:\QT6\Tools\mingw1310_64\bin;E:\QT6\Tools\Ninja;' + $env:PATH
E:\QT6\Tools\CMake_64\bin\cmake.exe -S . -B build -G Ninja `
  -DCMAKE_PREFIX_PATH=E:\QT6\6.10.2\mingw_64 `
  -DCMAKE_CXX_COMPILER=E:\QT6\Tools\mingw1310_64\bin\g++.exe `
  -DCMAKE_MAKE_PROGRAM=E:\QT6\Tools\Ninja\ninja.exe
E:\QT6\Tools\CMake_64\bin\cmake.exe --build build --parallel 4
E:\QT6\Tools\CMake_64\bin\ctest.exe --test-dir build --output-on-failure
```

### Qt Creator

仓库提供 `Qt Creator - Debug` 和 `Qt Creator - Release` CMake presets。使用 Qt Creator
打开根目录 `CMakeLists.txt`，选择 Qt 6.5+ Desktop Kit 和 Debug preset，即可点击 Build；
将运行目标设为 `wave-workbench` 后可直接 Run 或按 `F5` 调试。Qt Kit 配置、可用目标、
示例工程参数及命令行等价验证见 [docs/qt-creator.md](docs/qt-creator.md)。

启动内置演示场景：

```powershell
.\build\wave-workbench.exe
```

打开示例工程：

```powershell
.\build\wave-workbench.exe .\examples\handshake\project.wave.json
```

从工程生成 HDL、Python 和图形派生产物：

```powershell
.\build\wave-generate.exe .\examples\handshake\project.wave.json `
  .\examples\handshake\generated
```

该命令生成 testbench、可可靠转换的 SVA include、cocotb test，以及 SVG、PNG、PDF、
WaveDrom JSON。桌面应用的 File > Export Artifacts 提供全场景、当前选择或指定时间范围
导出，并可设置 PNG DPI、PDF 每页时间跨度和附加层。

无界面执行 Expected/Actual 对比：

```powershell
.\build\wave-compare.exe .\examples\handshake\project.wave.json `
  .\examples\handshake\compare --edge-tolerance-tick=5000
```

可选参数包括 X 规则、bus mask、时间窗口、relation-only 和
`--fail-on-difference`。输出为 JSON、CSV、HTML。

跨应用文件接口：

```powershell
.\build\wave-bridge.exe describe project.wave.json workspace.json
.\build\wave-bridge.exe import-signals project.wave.json signals.json output.wave.json
.\build\wave-bridge.exe link-frame project.wave.json frame-reference.json output.wave.json
.\build\wave-bridge.exe pinloom-entry project.wave.json exports pinloom-entry.json
```

契约见 [docs/integration-contracts.md](docs/integration-contracts.md)。

## 当前交互

- 波形画布的 lane 列表末尾提供 `+ Add signal` 行，点击后直接创建信号 lane。
- `Ctrl` + 鼠标滚轮：以鼠标所在时间点为锚缩放。
- `Shift` + 鼠标滚轮：水平滚动。
- 拖动期间按住 `Alt`：临时绕过当前 snapping，不修改永久设置。
- Draw 工具：在 bit lane 上拖动绘制 0/1 区间；纵向位置决定逻辑电平。
- Draw 工具：在 clock lane 上拖动并选择 Gated、Disabled 或 Run；三者分别表示保持低电平、
  驱动 X 或清除局部覆盖。
- Draw 工具：在 bus/enum/transaction/event lane 上拖动并输入值。
- 双击 bus/enum/transaction/event lane：从吸附后的时刻创建一个主刻度宽度的值区间。
- Pulse：在选定 bit lane 的光标处插入一个时钟周期或主刻度宽度的脉冲。
- Transition 工具：拖动可见 Event 菱形以移动对应波形边沿，步骤表时间同步更新。
- Cursor 工具：左键单击创建或移动唯一的活动光标；左右方向键按固定网格移动活动光标。
- 直接拖动以起点作为临时参考、终点作为活动光标；`Shift` + 左键在不移动活动光标的
  前提下创建临时参考。两者均显示“临时 → 活动”的带符号 `Δ` 时间宽度。
- `Ctrl` + 左键创建持久锁定光标，`Ctrl` + 拖动创建持久锁定区间；锁定对象使用区别于
  活动和临时光标的颜色并写入工程 Marker。
- 单击锁定光标将其选中；拖动或左右方向键移动选中对象，`Delete` / `Backspace` 删除，
  移动和删除均支持 Undo/Redo。
- 活动光标存在时，每个可见信号在左侧名称区域显示该时刻的值，包括 clock 覆盖及
  bit/bus/enum/transaction/event 的区间值。
- 再次点击 Cursor 工具退出光标编辑，活动、临时和选中状态清除，持久锁定光标保留。
- Relation 工具：从一个 Event 菱形拖动到另一个 Event 菱形，创建可编辑时序关系。
- Events 表：直接编辑 time/cycle、action、target、value、expected result、clock domain
  和 description；支持排序、文本过滤、添加、删除及双击定位。
- Relations 表：编辑 min/max delay、clock domain、condition、severity 和 description；
  condition 支持稳定 lane ID、唯一显示名、布尔运算和四态精确比较。
- Validation 表：显示关系满足/违反、不适用、条件错误、缺失或多目标、clock domain、
  非法值和未定义区间；双击结果定位到 lane 和 tick。
- Undo/Redo：每次完整拖动只产生一个编辑命令。
- Snap 下拉框：选择无吸附、固定网格、主刻度、时钟上/下沿、信号边沿或 marker。
- 信号边沿和 marker 在模型刷新时建立排序索引，鼠标吸附只执行二分查找。
- 在 Select 工具中跨 lane 拖动建立矩形选择；`Ctrl` 单击增减 lane 选择。
- Edit > Copy/Paste range：按稳定 lane ID 和相对整数 tick 复制多 lane 范围；粘贴为一个
  可完整撤销/重做的命令。
- Fit scenario 显示完整场景；Fit selection 定位选定时间范围。
- File 菜单：打开或使用原子替换方式保存 `project.wave.json`，也可打开 `.autosave`
  恢复快照并保存回原工程路径。
- 修改后 1.5 秒启动 Qt Concurrent 后台快照；generation 校验阻止旧结果成为最新状态，
  快照仅序列化工程模型及 trace 引用，不复制外部 trace 数据。
- File > Export Artifacts：导出 SystemVerilog/SVA/cocotb 和文档图。
- File > Import VCD/CSV Trace：后台解析外部 trace；可取消，旧 generation 结果不会发布。
- Imported Trace：勾选需要显示的实际信号，编辑 Expected lane 名称映射，或使用 Auto-map。
- Align：支持手动 offset、marker start 对齐及 clock edge/cycle 对齐；结果始终为整数 tick。
- Actual Trace：`Ctrl` + 滚轮缩放，`Shift` + 滚轮水平滚动，Fit 显示完整 trace。
- Compare 模式：Expected 在上、Actual 在下；配置 exact/ignore X/X wildcard、
  edge tolerance、bus mask、选择时间窗或 relation-only。relation condition 在实际 source
  transition 的整数 tick 采样。
- Compare Result：显示差异数、first mismatch、offset、缺失信号、宽度冲突、条件错误及
  false 守卫诊断；双击结构化差异可定位。
- Export report：以安全替换方式输出 JSON、CSV 和独立 HTML 报告。
- Resources：显示 Private Frame 等链接资源的 stable ID、hash、summary 和
  Resolved/Unresolved 状态。
- Groups：按 group stable ID 显示实际分组和成员；失效 group 引用显示为 unresolved。
- Edit > Add lane / Add group：创建具有稳定 ID 的 lane 或 group。双击 Signals 或 Groups
  条目可编辑名称、类型、位宽、符号性、进制、枚举映射、clock domain、所属 group、
  颜色、高度和可见性。
- Lane/Group 属性变更保留 stable ID 及现有波形数据，并作为单个命令撤销/重做；会使
  既有 Segment/Event 非法的位宽或类型变更、失效引用及含成员 group 的类型转换会整体
  拒绝，不产生部分修改。
- Edit > Remove selected lane / group：删除普通 lane 时同步删除其 Event、相关 Relation
  和不再有效的 trace 映射；删除 group 时保留成员 lane 并解除分组。删除与依赖清理可由
  单个 Undo 完整恢复。
- Edit > Move selected lane up/down：以 `Alt+Up` / `Alt+Down` 调整 Lane 或 Group 在
  `Scenario::lanes` 中的显示顺序；画布、Signals/Groups 和图形导出同步更新，HDL/Python
  生成仍按 stable ID 确定化，不受界面排列影响。
- Clocks：双击条目编辑名称、period、phase、有理数 duty、active edge 和 reset/disable
  condition。命令会按逻辑 cycle 一次性重定位项目内引用该时钟的事件及关联波形边界；
  若结果越出场景或反转区间则拒绝整项变更。
- File > Export and Open in Pinloom：先写入 JSON 归档条目，仅在用户确认后打开 URI。
- `waveworkbench://open` / `waveworkbench://compare`：按 Project/Scenario/Lane 稳定 ID
  和整数 tick 打开或定位。

所有时间均以整数 tick 持久化。画布中的浮点数只用于时间到像素的视图变换，不作为工程
时间事实。

### Relation condition

空 condition 表示无条件关系。非空表达式支持 `!`、`&&`、`||`、括号、
`==`/`!=` 及等价的 `===`/`!==`。lane 可用稳定 ID 或唯一显示名引用；含空格的显示名
使用反引号，例如 `` `payload data` == 0x35 ``。单 bit lane 可直接作为布尔值，
`true`/`false` 可作为布尔常量或比较值。

Validation 在预期 source Event 的 tick 采样 Scenario；relation-only Compare 在匹配到的
实际 source transition tick 采样 Actual trace。bit/bus/enum/clock 使用精确四态比较，
不会继承 Compare 的 X wildcard 或 bus mask。false 表示关系在该次 source 上不适用；
语法错误、歧义名称、缺失 lane、无采样值、非法字面量、缺失映射或宽度冲突均产生可定位
错误。解析器会检查所有操作数，不用短路规则隐藏无效引用。完整字段语义见
[docs/project-format.md](docs/project-format.md)。

## 目录与模块

```text
src/
  wavetime/      整数时间、时钟、cycle 和 snapping
  wavecore/      Project/Scenario/Lane/Segment/Event/Relation 领域模型
  waveproject/   JSON 序列化、迁移、未知字段保留和安全写入
  waveedit/      显式 command、undo/redo
  wavevalidate/  场景与关系验证、可定位诊断
  wavegenerate/  共享生成计划、HDL/Python 生成和图形导出
  waveimport/    标准 VCD/CSV 解析、只读 trace 索引、映射和时间对齐
  wavecompare/   区间比较规则、first mismatch、关系比较和报告生成
  waveintegration/  ZeroSlack、Private Frame、Pinloom 文件与 URI 契约
  cli/           无界面的 wave-generate、wave-compare、wave-bridge 命令
  app/           Qt Widgets 主窗口和 QAbstractScrollArea 自绘画布
tests/           核心与应用启动自动化测试
examples/        可移动的示例工程目录
docs/            工程格式等说明
```

`wavetime`、`wavecore` 和 `waveedit` 不依赖 QtWidgets。`waveproject` 仅依赖 QtCore
进行 JSON 和文件操作。波形采样点不对应独立 QWidget；画布仅绘制可见 lane 和可见
时间范围，并以二分查找定位首个可见 segment。

## 工程格式

工程目录的建议结构为：

```text
project.wave.json
traces/
generated/
exports/
```

格式细节、整数编码、稳定 ID、迁移和未知字段规则见
[docs/project-format.md](docs/project-format.md)。

## 测试状态

当前自动化测试覆盖：

- ps/ns/us/ms 精确换算、不可整除检测、溢出检测和大时间值往返。
- 多 clock domain、cycle 边沿和各类 snapping 基础算法。
- 参数化 clock、局部 gated/disabled 区间、清除覆盖、批量 cycle 重定时及完整撤销/重做。
- Lane/Group 创建、稳定 ID 成员引用、属性修改、依赖感知删除、非法结构转换拒绝及
  完整撤销/重做。
- Lane/Group 显示重排、边界拒绝、序列化顺序保持以及生成代码顺序无关性。
- 二进制、八进制、十进制和十六进制 bus 值；非整字宽的最高有效数字溢出检测。
- bit 的 0/1/X/Z、bus width/signedness、enum。
- segment 覆盖、合并、拆分和清除。
- 单命令撤销/重做。
- 多 lane 范围复制/粘贴及完整撤销/重做。
- Event 时间和值与关联 Segment 的双向同步。
- 画布式区间编辑到步骤表 Event 的同步。
- Marker command、Relation min/max delay、满足、违反、缺失目标和未定义区间。
- Relation condition 的运算符优先级、括号、稳定 ID/唯一名称、引号、四态值归一化、
  false 守卫、错误定位及 Expected/Actual 采样。
- JSON 往返、未知字段保留、schema 0 到 1 迁移。
- 整个工程目录移动后的相对路径恢复。
- Qt 桌面应用及磁盘示例工程的离屏启动；Lane 属性对话框截图及确认删除/Undo
  交互通过离屏回归。
- 共享生成计划、确定性 SystemVerilog/cocotb 输出、时钟覆盖调度和严格 SVA 转换；
  独立 assertion 模块及 testbench 均通过 SystemVerilog 语法检查。
- SVG、PNG、PDF、WaveDrom JSON 的范围、尺寸、页跨度及背景回归。
- `wave-generate` CLI 对磁盘示例工程的端到端生成。
- 生成的 cocotb 文件通过 Python 语法编译，SystemVerilog testbench 通过 Vivado `xvlog`。
- PNG 和由最终 PDF 栅格化所得页面完成视觉复核。
- VCD timescale、多值信号、`$dumpvars`、层级/总线名称、CSV 显式单位及取消。
- 外部 VCD 示例磁盘解析、名称映射、偏移安全移动和二分可见范围查询。
- 1000 个 signal、1,000,000 个 transition 的 100,000 次窗口查询基准。
- exact、ignore X、expected X wildcard、edge tolerance、time window、bus mask、
  enum equivalence 和 relation-only compare。
- first mismatch、缺失 signal、width mismatch、condition evaluation error、诊断可见性
  以及 JSON/CSV/HTML 报告。
- ZeroSlack signal-list、Private Frame sample、Pinloom entry 和 Wave Workbench URI。
- 主窗口后台 autosave 生成可由正式加载器读取的恢复快照。

最近一次验证结果：18 个 CTest 入口均通过，其中核心入口包含 23 组细分测试；完整验收
记录见 [PLAN.md](PLAN.md)。

## 当前限制

- FST 尚未接入；当前未捆绑兼容 FST 解析库，未实现自定义方言。
- SVA 仅在关系可无损表达为无 condition 的精确 bit 边沿、明确 clock/reset 且为精确
  cycle delay 时生成；带 condition 或其他不可无损转换的关系只产生诊断，不生成近似
  assertion。
