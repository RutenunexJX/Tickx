# Qt Creator 构建与调试

## 所需 Kit

选择 Qt 6.10.2 Desktop Kit。Ela 的私有 Qt 头文件依赖要求固定此补丁版本。该 Kit 必须提供：

- Qt Core、Gui、Widgets、WidgetsPrivate、Svg、Concurrent，以及运行测试所需的 Test。
- C++20 编译器。
- CMake 3.24 或更高版本。
- Ninja。
- 与编译器匹配的调试器。

本机安装 Qt Creator 18.0.2；命令行等价验证使用
`Desktop Qt 6.10.2 MinGW 64-bit` 的以下工具链：

```text
Qt:       E:\QT6\6.10.2\mingw_64
Compiler: E:\QT6\Tools\mingw1310_64\bin\g++.exe
Debugger: E:\QT6\Tools\mingw1310_64\bin\gdb.exe
CMake:   E:\QT6\Tools\CMake_64\bin\cmake.exe
Ninja:   E:\QT6\Tools\Ninja\ninja.exe
```

这些绝对路径仅说明已验证环境，不写入共享预设。其他机器应在 Qt Creator 的
Preferences > Kits 中选择其本机 Qt Desktop Kit。

## 首次打开

1. 在 Qt Creator 中打开仓库根目录的 `CMakeLists.txt`。
2. 在 Configure Project 页面选择 Qt Desktop Kit。
3. 选择 `Qt Creator - Debug` configure preset。
4. 完成配置后，将运行目标设为 `Tickx`。

若 Qt Creator 未显示 preset，可在项目 Build Settings 中使用等价配置：

```text
Build directory: <source>/build/qtcreator-debug
Generator:       Ninja
CMAKE_BUILD_TYPE=Debug
WAVEWORKBENCH_BUILD_TESTS=ON
```

Qt 的 CMake 前缀、编译器、调试器、CMake 和 Ninja 均由所选 Kit 提供，不应手工提交
`CMakeLists.txt.user` 或 `CMakeUserPresets.json`。

## Build 与 Debug

- 点击 Build 或按 `Ctrl+B` 构建当前项目。
- 选择 `Tickx` 后点击 Run，可启动内置演示场景。
- 点击 Start Debugging 或按 `F5`，可在 `src/app/main.cpp` 等源码中设置断点并调试。
- 调试多 Scenario 工程时，运行后的工具栏会显示 `Waveform` 选择器；切换不修改工程文件，
  `Ctrl+PageUp` / `Ctrl+PageDown` 可在画布内前后切换。正式工程重新打开时恢复最后一个唯一
  稳定 ID 波形，并恢复该波形最后的安全信号、整数 tick 和可见时间跨度；跨度在窗口最终布局后
  以光标为中心恢复，不依赖 Qt Creator 启动窗口的像素尺寸。Beat、Segment 和范围不会恢复。
  正式工程在导航或缩放停止 400 ms 后自动更新安全位置；调试进程异常终止后重新运行并采用较新
  `.autosave` 时，恢复数据和最后安全视图可同时返回，不要求先 Save 或正常关闭。恢复状态不会
  武装 Delete，Undo 仍为空；Untitled 和未提交快速新增信号不建立该位置身份。URI 可用
  `&scenario=<stable-id>&lane=<stable-id>&tick=<integer>` 显式覆盖位置并保留该尺度。
  调试 Group 时可点击标题箭头或按 Left/Right 收起/展开成员；该动作仅改变当前 Scenario
  会话视图，不会触发断点所观察的模型写入、Undo 或 Saved 变化。
- 若要直接打开磁盘示例，在 Projects > Run Settings 中设置：

```text
Command line arguments: examples/handshake/project.wave.json
Working directory:      <source>
```

运行磁盘示例不是调试前置条件；无参数启动不依赖工作目录。

## 可用目标

| 目标 | 类型 | 用途 |
|---|---|---|
| `Tickx` | GUI 可执行程序 | 主桌面应用，默认调试目标 |
| `wave-generate` | CLI 可执行程序 | 生成 HDL、Python 和图形派生产物；多 Scenario 时使用 `--scenario` |
| `wave-compare` | CLI 可执行程序 | 无界面 Expected/Actual 对比；多 Scenario 时使用 `--scenario` |
| `wave-bridge` | CLI 可执行程序 | 跨应用文件与 URI 契约；Scenario 级子命令支持 `--scenario` |
| `wave-cli` | CLI 可执行程序 | 机器可读 capabilities、new、signals、inspect、sample、window、edges、markers、relations、结构化 validate，以及整信号复制、安全 End 截断、完整 Group 生命周期、Enum、边沿 Relation/Marker 编辑、损坏波形/Event/身份、Event–Segment 关联、陈旧 cycle、悬空 Event/Lane/Relation ClockDomain、无效 Lane Group 与 Imported Trace 身份/源引用/映射恢复 |
| `wave-tests` | 测试可执行程序 | 领域、生成、导入、Compare 和集成测试 |
| `test` | CMake/CTest 目标 | 运行已登记的完整测试集 |

CLI 目标需要命令行参数，具体用法见根目录 `README.md`。调试主应用时应选择
`Tickx`，不要选择静态库目标。

## 命令行等价验证

Qt Creator Kit 注入的工具链可用以下命令等价验证：

```powershell
$env:PATH='E:\QT6\Tools\mingw1310_64\bin;E:\QT6\Tools\Ninja;' + $env:PATH
cmake --preset qtcreator-debug `
  -DCMAKE_PREFIX_PATH=E:\QT6\6.10.2\mingw_64 `
  -DCMAKE_CXX_COMPILER=E:\QT6\Tools\mingw1310_64\bin\g++.exe `
  -DCMAKE_MAKE_PROGRAM=E:\QT6\Tools\Ninja\ninja.exe
cmake --build --preset qtcreator-debug
ctest --preset qtcreator-debug
```

每个新的 PowerShell 会话都应先设置上述 PATH。仅给 CMake 传入编译器绝对路径并不足以
启动 MinGW 的 `cc1plus.exe`，因为它还需要从 PATH 加载同目录中的运行时 DLL。命令行显式
路径仅用于复现本机 Kit；Qt Creator 的 Desktop Kit 会注入对应构建环境，不需要重复填写。
