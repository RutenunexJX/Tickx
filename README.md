# Wave Workbench

当前版本：`v0.11.1`

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

- 主窗口中央组件直接为 WaveCanvas；不创建 Project、Inspector、Scenario Dock 或独立 Modes
  工具栏。波形始终可直接编辑，工具栏常驻只保留临时 `Measure / Markers`、缩放和
  `Fit scenario`；`Measure / Markers` 中的 Ctrl 点击/拖动会建立持久 Marker；
  建立持久范围时在同一固定工具栏显示批量赋值栏，不覆盖波形。Export 位于 File 菜单。
- 工程包含多个 Scenario 时，工具栏最左侧显示紧凑的 `Waveform` 选择器；只有一个 Scenario
  时整组控件（包括分隔线）完全隐藏。切换仅改变当前编辑目标，不重排工程数组、不写入模型、
  不新增 Undo，也不改变 Saved 状态；每个波形分别恢复其信号选择、时间光标、范围、缩放和滚动位置。
  `Ctrl+PageUp` / `Ctrl+PageDown` 可在不离开画布的情况下切换前后波形，到达首尾时不循环并
  明确提示返回方向；未通过校验的就地草稿会保留输入与焦点并阻止切换。已保存工程按规范化路径、
  Project 稳定 ID 和 Scenario 稳定 ID 记住最后编辑的波形，重新打开或调整 Scenario 数组顺序后
  仍恢复同一目标；目标已删除或身份重复时安全回到第一项并清除失效记忆。Save As 将当前选择绑定到
  新路径，URI 的显式 Scenario 始终优先于本地记忆。该偏好只存于编辑器设置，不进入工程 JSON。
  每个正式工程的唯一 Scenario 还分别记住最后安全位置：一条唯一、可见的非 Group 信号稳定 ID、
  整数编辑 tick 和当前可见的整数 tick 跨度。选中或定位信号、移动光标、缩放、Fit 及水平视图
  变化停止 400 ms 后，会为已有正式路径的工程去抖写入最新安全位置；不需要先 Save、切换波形或
  正常关闭。正常关闭、切换波形、New/Open 前及保存时仍立即写入。Untitled 工程和仍在就地命名的
  快速新增信号不写入该偏好。Save As 后经 New 再 Open、下次启动或异常退出后的恢复快照重开时，
  直接恢复该信号、时刻和时间细节尺度。跨度不保存像素
  滚动量，而是在最终窗口布局后以恢复光标为中心重建，因此窗口尺寸变化或滚动条出现不会退回完整
  时间轴。恢复只选择信号并定位光标，不恢复 Beat、Segment、显式范围或标题删除状态，状态栏明确
  显示 `no edit range restored`；因此重开后的 Delete 不会立即修改波形。信号已隐藏、删除或身份
  重复时只恢复安全时间和尺度并说明目标不可用；越界时间和跨度均钳到当前时间轴，旧设置中没有跨度
  或跨度损坏时仍可安全打开。
  同一会话再次切换优先使用更完整的内存上下文，范围、Segment、缩放和滚动不会被旧磁盘位置覆盖。
  URI 的显式 lane/tick 在恢复后覆盖本地位置，并成为后续正常打开的新安全位置。
  Undo/Redo 仍按全工程真实时间顺序执行，并在上一项编辑属于其他波形时自动切回该波形，
  避免撤销不可见内容。重名波形在选择器、标题和状态反馈中附带稳定 ID。
- 无命令行工程参数时直接进入 200 ns 空白波形；中央首先显示 `+ CLK`、`+ BIT`、`+ BUS` 和
  简短操作提示。`File > New` / `Ctrl+N` 立即恢复同一空白默认值，不再要求先配置工程。
- `+ CLK`、`+ BIT`、`+ BUS` 点击后在末行显示就地输入条。用户只需补齐名称以及 Clock
  period 或 Bus width（`1–65536`）；按 Enter 提交，或直接点击下一目标并在同一手势继续，Esc 取消。
  名称自动唯一、颜色随机分配。CLK 自动创建
  clock domain，只有一个时钟时 Bit/Bus 自动关联，多个时钟时就地选择。创建与补齐始终是
  一个撤销项。新 Bit 空白区按 `0` 显示，新 Bus 空白区按居中红色虚线和 `X` 显示。
- 标尺右上角 `End` 输入框直接修改时间轴终点，例如 `500 ns`。缩短到现有内容之前会在原处
  报错并保留输入，合法修改进入命令栈并可撤销；需要有意裁去尾部内容时使用 CLI 的显式
  `set-duration` + `truncate` 契约。
- 保存、另存为、新建、打开、关闭、导出或点击画布其他位置前，会先提交有效的就地创建、重命名、
  Bus/Enum 值和范围值草稿；鼠标左键、双击或右键会在提交后继续完成同一次目标操作，不要求第二次点击。
  非法草稿保留原值、位置与焦点并阻止后续动作。合法 End 草稿会在提交前记录用户所指时间；即使
  时间轴随后重缩放，同一次画布点击仍命中原时间目标，不需要再点一次。失焦提交不会把焦点从用户
  刚进入的 Bus 或其他编辑器抢回画布。Bus 与 End 同时待提交时按
  不丢位置的顺序处理；若 Undo/Redo 令 Bus 原位置落到 End 之外，不会钳到末拍，用户可延长 End
  后在原位置恢复，或按 Esc 放弃该草稿。
- 快速新增输入条是一个未结束事务。删除、重排、参数编辑、范围粘贴、End、Segment 双击和 Pulse 等
  已接入动作会先尝试完成新增；名称或参数无效时保留输入条与错误说明并阻止该动作。Esc 取消新增；
  防御性检测到旧的交错命令时，Ctrl+Z 逐条撤销后续命令，再取消新增，不会越过事务基线。
  开始快速新增时会关闭 Bus 编辑栏，隐藏控件也不能写入波形。
- 左侧信号名区域支持单击选择、拖动重排、`Delete` / `Backspace` 删除。单击后状态栏立即显示
  当前信号名、Delete 的实际目标和 `F2` 入口；若同次取消 Marker 或范围选择，也会明确报告。
  双击信号名或按 `F2` 就地重命名，入口显示目标信号名；Enter 或点击别处提交，Esc 取消并恢复
  当前信号及 Delete/F2 提示。右键参数菜单取消后保留同一目标反馈。空名或重名会保留编辑框与焦点并原处说明原因，
  成功重命名是一个可撤销命令。删除确认会先显示将清理的 Event/Relation 准确数量和 `Ctrl+Z`；
  完成后状态栏显示信号名、实际清理数量和恢复方式，Undo 后显示 `Ctrl+Y`。右键轻量入口可修改
  Clock 周期/频率、phase、duty 分子/分母、active edge 和 reset/disable condition，以及 Bit/Bus 常用参数；
  duty 分子/分母支持完整正 `qint64`，不会把合法大分数静默钳到较小范围。
  无效时间、duty 或颜色会在同一对话框中就地说明，保留全部输入并聚焦错误字段；真实
  修改后状态栏显示当前参数与 `Ctrl+Z`，原值确认显示 `no properties changed`，不新增 Undo、
  不清除 Redo，也不标记未保存。标题拖动期间按 Esc 会立即清除插入反馈并取消重排，随后释放
  鼠标不会提交 `MoveLaneCommand`；成功放下会显示信号名、原位置→新位置和 `Ctrl+Z`，原位放下
  明确显示顺序未改变且不增加 Undo。
- 没有显式波形范围时，选中非 Group 信号后，可通过 Edit 菜单的 `Duplicate selected signal`、`Ctrl+D` 或信号标题右键
  `Duplicate signal` 在原信号正下方立即创建完整副本。名称自动使用 `<原名>_copy` 并保证唯一，颜色重新
  随机分配；显示参数、分组和波形保持一致，但 lane/segment stable ID 独立。Clock 同时复制为独立时钟域，
  后续修改周期不会影响原时钟；普通信号保留原时钟关联。Event、Relation 与 trace mapping 不随波形复制。
  整个动作只产生一个 Undo，副本立即选中并可按 `F2` 改名。显式范围存在时，同一 Edit 动作和 `Ctrl+D`
  自动改为复制范围；从信号标题右键直接请求整信号复制时仍保留范围并提示先按 Esc。文本框焦点不会触发复制。
- 选中可见信号或 Group 后，Edit 菜单会显示对应的 `Hide selected signal` / `Hide selected group`，标题右键也提供
  `Hide signal` / `Hide group`；触发后立即从画布隐藏，不打开确认或属性窗口。标题选择、范围目标和便携面板同步清理，
  状态栏显示隐藏对象、当前 `Show N hidden items` 恢复入口和 `Ctrl+Z`。每次隐藏只产生一个 `Hide lane` / `Hide group`
  Undo，Redo 使用同一稳定 ID；显式范围存在时保持范围并提示先按 Esc。高级属性中的 `Visible` 仍用于完整参数编辑。

- 在高级 Lane/Group 属性中关闭 `Visible` 后，画布末尾和 Edit 菜单会显示
  `Show N hidden items`。按钮主体和 Edit 动作仍一次恢复全部；存在多个隐藏项时，按钮箭头会按名称列出
  隐藏信号与 Group，可只恢复其中一项。单项恢复保持原顺序和稳定 ID，立即选中并仅做必要的纵向揭示，
  不改变水平滚动、缩放或编辑光标；状态栏显示剩余隐藏数量和 `Ctrl+Z`。只剩一个隐藏项时按钮保持单击
  直达，不增加菜单步骤。单项或全部恢复均各自产生一个 Undo；显式范围存在时保持原选择并提示先按 Esc。
  被隐藏信号的标题选择、波形选择和 Bus 编辑栏会立即清除。
- 可见 Group 现在是标准可折叠层级：标题箭头、`Left` / `Right` 或右键
  `Collapse group` / `Expand group` 使用同一状态，成员标题缩进并显示连续层级引导线。折叠仅收起
  当前 Scenario 视图中的成员行，不修改 Lane `visible`、工程 JSON、Saved 或 Undo；若成员正处于
  Beat、Segment 或范围编辑，折叠会清除破坏性目标并安全指向 Group，但不会武装 Delete。查找、
  URI 或其他显式定位成员时自动展开；隐藏或删除折叠 Group 后成员立即恢复显示，不会遗留不可见
  信号。每个 Scenario 在当前会话中分别记住折叠状态。
- Group 标题与信号标题仍使用同一套直接操作：单击选择、拖动重排、双击或 `F2` 就地重命名、
  `Delete` / `Backspace` 删除；右键提供折叠入口及完整 Group 属性。选择、取消、完成及
  Add/Change/Move/Remove 的 Undo/Redo 均明确使用 group 名称；删除前说明成员将被解组，Undo
  原子恢复 Group 和成员关系。
- 长层级信号名在固定列宽内使用中间省略，保留可辨识的首尾；悬浮信号标题显示完整名称。Signals 与波形之间的
  分隔线可直接拖动，在 140–480 px 内实时调整名称列；双击分隔线按当前可见名称自动适配，Esc 取消未提交拖动。
  确认后的列宽作为应用界面偏好跨窗口恢复，不进入工程 Undo、不会改变 Saved 状态。
- 长信号列表中，拖动标题进入画布上/下边缘会持续自动滚动，插入线随当前真实位置更新；释放后停留在目标位置并只产生一个可撤销的 `MoveLaneCommand`。按 Esc 会停止滚动、恢复拖动前视口与顺序，并在状态栏说明信号仍处于原位置。
- Bus/Enum 单击只选择当前一拍；按住 Segment 主体或左右边界并形成实际拖动后，分别移动整体或
  修改边界；双击或 `F6` 选择完整 Segment 并修改现有值。`Delete` / `Backspace` 严格清除当前
  可见 Beat 或 Segment 目标为该信号的隐式值。右键波形可设置常用值、插入一拍 Pulse、编辑
  完整 Segment 或清除 Segment。Bus/Enum 已有 Segment 还可右键选择 `Duplicate segment before`
  或 `Duplicate segment after`，将完整值和语义元数据一步复制到相邻等宽区间。Edit > Segment
  同时提供明确命名的前后复制、前后移动及四种边界扩展/收缩命令；目标越过时间轴首尾时只说明
  原因，不增加 Undo。仅选中 Segment 时，`Ctrl+D` 仍复制整条当前信号；建立显式时间范围后，
  `Ctrl+D` 优先重复该范围，快捷键含义始终跟随画布上可见的编辑目标。
  清除后状态栏显示信号、时间范围、隐式结果和 `Ctrl+Z` 提示；
  双击改值会显示信号、时间范围、规范化后的结果与撤销提示；输入未改变时明确说明未修改且不新增
  Undo。Pulse 与单击/悬浮共用同一拍范围，插入后保持该拍选中并显示实际结果。若任一单信号写值
  同步移除引用消失边沿的 Relation，会说明数量，撤销会原子恢复波形、Event 与关系。
- Segment 主体移动、Ctrl 复制和左右边界缩放期间，预览标签与状态栏持续显示目标起点、终点和
  `width`，无需依靠刻度估算；释放后仍按一次手势提交一个 Undo。同步模式下，右键命中按真实点击
  位置判断，不会因编辑光标量化到下一拍而丢失 Segment 菜单。
- 工具栏持续显示当前目标类型（Signal、Beat、Segment、Range 或 Measure）、信号、精确范围和值；
  Sync 同时显示关联时钟和实际每步周期，未关联 Clock 时明确显示 `Grid`及固定步长，Async 显示
  `1 tick/step`。普通状态消息只报告本次选择或操作结果；鼠标指针下的信号和采样值位于
  独立常驻区域，不会覆盖上一次操作结果。完整操作提示位于目标标签悬浮说明和 Edit > Segment 菜单。
  `F6` 选择编辑光标处的显式 Segment，`F7` / `F8` 在同一信号上选择上一/下一显式 Segment；
  这些命令也可从仅有信号与光标的状态直接进入相邻段。导航不修改模型。Wave Edit 指针状态使用实际指针位置显示
  `pointer <signal> · value <value>`，不把吸附到下一边界的值误报为指针下结果。
- 选中 Bus/Enum Segment 后，按 `Enter` 或直接输入字符会编辑该完整 Segment，不会退回当前拍。
  Segment 值栏内 `Tab` / `Shift+Tab` 提交并打开下一/上一显式 Segment；到达首尾时保留当前目标并
  明确提示，确认原值不会移除预设元数据或产生空历史。边界和位置优先直接拖动，也可使用
  Edit > Segment 的明确命令按当前 Sync 拍或 Async tick 精确调整；每次只产生一个 Undo，并沿用
  相邻内容和最小宽度约束。按 Esc 清除 Segment 编辑状态时保留当前信号与精确时间，可继续导航或
  用 `F6` 恢复选段。`Ctrl+Space`、`Ctrl+Tab`、`Alt+Left/Right` 和方括号不再劫持 Windows 输入法、
  常见窗口导航或键盘布局相关输入。
- Bus/Enum 的 Beat 或 Segment 编辑器显示在当前目标行附近，并在目标行上方或下方自动避让；
  滚动和连续录入时随目标重新定位。工具栏不再长期占用一整段空间，进制、直接值、最近值以及
  0/X/Z/Don’t care 仍保持就地可用。
