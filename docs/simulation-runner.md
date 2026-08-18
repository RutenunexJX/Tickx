# Simulation Runner

`wave-sim-runner` 是 Wave Simulation 的实验性独立进程边界。当前切片只探测
Verilator 与 C++ 编译器，不生成、编译或 elaboration 任何 DUT，也不修改工程文件。

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

`--cancel-after-ms` 用于自动化和宿主生命周期联调。正式 GUI 的 Stop 操作应直接调用
`ToolchainProbeRunner::cancel()`，不需要启动第二个控制进程。

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

本切片不提供正式 GUI 入口。下一切片在该 runner 上增加固定 fixture 的 harness、
Verilator 调用和 VCD 读取，不应绕过此进程状态模型。
