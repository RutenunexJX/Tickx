# Wave Workbench

当前版本：`v0.12.0`

Wave Workbench 是用于 FPGA 数字时序设计与验证场景编排的独立桌面应用。工程中的
`Project`、`Scenario`、`Lane`、`Segment`、`Event`、`Relation` 是唯一事实源；画布、
步骤表和后续生成文件均为该模型的视图或派生产物。

当前版本已完成阶段 1 至阶段 6 及后续交互迭代。桌面端现收敛为单一波形画布，不再创建
Project、Inspector、Scenario Dock 或模式切换栏。它提供整数 tick
时间模型、clock/bit/bus/enum/transaction/event/group lane、自绘可滚动画布、光标锚定
缩放、多 lane 框选、区间绘制、时钟局部门控/禁用、时钟参数编辑与周期
事件重定时、Lane/Group 创建、属性编辑、显示重排与事务化删除、复制/粘贴、撤销/重做、
Event/Segment 双向同步、Marker、Relation、版本化 JSON 工程安全保存与迁移、包含未命名工程的后台恢复快照，
以及由共享行为语义生成的 SystemVerilog、SVA、cocotb、SVG、PNG、PDF 和 WaveDrom
JSON。VCD/FST/CSV 导入、报告生成及跨应用桥接能力保留在独立 CLI 和领域模块中；嵌入式
Simulation Result 工作区直接复用同一比较领域模块。验收证据见
[PLAN.md](PLAN.md)。

Module Manifest v4 可携带目标模块依赖闭包中的 unresolved module 实例及其参数、
端口关联。结果工具栏通过 `Stubs (selected/total)` 显式选择可支持项；默认不选择时，
runner 在调用工具链前拒绝运行。选择后只生成无行为的 input-only 被动 stub，并将选择、
生成文件和构建指纹纳入 session 与运行证据。interface、program、语法不完整及关联方式
不兼容的依赖不会被静默处理。

ZeroSlack 的嵌入式 Simulation Result 工作区在 Actual 区域提供内部信号层级浏览器。
VCD scope 按原始组件构建实例树，支持搜索、scope 级复选和单信号复选；复选结果直接控制
Actual 波形可见集合。初次打开优先显示场景已映射的端口与 observation，其他内部信号由用户
按需加入。Verilator harness 追踪 99 层层级，并启用 struct 与下划线信号追踪；这不改变
Stimulus Scenario、Module Manifest 或 `wavewidgets` v1 C ABI。

FST 通过随应用分发的 Wellen 0.25.6 辅助程序读取。打开文件时只建立 scope、信号、位宽与
时间范围元数据；transition 优先为映射信号加载，无映射时使用最多 32 个信号的有界初始集，
其后仅在用户勾选时按最多 64 个一批加载。取消、文件
身份和 generation 检查会阻止旧批次覆盖当前 trace；`wavewidgets` 仅在辅助程序与共享库
相邻时声明 `on-demand-fst-trace/v1`，VCD/CSV 路径不依赖该辅助程序。

输出 watch lane 可直接绘制期望区间。Simulation Result 的 Compare 动作只比较具有期望区间
的输出，按完整 lane 身份过滤实际 trace，并在期望画布、实际波形和差异表中
同步标出不一致区间；单击差异会同时定位期望 lane 与实际 signal。Stimulus Scenario v5 以
`expectedSegments` 持久化该意图，且运行计划仍只消费 `segments`，期望值不会被驱动进 DUT。

Review 区域还提供轻量 trace 检查：指定时刻取值、区间稳定性，以及源边沿到目标边沿的
响应窗口。检查定义随 Stimulus Scenario v5 保存并在端口安全迁移时同步重映射；检查结果
只由当前 Actual TraceIndex 计算，场景或 trace 变化后立即失效，不写回工程或场景文件。

