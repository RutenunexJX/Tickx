# Wave Workbench

Wave Workbench 是用于 FPGA 数字时序设计与验证场景编排的独立桌面应用。工程中的
`Project`、`Scenario`、`Lane`、`Segment`、`Event`、`Relation` 是唯一事实源；画布、
步骤表和后续生成文件均为该模型的视图或派生产物。

当前版本已完成阶段 1 至阶段 6 及后续交互迭代。桌面端现收敛为单一波形画布，不再创建
Project、Inspector、Scenario Dock 或模式切换栏。它提供整数 tick
时间模型、clock/bit/bus/enum/transaction/event/group lane、自绘可滚动画布、光标锚定
缩放、多 lane 框选、区间绘制、时钟局部门控/禁用、时钟参数编辑与周期
事件重定时、Lane/Group 创建、属性编辑、显示重排与事务化删除、复制/粘贴、撤销/重做、
Event/Segment 双向同步、Marker、Relation、版本化 JSON 工程安全保存与迁移、后台恢复快照，
以及由共享行为语义生成的 SystemVerilog、SVA、cocotb、SVG、PNG、PDF 和 WaveDrom
JSON。VCD/CSV 导入、Expected/Actual 比较、报告生成及跨应用桥接能力保留在独立 CLI 和
领域模块中，不再占用桌面编辑界面。验收证据见
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
WaveDrom JSON。桌面应用的 Export 工具提供全场景、当前选择或指定时间范围
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

- 主窗口中央组件直接为 WaveCanvas；不创建 Project、Inspector、Scenario Dock 或独立 Modes
  工具栏。波形始终可直接编辑，工具栏只保留临时 `Measure`、缩放和 `Fit scenario`；Export
  位于 File 菜单。
- 无命令行工程参数时直接进入 200 ns 空白波形；中央首先显示 `+ CLK`、`+ BIT`、`+ BUS` 和
  简短操作提示。`File > New` / `Ctrl+N` 立即恢复同一空白默认值，不再要求先配置工程。
- `+ CLK`、`+ BIT`、`+ BUS` 点击后在末行显示就地输入条。用户只需补齐名称以及 Clock
  period 或 Bus width，按 Enter 提交、Esc 取消；名称自动唯一、颜色随机分配。CLK 自动创建
  clock domain，只有一个时钟时 Bit/Bus 自动关联，多个时钟时就地选择。创建与补齐始终是
  一个撤销项。新 Bit 空白区按 `0` 显示，新 Bus 空白区按居中红色虚线和 `X` 显示。
- 标尺右上角 `End` 输入框直接修改时间轴终点，例如 `500 ns`。缩短到现有内容之前会在原处
  报错并保留输入，合法修改进入命令栈并可撤销。
- 左侧信号名区域支持单击选择、拖动重排、双击重命名、`Delete` / `Backspace` 删除；右键打开
  轻量参数入口。Clock 可直接修改频率或周期，Bit 可修改颜色、高度和 clock domain，Bus 还可
  修改位宽、符号性和进制。所有提交均进入命令栈。
- 单击非 Bit Segment 进行选择；拖动主体整体移动，拖动左右手柄修改边界，双击修改现有值，
  `Delete` / `Backspace` 清除为该信号的隐式值。右键波形可设置常用值、插入一拍 Pulse、编辑
  完整 Segment 或清除 Segment。提交后状态栏显示信号、时间范围、结果和 `Ctrl+Z` 提示。
- Bit lane 悬浮时高亮当前拍并显示将变为 `0` 或 `1`；单击只翻转这一拍，横向拖动逐拍翻转
  覆盖范围。选中 Bit 后可直接按 `0`、`1`、`X`、`Z` 写入当前拍，也可从右键菜单选择四态值。
- `Shift` + 拖动在一个或多个 lane 上建立时间范围；Edit 菜单的 Copy/Paste range 按稳定 lane ID
  和相对整数 tick 复制，粘贴作为一个可撤销命令。左右方向键移动直接编辑时间光标。
- Bit Event 菱形无需切换模式即可直接拖动。预览期间模型不变，虚线只覆盖原/新边沿中较早位置
  到后继 Segment 结束的局部范围；释放后以一个命令同步更新 Segment 与 Event。
- 单击 Bus 波形会在附近显示上下文、直接值输入框以及 `0`、`X`、`Z`、`Don't care`。预设可
  单击或拖放，直接值按 Enter 提交；非法值保持浮层与焦点并解释原因。默认宽度是一拍，插入后
  可拖动边界修改宽度；旧工程的 `Reserved` 语义继续按零值兼容读取。