- 放大长时间轴后，Wave Edit 的 Shift 范围选择、范围端点、Bit 多拍、Bit 边沿、Segment 移动与左右边界拖动在进入波形区左右 48 px 时持续自动滚动；每次滚动都会更新真实时间、吸附与预览，状态栏显示方向。释放后停留在目标视图，仍只提交原手势对应的一个命令；Esc 或异常失去左键会停止滚动、恢复起始视图，并保持模型、Undo 与 Saved 状态不变。
- 建立显式时间范围后，现有单个 `Fit scenario` 动作会就地改为 `Fit selection`；点击或按 `Ctrl+0`
  让范围左右端点准确占满波形视口，同时保留选区、固定范围栏和编辑能力。状态栏说明 Esc 可清除选区；
  清除后同一动作恢复为 `Fit scenario`，再次按 `Ctrl+0` 返回完整概览。工具栏 Zoom 在编辑光标可见时
  以该光标为锚并保持其屏幕位置，光标离屏时回退到视口中心。缩放过程不修改模型、Undo 或 Saved
  状态，也不增加第二个 Fit 按钮。
- 单击信号名或使用 `Up` / `Down` 选中信号后，画布获得焦点时，`Left` / `Right` 按工具栏当前
  `Timing` 移动编辑光标；`Shift+Left` / `Shift+Right` 使用同一步长建立或调整显式范围。具有
  有效关联 Clock 的信号显示 `Sync`，严格落到前一/后一有效拍边界，并保留 period、phase 和
  active edge；未关联 Clock 的信号显示 `Grid`，使用 10 ns 固定步长，不再伪装为 Sync。光标原先不在
  对应网格上时也按方向选择严格相邻边界。Async 每次移动 1 tick；Scenario 首尾始终作为可达边界。
  已有多 lane 范围继续保留完整 lane 集合，当前活动
  信号决定公开步长；折叠后保留信号目标但不武装整条信号 Delete。固定范围栏、Timing 按钮悬浮说明
  和状态栏均显示实际步长；文本框持有焦点时方向键仍只编辑文本。该交互不修改模型、Undo 或 Saved。
- 同一时间锚点上，`Shift+Home` / `Shift+End` 可将活动端一次移到 0 或 Scenario End，适合长时间轴的整段填值、复制或清除；跨过锚点时范围自然翻向另一侧，回到锚点时折叠但保留信号目标。已选多信号集合、固定范围栏、水平可见位置和边界无效果反馈同步更新。
  信号选中状态与固定范围栏均公开该入口；范围值、End、重命名等文本框持有焦点时，Shift+Home/End 仍只选择文本。该交互不修改 Scenario、Undo 或 Saved 状态。
- 需要整段操作时，Edit 菜单的 `Select full signal range` 或 `Ctrl+A` 会把当前信号、或已有多信号目标集合的时间范围直接设为 0–Scenario End；不会自动把其他不同类型信号加入目标，也不改变当前光标、缩放和视口。固定范围栏立即显示完整范围，可继续 Copy、Repeat、Paste、Clear 或批量赋值；Cut 仍可从 `Ctrl+X`、Edit 或右键执行。
  该选择只改变编辑上下文，不修改 Scenario、不进入 Undo、不改变 Saved；未提交的范围值草稿会阻止范围变化并保留焦点。范围值、End、重命名等文本框持有焦点时，`Ctrl+A` 仍只全选文本。
- 显式时间范围存在时，`Shift+Up` / `Shift+Down` 从当前活动信号向相邻可见信号扩展或收缩目标集合，并自动跳过 Group；缩回锚点后继续反向按键可向另一侧重新扩展。鼠标框选记录释放端作为活动信号，键盘可直接接续；长列表仅滚动到足以显示新活动行。
  固定范围栏即时更新信号数量、Bit/Bus/Enum 类型和 Enum 共有符号；跨类型时保留 Copy/Repeat/Paste/Clear。未提交的 Bus/Enum 范围值会原位阻止目标变化并恢复输入焦点。该交互不修改 Scenario、Undo 或 Saved 状态。
- Wave Edit 画布获得焦点且未在拖动时，`Home` / `End`（也接受 `Ctrl+Home` / `Ctrl+End`）直接把编辑光标和水平视图跳到 0 或场景 End；当前缩放、信号目标和显式范围均保留，状态栏显示到达的边界及反向快捷键。内联 End、重命名、Bus/范围值等文本框继续自行处理 Home/End，不会触发时间轴跳转；导航不修改模型、Undo 或 Saved 状态。
- Wave Edit 中存在已选信号时，画布上的 `Tab` / `Shift+Tab` 只把编辑目标移动到下一拍/上一拍，
  并以拍级高亮确认位置；它不写值、不创建 Undo。`PageDown` / `PageUp` 按当前可见时间跨度向后/
  向前翻一页，并只滚动到显示新目标所需的位置；到达首尾时原位说明，不改变模型或选择。没有可导航的
  波形目标时，Tab/Shift+Tab 交回给标准焦点链，不会把键盘焦点困在画布中。
- 已知目标时间时，按 `Ctrl+G` 打开临时 `Go to` 栏，可输入小数 `ps` / `ns` / `us` / `ms`、整数 `tick`，也可输入 `cycle N`；后者使用当前信号的时钟域，或项目中唯一可判定的时钟。Enter 或 `Go` 将编辑光标准确定位，并只滚动到足以看见目标的位置；当前信号、缩放、Scenario、Undo 与 Saved 状态保持不变。小数时间使用字符串有理换算，只接受能被项目 timebase 精确表示的值，不做浮点舍入；结果以最多三位小数的紧凑单位显示，例如 `2500 ps` 显示为 `2.5 ns`。
  非法或越界输入会在原位显示错误和允许范围，不弹模态窗口；Esc 或关闭按钮收起跳转栏并保留结果，再次按 `Ctrl+G` 会预填当前光标。显式时间范围存在时，同一入口改为 `Range edge ▾`：预填活动端点、显示固定锚点，并提供 `Other edge` 切换；也可直接点击固定范围栏的范围摘要进入。点击 `Range edge ▾` 原位切换为 `Range width ▾`，可从固定锚点输入正的小数物理时间、整数 tick 或 `cycle N`；`Other edge` 在宽度模式下反转延伸方向。Enter 或提交按钮精确调整选择端点或宽度，允许端点跨过锚点但拒绝空范围、零宽和越过时间轴的宽度，保留全部目标信号且不修改 Scenario、Undo 或 Saved。关闭后恢复原范围工具栏；若用户转到其他画布目标，过期的精确编辑器自动关闭。未提交的内联草稿继续优先阻止导航。
- 单击信号名选中当前信号后，状态栏直接提示边沿导航；Wave Edit 画布获得焦点时，`Ctrl+Left` / `Ctrl+Right` 严格跳到当前时间之前/之后的最近真实边沿，不会停在当前位置。Bit、Bus 与 Enum 使用 Segment 起止边界，Clock 同时使用周期上升/下降沿和覆盖区段边界；没有相邻边沿时光标保持原位，并提示使用 Home 或 End。该导航保留缩放和信号目标，不修改模型、Undo 或 Saved 状态。
- 需要从当前光标准确选到信号变化点时，`Ctrl+Shift+Left` / `Ctrl+Shift+Right` 将已有 `Ctrl` 边沿导航与 `Shift` 范围选择组合：按当前活动信号的前一/后一真实边沿创建或调整范围。Bus/Bit/Enum 使用 Segment 边界，Clock 使用周期上升/下降沿及覆盖边界；已有多信号目标保持不变。
  没有相邻边沿时范围与光标原位保持，并提示 `Shift+Home` / `Shift+End` 的时间轴边界路径。固定范围栏和信号选中反馈直接公开组合键；文本框持有焦点时仍执行文本按词选择。该交互不修改 Scenario、Undo 或 Saved 状态。
- Wave Edit 画布获得焦点时，`Up` / `Down` 选择当前行上方/下方的可见信号并跳过 Group；无选择时分别从末条/首条信号开始。时间光标、水平位置和模型保持不变，长列表只滚动到足以完整显示目标行，不额外滚到末尾添加区。到达首末信号时保持原目标并说明反向键；键盘选中不会武装整条信号 Delete。显式范围或拖动期间先提示完成/按 Esc，不隐式清除操作；End、重命名及其他画布内文本框获得焦点时 Up/Down 不会冒泡为信号导航。`Alt+Up` / `Alt+Down` 的可撤销重排语义不变。
- 长信号列表中按 `Ctrl+F` 可打开紧凑的临时查找栏；输入可见信号名称或稳定 ID 后立即选中并最小滚动到首个匹配项，名称与 ID 均不区分大小写，Group 和隐藏信号不作为结果。`Enter` / `Down` 前进，`Shift+Enter` / `Up` 后退，首尾循环会明确提示；无匹配时输入框原位标红，当前信号不被清除。
  按 Esc 或关闭按钮收起查找栏并保留找到的信号，再次按 `Ctrl+F` 会保留并全选上次查询。查找保持编辑光标、水平视图、Scenario、Undo 与 Saved 状态；显式时间范围存在时不会静默改变目标，而是提示先按 Esc 清除范围。
- Wave Edit 中，当前所选非 Group 信号的标题右端显示编辑光标处的采样值，使用与编辑光标一致的青色；Bit 的隐式值显示 `0`，Bus/Enum 的隐式值显示 `X`，Clock 按周期与覆盖状态采样。只为所选信号预留值宽度，名称继续中间省略，未选信号不增加杂讯；拖动预览期间暂时隐藏模型值，避免把未提交预览误认为结果。Up/Down 和 Ctrl+Left/Right 的状态反馈同步携带值；Measure 仍按活动光标显示全部信号值。
- Wave Edit 选中 Bus 或 Enum 后，状态栏在鼠标选中、Up/Down 切换及 Ctrl+Left/Right 到达边沿时直接提示
  `Enter edits value`。单击 Bus/Enum 只选中当前拍，不弹出编辑器；Enter 打开当前拍。双击隐式拍打开 Beat 编辑，
  双击显式 Segment 才进入完整 Segment 编辑。编辑器浮动显示在
  当前目标附近，以两行分别公开目标范围及操作控件，并提供 Beat/Segment 显式切换、前一项、后一项、
  Clear、Apply 与关闭按钮，以及 HEX/BIN/DEC/OCT 输入进制、直接值、该信号最近使用值和 `0`、
  `Reserved`、`X`、`Z`、`Don't care`。画布获得焦点后按 Enter 使用当前编辑 tick 打开同一编辑器并预填精确采样值；
  Segment 边沿直接使用整数 tick，不经过像素往返，因此不会误取边沿前的隐式 `X`。Enum 隐藏
  Bus 专用预设，输入框提供已声明
  符号的补全与完整提示；Enum 草稿可用 `Up` / `Down` 直接循环声明符号。非法 Enum 值保留草稿和
  焦点并列出可用符号。编辑条内按 Enter 提交一个
  可撤销的一拍写值并结束；按 `Tab` 提交后直接进入下一拍，按 `Shift+Tab` 提交后回到上一拍。
  未修改的隐式 `X` Beat 可直接用 Tab/Shift+Tab 跳过，不会把隐式值实体化或污染历史。Bus 数值草稿
  可用 `Up` / `Down` 按当前进制精确加减 1，范围边界会原位钳制；`Ctrl+Up` / `Ctrl+Down` 按当前
  lane 的最近自定义值循环。`Ctrl+Enter` 提交当前值后保持同一 Beat/Segment 打开，便于继续校正。
  当前 Beat 同时由编辑栏中的精确范围和画布上的透明琥珀框标明。Sync 下按关联时钟拍推进，Async
  下保持当前非对齐偏移；非法值保留原拍、草稿和焦点。到达时间轴首尾时写值仍会提交，但目标保持
  原位并提示可用的反向按键；重复确认相同值不产生空历史。切换范围或前后导航会先提交有效草稿，
  无效草稿则留在原目标并显示原位校验；Esc 或关闭按钮放弃草稿、保留波形目标，且不改变模型、Undo
  或 Saved 状态。已有 Bus/Enum Segment 的双击仍用于编辑完整 Segment，连续拍导航只作用于 Beat。
  单击显式 Bus/Enum 内容后内部目标也保持为 Beat，不会因命中所在 Segment 而使 Delete 或直接输入
  扩大到整段；拖动主体、拖动边界、双击或 F6 才进入 Segment。Beat 下 Delete/Backspace 与可见
  Clear 只清除当前拍至隐式 `X`；Segment 下 Clear 清除完整段，并自动回到该区间首拍，避免保留
  已不存在的 Segment。新建 Bus/Enum 的隐式 `X` 拍与显式值拍使用同一 Beat 选择；编辑合法草稿后
  单击另一拍、双击 Segment 或右键打开菜单均在提交旧值后直接进入新目标。以上清除均为一个 Undo，
  重复清除只反馈 `no values changed`。
- Clock 波形右键可对一拍执行 Gate、Drive X 或 Run。Run 清除该拍覆盖后保持拍级选择，并显示
  `restored normal clock waveform` 与撤销提示；已正常运行时显示 `no values changed`，不新增
  Undo 或清除 Redo。Wave Edit 选中 Clock 后也可直接按 `G`、`X`、`R` 对当前完整周期执行
  Gate、Drive X、Run，结果继续保持该周期选中并使用同一命令栈。
- 显式时间范围的 Clear（Clock 显示为 Run）在点击前投影真实清除结果：只有至少一条目标信号存在
  显式值或覆盖时才可用。安全操作直接说明受影响信号数、精确范围以及
  `No Relation will be removed`；若会清理依赖，按钮改为琥珀色 `Clear ⚠N`，悬浮说明列出准确
  Relation 描述和源/目标端点。固定范围栏与范围右键使用同一预检；Edit 和右键中的 Cut 也公开
  相同影响，但即使源范围已是隐式值仍可用于复制，并明确说明不会移除源值。Delete 或 `R` 的
  执行层兜底对无效果范围只反馈原因，不创建 Undo、不清除 Redo，也不改变选择。
- Relation 约束连线默认隐藏，避免持续遮挡波形；`Edit > Show relation constraints` 可随时显示/隐藏。
  无论常规连线是否显示，会被当前编辑删除的 Relation 仍以琥珀色连线和端点提醒，不隐藏风险。
- 同一提交前预检覆盖显式范围的 `0` / `1` / `Reserved` / `X` / `Z` / `Don't care`、Clock Gate/Disable 和
  Bus/Enum 文本写值。预设按钮显示实际会变化的信号数；等值按钮直接禁用，安全按钮明确不删除
  Relation，风险按钮显示琥珀色 `值 ⚠N` 并列出准确关系和端点。Bus/Enum 草稿在 Enter 前即时
  区分有效、越位宽/非法、等值和 Relation 风险；无效果草稿及键盘 `0/1/X/Z/G` 的执行层兜底
  保持选区、Undo/Redo、Saved 和 autosave。工具提示继续公开原快捷键，不增加确认框或编辑模式。