Simulation Result 工具栏的 `Run all (N)` 会按存储顺序串行运行当前模块的全部场景。Batch
页逐项显示等待、运行、通过、失败或取消状态，以及诊断、模型构建/缓存来源和耗时；单项失败
不会阻止后续场景，Stop 会取消当前项及剩余队列。每项使用独立结果文件并共享已有构建缓存，
点击成功行可切换到该场景的 Actual 波形。批量结果是运行期派生状态，场景编辑或单独重跑后
立即失效，不进入 Stimulus Scenario 或 simulation session schema。

Module Manifest v5 为端口和内部 observation 携带稳定语义身份、声明位置及 Slang 关系
提供的 driver 位置。Actual 波形或层级树中的映射信号可直接返回 ZeroSlack 的声明或具体
driver；ZeroSlack 中的信号也可定位到已打开结果中的对应 Actual 行。匹配优先使用语义身份，
仅在身份缺失时使用便携源码位置，未映射或歧义信号不会按名称猜测。共享工作区以
`result-source-navigation/v1` 声明该能力，`wavewidgets` C ABI 与 workspace contract
仍保持 v1。

Module Manifest 中的多个语义时钟候选会分别导入为独立 ClockDomain，不再因候选数大于一而
全部放弃。Simulation Result 工具栏直接显示 `Clocks (N)`，可逐个编辑周期、相位、占空比和
有效边沿；`Timing` 可在关联时钟网格与 1 tick 异步编辑之间切换。运行计划分别驱动每个时钟，
并保留任意 tick 的普通输入事件。仅修改时钟或异步刺激会复用已编译模型。

Module Manifest v3 将 packed struct、固定 unpacked array 和显式 modport interface 输入
展开为分组的可编辑 leaf lane。每个 leaf 保留 Slang selector、类型、方向和稳定 trace 名；
runner 生成 wrapper 重建原始端口，Stimulus Scenario v5 可在结构化根重命名后安全迁移既有
激励。当前 interface 范围限于无构造端口且显式指定 modport 的类型，`inout/ref`、动态数组
及宽度超过 64 bit 的 leaf 会给出明确诊断。

ZeroSlack 的正式 Wave Simulation 工作流通过版本化 `wavewidgets` C ABI 将完整工作区
嵌入编辑区 Wave Tab，无需用户手工传递文件。独立应用链接同一共享控件实现并继续作为
可单独运行的工具。当前兼容 Module Manifest v1-v5；v2 可携带选中 `always` 的观察范围
和内部信号 access path，v3 增加结构化输入 leaf 契约，v4 增加 unresolved dependency，
v5 增加结果与源码导航元数据，并将构建或运行错误以源码文件、行、列回传 ZeroSlack。

同一共享库还提供 `wave-workbench.waveform-view/v1` 轻量控件。宿主通过严格的
`wave-preview/v1` payload 提交 Symbolic Preview 或 Simulated Result；控件支持 generation
全量替换、稳定 lane/cursor 状态、源码跳转、full/compact 密度及 system/light/dark 主题，
但不解析 RTL 或启动仿真。接口见
[docs/wave-preview-v1.md](docs/wave-preview-v1.md)。

## 构建

要求：

- Qt 6.5 或更高版本，包含 Core、Gui、Widgets、Svg、Concurrent。
- CMake 3.24 或更高版本。
- 支持 C++20 的编译器。
- Ninja。
- Rust stable 与 Cargo（默认启用 Wellen FST reader；不需要 FST 时可配置
  `-DWAVEWORKBENCH_ENABLE_WELLEN=OFF`）。

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

工程只有一个 Scenario 时自动使用该 Scenario；工程包含多个 Scenario 时必须追加
`--scenario=<稳定 ID 或唯一名称>`。旧版第三个位置参数形式的 Scenario ID 仍兼容。

该命令生成 testbench、可可靠转换的 SVA include、cocotb test，以及 SVG、PNG、PDF、
WaveDrom JSON。桌面应用的 Export 工具提供全场景、当前选择或指定时间范围
导出，并可设置 PNG DPI、PDF 每页时间跨度和附加层。指定范围或分页跨度无效时，错误会在同一窗口就地显示，
保留范围、尺寸、勾选项与错误字段焦点，修正后直接继续。

无界面执行 Expected/Actual 对比：