- 时间交互采用无设置项的轻吸附：距离可见刻度、Clock 边沿或信号边沿 7 像素以内自动对齐，
  并显示吸附时间提示；超出范围使用原始整数 tick，按住 `Alt` 可临时绕过吸附。
- `Ctrl` + 鼠标滚轮以指针所在时间为锚缩放，`Shift` + 滚轮水平滚动；中键拖动或按住空格再左键
  拖动可平移时间轴。`Fit scenario` 显示完整场景。
- `Measure` 是临时状态：左键创建或移动唯一活动光标；直接拖动以起点作为临时参考、终点作为
  活动光标，`Shift` + 左键创建临时参考。两者显示带符号的 `Δ` 时间宽度。
- `Ctrl` + 左键创建持久锁定光标，`Ctrl` + 拖动创建持久锁定区间。锁定对象使用独立颜色并写入
  工程 Marker；单击后可拖动或用方向键移动，`Delete` / `Backspace` 删除，均支持 Undo/Redo。
- 活动光标存在时，每个可见信号在左侧名称区域显示该时刻的采样值。再次点击 `Measure` 或按
  Esc 返回直接波形编辑，活动、临时和选中状态清除，持久锁定光标保留。
- Undo/Redo 位于 Edit 菜单，并保留 `Ctrl+Z` / `Ctrl+Y`；工具栏不重复显示按钮。
- 状态栏常驻显示 `Not saved`、`Unsaved changes`、`Saved` 或 `Recovery loaded · Save required`；
  自动恢复快照不会被误报为正式保存。File 菜单使用原子替换方式保存 `project.wave.json`，首次
  保存 Untitled 工程时从文件名推断项目名。修改后 1.5 秒启动 Qt Concurrent 后台恢复快照。
- File > Export 导出 SystemVerilog/SVA/cocotb 和文档图。VCD/CSV 导入、Expected/Actual 对比、报告和
  跨应用桥接由 `wave-compare`、`wave-bridge` 等 CLI 提供，不占用桌面工作区。
- 特殊 lane 或 group 仍可从 Edit 菜单创建并编辑完整结构属性；属性变更、依赖感知删除和显示顺序
  调整均保留 stable ID，并以单个命令撤销/重做。
- `waveworkbench://open` 按 Project/Scenario/Lane 稳定 ID 和整数 tick 打开或定位。
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
- 单命令撤销/重做，以及快速创建命令的原子替换、取消和场景时长 Undo/Redo。
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
- 画布快速添加 CLK/Bit/Bus、唯一命名、随机颜色、默认时钟域、信号名拖动重排、双击重命名、
  右键轻量参数、Delete 删除以及菜单快捷键均通过离屏回归。
- 无模式直接编辑的 Segment 选择、整体移动、边界缩放、双击改值、Delete 清除与 Undo，Bit
  单拍/多拍翻转、0/1/X/Z 键盘输入、Shift 跨 lane 框选和中键平移均通过离屏回归。
- Event 菱形局部虚线预览期间模型不变，释放、Undo 与 Redo 后 Event/Segment 保持同步。
- Bus 浮层覆盖直接值、非法值纠错、0/X/Z/Don't care 单击与拖放以及波形右键；无配置空白
  工程、End 直接输入、保存状态和 Measure 退出具有独立离屏回归。
- Waveform 工具栏离屏检查确认仅有一个临时 Measure 动作，不存在 Edit、Transition、Export 与
  Undo/Redo 按钮，菜单快捷键仍存在。
- 独立用户旅程从空白工程完成三类信号创建、波形编辑、纠错、时间轴延长、测量、Save As、
  文件回读和 1440×900 截图，全程 offscreen。
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

最近一次验证结果：21 个 CTest 入口均通过，其中核心入口包含 25 组细分测试；常规构建和
Qt Creator Debug 构建均已验证。完整验收记录见 [PLAN.md](PLAN.md)。

## 当前限制

- FST 尚未接入；当前未捆绑兼容 FST 解析库，未实现自定义方言。
- SVA 仅在关系可无损表达为无 condition 的精确 bit 边沿、明确 clock/reset 且为精确
  cycle delay 时生成；带 condition 或其他不可无损转换的关系只产生诊断，不生成近似
  assertion。