- Bit lane 悬浮时高亮当前拍并显示将变为 `0` 或 `1`；单击只翻转这一拍，横向拖动逐拍翻转
  覆盖范围。选中 Bit 后可直接按 `0`、`1`、`X`、`Z` 写入当前拍，也可从右键菜单选择四态值；
  写值后仍保持原拍级选择，不会扩大到相邻同电平 Segment。`Delete` / `Backspace` 或右键
  `Clear beat to implicit 0` 只清除所选一拍（拖动选择时为所选多拍），保持选择并提供
  `Ctrl+Z` 恢复提示。单击/拖动翻转或拍级 Clear/Delete 若删除了被引用的边沿，状态栏会说明
  Relation 数量以及撤销将同步恢复波形和关系。再次写入当前相同值只显示 `no values changed`，
  不替换 stable ID、不标记工程修改、不新增 Undo，也不丢弃已有 Redo。拍级选中框在鼠标离开
  画布后仍保留，悬浮高亮仅表示即将操作的拍；跨信号定位、右键信号标题或开始拖动 Bit 边沿时
  清除旧拍选择，避免陈旧目标残留。
- Wave Edit 的被动悬浮只提供操作预览，不再暗中移动编辑光标。Bit 提示精确拍范围、当前值和单击/
  拖动结果；Bus/Enum 的显式 Segment 与隐式 `X` 拍均显示值、起止和宽度，并以透明预选框标明目标；
  Clock 提示完整周期范围。由此，左侧采样值和工具栏 Target 始终对应已确认的编辑光标，而非鼠标经过点。
- `Shift` + 拖动在一个或多个 lane 上建立持久时间范围；顶部固定工具栏提供可直接点击的
  `Copy`（复制完整 lane 集合和整数时间宽度）、`Repeat`（不改剪贴板并紧邻重复完整范围）、
  `Paste` 与 `Clear`（仅清除所选区间）。低频且会清除源区间的 Cut 不再占据固定栏，但
  `Ctrl+X`、Edit 和选区右键入口保持。同类型 Bit/Bus/Enum 另外显示批量赋值控件，不覆盖或截获波形点击，也不造成画布
  上下跳动。范围左右边界显示可拖动手柄；修正端点时保留 lane 集合和工具栏，复用 7 像素轻吸附，
  按住 `Alt` 临时绕过。拖出范围及修改端点期间，画布标签和状态栏实时显示精确起点、终点与
  `width`；端点修正只改变选择，不修改模型或占用 Undo。Bit 可用按钮或 `0`、`1`、
  `X`、`Z` 键一次写入完整范围；Bus 可直接输入值或选择 `0`、`Reserved`、
  `X`、`Z`、`Don't care`，每条 lane 按自身位宽生成值。Enum 只显示符号输入框并提供声明符号
  补全；多条 Enum 同选时只列出全部目标共有的符号，非法输入保留选区、草稿和焦点。提交整个
  Enum 范围只形成一个 Undo。混合类型隐藏所有无效赋值控件，但保留 `Copy`、`Repeat`、`Paste` 与 `Clear`，
  不允许产生部分写入。一次批量赋值或范围清除对应一个 Undo/Redo；
  对完整范围重复赋相同值只确认结果，不生成空历史或清除 Redo；
  `Delete` / `Backspace` 与 `Clear` 均清除所选信号区间并保持范围选中。Edit、右键或 `Ctrl+X` 的 `Cut` 同样
  保持选择，源清除为一个 Undo，随后 Paste 为另一个 Undo。同一时间边沿仍存在时复用稳定
  Event ID，Relation 保持不变；边沿确实消失时仅删除引用它的 Relation，状态栏说明删除数量、
  原因以及 `Ctrl+Z` 会同时恢复波形和关系。范围栏提供显式关闭按钮；关闭后保留当前信号和编辑光标。
  在选区内右键不会丢失目标，而是直接提供 Copy、Cut、Paste、Repeat after、Clear、Fit、可用时的 Edit value
  以及 Close selection。单击选区正文会保持范围并提示直接拖动；拖动正文可将全部目标信号的
  所选时间段整体移动，`Ctrl` + 拖动则复制且保留源区间。源选区使用实线，未提交目标使用虚线，
  标签与状态栏实时显示 Move/Copy、精确起止和宽度。两种拖动均沿用 Sync/Async、7 像素轻吸附、
  `Alt` 临时绕过及水平边缘自动滚动；复制目标与源区间重叠时显示禁止状态并拒绝提交。释放后结果
  范围继续选中，系统剪贴板不变；移动只清除源区间内的显式内容，完整选中的 Segment 尽量携带
  stable ID/Event，部分边界和被覆盖目标按既有 Relation 清理规则处理。操作越过 End 时原子延长
  时间轴，单击、原位拖动、无效果目标、Esc 或异常失去左键均不制造空历史，其中取消还会恢复
  起始选区与自动滚动前视图。点击信号名、时间标尺或选区外的非 Bit 波形可在同一次安全点击中
  关闭旧范围并改选；选区外的 Bit 正文仍要求第一次点击只关闭范围，避免无意翻转。状态栏明确
  反馈每种结果。
  拖动过程中可随时按下或释放 `Ctrl`，预览、鼠标指针和最终提交会立即在 Move/Copy 间切换；
  不必在按下鼠标前决定。范围 Move、Copy、紧邻重复及显式目标 Paste 的 Undo 会恢复原源选区，
  Redo 会恢复结果选区，并在目标离开视口时做最小滚动揭示；Segment 的移动、复制、边界调整和
  键盘微移同样恢复对应 Segment。恢复只绑定产生该操作的历史状态，不会让其他 Undo 跳回旧目标。
  范围正文还可直接纵向拖到连续的兼容信号块，不再需要 Copy、重新选择目标再 Paste。拖动保持
  按下行在多信号块中的相对位置，源信号使用实线、目标信号使用虚线，并显示
  `req → ack` 一类映射；靠近画布上下边缘时持续纵向滚动。Bit 只接收 Bit，Bus/Enum 还要求
  位宽和值均兼容；不兼容目标在释放前以红色虚线、禁止指针和具体原因拒绝。`Ctrl` 纵向拖动可在
  相同时间把范围复制到不相交的目标信号，普通拖动则原子移动；重叠的多信号映射先读取全部源值，
  再清理源/目标并写入结果，避免 `A → B、B → C` 的链式污染。Esc、失去左键或无效果释放会恢复
  原选择及滚动位置；成功提交后保留目标选区，Undo/Redo 在源块和目标块之间准确往返。
  Bus/Enum 范围的当前值载入使用虚拟基线：Bus 隐式 `X`、带预设扩展元数据或拍内已有变化的内容
  可显示为可编辑 token；未修改 token 不实体化隐式值，也不重写原 Segment 或扩展元数据。只有被用户
  修改的 token 才以当前编辑拍为单位替换完整拍；单信号和批量命令均不会因纯保留步骤扩 End 或同步
  Event。超宽 Bus 在生成值文本前执行 8192 字符预算，超限时禁用载入并说明原因，避免先分配大量
  中间字符串。最终虚线预演会在提交前显示实际覆盖结果。
  拖动目标不再只是抽象矩形：Bit 会按实际 0/1/X/Z 形状显示虚线幽灵波形，Bus 会显示数值、
  `X`/`Z` 与 Don’t care 语义外观，Enum 显示将写入的符号轮廓，Clock 只显示真正会被转移的
  gated/disabled 覆盖，不重复伪造基础时钟。预览使用目标信号颜色并保留外围虚线框；精确标签优先
  放在范围外，避免遮住待写入内容。目标无效或 Copy 与源重叠时改为红色 `Cannot drop`，不绘制
  可能被误认为可提交的内容。绘制只提取当前可见目标行的源区间，不复制整条 Lane。
  显式范围只包含 Clock 时，固定栏直接显示 `N Clock`、精确时间以及 `Run`、`Gate`、`Disable`；
  `R` / `G` / `X` 提供相同的键盘入口。Run 清除范围内显式覆盖，Gate 写入 gated，Disable 写入
  disabled/X；使用 `Shift+Up` / `Shift+Down` 选中多条 Clock 后仍一次作用于完整时间范围，并只产生
  一个 Undo。结果已经一致时显示 `no values changed`，不污染历史；Clock 与其他类型混选时保持
  原范围并拒绝 G/X/R，避免部分修改。
  范围值草稿合法时，点击下一信号或时间位置会先原子提交整段赋值并在同一次点击完成改选；任一目标
  位宽或值校验失败时，范围、目标集合、草稿和输入焦点均保留，不产生部分写入。输入框内第一次
  Esc 只丢弃未提交草稿并保留范围，状态栏明确提示；未修改状态下再次 Esc 才关闭范围。固定栏的
  关闭按钮仍可一步关闭范围。
  需要重复协议片段时，点击固定栏的 `Repeat`、直接按 `Ctrl+D`、在选区内右键或使用 Edit 菜单动态显示的
  `Duplicate selected range after`，会把
  所有已选信号的完整范围直接写到紧邻后方；不需要先 Copy、移动光标再 Paste，也不会覆盖系统
  剪贴板。隐式空白、Segment 扩展语义和多信号顺序随范围一起保留，结果范围自动保持选中；若
  覆盖导致被引用边沿消失，状态栏说明 Relation 清理及恢复方式。一次操作对应一个
  `Duplicate range` Undo/Redo，越过 End 时同时延长时间轴；后一范围已经一致时只移动选择并显示
  `no values changed`，不增加历史或清除已有 Redo。
  复制或剪切后可在目标波形时间右键选择 `Paste copied range here`；同一次右键会清除旧范围并直接
  打开菜单，无需先点空白处。单 lane 内容以当前点击或选中的兼容信号为实际目标，结果显示
  `source → target`、目标时间、宽度和 `Ctrl+Z`；Bit→Bus 等类型不匹配及 Bus/Enum 位宽不匹配
  会原子拒绝并说明原因。复制内容是携带源名称、类型和位宽的自描述快照；删除源信号后仍可右键
  兼容目标完成 Paste，无目标直接粘贴会说明需要选择几个目标。旧 schema 1 剪贴板在源信号仍存在时
  继续兼容。固定范围栏显示可见 `Paste`，并在点击前预检剪贴板 schema、复制宽度、目标数量、
  信号类型、Bus/Enum 位宽和每段值；不兼容时按钮保持可见但禁用，提示直接说明具体配对和原因，
  不再要求点击后才发现失败。可用时提示会预告复制信号数、完整宽度、固定的选区左端点、逐信号
  映射、当前选区宽度差异及必要的 End 延长。多 lane clipboard 与显式目标选择数量相等时，
  按双方可见顺序一一映射，数量或任一配对不兼容时零写入。未建立显式多 lane 目标而使用右键 Paste
  时仍保持复制时的完整源 lane 集合，避免猜测配对。粘贴完整宽度超过当前 End 时会原子延长时间轴，
  保持当前缩放并把新尾部滚入视野；状态显示新的 End 和 `Ctrl+Z`，一次 Undo 同时恢复 End 与波形，
  也可直接在 End 处开始粘贴。目标已与复制范围一致时显示 `no values changed`，
  不新增 Undo 或清除 Redo。粘贴后的目标范围保持显式选中，固定范围栏立即可用于再次
  Copy、Repeat、Paste、Clear 或批量编辑；需要在非相邻时间再次粘贴时，直接单击或拖动标尺，
  结果范围会关闭并转换为相同顺序的标题目标，新时间立即用于下一次 Paste，无需重新选择信号。
  Undo 后选择仍保留，Esc 清除。Edit 菜单及
  `Ctrl+C` / `Ctrl+X` / `Ctrl+V` 入口继续保留；当内联文本框获得焦点时，这三个快捷键只操作文本，
  不会修改波形。
  显式范围存在时，固定栏 `Paste` 与 `Ctrl+V` 始终使用固定栏显示的范围左端点；即使键盘范围的活动端点位于右侧，也不会改用活动光标。无显式范围时仍按编辑光标或右键点击时间粘贴。
  该规则与按钮提示、粘贴后的持久选区、状态栏目标时间和单步 Undo 保持一致。
  不需要用目标宽度框出时间矩形时，可在左侧信号名区域用 `Ctrl` / `Shift` 选择与复制内容数量相同的
  目标信号，再用 `Ctrl+G` 或直接单击/拖动时间标尺定位，并按 `Ctrl+V`。标题选择按画布可见顺序映射复制信号，
  `Target` 同时显示活动信号和落点；状态栏在选择时预检数量、类型、Bus/Enum 位宽和值，并在可用时
  明确提示 Ctrl+V。预检通过后，目标区间会在提交前直接显示半透明虚线 Paste 波形、范围边界和
  精确时间标签；左侧标题同时显示 `当前值→预览值`，相同值显示 `=值`。单击或拖动标尺时预览实时
  跟随，Scenario、Saved 和 Undo 保持不变；标题数量、类型、位宽或值不兼容时不绘制预览，也不会
  回写原源信号。成功后结果时间范围继续选中；
  从该结果范围单击或拖动标尺会保留相同有序信号集合并恢复标题目标，可在新时间连续 Paste。
  Undo 恢复每次命令前的标题集合与光标，Redo 恢复对应结果范围。Paste 目标优先级固定为显式时间范围、标题选择、
  单信号目标、最后才是无显式目标时的原复制信号集合。
  当剪贴板包含 Wave 范围时，信号标题右键菜单顶部会直接显示
  `Paste copied range into N selected signals at T`。动作提示在执行前给出复制宽度及每一项
  `source → target` 映射；数量、类型、Bus/Enum 位宽或值不兼容时，动作仍可见但禁用，并显示
  对应配对的具体原因。选择该动作与 `Ctrl+V` 复用同一预检和 Paste 命令。
- Bit Event 菱形无需切换模式即可直接拖动。预览期间模型不变，虚线只覆盖原/新边沿中较早位置
  到后继 Segment 结束的局部范围；释放后以一个命令同步更新 Segment 与 Event。
- Bus 快捷预设直接作用于当前目标，不再要求从浮层拖放。`Reserved` 写入显式全零波形并保留独立
  预设语义，画布和操作结果显示 `RESERVED`，不会与普通 `0` 混淆；`Don't care` 使用灰色纹理显示，并在
  Expected/Actual 比较中忽略对应目标区间；普通 `X` 仍是有意义的未知值，不会被自动当作通配。
  自定义值按 Enter 提交并进入每条信号独立的最近值列表；预设值不挤占最近自定义值。整段改值或
  应用预设只替换 Bus 预设语义，保留 Segment 的其他扩展元数据。
- 波形修改默认使用最小拍单位：具有有效关联时钟的 Bit 拍、Segment 移动/缩放、范围边界和
  Bus/Enum 写值显示 `Sync`，以关联时钟的有效边沿为一拍；未关联时钟时显示 `Grid`，使用 10 ns 固定拍。
  点击工具栏 Timing 动作切换为 `Async` 后，可把信号边沿放在任意整数 tick，仍保留 7 像素轻吸附；
  按住 `Alt` 临时绕过吸附。再次点击恢复当前信号的 Sync 或 Grid。
  切换模式会取消尚未释放的波形拖动，不提交半成品。
- 在非 Bit、非 Clock Segment 主体上普通拖动为移动；按住 `Ctrl` 拖动为复制。复制预览使用虚线并
  标注 `Copy`，原 Segment 保持可见；释放后作为一个可撤销命令提交。复制到完全相同范围不产生
  空历史，也不会清除已有 Redo。
- 时间交互采用无设置项的轻吸附：距离可见刻度、Clock 边沿或信号边沿 7 像素以内自动对齐，
  并显示吸附时间提示；超出范围使用原始整数 tick，按住 `Alt` 可临时绕过吸附。