```powershell
.\build\wave-compare.exe .\examples\handshake\project.wave.json `
  .\examples\handshake\compare --edge-tolerance-tick=5000
```

可选参数包括 X 规则、bus mask、时间窗口、relation-only 和
`--fail-on-difference`。多 Scenario 工程必须用 `--scenario=<稳定 ID 或唯一名称>` 明确
目标；成功输出同时回显 Scenario 名称与 ID。输出为 JSON、CSV、HTML。

跨应用文件接口：

```powershell
.\build\wave-bridge.exe describe project.wave.json workspace.json
.\build\wave-bridge.exe import-signals project.wave.json signals.json output.wave.json
.\build\wave-bridge.exe link-frame project.wave.json frame-reference.json output.wave.json
.\build\wave-bridge.exe pinloom-entry project.wave.json exports pinloom-entry.json
```

`import-signals` 与 `pinloom-entry` 在多 Scenario 工程中同样要求尾随
`--scenario=<稳定 ID 或唯一名称>`；`describe` 与 `link-frame` 仍是工程级操作，且
workspace manifest 的 Generate/Compare 命令模板会公开该选择器。

契约见 [docs/integration-contracts.md](docs/integration-contracts.md)。

统一无界面读取、校验与原子波形编辑：

```powershell
.\build\wave-cli.exe capabilities --pretty
.\build\wave-cli.exe generate .\examples\handshake\project.wave.json .\generated
.\build\wave-cli.exe compare .\examples\handshake\project.wave.json .\compare
.\build\wave-cli.exe bridge describe project.wave.json workspace.json
.\build\wave-cli.exe new .\stimulus "--name=Handshake stimulus" `
  "--duration=80 ns" --operations=.\tests\fixtures\cli-new-operations.json --pretty
.\build\wave-cli.exe inspect .\examples\handshake\project.wave.json --summary --pretty
.\build\wave-cli.exe signals .\examples\handshake\project.wave.json `
  "--match=req" --kind=bit --exact --pretty
.\build\wave-cli.exe sample .\examples\handshake\project.wave.json "--at=80 ns" `
  --lane=req "--lane=data[7:0]"
.\build\wave-cli.exe window .\examples\handshake\project.wave.json `
  "--start=70 ns" "--end=130 ns" --lane=req "--lane=data[7:0]"
.\build\wave-cli.exe edges .\examples\handshake\project.wave.json `
  "--start=70 ns" "--end=130 ns" --edge=rising --lane=req --lane=ack --pretty
.\build\wave-cli.exe markers .\examples\handshake\project.wave.json `
  "--match=request" --kind=phase "--start=70 ns" "--end=130 ns" --pretty
.\build\wave-cli.exe relations .\examples\handshake\project.wave.json `
  "--match=within" "--start=70 ns" "--end=130 ns" --lane=req --pretty
.\build\wave-cli.exe validate .\examples\handshake\project.wave.json
.\build\wave-cli.exe apply .\examples\handshake\project.wave.json `
  .\tests\fixtures\cli-operations-range.json --dry-run --pretty
.\build\wave-cli.exe apply .\examples\handshake\project.wave.json `
  .\tests\fixtures\cli-duration-truncate-operations.json --dry-run --pretty
```

