# Simulation Runner

`wave-sim-runner` 是 Wave Simulation 的正式独立进程边界。它提供工具链探测，
并可使用版本化 Module Manifest 与 Stimulus Scenario 契约运行固定 DUT 或宿主提供的
完整模块源码镜像。runner 不修改输入工程文件；宿主可请求生成带 VCD 引用的结果工程。

## 探测

```powershell
wave-sim-runner probe --pretty
wave-sim-runner probe `
  --verilator=C:\tools\verilator\bin\verilator.exe `
  --cxx=C:\tools\mingw\bin\g++.exe `
  --timeout-ms=5000 --pretty
```

发现顺序是确定的：

1. 命令行显式路径；
2. Verilator 的 `VERILATOR` 或 `VERILATOR_ROOT`，编译器的 `CXX`；
3. 当前进程 `PATH`。编译器按平台从 `g++`、`clang++`、`cl` 或 `c++` 中选择。

显式配置不存在时不会猜测相似目录。runner 不经过 shell，不读取 SystemVerilog，
也不访问 ZeroSlack 或 Wave Workbench 的内部状态。

当前最低兼容版本为：

| 工具 | 最低版本 |
| --- | --- |
| Verilator | 5.0.0 |
| GCC/G++ | 8.0.0 |
| Clang | 7.0.0 |
| MSVC | 19.20.0 |

这些下限是下一切片固定 harness 的 C++17 执行基线，不代表正式支持矩阵已经冻结。
无法识别实现或版本时返回 `version-unrecognized`，不会按“可能兼容”继续。

## 异步边界

可嵌入入口位于 `wave/simulation_runner.h`：

- `ProcessRunner` 异步启动单个进程，分别捕获 stdout/stderr，限制输出容量，并提供
  `cancel()` 和硬超时；实现不调用 `waitForStarted`、`waitForFinished` 或 GUI
  `processEvents`。
- `ToolchainProbeRunner` 并行探测两个工具，在两个结果都终止后只回调一次。
- runner 依赖 QtCore 事件循环，不依赖 QtWidgets；GUI 直接持有该对象，不在 UI 线程
  同步等待。
- 默认在请求环境的 PATH 之外继承 runner 进程当前 PATH；需要严格隔离的测试或宿主可将
  `inheritCurrentProcessPath` 设为 `false`，此时空 PATH 不会回退到系统环境。
- `VerilatorSimulationRunner` 依次执行契约校验、工具链探测、运行时激励生成、构建缓存解析、
  必要时的模型构建、仿真和 VCD 导入。所有外部进程均复用 `ProcessRunner`，调用线程不会
  同步等待；stage 回调携带 generation，GUI 会忽略旧 generation 的阶段与完成回调。

`--cancel-after-ms` 用于自动化和宿主生命周期联调。GUI 的 Stop 操作直接调用
`VerilatorSimulationRunner::cancel()`，不启动第二个控制进程。

## 固定 Fixture 闭环

```powershell
wave-sim-runner run-fixture `
  --manifest=tests\fixtures\simulation\fixed-counter\manifest.json `
  --stimulus=tests\fixtures\simulation\fixed-counter\stimulus.json `
  --workspace=tests\fixtures\simulation\fixed-counter `
  --artifacts=build\simulation-runs `
  --build-cache=build\simulation-build-cache `
  --pretty
```

该命令严格恢复 Manifest 与 Stimulus 的同一模块契约，生成稳定 C++ runtime harness，
调用 Verilator 构建并运行模型，随后将 VCD 解析为现有 `TraceIndex`。每次运行使用唯一目录，
保留私有运行时激励与 VCD；harness、Verilator object directory 和仿真可执行文件保存在
按构建指纹寻址的共享缓存中。当前固定运行范围支持：

- module-definition 目标；
- 名称可直接映射为 C++ 标识符的模块与端口；
- 宽度不超过 64 bit 的固定 integral input/output；
- packed struct、固定 unpacked array，以及无构造端口且显式指定 modport 的 interface；
  这些结构由 manifest v3 的 Slang selector 生成 wrapper 后重建，VCD 使用稳定 flat trace
  名映射回 leaf lane；
- 已知二值输入、时钟及禁用时钟区间；
- Manifest 中的 source、include、define 与可求值 parameter。

带构造端口或缺少显式 modport 的 interface、inout/ref、动态/关联数组、宽度超过 64 bit
的 leaf、未知位宽、X/Z 激励与未完整 elaboration 的目标会
返回明确的 `unsupported-fixture` 状态码（为了兼容已有自动化名称）或契约错误，不会静默降级。

固定测试 DUT 位于 `tests/fixtures/simulation/fixed-counter/`。常规自动化使用确定性的
独立进程 fixture 验证完整编排与 TraceCanvas 渲染；若 CMake 配置时发现真实 Verilator，
会额外注册 `wave-fixed-fixture-real-verilator`，对同一 DUT 执行真实编译和仿真。

## 宿主模块运行

```powershell
wave-sim-runner run-module `
  --manifest=run\module-manifest.json `
  --stimulus=run\stimulus.json `
  --workspace=run\source-mirror `
  --artifacts=run\results `
  --build-cache=cache\simulation-builds `
  --result-project=run\result.wave.json