- Wave Edit 可直接在时间标尺上单击定位编辑光标，按住左键横向拖动可连续查样；拖动只改变光标、
  采样反馈和必要的水平视图，不修改波形或 Undo。已在左侧选中的单个或多个信号继续作为编辑目标，
  `Target`、状态栏和 Paste 预检随标尺位置实时更新，可直接右键所选名称或按 `Ctrl+V`。拖动中按
  Esc、或在失去左键状态时，光标、起始水平位置及标题目标会完整恢复；若按下前存在显式范围，
  取消还会恢复原范围、信号集合和固定范围栏。若正常释放，旧范围关闭并把原信号集合转换为同顺序
  标题目标，同时确认新时间；后续 Paste 不会退回源信号或要求重新选目标。
  标尺提示同时公开目标保持、Paste、空格/中键平移、Ctrl+滚轮缩放及 Shift+滚轮横向滚动。
- `Ctrl` + 鼠标滚轮以指针所在时间为锚缩放，`Shift` + 滚轮水平滚动；中键拖动或按住空格再左键
  拖动可平移时间轴。工具栏 Zoom 以可见编辑光标为锚，`Ctrl+0` 按当前上下文执行
  `Fit selection` 或 `Fit scenario`。
- `Measure / Markers` 是临时状态：左键创建或移动唯一活动光标；直接拖动以起点作为临时参考、终点作为
  活动光标，`Shift` + 左键创建临时参考。单击与方向键显示最终 `Cursor`，Shift 与拖动统一显示
  `Reference`、`Cursor` 和带符号的 `Δ`；释放后保留最终测量结果。
- `Ctrl` + 左键创建持久锁定光标，`Ctrl` + 拖动创建持久锁定区间。锁定对象使用独立颜色并写入
  工程 Marker；单击后可拖动或用方向键移动，`Delete` / `Backspace` 删除，均支持 Undo/Redo。
  创建、选择、移动和删除后状态栏显示名称与精确时间/区间；真实修改显示 `Ctrl+Z`，时间轴边界
  明确显示未改变且不新增 Undo。删除后再次创建时会跳过仍在使用的同类编号，持久 Marker 名称
  在当前场景内保持唯一。
- 活动光标存在时，每个可见信号在左侧名称区域显示该时刻的采样值。再次点击 `Measure / Markers` 或按
  Esc 返回直接波形编辑，活动、临时和选中状态清除，持久锁定光标保留。进入 Measure 会关闭
  Bus 直接编辑面板，测量 Bus 时也不会重新弹出；波形右键只提示返回直接编辑，不提供写值命令。
  退出时取消尚未释放的平移、空格手势与吸附提示；左键或右键选择信号标题会取消锁定 Marker 选中，
  并把状态栏切换为当前信号及 Delete/F2 的真实目标；取消右键参数菜单后该反馈仍保留。
- Undo/Redo 位于 Edit 菜单，并保留 `Ctrl+Z` / `Ctrl+Y`；工具栏不重复显示按钮。执行后状态栏
  显示 `Undid` / `Redid`、具体命令及相反快捷键，明确本次恢复结果和下一步。可见、启用且可编辑的
  文本框持有焦点时，Ctrl+Z/Ctrl+Y 只查询该文本会话；即使字段已无本地历史，也不会泄漏到波形命令栈。
- 状态栏常驻显示 `Not saved`、`Unsaved changes`、`Saved` 或 `Recovery loaded · Save required`；
  自动恢复快照不会被误报为正式保存。File 菜单使用原子替换方式保存 `project.wave.json`，首次
  保存 Untitled 工程时从文件名推断项目名。Open/Save As 默认显示 `*.wave.json`；Save As 只输入工程名时
  自动补全 `.wave.json`，显式扩展名保持不变。首次 Save As 从 Documents（不可用时 Home）开始；成功保存或打开后记住目录。
  `File > Open Recent` 保留最近 5 个有效工程，以文件名和父目录区分、完整路径置于提示；点击当前脏工程只显示 Already open，不重载或触发丢弃确认。
  单个本地 `.wave.json`、`.json` 或 `.autosave` 文件可直接从资源管理器拖到波形画布打开；它复用同一草稿门禁、未保存确认、恢复选择和 Recent 更新，
  与固定 Bus 编辑栏互不干扰。普通工程成功打开后状态栏显示 `Opened <path>`；恢复快照
  和迁移警告仍显示各自说明。Open 先选择目标文件；取消选择不会触发未保存确认或改变当前工程，只有选定
  目标后才询问 Save/Discard/Cancel。修改后 1.5 秒启动 Qt Concurrent 后台恢复快照；首次 Save As 前的 Untitled 波形同样写入应用恢复目录，
  无参数启动时自动恢复有效快照，并明确显示 `Recovery loaded · Save required`；此时 Save 仍打开 Save As，要求选择正式工程文件。
  启动或 Open 正式工程时会自动采用有效且更新的快照，较旧或损坏快照不替代正式文件。正式保存成功后清除已完成的快照，
  保存期间仍在途的过期写入完成后也会再次清除；在未保存提示中选择 Discard 同样等待并清除在途快照，下一次打开不会恢复已明确放弃的修改。
  打开或正式保存后会记录工程文件 SHA-256。再次覆盖同一路径前若磁盘文件已被其他应用修改，就地提供
  `Reload`、`Save As…`、`Overwrite` 和 `Cancel`；只有用户明确选择 Overwrite 才覆盖外部变更。
- File > Export 导出 SystemVerilog/SVA/cocotb 和文档图。VCD/FST/CSV 导入、Expected/Actual 对比、报告和
  跨应用桥接由 `wave-compare`、`wave-bridge` 等 CLI 提供，不占用桌面工作区。
- 特殊 lane 或 group 仍可从 Edit 菜单创建并编辑完整结构属性；空名称、与现有信号或 group 重复的名称、无效位宽、Enum 映射、时钟/分组引用或颜色，
  以及会使现有 Segment/Event 失效的类型或位宽变更，均会在窗口内原位提示并保留全部草稿与焦点；
  原值确认不会产生脏状态或撤销项。属性变更、依赖感知删除和显示顺序调整均保留 stable ID，并以单个命令撤销/重做。
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

## 测试状态

当前自动化测试覆盖：

- 整数与小数 ps/ns/us/ms 精确换算、紧凑小数显示、不可整除检测、溢出检测和大时间值往返。
- 多 clock domain、cycle 边沿和各类 snapping 基础算法。
- 参数化 clock、局部 gated/disabled 区间、清除覆盖、批量 cycle 重定时及完整撤销/重做。
- Lane/Group 创建、稳定 ID 成员引用、属性修改、依赖感知删除、非法结构转换拒绝及
  完整撤销/重做。
- Lane/Group 显示重排、边界拒绝、序列化顺序保持以及生成代码顺序无关性。
- 二进制、八进制、十进制和十六进制 bus 值；非整字宽的最高有效数字溢出检测。
- bit 的 0/1/X/Z、bus width/signedness、enum。
- segment 覆盖、合并、拆分和清除。
- 单命令撤销/重做，以及快速创建命令的原子替换、取消和场景时长 Undo/Redo。
- 多 lane 范围复制/粘贴、同类型 Bit/Bus/Enum 原子批量赋值、不同 Bus 位宽值生成、Enum 共有符号
  补全、失败零部分写入及完整撤销/重做。
- Event 时间和值与关联 Segment 的双向同步；重分段时按 Segment ID/tick 复用稳定 Event ID，
  source/target 边沿消失时精确清理依赖 Relation，无关关系保持原样。linked 与普通 Event 删除均
  覆盖依赖清理及完整 Undo/Redo。
- 画布式区间编辑到步骤表 Event 的同步。
- Marker command、Relation min/max delay、满足、违反、缺失目标和未定义区间。
- Relation condition 的运算符优先级、括号、稳定 ID/唯一名称、引号、四态值归一化、
  false 守卫、错误定位及 Expected/Actual 采样。
- JSON 往返、未知字段保留、schema 0 到 1 迁移。
- 整个工程目录移动后的相对路径恢复。
- Qt 桌面应用及磁盘示例工程的离屏启动；Lane 属性对话框以真实 OK 点击连续验证空名称、与已有 `req` 大小写冲突的 `REQ`、
  无效位宽、Enum 映射与颜色，以及 `data[7:0]` 8→4 位导致现有 `0x35` Event 失效时的原位纠错、草稿/焦点保留；
  恢复原值后保持 Saved、无 Undo 项并给出明确无变化反馈。确认删除/Undo 交互也通过离屏回归，`req` 删除前显示 3 个 Event、
  1 条 Relation 和恢复方式，完成及 Undo 状态均有专项断言。
- 画布快速添加 CLK/Bit/Bus、唯一命名、随机颜色、默认时钟域、信号名拖动重排、右键轻量参数、
  标题选中与右键取消后的信号名/Delete/F2 目标反馈、Delete 删除以及菜单快捷键均通过离屏回归；
  双击/F2 无模态重命名、入口信号名、Esc 取消目标恢复、原处校验、真实失焦提交、
  无效新增阻止删除/Bus 控件/Segment 双击并可 Esc 恢复、有效新增先提交再删除、两步 Undo 顺序、
  多个交错命令逐次恢复、右键参数真实结果/原值确认/Undo/Redo、无效时间/颜色原位纠错与草稿/焦点保留、Marker 模式标题拖动 Esc 取消、
  跨位置/原位拖放结果以及 Undo/Redo 恢复提示均具有专项 offscreen 覆盖。
  长列表专项另覆盖上下边缘持续滚动、真实插入位置、Esc 复原视口与顺序、跨视口移动的一步 Undo，以及 Up/Down 从无选择进入首条、跳过 Group、首末边界、最小垂直滚动、End 文本框焦点隔离、键盘选择 Delete 安全和隐式 Bit `0` 值反馈。
  Wave Edit 长时间轴专项覆盖右向 Segment 模型外预览、Esc 视图复原、释放单命令提交、Undo 回到 Saved、释放后计时器停止、左向 Shift 范围持续扩展、同一 Fit 动作的 selection/scenario 切换与全局恢复、放大状态下 End→末端、End 文本框 Home 焦点隔离、Ctrl+Home→起点和缩放保持，以及 Bus Segment/Clock 周期边沿的 Ctrl+Left/Right 相邻跳转、`0x35`/`X`/`0`/`1` 采样反馈、左侧所选值标签、边界反馈、信号选中提示和显式范围期间 Up/Down 的无损阻断，并验证两条 Enum 的 50–100 ns 范围、共有符号、非法草稿门禁、原子提交、Undo 与 Esc 清理。
  同一专项继续覆盖 `Shift+Left/Right` 对现有双 Enum 范围的活动端点扩缩、从所选 Bus 在 0 ns 创建 10 ns 范围、固定栏即时切换、折叠后保留信号目标，以及全程 Scenario/Undo/Saved 零变化；离屏截图验证范围栏、首拍选区、端点手柄和网格同时可见。
  第 78 轮继续覆盖从单条 Enum 键盘时间范围以 `Shift+Down` 扩展到相邻 Enum、`Shift+Up` 收回锚点、共有符号与固定栏实时刷新、未提交范围值草稿门禁，以及 Scenario/Undo/Saved 零变化；离屏截图验证两行选区、网格和固定范围栏同时可见。
  第 79 轮继续覆盖 Bus 0–10 ns 范围以 `Shift+End` 一次扩至 0–1 us、重复按键的边界无效果反馈、`Shift+Home` 回到锚点并折叠、折叠后重新建立范围，以及范围值输入框内 Shift+Home 只选择文本；离屏截图验证完整范围、末端手柄、波形和网格同时可见。
  第 80 轮继续覆盖 `Ctrl+A` 从当前 Bus 直接选择 0–1 us、固定栏与视口保持、多 Enum 既有目标集合保留、范围值输入框只全选文本，以及 Scenario/Undo/Saved 零变化；离屏截图验证 `2 Enum · 0 ps–1 us`、两行透明选区、波形和网格同时可见。
  第 81 轮继续覆盖 Bus 以 `Ctrl+Shift+Right` 从 0 精确选至 50 ns/100 ns 边沿、反向收回与无更早边沿反馈，Clock 从 0 精确选至 5 ns 半周期边沿，以及范围值输入框的组合键文本选择隔离；离屏截图验证 `1 Bus · 0 ps–50 ns`、边沿手柄、波形和网格同时可见。
  第 82 轮继续覆盖从 50–100 ns 复制 Bus 值、以键盘建立 0–10 ns 目标且活动光标停在 10 ns，再由 `Ctrl+V` 按固定栏公开的 0 ps 起点写入；断言 5 ns 已为 `0x35`、结果范围为 0–50 ns、状态显示 0 ps、单步 Undo 回到 Saved。离屏截图同时显示 `1 Bus · 0 ps–50 ns` 与 10 ns 活动光标。
  第 83 轮继续覆盖 `Ctrl+F` 按可见信号名称/ID 即时定位、Group 排除、大小写不敏感、首项/前后项/循环、无匹配、长列表最小滚动、水平视图保持、显式范围门禁、关闭/重开与 Scenario/Undo/Saved 零变化；离屏截图验证紧凑查找栏、结果计数、所选信号、波形和时间轴同时可见。
  第 84 轮继续覆盖 `Ctrl+G` 精确时间跳转、整数单位与 `cycle N`、非法/越界输入原位反馈、当前信号与缩放保持、最小水平滚动、显式范围门禁、关闭/重开、Bus/Enum 内联编辑隔离及 Scenario/Undo/Saved 零变化；离屏截图验证紧凑跳转栏、375 ns 光标、所选 Bus 信号、波形、时间轴和网格同时可见。
  第 85 轮继续覆盖 Edit/右键/`Ctrl+D` 复制、相邻插入、唯一名称、不同随机颜色、属性与波形一致、lane/segment ID 独立、ClockDomain 独立、Event/Relation 不复制、显式范围门禁、单步 Undo/Redo 和状态反馈；离屏截图验证 `bus`/`bus_copy` 相邻、波形一致且副本已选中。
  第 86 轮继续覆盖 Edit/右键信号与 Group 一步隐藏、动态动作名称、显式范围无损门禁、选择清理、实时隐藏计数、无模态窗口、单步 Undo/Redo、恢复全部隐藏项及 Saved 基线回归；离屏截图验证 `req` 已隐藏、其余波形与视图保持且 `Show 2 hidden items` 和恢复说明同时可见。
  第 87 轮继续覆盖多个隐藏项按名称单独恢复、主体按钮恢复全部、单隐藏项直达模式、原顺序和水平视图保持、恢复后选中、显式范围无损门禁、单步 Undo/Redo 及 Saved 基线回归；离屏截图验证恢复菜单同时列出隐藏 Group、`req` 与全部恢复，单独恢复 `req` 后 Group 仍隐藏且 `Show 1 hidden item`、选中态和结果反馈同时可见。
  第 110–119 轮继续覆盖已选 Bus/Enum Segment 的 Enter/直接输入、Tab/Shift+Tab 连续显式段录入、
  首尾目标保留、原值与预设元数据零变化、光标选段、双向导航、按 Sync 拍调整左右边界、元数据
  保持、每次单步 Undo，以及 Esc 退出后保留信号和时间并可立即恢复选段。随后交互收敛回归覆盖
  F6/F7/F8 菜单命令、Ctrl+D 始终复制整条信号、旧冲突快捷键零模型变化、持续目标/步长标签、
  短状态结果及 Bus 编辑器在画布目标附近随滚动定位；全程比较 Scenario、选择范围、状态反馈和历史。
