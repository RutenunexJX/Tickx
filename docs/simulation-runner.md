# Simulation Runner

`wave-sim-runner` 是 Wave Simulation 的实验性独立进程边界。它提供工具链探测，
并可使用版本化 Module Manifest 与 Stimulus Scenario 契约运行仓库内固定 DUT。
runner 不修改输入工程文件，正式 GUI 尚未接入该入口。

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
- runner 依赖 QtCore 事件循环，不依赖 QtWidgets；正式 GUI 后续可以直接持有该对象，
  不需要在 UI 线程同步等待。
- 默认在请求环境的 PATH 之外继承 runner 进程当前 PATH；需要严格隔离的测试或宿主可将
  `inheritCurrentProcessPath` 设为 `false`，此时空 PATH 不会回退到系统环境。
- `VerilatorSimulationRunner` 依次执行契约校验、工具链探测、harness 生成、模型构建、
  仿真和 VCD 导入。所有外部进程均复用 `ProcessRunner`，调用线程不会同步等待。

`--cancel-after-ms` 用于自动化和宿主生命周期联调。正式 GUI 的 Stop 操作应直接调用
`ToolchainProbeRunner::cancel()`，不需要启动第二个控制进程。

## 固定 Fixture 闭环

```powershell
wave-sim-runner run-fixture `
  --manifest=tests\fixtures\simulation\fixed-counter\manifest.json `
  --stimulus=tests\fixtures\simulation\fixed-counter\stimulus.json `
  --workspace=tests\fixtures\simulation\fixed-counter `
  --artifacts=build\simulation-runs `
  --pretty
```

该命令严格恢复 Manifest 与 Stimulus 的同一模块契约，生成独立 C++ harness，调用
Verilator 构建并运行模型，随后将 VCD 解析为现有 `TraceIndex`。每次运行使用唯一目录，
保留 harness、Verilator object directory、仿真可执行文件和 VCD。当前 S5 范围仅支持：

- module-definition 目标；
- 名称可直接映射为 C++ 标识符的模块与端口；
- 宽度不超过 64 bit 的固定 integral input/output；
- 已知二值输入、时钟及禁用时钟区间；
- Manifest 中的 source、include、define 与可求值 parameter。

interface、inout/ref、unpacked array、未知位宽、X/Z 激励与未完整 elaboration 的目标会
返回明确的 `unsupported-fixture` 或契约错误，不会静默降级。

固定测试 DUT 位于 `tests/fixtures/simulation/fixed-counter/`。常规自动化使用确定性的
独立进程 fixture 验证完整编排与 TraceCanvas 渲染；若 CMake 配置时发现真实 Verilator，
会额外注册 `wave-fixed-fixture-real-verilator`，对同一 DUT 执行真实编译和仿真。

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

`run-fixture` 输出 `wave-workbench.simulation-run/v1` JSON，包含终止阶段、诊断、全部
artifact 路径、工具链证据、构建/运行进程证据，以及导入后 TraceIndex 的 signal 和
transition 计数。失败状态区分输入、Manifest、Stimulus、契约、能力限制、工具链、
harness、构建、运行、VCD 导入、超时与取消。

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

本切片不提供正式 GUI 入口，也不接受 ZeroSlack 当前选中的用户模块。下一切片应从
ZeroSlack 选中目标生成临时源码镜像和契约，并继续复用该异步状态模型。
