# Qt Creator 构建与调试

## 所需 Kit

选择 Qt 6.5 或更高版本的 Desktop Kit。该 Kit 必须提供：

- Qt Core、Gui、Widgets、Svg、Concurrent。
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
4. 完成配置后，将运行目标设为 `wave-workbench`。

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
- 选择 `wave-workbench` 后点击 Run，可启动内置演示场景。
- 点击 Start Debugging 或按 `F5`，可在 `src/app/main.cpp` 等源码中设置断点并调试。
- 若要直接打开磁盘示例，在 Projects > Run Settings 中设置：

```text
Command line arguments: examples/handshake/project.wave.json
Working directory:      <source>
```

运行磁盘示例不是调试前置条件；无参数启动不依赖工作目录。

## 可用目标

| 目标 | 类型 | 用途 |
|---|---|---|
| `wave-workbench` | GUI 可执行程序 | 主桌面应用，默认调试目标 |
| `wave-generate` | CLI 可执行程序 | 生成 HDL、Python 和图形派生产物 |
| `wave-compare` | CLI 可执行程序 | 无界面 Expected/Actual 对比 |
| `wave-bridge` | CLI 可执行程序 | 跨应用文件与 URI 契约 |
| `wave-tests` | 测试可执行程序 | 领域、生成、导入、Compare 和集成测试 |
| `test` | CMake/CTest 目标 | 运行已登记的完整测试集 |

CLI 目标需要命令行参数，具体用法见根目录 `README.md`。调试主应用时应选择
`wave-workbench`，不要选择静态库目标。

## 命令行等价验证

Qt Creator Kit 注入的工具链可用以下命令等价验证：

```powershell
cmake --preset qtcreator-debug `
  -DCMAKE_PREFIX_PATH=E:\QT6\6.10.2\mingw_64 `
  -DCMAKE_CXX_COMPILER=E:\QT6\Tools\mingw1310_64\bin\g++.exe `
  -DCMAKE_MAKE_PROGRAM=E:\QT6\Tools\Ninja\ninja.exe
cmake --build --preset qtcreator-debug
ctest --preset qtcreator-debug
```

命令行显式路径仅用于复现本机 Kit；在 Qt Creator 内不需要重复填写。