- Measure 离屏回归覆盖活动/临时光标、持久锁定点/区间、精确 `Δ`、创建/选择/拖动/方向键/删除
  结果、活动光标单击与方向键最终值、Shift 参考点、正负拖动的 Reference/Cursor/Δ、边界无效果
  反馈、一次 Undo 直达前一真实移动、Delete 的 Undo/Redo 恢复提示、删除较早点后的名称冲突规避、
  进入模式时关闭 Bus 直接编辑面板、Measure 内 Bus 点击/波形右键隔离、标题/Marker 选择互斥、
  右键标题菜单取消与 Delete/F2 目标反馈，以及按钮/Esc 中断平移后的状态隔离。
- 无模式直接编辑的 Segment 选择、整体移动、边界缩放、双击改值、Delete 清除、Relation 清理提示
  与 Undo/Redo 确认，Bit
  单拍/多拍翻转、0/1/X/Z 键盘与右键写值后的拍级选择、拍级 Clear/Delete、Undo/Redo 和中键
  平移均通过离屏回归；鼠标离开后的持久拍级选中框、跨信号/边沿状态清理也有专项覆盖；
  Bit 右键一拍 Pulse 的精确 60–70 ns 范围、结果反馈、关系安全及完整 Undo/Redo 具有专项覆盖；
  单击翻转与右键/键盘拍级 Clear 删除依赖边沿时的 Relation 提示及精确 Undo/Redo 具有专项覆盖；
  单拍与多 lane 范围重复写同值后的 stable ID、空历史抑制、一次 Undo 直达前一真实编辑及 Redo
  保留具有专项覆盖；Shift 跨 lane 持久框选覆盖
  Bit/Bus 整段赋值、边界与相邻值保持、可见 Copy 的实际点击与 clipboard lane/宽度校验、
  左右范围手柄拖动、lane 集合与模型保持、同次右键清除旧范围并 Paste here、单 lane `req → ack`
  目标映射、Bit→Bus 原子拒绝、正常 Clock 空 Paste 的无效果反馈/Redo 保留、可见 Paste 按钮、
  2→1 数量拒绝、两 lane 顺序目标映射及 960 像素无裁切命中、粘贴结果持久选中、标题/标尺同次
  清除并重定向、schema 2 元数据/旧 schema 1 兼容、复制后确认删除源信号、无目标恢复提示、
  指定目标成功 Paste 及删除/Paste 两步 Undo、End 前 5 ns 完整粘贴 10 ns、End 自动延长/尾部滚入
  视野/单步 Undo、波形正文防误触、单步 Undo 后选择保留、混合类型无效控件
  隐藏和零修改、缩放/刷新定位以及 Esc/外部点击清除。
- Event 菱形局部虚线预览期间模型不变，释放、Undo 与 Redo 后 Event/Segment 保持同步。
- Clock 真实右键 Gate→Run→重复 Run、拍级选择、无效果反馈、一次 Undo/Redo 及基线恢复具有专项
  offscreen 覆盖。
- Bus 就近浮动编辑面板覆盖 Beat/Segment 目标、HEX/BIN/DEC/OCT、直接值、每信号最近值、
  0/X/Z/Don't care 单击以及波形右键；未提交值的
  点击别处提交、隐藏草稿恢复、Save/New/Open/Close/Export 门禁、Bus/End 冲突顺序、鼠标失焦
  坐标保护及 Undo 后越界草稿的 End 延长恢复均有专项离屏回归。长时间轴专项另覆盖选中 Bus 后
  Enter 在 50 ns Segment 精确预填 `0x35`、焦点与上下文提示、Esc 零修改取消、再次 Enter 提交
  `0x5a`、单步 Undo 精确恢复 Scenario 与 Saved，以及编辑面板离屏截图。第 88 轮继续覆盖裸 HEX
  输入规范化、最近自定义值、Don’t-care 比较语义、Sync/Async 边沿约束、Alt 绕过轻吸附、
  Ctrl+拖动复制预览/提交/Undo/Redo、相同范围空历史抑制和 Segment 扩展元数据保留。第 89 轮
  继续覆盖 Beat 当前目标高亮、Tab/Shift+Tab 前后连续录入、Enter 完成、Sync 一拍推进、Async
  偏移保持、非法值原位纠错、时间轴首尾反馈、重复确认空历史抑制及逐拍独立 Undo。第 90–93 轮
  继续覆盖隐式 X 跳拍、数值草稿加减、每 lane 最近值循环及 Ctrl+Enter 留在当前目标；第 94–96
  轮覆盖画布拍导航、可见时间页导航及 Clock G/X/R 周期编辑；第 97–99 轮覆盖 Segment/范围精确
  时序预览、右键相邻复制、单步 Undo/Redo、End 边界无历史反馈及同步模式真实右键命中。
- 第 241–250 轮继续覆盖显式范围中的 `Ctrl+G`/范围摘要精确端点入口、固定锚点和活动端点预填、
  `Other edge` 切换、跨锚点、`cycle N`、非法时间与空范围原位拒绝、单/多信号目标保持、未提交
  Enum 范围值草稿门禁、转到其他画布目标时自动关闭，以及 Scenario/Undo/Saved 零变化；离屏截图
  验证紧凑 `Range edge` 栏、50–70 ns 选择、Bus 波形、刻度和网格同时可见。
- 第 251–260 轮继续覆盖 `2.5 ns` Go to、`37.5 ns` 范围端点、Timeline End 的 `.5 ns`
  增量、`0.012 us` Clock 周期、`120.5 ns` 导出范围与 `25.5 ns` PDF 跨度；核心用例覆盖
  正负小数、`.125 us`、尾随零、最小有符号值、溢出、timebase 不可整除和亚 ps 拒绝。`1.5 tick`
  继续明确拒绝，所有输入均无浮点舍入；离屏截图验证 `2.5 ns` 输入、光标标签、Bus 波形和网格。
- 第 261–270 轮继续覆盖精确范围的端点/宽度双模式：点击模式标签切换，宽度以固定锚点和当前活动
  方向计算，接受 `25.5 ns`、`12500 ticks` 与 `cycle 3`；`Other edge` 原位反转方向。零宽及越过
  0/End 的宽度保留原选择并显示方向化上限；1440 与 960 像素离屏截图确认全部控件、透明选区、
  Bus 波形、刻度和网格同时可见，Scenario、Undo 与 Saved 均保持不变。
- 第 271–280 轮新增可嵌入 `waveautomation` 与统一 `wave-cli`：版本化 inspect/validate/apply
  JSON、五类原子 operation、文件/stdin、dry-run、独立/原地原子输出、SHA-256 冲突保护、稳定
  身份与颜色、结构化错误和退出码均通过核心及进程级回归；PowerShell 用户旅程确认 dry-run 与
  实际结果 SHA 一致，源工程不变。
- 第 281–290 轮使 CLI 可直接承担日常定位与批量编辑：`inspect --summary` 省略完整波形载荷，
  `sample` 按 tick、物理时间或周期返回指定信号值；range 与 End 接受精确物理时间/周期；新增
  `delete-signal`、`move-signal`、`update-signal`、`update-clock`，批次仍保持确定性、依赖清理
  和失败无部分结果。
- 第 291–300 轮补齐局部波形工作流：`window` 只返回指定时间与 Lane 的 Segment、首尾值和
  Event/Relation/Marker 上下文；`set-range.assignments` 原子写入多信号，`transfer-range`
  复制/移动同信号或跨兼容信号范围；`assert-value`、默认覆盖拒绝、显式 overwrite 及
  `--in-place --backup` 将错误阻止和恢复纳入同一契约。
- 第 301–310 轮使 CLI 可独立建立刺激工程：`new` 生成确定性空白工程并可在首次写盘前应用
  文件或 stdin operations；`set-sequence` 用一个 operation 写入单/多信号重复序列，支持
  tick、物理时间或时钟周期步长并自动延长 End。结果加入结构化变更摘要，已有输出、无时钟周期
  序列、非法值和批次中途失败均不会产生或覆盖文件。
- 第 311–320 轮消除 CLI 使用前必须查稳定 ID 的步骤：新增紧凑 `signals` 查询；Scenario、
  Lane、Clock 与 Group 的命令行及 operation 引用可使用唯一名称或稳定 ID，同批次新增对象也可
  立即按名称引用。歧义名称、缺失目标和无效查询原子拒绝，成功查询、采样、窗口和编辑报告仍返回
  规范 ID，便于脚本稳定串联。
- 第 321–330 轮补齐从零建立符号刺激的模型断点：新增 `add-group`，`add-signal` 支持 Enum、
  声明映射和直接归组，`update-signal` 可完整替换 Enum 映射。Group/Clock/Lane 可在同批次按
  名称串联；溢出值、非 Group 归组及会使既有 Segment/Event 失效的映射均原子拒绝。
- 第 331–340 轮补齐 CLI 时序意图：Relation 以 Lane 名称/ID 与精确波形边沿时刻寻址，支持
  增删改、tick/物理时间/周期延迟、时钟上下文和确定性 ID；Marker 支持唯一命名的点/区间、
  五种用途类型及按名称增删改。不存在或歧义边沿、冲突时钟、非法延迟/几何及批次中途失败
  均原子拒绝，不暴露会随 Segment 编辑重映射的内部 Event ID。
- 第 341–350 轮补齐外部集成发现与 Group 生命周期：`capabilities` 在无工程条件下机器返回
  命令、22 类 operation、选择器、时间和值域及安全特性，嵌入库复用同一能力文档；新增
  `update-group`、`move-group`、`delete-group`，支持同批次名称串联、无效果抑制及删除前成员
  影响报告。删除 Group 保留全部成员信号并解除归组，非法类型、冲突名称和批次中途失败原子拒绝。
- 第 351–360 轮将桌面端一键复制语义纳入 CLI：`duplicate-signal` 默认紧邻源信号，复制全部
  属性、扩展数据与 Segment，自动生成唯一名称、确定性 Lane/Segment ID 和不同颜色；Clock
  同时复制独立 ClockDomain。副本不继承 Event、Relation 或 trace mapping，报告明确公开边界；
  同批次可按新名称继续复制，非法 Group、名称/身份冲突和非 Clock 使用 `newClockId` 原子拒绝。
- 第 361–370 轮补齐 CLI 时间轴缩短闭环：空白尾部可直接缩短；会损失内容时必须显式设置
  `truncate`。结果报告 Segment/Marker 裁边及越界 Segment/Event/Relation/Marker 删除计数，
  命令支持完整 Undo/Redo，拒绝路径不产生部分写入。
- 第 371–380 轮新增紧凑 `edges` 查询：按信号、半开时间范围及
  `initial`/`rising`/`falling`/`change` 筛选精确波形边沿，并直接返回前后值、动作、时钟、
  `candidateCount` 与 `relationEndpoint`。结果不暴露内部 Event/Segment ID；Clock、Group、
  歧义端点、非法范围和上限均有结构化反馈。
- 第 381–390 轮新增紧凑 `relations` 查询：按 Relation 文本、严重性、Lane 和端点时间范围
  定位既有关系，直接返回 Relation ID、源/目标 Lane/时刻、实际延迟、声明范围和端点状态，
  不要求调用方连接内部 Event 数组。缺失/重复/脱离波形端点及超出延迟范围均可直接诊断，
  查询保持只读。
- 第 391–400 轮新增紧凑 `markers` 查询：按稳定 ID、名称、备注、用途类型及半开时间范围
  定位点或区间标注，直接返回精确起止、宽度、格式化时间和可寻址状态。重复 ID/名称、空身份、
  越界或反向区间及 Point/范围冲突均以稳定问题码返回；单时钟工程的周期范围可自动推断并公开
  所用 Clock，查询保持只读。
- 第 401–410 轮补齐损坏 Relation 的恢复路径：`update-relation` 不再要求旧端点先完整；
  缺失、重复、脱离波形、越界或歧义端点可用显式 Lane+时刻原子替换，健康的另一端保持不变。
  部分替换、歧义边沿、重复/空 Event ID 均拒绝且不产生输出；`edges` 同步公开
  `eventIdCount`，避免把不可稳定引用的边沿误报为可用端点。
- 第 411–420 轮补齐损坏 Marker 身份的恢复路径：`markers` 为每项返回与当前完整快照绑定的
  不透明 `markerRef`；空/重复 ID、损坏几何可精确改 ID、修正或删除，旧引用在内容或索引变化后
  原子失效。稳定 ID 和唯一名称不再绕过重复 ID 取第一项；扩展载荷参与引用校验但不会被查询暴露。
- 第 421–430 轮补齐损坏 Relation 身份的恢复路径：`relations` 为每项返回快照绑定的
  `relationRef`、ID 命中数、可寻址状态和稳定身份问题；重复或空 ID 可精确改 ID、联合修复端点
  或删除。直接稳定 ID 不再静默选择重复首项，内容或索引变化会使旧引用原子失效。
- 第 431–440 轮消除 `validate` 对结构身份损坏的假阳性：Project、ClockDomain、Scenario、
  Lane、Segment、Event、Relation 与 Marker 的空或重复稳定 ID 均返回机器可读对象路径和计数，
  计入 error 并使 CLI 以退出码 4 拒绝；语义问题与身份问题分别汇总，健康工程结果保持不变。
- 第 441–450 轮消除 `apply` 对无效候选的假成功：编辑前后对完整工程校验并比较 error 指纹，健康工程
  不得产生无效结果；损坏工程可保存错误数下降且无新增错误的渐进修复。无关编辑若保留全部既有
  error，或显式修复用新错误替换旧错误，均退出 4、不返回候选工程且不写文件；`validationGuard`
  直接报告错误变化及接受原因。
- 第 451–460 轮消除 Marker 查询与工程校验不一致造成的假绿：`validate` 现与 `markers` 共用
  名称和几何规则，空/重复名称、越界/反向区间及 Point/范围冲突均给出精确属性路径并计入 error。
  `markerSummary` 区分名称与几何问题；无关编辑受后置门禁阻断，普通名称或几何修复可按错误数
  下降安全分步保存。
- 第 461–470 轮消除“校验已定位、修复仍需再查询”的往返：可恢复的 Marker/Relation 问题直接
  携带快照绑定 `markerRef` / `relationRef` 和允许的 operation；重复身份按数组索引生成不同
  引用，Relation 语义问题也可直接修复。顶层区分可修复问题数与去重目标数，已满足或 condition
  不适用的信息项不产生虚假修复入口。
- 第 471–480 轮消除 Relation 校验仍需解析 message 或再查上下文的歧义：语义问题直接返回对象
  索引、精确属性路径、明确 `repairProperties`，以及与 `relations` 查询共用的端点/时序快照。
  缺失端点、延迟违例、condition、时钟冲突和观察窗口不足均指明修复侧；快照不暴露内部 Event
  ID 或扩展载荷。