```

`run-module` 与固定 fixture 共用同一个异步流水线，但要求输出结果工程。成功后工程包含
相对 VCD 引用和自动信号映射，可由
`wave-workbench --load-first-trace result.wave.json` 直接打开结果波形。输入
Manifest、Stimulus 与源码镜像由宿主负责生成；runner 仍严格校验三者契约，不读取宿主
编辑器状态，也不回退到磁盘中的其他源码。

## 构建缓存与 generation

模型缓存使用 `wave-workbench.simulation-build-cache/v1` 记录。SHA-256 构建指纹覆盖：

- 原始 Module Manifest，因此端口、parameter、define 和源文件清单变化都会失效；
- Manifest 中所有 design/header 源码的内容摘要；
- 稳定 runtime harness 的内容摘要；
- Verilator/C++ 编译器的解析路径、版本、类型、显式参数和会影响编译的环境字段。

Stimulus Scenario 不属于构建指纹。每次运行把当前激励写入私有 runtime plan，并传给已编译
模型；因此仅修改激励会跳过 `BuildModel`。缓存命中前会同时校验元数据 evidence 和实际
可执行文件摘要；损坏或不完整条目不会被复用。缓存未命中时先在唯一 staging 目录构建，
元数据与可执行文件验证完成后再发布，不会把取消构建留下的半成品作为有效条目。

每次运行携带单调 generation。结果工程旁的声明文件与进程锁保证同一路径只有当前
generation 可以物化结果；较旧运行无论先启动后完成，还是在较新运行之后才启动，均返回
`superseded`，不能覆盖新场景。未显式传入 `--generation` 时 runner 使用跨进程时间基编号。

## 结构化结果

stdout 始终输出 `wave-workbench.toolchain-probe/v1` JSON。总体状态为：

- `ready`
- `unavailable`
- `incompatible`
- `failed`
- `timed-out`
- `cancelled`

每个工具均包含请求程序、解析后的绝对路径、参数、原始 stdout/stderr、退出码、耗时、
截断状态、识别版本、最低版本和诊断。缺失工具、非零退出、崩溃、超时与取消不会压缩成
同一个布尔失败。

`run-fixture` 与 `run-module` 输出 `wave-workbench.simulation-run/v1` JSON，包含 generation、
终止阶段、诊断、全部 artifact 路径、构建缓存指纹/命中状态、工具链证据、构建/运行进程
证据，以及导入后 TraceIndex 的 signal 和 transition 计数。失败状态区分输入、Manifest、
Stimulus、契约、能力限制、工具链、harness、构建、运行、VCD 导入、结果工程物化、超时、
取消与被新 generation 取代。

构建或运行失败时，报告的 `diagnostics` 数组按条目提供 `sourceFile`、`line`、`column`、
`severity`、`stage`、`code` 和 `message`。源码路径必须位于 workspace 内并转换为相对路径；
无法解析工具输出时生成指向目标模块声明的保守诊断。ZeroSlack 将这些条目映射为可跳转通知。

退出码：

| 退出码 | 含义 |
| --- | --- |
| 0 | ready |
| 2 | 命令用法错误 |
| 3 | unavailable |
| 4 | incompatible |
| 5 | failed |
| 6 | timed-out |
| 7 | cancelled |
| 8 | superseded |

宿主以 `wave-workbench --load-first-trace result.wave.json` 打开结果时，WaveWorkbench
会异步载入首个 trace 引用，不阻塞窗口创建；普通项目打开行为不受影响。结果工作区上方
为可编辑的 `Stimulus`，下方为只读的 `Actual`，并提供 `Run`、`Stop`、`Rerun` 和
`Ready`、`Compiling`、`Running`、`Current`、`Stale`、`Failed` 六种可见状态。

结果工程的 `waveSimulation.session` 扩展使用
`wave-workbench.simulation-session/v1`。它保存重跑所需的 Manifest、Stimulus、工作区、
产物目录、构建缓存目录、结果工程及显式工具参数；进程环境不写入工程，加载时重新从当前进程获取。
图形激励发生变化后状态转为 `Stale`，Run/Rerun 将当前内存场景直接导出并运行，成功后
直接刷新 Actual 波形；取消、失败或 superseded 均不会用旧结果覆盖当前场景。仅激励变化时
复用已验证模型；RTL 或编译契约变化时自动重新构建。