`wave-cli capabilities` 无需工程即可让宿主机器读取命令、operation、选择器、时间和值域能力。
`generate`、`compare` 与 `bridge` 是对同目录旧工具的兼容适配器，旧入口继续有效；兼容适配器保留
旧工具的文本输出和退出码，并在 capabilities 的 `compatibilityCommands` 中明确标记为非结构化输出。便携包的最小操作示例和
运行顺序见 [docs/cli-quick-start.md](docs/cli-quick-start.md)。
能力文档同时公开 Draft 2020-12 JSON Schema 相对引用，以及每个 operation 的必填/可选字段、幂等性、
危险性和是否修改工程；Portable install 组件会将 Schema、CLI 文档、最小 operations 示例和 handshake 工程一并安装。
其他命令对 stdout/stderr 使用版本化 JSON，可从零创建工程，也支持 operations 从 stdin 输入、
显式输出文件、原地原子替换、dry-run、源 SHA-256 并发保护和稳定 ID；可按物理时间或时钟周期
采样、按名称或 ID 精简定位信号、直接查询可作为 Relation 端点的精确边沿、在不接触内部
Event ID 的情况下定位和审计既有 Relation、按名称/类型/时间窗口定位并诊断既有 Marker、
批量写入共享步长的多信号序列、查询局部波形、
清除、复制/移动
范围、延长或安全缩短时间轴，并新增、删除、重排或修改信号及其时钟。空白尾部可直接缩短；
若会裁剪现有波形则必须显式设置 `truncate`，预演和结果逐项报告裁剪/删除数量。空工程可直接
建立 Group、Clock、
Bit、Bus 与 Enum，Group 可修改、重排和删除且删除时保留成员，Enum 声明映射也可原子替换；
既有信号可用一个 operation 复制完整属性和波形，Clock 副本使用独立时钟域；现有波形边沿之间
可建立 Relation，点或区间
Marker 可完整增删改。Scenario、Lane、Clock、Group 与 Marker 选择器接受稳定 ID 或唯一的
大小写不敏感名称，Relation 以稳定 ID 管理，成功报告统一返回规范 ID。
值断言、默认拒绝覆盖和原地写入备份用于避免或恢复错误。可嵌入 Qt/C++ 应用的同层入口由
`waveautomation` 静态库提供。完整命令、时间语法、operation 字段和退出码见
[docs/automation-cli.md](docs/automation-cli.md)。

## 当前交互

详细操作见 [交互手册](docs/interaction.md)。


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
  wavefst/       Wellen FST 元数据与按信号 transition 加载边界
  wavecompare/   区间比较规则、first mismatch、关系比较和报告生成
  waveintegration/  ZeroSlack、Private Frame、Pinloom 文件与 URI 契约
  waveautomation/  可嵌入的机器可读 inspect、validate 与原子编辑 API
  cli/           wave-cli、wave-generate、wave-compare、wave-bridge 命令
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

## Verification

Run the configured CTest suite for the build being released. Current contracts are covered by model,
automation, ABI, external-reload and UI tests. Historical round-by-round results are available in Git;
they are not evidence of a test run against today's checkout.

## Suite application protocol

WaveWorkbench provides `wave://project?...` resources through
`suite-app/v1`, exposes `wave.project.open`, and publishes the
`wave.waveform` Surface. The Surface uses native mode with ABI version 1,
library `wavewidgets`, and factory
`wavewidgets_create_simulation_workspace_v1`; external application opening is
always declared as the fallback.

The adapter delegates project parsing and opening to the existing Wave model
and window APIs. The neutral Runtime does not duplicate wave-project state,
and other applications do not include WaveWorkbench private headers. When the
Runtime is missing, the standalone editor, CLI, bridge, and widget ABI remain
unchanged.

## 当前限制

- FST 当前只加载数字 bit-vector 信号；analog/string 等 Wellen 值类型会被跳过并给出诊断。
  读取器必须与应用或 `wavewidgets` 位于同一目录，详细规模门禁与归属见
  [docs/fst-reader.md](docs/fst-reader.md)。
- SVA 仅在关系可无损表达为无 condition 的精确 bit 边沿、明确 clock/reset 且为精确
  cycle delay 时生成；带 condition 或其他不可无损转换的关系只产生诊断，不生成近似
  assertion。
- `+ CLK` / `+ BIT` / `+ BUS` 仍位于信号列表末尾；长列表滚动时常驻或粘性添加入口尚未交付。


## Scenario lifecycle and theme

Create, duplicate, rename, delete and reorder use stable Scenario identities and undoable GUI commands.
The last Scenario cannot be deleted. Current selection falls back safely after deletion; external reload
preserves identity when possible. CLI operations are documented in [automation CLI](docs/automation-cli.md).
Light/dark semantic colors follow the system palette; the reusable view also exposes explicit theme modes.