- 第 481–490 轮将同一套直接恢复能力扩展到波形内容：非法 Bus/Enum Segment 和未定义区间现
  返回精确 Lane/Segment 索引、属性路径、紧凑 `waveformContext`、半开 `repairRange` 及可执行
  的 `set-range`/`clear-range`。读取器允许值语义错误进入 `validate`，但负时间、空区间和重叠
  等结构损坏仍在加载边界拒绝，外部生成工程因而可以先诊断再原子修复。
- 第 491–500 轮将直接恢复能力扩展到 Event：目标 Lane 缺失或位于 Scenario 半开区间之外时，
  `validate` 返回精确 Event 路径、紧凑 `eventContext` 及安全恢复选项。可表示的正向越界可直接
  延长 End；负时间、最大 tick 或不应保留的 Event 可按唯一稳定 ID 删除，并同步清理 Relation
  与有效波形依赖。重复 ID 不暴露不安全入口，也不能被 `delete-event` 静默选中。
- 第 501–510 轮消除 Event 与关联 Segment 不一致时的校验假绿：唯一且可恢复的关联现在会返回
  Lane、时刻和值的精确路径，以及不暴露内部 Segment ID 的关联目标快照。`repair-event-link`
  原位恢复 Event，并保留 Event ID、Relation 端点和波形内容；健康对象、重复 Event ID、重复关联、
  丢失目标及非法 Segment 均拒绝执行，不会退化为静默删除。
- 第 511–520 轮消除 Event 的 cycle 元数据与当前时刻脱节时的校验假绿：负 cycle、时钟无法唯一
  解析、周期计算溢出或时刻不在活动边沿均返回精确路径和预期时刻。`clear-event-cycle` 只移除
  已失效的逻辑周期约束，当前 Event 时刻、ClockDomain、稳定 ID、Relation 与波形保持不变；
  Event–Segment 关联修复也会保留仍正确的 cycle，并清除因目标时刻改变而陈旧的 cycle。
- 第 521–530 轮消除 Event 显式 ClockDomain 悬空时的校验假绿：问题返回精确引用路径、当前匹配
  数、Lane 时钟及明确恢复动作。`repair-event-clock` 在 Lane 时钟唯一有效时采用它，Lane 无时钟
  或不存在时清除无效显式引用，并同步保留或清理 cycle；Lane 时钟也悬空、当前时钟歧义或 Event
  身份歧义时只诊断，不暴露不安全恢复入口。
- 第 531–540 轮将 ClockDomain 完整性扩展到 Lane：Clock Lane 必须引用唯一存在的时钟域，普通
  Lane 的非空引用必须有效，Group 不得携带时钟引用。`repair-lane-clock` 对普通 Lane 清除悬空
  引用，对 Clock Lane 仅在工程恰有一个时钟域时自动关联；多时钟、歧义 ClockDomain 或重复 Lane
  ID 均拒绝猜测。修复保留波形、Event、Relation 与 Segment，只清理已无法解释的继承 cycle。
- 第 541–550 轮消除 Lane 分组引用损坏带来的“未解析分组”和信号组织歧义：非空 `groupId`
  必须唯一指向同一 Scenario 中另一条 Group Lane，Group 不得嵌套，普通 Lane 不得自引用或指向
  非 Group。`repair-lane-group` 只解除无效分组，不按名称、类型或顺序猜测替代目标；波形、
  Event、Relation、Segment、ClockDomain 和显示属性均保持不变。
- 第 551–560 轮消除 Relation 显式 ClockDomain 悬空时的校验假绿：非空引用必须唯一存在；
  `relationClockContext` 直接公开 Relation 与两端时钟匹配数、冲突状态和恢复动作。
  `repair-relation-clock` 只采用两端唯一共同有效时钟，或在两端均无时钟时清空无效引用；
  当前引用歧义、上游时钟损坏、端点冲突或 Relation ID 歧义均拒绝猜测。
- 第 561–570 轮消除 Imported Trace 映射损坏延迟到 Compare 才暴露的问题：映射键必须指向
  工程中存在的非 Group Lane，Actual signal ID 不得为空。`validate` 返回具体 Trace、映射键、
  问题码和快照绑定 `traceRef`；`repair-trace-mapping` 只删除已证明无效的单条映射，不读取、
  改写或猜测外部 VCD/FST/CSV 信号。完整 `inspect` 公开映射表，`--summary` 仍只返回计数。
- 第 571–580 轮消除 Imported Trace 身份与 Compare 选择歧义：空或重复 Trace ID 现作为
  工程级结构错误返回具体数组路径和不同的 `traceRef`；`repair-trace-identity` 只修改所选
  快照的 ID，可采用显式唯一 ID 或确定性生成值，并保留路径、格式、偏移、映射和扩展载荷。
  `wave-compare` 在多 Trace 时要求显式 `--trace-id`，重复 ID 不再静默取第一项；报告区分
  `unmapped-signal` 与 `missing-signal`，后者保留已失效的 Actual signal ID。
- 第 581–590 轮消除 Imported Trace 源引用错误延迟到解析阶段才暴露的问题：`path` 必须非空，
  `format` 必须为大小写不敏感的 VCD、FST 或 CSV；`validate` 按 Trace 聚合返回精确字段路径、
  `empty-path`/`unsupported-format`、支持格式和快照绑定 `traceRef`，并明确不检查文件系统。
  `repair-trace-reference` 要求显式提供缺失属性，只有修复后的引用整体有效时才原子更新路径/
  格式；`wave-compare` 在解析前分别说明空路径、未知格式和目标不是文件。
- 第 591–600 轮消除多 Scenario 工程被独立 CLI 静默按数组首项处理的问题：`wave-cli`、
  `wave-generate`、`wave-compare`、`wave-bridge import-signals` 与
  `wave-bridge pinloom-entry` 共用同一选择规则。单 Scenario 保持零配置；多 Scenario 必须
  显式提供稳定 ID 或大小写不敏感的唯一完整名称；空值、不存在、重名、重复稳定 ID 均在读取
  Trace、信号清单或写出文件前拒绝。Generate 的旧位置 Scenario ID 保持兼容。
- 第 601–610 轮消除桌面端多 Scenario 工程始终隐式编辑数组首项的问题：多波形工程显示紧凑
  `Waveform` 选择器，单波形工程保持零额外控件；启动 URI 按唯一稳定 ID 选择初始波形但不再
  重排 Scenario 数组。切换保存各波形的选择、光标、范围和视口上下文；全工程命令历史记录每次
  编辑所属波形，跨波形 Undo/Redo 会自动显示实际受影响的目标，同时保留工程级时钟修改的正确
  时间顺序。
- 第 611–620 轮消除重复打开工程后重新定位波形及键盘编辑时离开画布点选下拉框的成本：已保存
  工程按路径、Project ID 和唯一 Scenario ID 记住最后波形，数组重排后仍按稳定身份恢复；
  删除目标、重复身份或单波形工程安全回退且不污染工程。`Ctrl+PageUp` / `Ctrl+PageDown`
  提供不循环的前后切换和首尾反馈，所有就地草稿门禁继续生效；Save As 绑定新路径，URI 显式
  目标覆盖记忆。
- 第 621–630 轮继续消除重开工程后查找信号、重新定位时间的步骤：每个唯一 Scenario 按稳定
  身份记住最后一条可见信号和整数 tick，Save As→New→Open、跨波形首次进入及重启均可恢复；
  不恢复拍、Segment、范围或标题删除目标，隐藏/删除/重复信号降级为只恢复时间。会话内完整
  选择与视口优先于持久位置，显式 URI lane/tick 覆盖并更新后续位置。
- 第 631–640 轮继续消除恢复位置后重复缩放的步骤：按 Scenario 持久化与像素无关的可见 tick
  跨度，在首次显示和滚动条布局稳定后以安全光标为中心重放；窗口尺寸变化、隐藏信号降级和
  Scenario 数组重排均保持同一时间细节尺度。会话内完整视口仍优先；URI 的显式 lane/tick
  保留该尺度但重新锚定显式时间。缺失或损坏的旧跨度设置不阻止打开，超长跨度钳到当前 End。
- 第 641–650 轮补齐异常退出前最后操作位置：已有正式路径的工程在选择、定位、缩放及水平视图
  连续变化停止 400 ms 后，只写入安全 Lane、tick 和语义跨度，不改变工程、Saved、Undo 或
  autosave generation。恢复快照沿用正式工程身份，因此异常退出后数据与最后工作位置一起恢复；
  多次快速变化只保留最终状态。Untitled 工程与未提交的快速新增信号保持隔离，恢复仍不武装
  Beat、Segment、显式范围或 Delete 目标。
- 第 651–660 轮将既有 Group 从空白分隔行收敛为可折叠画布层级：标题箭头、Left/Right 和右键
  入口统一，成员缩进并显示引导线；折叠只改变逐 Scenario 会话视图，不改变模型、Saved 或 Undo。
  折叠当前编辑成员时安全清除 Beat/Segment/范围目标，显式定位自动展开；隐藏或删除折叠 Group
  会恢复成员显示。专项 offscreen 截图分别验证折叠态与展开态。
- 第 661–670 轮将信号归组从“打开完整属性窗口再手工重排”收敛为一次直接操作：信号标题右键
  `Move to group` 可直接选择 Group，`No group` 原位解除；也可把信号标题拖到 Group 行，整行
  蓝色目标反馈与普通排序插入线明确区分。归属和位置只形成一个 Undo 状态，折叠 Group 接收后
  自动展开并选中新成员；重复、无效、歧义及 Group 嵌套不会污染历史或波形。
- 第 671–680 轮继续压缩首次建组：`Edit > Add group…` 只询问名称，空名和重名在原对话框即时
  阻止；信号右键的 `New group with this signal…` 可一次创建 Group、放入首成员并整理位置。
  新 Group 出现在信号正上方，信号保持选中；Undo/Redo 原子恢复存在性、归属和顺序，高级属性
  仍留在 Group 标题右键，画布底部不增加常驻按钮。
- 第 681–690 轮消除多条相关信号逐条归组的重复操作：信号名支持 `Ctrl+单击` 增减选择、
  `Shift+单击` 连选可见信号，右键任一已选信号可一次移动到 Group、解除归组或用全部所选
  信号创建新 Group。批量命令保持显示顺序，只移动实际需要迁移的信号，并形成一个 Undo 状态；
  多选时重命名和复制继续明确限定单信号。
- 第 691–700 轮补齐多选后的清理闭环：右键或 Edit 菜单可将所选信号一次隐藏；Delete、
  Backspace 或右键可先预览事件、关系及导入映射影响，再一次删除全部所选信号。隐藏和删除
  各只形成一个 Undo 状态；Undo 恢复原模型和标题多选，Redo 清除已失效目标。删除仍保留其他
  Scenario 正在使用的同 ID trace mapping，撤销也不会覆盖命令执行后新导入的 Trace。
- 第 701–710 轮补齐多选后的直接重排：抓住任一已选信号名称可将全部选择按当前画布顺序作为
  连续块拖动，一次 Undo，原 Group 归属保持；拖到 Group 标题则一次归组并区分实际迁移与已在
  目标中的成员。插入线和 Group 行直接显示选择数量，Esc 取消保留模型、选择和 Redo；只单击
  不拖动仍收敛为单信号。
- 第 711–720 轮补齐多选后的精确步移：`Alt+↑/↓` 将全部选择按画布顺序整体移动一个可见行，
  隐藏 Lane、隐藏 Group 或折叠成员不会造成“按了但画面没动”。Edit 菜单显示选择数量，顶部/
  底部边界自动禁用且不产生历史；Undo/Redo 同时恢复连续块、三行高亮、活动信号和 Saved。
- 第 721–730 轮消除多选后逐条复制的重复操作：标题多选时 `Ctrl+D`、Edit 菜单和右键菜单
  会按 Scenario 顺序复制全部选择，在最后一个源信号后形成连续副本块。每个副本获得唯一名称、
  新 Lane/Segment 身份和重新分配的可读颜色，同时保留波形、属性及 Group 归属；Clock 各自
  获得独立时钟域。Event、Relation 和 Trace 映射不会被猜测复制。新副本立即成为多选，一次
  Undo/Redo 同时恢复源/副本选择、活动信号和 Saved。
- 第 731–740 轮消除复制快捷键的目标冲突：Edit 菜单只保留一个动态复制动作，显式时间范围优先于
  标题或 Segment 目标，`Ctrl+D` 会将完整多信号范围立即重复到其后；没有范围时继续复制单条或
  多条信号。范围复制不改系统剪贴板，保持结果范围选中，并原子处理 End 延长、Relation 清理及
  Undo/Redo；零变化只推进选择，不污染历史。范围存在时隐藏整项仍保留选择并说明按 Esc 恢复。
- 第 741–750 轮把范围重复从隐藏知识改为固定栏主操作：可见 `Repeat` 取代低频 Cut 按钮，一次点击
  即将完整单/多信号范围重复到其后，仍沿用 `Ctrl+D` 的剪贴板隔离、End 延长、Relation 清理和
  选择感知 Undo/Redo。Cut 功能未删除，继续由 `Ctrl+X`、Edit 和右键提供。范围栏按钮留白与间距
  小幅收紧，960 像素窗口下全部可见、可命中，混合类型也明确显示 Copy/Repeat/Paste/Clear。
- 第 751–760 轮把范围 Paste 的失败反馈前移到点击前：固定栏按钮统一预检 schema、复制宽度、
  目标数量、类型、Bus/Enum 位宽和每段值。数量、类型、位宽或值不兼容时按钮保持可见但禁用，
  提示直接指出具体源→目标配对和修正条件；有效目标则提前显示复制宽度、固定落点、全部映射、
  选区宽度差异和 End 延长。`Ctrl+V`、Edit 与右键入口继续保留运行时原子校验。
- 第 761–770 轮消除多信号 Paste 必须重框时间矩形的步骤：左侧标题单选/多选现在直接成为
  编辑光标处的粘贴目标，按可见顺序映射；`Ctrl+G` 定位不会丢失目标。标题选择时状态栏即时说明
  Ctrl+V 或具体数量/类型失败，`Target` 显示活动信号和落点；显式选择不兼容时不再静默回贴源信号。
  粘贴结果转为持久范围，一次 Undo 恢复原标题集合与光标，Redo 恢复结果范围。
- 第 771–780 轮把标题目标 Paste 从需记忆的快捷键变为可发现、可确认的就地动作：只要剪贴板含
  Wave 范围，既有信号标题右键菜单顶部便显示目标数量或名称和精确落点。有效动作的提示在提交前
  列出复制宽度与全部源→目标映射；数量或类型不兼容时动作保持可见但禁用，并给出相同的精确原因。
  入口继续复用标题预检、`pasteAtCursor()`、单命令 Undo/Redo 和结果范围，不增加新模式或算法。
- 第 781–790 轮消除标题目标与鼠标定位的冲突：左侧单选/多选目标后，单击或拖动时间标尺只改变
  编辑时间，不再关闭标题选择；`Target`、状态栏、逐项 Paste 映射和右键动作同步使用新落点。
  标尺拖动按 Esc 或异常失去左键会恢复目标、光标和水平视图；从显式范围开始时还恢复完整范围与
  固定栏。由此常规鼠标流程收敛为“复制→选目标→点标尺→Paste”，无需记忆 `Ctrl+G`。
- 第 791–800 轮消除一次 Paste 后再次定位必须重选目标的问题：粘贴结果范围在标尺正常释放时
  转换为同一有序标题目标，`Target`、状态栏和逐项映射使用新时间，可立即再次 Paste；取消仍恢复
  原结果范围。两次非相邻 Paste 的 Undo/Redo 分别恢复各自提交前目标与结果范围，不发生源信号回退。
- 第 801–810 轮消除多信号 Paste 提交前只能阅读文字映射的盲操作：兼容标题目标现在按实际复制
  内容显示半透明虚线波形、覆盖边界、起止时间及 `当前值→预览值`；标尺拖动实时移动预览，Esc
  恢复原落点。预览复用同一兼容性预检，不兼容时完全隐藏，提交后消失，Undo 恢复，且全程零模型
  与零命令变化。
- 第 811–820 轮将 Paste 可能清理 Relation 的影响提前到提交前：预览按实际覆盖后的完整 Segment
  拓扑和 Event 复用顺序计算将消失的波形边沿；无影响时仍使用蓝色，存在依赖清理时改用琥珀色，
  画布标签、标题右键 Paste 和状态栏均显示准确数量及 `Ctrl+Z` 恢复说明。提交结果和 Undo 恢复
  与预检逐项一致，预检过程不修改模型、历史、保存状态、稳定 ID 或 autosave。
- 第 821–830 轮消除“知道会删关系，但不知道删哪一条”的定位成本：风险标签直接列出首条关系的
  源/目标信号与精确时间，右键 Paste 和状态栏同时给出 Relation 描述及全部端点；受影响连线改为
  带光晕的琥珀色虚线，端点加环且中点显示删除标记。未受影响连线保持原严重级别样式，预览离开后
  所有强调立即消失。
- 第 831–840 轮将同一 Paste 投影扩展到显式时间范围：选区仍显示用户选定宽度，虚线边界和标签
  单独显示实际复制宽度，提交前即可看出 40–60 ns 目标只会覆盖 40–50 ns。固定范围栏与范围右键
  现在复用同一数量、类型、宽度、值和 Relation 预检；已知无效动作直接禁用。提交后的等值结果
  不残留虚线，Undo 返回原目标时恢复预览。
- 第 841–850 轮把“粘贴后才发现没有变化”收口为提交前反馈：波形、Relation 清理和 End 延长均无
  变化时，固定范围栏、范围右键、波形右键、标题状态和标题右键统一禁用 Paste 并说明目标已匹配。
  宽选区仍以中性虚线保留实际复制宽度说明；直接 `Ctrl+V` 的运行时兜底只报告跳过，不改变选择、
  Undo/Redo 或 Saved。多信号波形右键继续使用原复制目标集合，单信号则使用点击信号。
- 第 851–860 轮把同一提交前反馈扩展到显式范围 Clear/Run：纯投影精确识别实际受影响信号和
  Relation，不消耗稳定 ID；无效果动作在固定栏和右键直接禁用，安全动作明确保证不删除 Relation，
  风险动作以 `Clear ⚠N` 和准确端点预警。范围 Cut 与 Edit > Cut 复用同一结果，但保留“只复制、
  无源值可清除”的有效路径；真实提交、Undo 与预检逐项一致。
- 第 861–870 轮继续把范围预设、Clock Gate/Disable 和 Bus/Enum 草稿纳入同一三态反馈：只统计
  真正变化的信号，等值按钮提交前禁用，风险按钮以 `值 ⚠N` 定位 Relation；文本草稿在 Enter 前
  完成位宽/符号、无效果及风险判断。快捷键仍经过无损执行守卫，实际提交与 Undo 的关系影响、
  选择和历史恢复逐项一致。
- 第 871–880 轮把范围 Repeat 纳入提交前目标投影：固定按钮、范围右键、Edit 和 `Ctrl+D` 在执行前
  显示后续目标、信号数、End 延长和 Relation 影响；风险按钮为 `Repeat ⚠N`，无效果入口直接禁用。
  悬浮显示目标虚线波形并强调受影响 Relation；直接快捷键的无效果守卫保持源选区、剪贴板和既有
  Redo。成功提交才移动到目标范围，实际清理和一次 Undo 与预检一致。
- 第 881–890 轮把范围 Move 与 Ctrl+拖动 Copy 纳入同一释放前投影：蓝色表示安全目标，琥珀色
  `Move/Copy ⚠N` 标出将删除的 Relation 及准确端点，中性色表示无需提交，`End →` 提前公开时间轴
  延长。预览按真实“先清源、再清目标、最后写入”顺序处理跨信号映射；无效果释放保持源选区、Redo
  与剪贴板，成功结果才进入一次历史。重叠、类型不兼容和动态 Ctrl 意图切换继续即时反馈。
- 第 891–900 轮把单个 Segment 的主体移动、Ctrl+拖动复制和左右边界调整纳入同一释放前投影。
  受影响时段直接叠加最终虚线波形，而非只显示范围框；安全、Relation 风险和无变化分别使用蓝色、
  琥珀色和中性色。状态栏与画布标签提前列出准确 Relation 描述和端点；无变化复制在创建命令前
  跳过，保留源 Segment、Redo 和剪贴板，真实提交与一次 Undo 严格对应预览。
- 第 901–910 轮把 Edit > Segment 的八项精确命令和 Bus/Enum 右键相邻复制纳入点击前预检。
  每个入口直接公开目标范围、不可用原因、无变化状态或 `⚠N` Relation 风险；不可执行和无效果
  动作提前禁用。悬浮安全动作显示蓝色最终虚线波形，风险动作显示琥珀色波形、受影响连线和准确
  端点；移向非预览项或关闭菜单立即清除。点击后实际修改、关系清理与一次 Undo 严格对应预检，
  且八项投影不再进入高频指针状态更新路径。
- 第 911–920 轮把 Bus/Enum 的 Beat/Segment 就地编辑器纳入同一提交前预检。文本草稿在输入时
  即区分越位宽/非法符号、无变化、安全修改和 Relation 风险；0/X/Z/Don't care/Clear 在悬浮时
  显示最终虚线波形。无变化改为 `Done` 且不创建历史，无效草稿禁用 Apply 与前后导航，危险 Clear
  显示琥珀色 `⚠N`、准确 Relation 描述与端点。目标在提交前失效时会原位阻断；实际修改、依赖清理
  和一次 Undo 与预演一致。
- 第 921–930 轮将连续多拍录入收敛到既有 Bus/Enum Beat 编辑框：可直接粘贴逗号、分号、空格、
  Tab 或换行分隔的值，`X/Z` 自动扩展到完整 Bus 位宽。编辑器在提交前显示全部目标拍、规范化值、
  Relation 风险及 `End →`，预设悬浮结束后恢复整段草稿；一次 Apply 只产生一条 Undo，并选中完整
  结果范围。非法列表定位到具体序号，等值列表显示 `Done` 且不污染历史；Segment 中的列表会阻断
  提交，点击 `Segment` 即切回 Beat 并保留草稿，避免重新输入。
- 第 931–940 轮补齐 Bit 精确模式录入：框选单个 Bit 信号的完整拍范围后，可在既有范围栏输入
  紧凑或分隔的 `0/1/X/Z` 模式，支持 `0b` 前缀和最多 1024 个目标拍；短模式仅在能整除选中拍数时
  自动重复。输入时即
  显示完整蓝色虚线结果、拍数和重复次数；非法长度变红且原位阻断，等值模式不进入历史，Relation
  风险改为琥珀色并列出准确依赖。提交复用单信号序列命令，一次 Undo 同时恢复波形、Event 和 Relation。
- 第 941–950 轮把同一 Bit 模式扩展到多信号范围：一次框选共享拍网格的多个 Bit 信号后，只需输入
  一次模式，即可同时看到每条信号的最终虚线波形并以一条命令提交；一次 Undo/Redo 原子恢复全部
  波形、Event、Relation 和选区。不同拍网格会在输入阶段明确变红阻断，等值批次不进入历史，
  Relation 风险按所有目标的联合结果预检。离屏视觉验收同时修复了多行选区画刷状态泄漏，第二行
  不再被不透明蓝块覆盖，刻度、原波形和预演结果保持可见。
- 第 951–960 轮消除多 Bit 不同激励仍需逐条编辑的重复步骤：多信号范围内输入 `01 / 0011`，
  即按画布从上到下分别映射模式；未使用 `/` 时仍广播同一模式。每条模式可在整除目标拍数时独立
  重复，输入阶段直接显示 `req=01×2; ack=0011×1` 和两条最终虚线波形。模式数量不等于信号数、
  某条模式不整除或值非法时会点名目标并原位阻断；等值映射不进入历史，提交仍只有一次 Undo。
- 第 961–970 轮压缩 Bit 长电平区间的录入：既有模式字段支持 `symbol*N`，例如 `0*32` 表示连续
  32 拍低电平，`X*4` 表示连续四拍未知态。该写法同时适用于单模式广播和 `/` 分隔的逐信号模式，
  输入阶段仍显示展开后的逐拍虚线结果及 `req=0011; ack=1100` 映射。计数必须是紧邻符号的正十进制，
  `0*0`、`0*` 和超过 1024 符号的展开会在原字段内点名信号并阻断；等值结果不进入历史，成功提交
  仍只有一次 Undo。
- 第 971–980 轮将同一压缩方式扩展到既有 Bus/Enum 连续拍编辑器：`0x00*8 0xff*2` 和
  `IDLE*4 WAIT_ACK*2` 会在输入阶段展开为逐拍虚线结果，仍由一次 Apply 和一次 Undo 完成。
  原有逗号、分号、空格、Tab、换行列表及单值编辑保持兼容；合法的完整 Enum 符号优先于游程解释。
  零计数、缺失计数和超过 1024 值的展开原位阻断，错误态不再显示伪展开数量。输入字段扩至
  160–240 px，使紧凑表达式可直接校对，同时保留 `X (implicit)` 提示并通过 960 px 包含性回归。
- 第 981–990 轮消除“已经框选范围，却仍需退出范围栏再打开单拍浮层”的 Bus/Enum 序列编辑割裂：
  既有范围字段现在同时接受整段单值、共享逐拍序列及按画布从上到下用 `/` 分隔的逐信号序列，
  每项继续支持 `value*N`。单值语义保持整段赋值；序列必须覆盖完整拍，短序列只在能整除选中拍数时
  重复，多信号必须共享拍网格。输入阶段直接绘制每条目标信号的最终虚线波形并预检 Relation；
  非整除、数量不匹配、值非法或拍网格不一致均原位阻断。等值序列不创建命令，成功提交以一次
  Undo/Redo 原子恢复全部波形、Event、Relation 和选区。
- 第 991–1000 轮消除修改既有 Bus/Enum 序列时必须重新抄写全部拍值的成本：完整显式拍范围的
  值字段末尾提供载入动作，一次取得当前逐拍值；多信号按画布从上到下使用 `/` 分组。16 拍以内
  展开为便于定位的逐拍文本，较长同值区间自动使用 `value*N` 压缩；载入只改变编辑字段，不改变
  Scenario、Saved 或历史。用户修改任意一项后仍看到原有最终虚线预演，并以一次 Undo 提交。
  该轮对隐式 X、拍内变化和预设扩展元数据采取禁用载入的保守策略；后续可用性收敛已改为
  虚拟基线，未改 token 不实体化或重写，修改 token 才覆盖完整编辑拍。不同拍网格或不能安全表示的值仍禁用
  载入并说明原因。首次 Esc 仅隐藏载入文本并保留选区，第二次才关闭范围。
- 第 1001–1010 轮补齐载入文本与画布拍位的直接对应：在当前值文本中移动光标或选中 token，
  画布会自动露出并以青色虚线强调准确的信号和拍，状态栏同时显示 `beat 2 of 4`、物理范围与值。
  多信号 `/` 映射会定位到对应 Lane，`value*N` 则强调完整连续游程并显示起止拍序号。该定位只
  改变可见位置和临时覆盖层，不改变编辑光标、范围选区、Scenario、Saved 或历史；全文选择不
  假定具体 token，Esc 隐藏、切换范围或工程后立即清除，避免把旧 token 误关联到新文本。
- 第 1011–1020 轮将 token 定位延伸到正在修改的有效 Bus/Enum 序列：第一次输入后不再清空
  拍位映射，用户可连续点选第二、第三个 token 并立即看到对应信号与拍。解析器同步记录每个
  token 的字符范围、展开偏移和展开数量；短模式重复时一个 token 会同时框出所有命中拍，
  共享序列会同时框出所有目标信号，`value*N` 会框出完整展开区间。状态栏显示目标出现次数、
  信号数及紧凑拍号；跨 token 选择、非法草稿、Esc 或范围切换会清除定位，避免歧义。定位仍只
  影响临时覆盖层和必要滚动，最终预演、Relation 预检、一次 Undo 提交及等值抑制语义保持不变。
- 第 1021–1030 轮补齐反向定位：安全载入文本或有效 Bus/Enum 序列草稿存在时，悬浮预演拍显示
  手形指针和 token 提示，单击该拍会直接滚动并选中文本中的对应 token，可立即键入替换。重复
  模式会选择同一 token 并保留所有受影响拍的局部框；共享序列从任一目标 Lane 点击都选择同一
  token，同时以被点击 Lane/拍作为首目标；逐信号 `/` 序列只选择对应分组。该点击优先于原有
  “焦点离开即提交”路径，因此不会意外应用草稿，也不改变编辑光标、显式范围、Scenario、Saved
  或历史。范围边界及 Ctrl/Shift/Alt 操作仍沿用原交互；Esc 或 Enter 后恢复常规范围拖动。
- 第 1031–1040 轮收口 Bus/Enum 序列的连续编辑与恢复：实际逐字符替换经历短暂非法文本后，
  被点击 Lane/拍仍保持为主目标；主目标改为亮色实线框，其他受同一 token 影响的重复拍或共享
  Lane 使用弱化虚线框，状态栏明确主信号、拍号、物理范围和关联数量。Tab/Shift+Tab 按画布
  Lane 与时间顺序在目标间前后移动且不提交、不循环；游程 token 保持原子。Edit 菜单及
  Ctrl+Z/Ctrl+Y 在活动值字段中只撤销/重做草稿；即使当前无文本历史，也不穿透到波形命令栈。
  载入当前值后，文本 Undo 会恢复 `loadedExisting` 安全基线，第一次 Esc 也先恢复基线，下一次
  才隐藏；若波形命令栈随后 Undo/Redo，旧载入文本、映射与锚点会立即失效，不能伪装成新模型的
  当前值。token 单击改为释放且未超过拖动阈值后生效，因此未修改载入态可继续直接拖动整个范围；
  修改中草稿的拖动会原位阻断、不隐式提交或丢失文本，释放后也不会残留禁用指针。仅无修饰
  Tab 和 Shift+Tab 导航，Ctrl+Shift+Tab 保留系统原语义。
- Waveform 工具栏离屏检查确认常驻动作仅有临时 `Measure / Markers`、缩放和 Fit；持久选择期间的批量赋值栏
  固定在工具栏中，960 像素宽度下无裁切且显隐不移动画布。不存在 Edit、Transition、Export 与
  Undo/Redo 按钮，菜单快捷键仍存在。
- 独立用户旅程从空白工程完成三类信号创建、信号就地重命名、波形编辑、Bus/End 双草稿纠错、
  时间轴延长、测量、Save As 无扩展名补全、Open 通配过滤/安全取消、保存→New→Open 实际回读与成功反馈、
  脏工程 Open 取消/选定后的确认顺序、
  Export 无效范围/
  PDF 跨度原位纠错、修正后目录选择和
  1440×900 截图，全程 offscreen。
- 共享生成计划、确定性 SystemVerilog/cocotb 输出、时钟覆盖调度和严格 SVA 转换；
  独立 assertion 模块及 testbench 均通过 SystemVerilog 语法检查。
- SVG、PNG、PDF、WaveDrom JSON 的范围、尺寸、页跨度及背景回归。
- `wave-generate` CLI 对磁盘示例工程的端到端生成。
- `wave-cli` 覆盖确定性 new、完整/精简 inspect、紧凑信号定位、按时刻采样、Relation 就绪
  边沿查询、既有 Relation 与 Marker 紧凑查询、损坏 Relation 显式端点修复、损坏 Marker
  与 Relation 快照引用修复、局部窗口查询、
  capabilities、validate、三十三类
  原子 operation、多信号序列/范围写入、范围转移、内容断言、精确物理时间/周期、stdin、
  名称/稳定 ID 选择器、同批次名称引用、dry-run、独立输出、原地替换与备份、SHA-256/备份冲突、
  整信号复制、Group 完整生命周期、Enum 创建与映射更新、边沿 Relation 与 Marker 管理、
  安全 End 缩短与显式内容截断、稳定身份、变更摘要、依赖清理、歧义拒绝、失败无部分结果、
  Marker 完整性校验、可操作校验引用、结构化 Relation/波形/Event 诊断、非退化后置校验、渐进式恢复、
  JSON 契约与 Windows PowerShell 管道。
- 生成的 cocotb 文件通过 Python 语法编译，SystemVerilog testbench 通过 Vivado `xvlog`。
- PNG 和由最终 PDF 栅格化所得页面完成视觉复核。
- VCD timescale、多值信号、`$dumpvars`、层级/总线名称、CSV 显式单位及取消；Wellen FST
  元数据与按信号加载、配对 VCD 等价性、规模门禁、取消和旧 generation 拒绝。
- 外部 VCD 示例磁盘解析、名称映射、偏移安全移动和二分可见范围查询。
- 1000 个 signal、1,000,000 个 transition 的 100,000 次窗口查询基准。
- exact、ignore X、expected X wildcard、edge tolerance、time window、bus mask、
  enum equivalence 和 relation-only compare。
- first mismatch、缺失 signal、width mismatch、condition evaluation error、诊断可见性
  以及 JSON/CSV/HTML 报告。
- ZeroSlack signal-list、Private Frame sample、Pinloom entry 和 Wave Workbench URI。
- 文件对话框、最近工程和工程拖放通过完整用户旅程验收：首次 Save As 位于 Documents/Home；保存后最近工程立即出现；New 后 Open 保持最近目录；当前脏工程的最近项为安全无操作；New 后最近项一键恢复 650 ns/3 lane 工程；再将独立 700 ns 工程拖入画布，断言加载、Saved/Opened 和 Recent 置顶。所有 GUI 测试的设置目录相互隔离且不触碰真实用户设置；Bus 快捷值不再使用 MIME 拖放，工程文件拖入仍保持独立。
- 主窗口后台 autosave 生成可由正式加载器读取的恢复快照；首次保存前的 Untitled 编辑也写入隔离恢复目录，无参数重启自动恢复且 Save 仍要求正式位置；启动/Open 已保存工程自动采用有效且更新的快照并显示 Save required，较旧或损坏快照回退正式工程。正式工程的恢复快照同时复用该工程最后一次去抖写入的安全 Lane、tick 与尺度，状态栏同时报告 `Recovery snapshot loaded`、`resumed` 和 `no edit range restored`；正式保存清除已完成/在途快照并保留 Saved 反馈；New/Open/Close 的 Discard 也等待在途 worker、永久清除快照且不重新调度。

- 命令历史为每个真实状态保留稳定保存点标识；正式保存后继续编辑，再用 Undo 回到保存版本时，
  状态立即恢复为 `Saved`、窗口星号消失且不再产生关闭保存提示。Redo 离开保存点后重新显示
  `Unsaved changes`。若 Undo 发生在 autosave 正在写入或已经完成之后，过期恢复快照会在后台结果
  返回后或当场删除；新分支不会因与旧分支栈深度相同而被误判为 Saved。
最近完整静默验收：默认、Qt Creator Debug、Qt Creator Release 三套构建均成功，三套各
76/76 CTest 通过，offscreen 串行执行耗时分别为 41.41 s、38.61 s、37.16 s。高风险定向集合
覆盖核心命令、文件冲突、信号管理、Wave Edit、完整用户旅程和 CLI 兼容入口，6/6 通过。
本轮新增的桌面交互、CLI 兼容适配、JSON Schema 和 Portable install 契约均已进入上述全量验收。
扩展后的 `wave-wave-edit-smoke` 覆盖多标题目标的标尺单击、拖动、Esc 恢复、动态 Paste 落点与
逐项映射、鼠标定位后真实右键粘贴、显式范围被标尺临时关闭后取消恢复，以及结果范围转标题目标后
在 40 ns 与 60 ns 连续 Paste、两步 Undo/Redo；新增覆盖兼容目标预览范围与有序 Lane、标尺拖动
实时移动、取消恢复、不兼容隐藏、提交消失及 Undo 恢复，并区分无影响预览与会清理一个 Relation
的风险预览，断言预检数量、准确 Relation 描述与 `req @ 80 ns → ack @ 110 ns` 端点、菜单/状态
说明、真实清理结果及 Undo 恢复完全一致；新增覆盖显式范围的实际复制宽度预览、数量不匹配时
固定栏/右键同步禁用、Relation 风险强调、提交后无变化抑制及 Undo 恢复，并覆盖 Clock 等值范围
在固定栏、范围右键、波形右键、标题状态和标题右键的提交前禁用、宽度中性预览、直接 `Ctrl+V`
跳过以及既有 Redo 保留；进一步覆盖显式范围 Clear/Run 的安全、Relation 风险和无效果三态，
断言 `Clear ⚠1`、准确 Relation 描述与端点、实际清理、单步 Undo、固定栏/右键禁用、Cut 仍可复制，
以及 Delete/`R` 运行时兜底保留选择、模型、历史和既有 Redo；新增覆盖 Bit 安全/风险/等值按钮、
实际受影响信号数、Relation 精确清理与 Undo，Bus 有效/越位宽/等值草稿的 Enter 前反馈，以及
Clock Gate/Disable 的安全/等值状态和键盘兜底 Redo 保留。模型、命令历史和视图恢复语义同步断言。
新增覆盖范围 Repeat 的安全、Relation 风险、无效果和纯 End 延长四类结果，断言固定按钮、范围右键、
Edit 动作和 `Ctrl+D` 使用同一目标；风险预览准确显示 100–120 ns 虚线波形、受影响 Relation 描述
与端点，真实提交和 Undo 一致；无效果路径保持 20–30 ns 源选区及既有 Redo；210–220 ns 末尾范围
提前显示并提交 `Extends End to 230 ns`。
新增覆盖范围 Move/Ctrl+拖动 Copy 的安全、Relation 风险、无效果和纯 End 延长四类投影；
同信号与跨信号 Move 均在释放前准确报告
`ack must rise within 1..4 cycles after req · req @ 80 ns → ack @ 110 ns`，实际删除与 Undo
一致。安全 Copy 明确保证不删除 Relation；已匹配 Copy 和空白 Clock Move 不创建命令、不移动
源选区且保留既有 Redo；210–220 ns 到 220–230 ns 的 Copy 在释放前显示并提交 End 延长。
标题批量、信号标题、画布添加、光标模式、自动滚动、用户旅程和 Edit 菜单八项专项继续通过。
离屏截图 `build/wave-edit-smoke-header-paste-menu.png` 已复核菜单布局和可发现入口；
`build/wave-edit-smoke-header-ruler-target.png` 已复核 40 ns 光标、双标题目标、Target 和映射反馈；
`build/wave-edit-smoke-header-target-paste.png` 已复核 40–50 ns 双目标结果、固定栏、刻度、网格
和明确的粘贴结果反馈；`build/wave-edit-smoke-range-ruler-retarget.png` 已复核结果范围关闭后
双标题目标与 60 ns 落点，`build/wave-edit-smoke-header-repeated-paste.png` 已复核 40–50 ns
与 60–70 ns 两组非相邻粘贴结果；`build/wave-edit-smoke-header-paste-preview.png` 已复核
60–70 ns 的两条差异化虚线波形、范围边界、`1→0` / `0→1` 标题值和 Ctrl+V 提示；
`build/wave-edit-smoke-header-paste-relation-warning.png` 已复核 80–90 ns 琥珀色风险预览、
`removes 1 relation` 标签、标题差异值及可恢复反馈；
`build/wave-edit-smoke-header-paste-relation-identity.png` 已复核具体端点标签、受影响连线光晕、
虚线、双端点环和中点删除标记；`build/wave-edit-smoke-explicit-range-paste-preview.png`
已复核 40–60 ns 选区与 40–50 ns 实际 Paste 宽度并存；
`build/wave-edit-smoke-explicit-range-paste-relation-warning.png` 已复核显式范围中的 Relation
风险端点、琥珀色连线和固定范围栏；`build/wave-edit-smoke-paste-no-effect-preflight.png`
已复核等值 Clock 的禁用 Paste、中性 40–50 ns copied width、40–60 ns 原选区及无变化反馈；
`build/wave-edit-smoke-range-clear-relation-warning.png` 已复核 80–100 ns 双信号范围的
琥珀色 `Clear ⚠1`、受影响 Relation 强调与精确端点；
`build/wave-edit-smoke-range-clear-no-effect-preflight.png` 已复核已正常运行 Clock 范围的禁用
Run、仍可用的 Cut、网格、波形和无变化反馈；
`build/wave-edit-smoke-range-value-relation-warning.png` 已复核双信号范围的 `0 ⚠1`、
实际受影响信号数、Relation 强调与精确端点；
`build/wave-edit-smoke-range-value-no-effect-preflight.png` 已复核既有 DISABLED Clock 范围的
禁用 Disable、仍可用的 Run/Gate、快捷键无变化反馈和完整波形。
`build/wave-edit-smoke-repeat-relation-warning.png` 已复核琥珀色 `Repeat ⚠1`、100–120 ns 目标
虚线、准确 Relation 端点、连线光晕和 Ctrl+D 提示；
`build/wave-edit-smoke-repeat-no-effect-preflight.png` 已复核等值 Clock 范围的禁用 Repeat、
30–40 ns 中性目标、时间刻度和 `no Repeat needed` 反馈。
`build/wave-edit-smoke-range-move-preview.png` 已复核 80–100 ns 源范围、120–140 ns 琥珀虚线
目标、`Move ⚠1`、准确 Relation 端点及连线删除标记；
`build/wave-edit-smoke-range-copy-no-effect-preflight.png` 已复核 20–40 ns 源范围、
160–180 ns 中性目标与 `no Copy needed`；
`build/wave-edit-smoke-range-copy-end-extension-preflight.png` 已复核 220–230 ns 目标、
`End → 230 ns` 和安全 Relation 保证。
`build/wave-edit-smoke-segment-move-preview.png` 已复核 Segment Move 只在受影响时段叠加
最终虚线波形、准确宽度和安全 Relation 保证；
`build/wave-edit-smoke-segment-copy-no-effect-preflight.png` 已复核中性无变化 Copy、
源 Segment 保持和 `no Copy needed`；
`build/wave-edit-smoke-segment-copy-relation-warning.png` 已复核琥珀色 Copy 目标、
准确 Relation 描述、端点强调及释放后可恢复反馈。
`build/wave-edit-smoke-segment-menu-safe-preview.png` 已复核 Edit > Segment 安全动作悬浮时
30–100 ns 蓝色最终虚线波形、精确宽度和无 Relation 保证；
`build/wave-edit-smoke-segment-menu-relation-warning.png` 已复核菜单动作的 `⚠1`、琥珀色目标、
受影响 Relation 连线、准确描述和端点；
`build/wave-edit-smoke-segment-menu-no-effect-preflight.png` 已复核相邻等值目标的禁用
`no change`、边界不可用状态、波形与网格。
`build/wave-edit-smoke-bus-editor-safe-preview.png` 已复核 Segment 草稿在 Enter 前显示蓝色
最终虚线波形、精确范围和无 Relation 保证；
`build/wave-edit-smoke-bus-editor-no-effect-preflight.png` 已复核等值草稿使用中性色、
`Done` 与 `no command will run`；
`build/wave-edit-smoke-bus-editor-invalid-preflight.png` 已复核越位宽输入变红并禁用提交与导航；
`build/wave-edit-smoke-bus-editor-clear-relation-warning.png` 已复核危险 Clear 的琥珀色
`⚠1`、implicit X 最终波形、准确 Relation 描述及端点。
`build/wave-edit-smoke-bus-sequence-safe-preview.png` 已复核 `0x12, 0x34, X, 0x56` 的四拍
完整预览、整宽 X、禁用歧义导航和 `Apply 4`；`build/wave-edit-smoke-enum-sequence-safe-preview.png`
已复核 Enum 符号列表使用同一连续拍路径；`build/wave-edit-smoke-bus-sequence-invalid-preflight.png`
与 `build/wave-edit-smoke-bus-sequence-no-effect-preflight.png` 已复核具体错误序号及零历史路径；
`build/wave-edit-smoke-bus-sequence-end-extension-preview.png` 已复核提交前的 `End → 240 ns`；
`build/wave-edit-smoke-bus-sequence-relation-warning.png` 已复核七拍合并前的 `Apply 7 ⚠1`、准确
Relation 描述与端点；`build/wave-edit-smoke-bus-sequence-segment-blocked.png` 已复核
Segment 列表阻断及保留草稿的 Beat 恢复入口。
`build/wave-edit-smoke-bit-pattern-safe-preview.png` 已复核 `0b01` 在四拍范围内重复两次的完整
蓝色虚线波形、模式字段和即时状态；`build/wave-edit-smoke-bit-pattern-invalid-preflight.png`
与 `build/wave-edit-smoke-bit-pattern-no-effect-preflight.png` 已复核拍数不整除时的红色阻断及
等值模式零历史路径；`build/wave-edit-smoke-bit-pattern-relation-warning.png` 已复核单值模式
覆盖四拍前的琥珀色最终波形、准确 Relation 描述、端点强调及一次 Undo 恢复。
上述已完成的 GUI 路径使用 offscreen，迭代期间未操作桌面、未打包、未提交、未推送。完整验收记录见
[PLAN.md](PLAN.md)。

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
- 桌面端可切换已有 Waveform/Scenario，但尚未提供安全的新建、复制、删除或重排生命周期。
  当前命令历史仍绑定 Scenario 对象地址，在引入稳定身份绑定前不对 Scenario 容器做可使现有历史失效的结构修改。
- 画布当前仍使用深色主题，浅色背景/主题切换尚未交付。
- `+ CLK` / `+ BIT` / `+ BUS` 仍位于信号列表末尾；长列表滚动时常驻或粘性添加入口尚未交付。
