# Tickx CLI 快速开始

便携包中的主自动化入口是 `wave-cli.exe`。该程序不创建窗口，可供 Codex、脚本和 CI 直接调用。

## 1. 发现能力

```powershell
.\wave-cli.exe capabilities --pretty
```

输出为 `wave-workbench.capabilities/v1` JSON。`compatibilityCommands` 中带有
`compatibilityAdapter: true` 的 `generate`、`compare` 和 `bridge` 命令会转发到同目录中的旧工具；
它们保留旧工具的文本输出和退出码。其余命令使用结构化 JSON 输出与统一退出码。

## 2. 检查示例工程

```powershell
.\wave-cli.exe inspect .\examples\handshake\project.wave.json --summary --pretty
.\wave-cli.exe validate .\examples\handshake\project.wave.json --pretty
```

## 3. 从零创建波形

先进行只读预演：

```powershell
.\wave-cli.exe new .\quick-start.wave.json `
  "--name=CLI quick start" "--duration=80 ns" `
  --operations=.\docs\examples\quick-start.operations.json `
  --dry-run --pretty
```

确认 JSON 报告中的 `ok`、`changed` 和 operation 结果后，删除 `--dry-run` 写入工程：

```powershell
.\wave-cli.exe new .\quick-start.wave.json `
  "--name=CLI quick start" "--duration=80 ns" `
  --operations=.\docs\examples\quick-start.operations.json --pretty
```

该示例创建一个 10 ns 时钟、一个 Bit 信号和一个 8-bit Bus，并写入四拍序列。

## 4. 调用兼容工具

```powershell
.\wave-cli.exe generate .\examples\handshake\project.wave.json .\generated
.\wave-cli.exe compare .\examples\handshake\project.wave.json .\compare
.\wave-cli.exe bridge describe `
  .\examples\handshake\project.wave.json .\workspace-manifest.json
```

既有脚本仍可直接调用 `wave-generate.exe`、`wave-compare.exe` 和 `wave-bridge.exe`。
完整参数、JSON 字段、选择器和退出码见 [automation-cli.md](automation-cli.md)。

## 5. 在源码仓库中生成 Windows 便携包

以下命令只在 Tickx 源码仓库根目录中执行；已生成的便携包不包含
`scripts/package-windows.ps1`。从 Release 构建树生成可复现的便携目录、ZIP 和 SHA-256 文件：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\package-windows.ps1 `
  -BuildDirectory .\build\qtcreator-release `
  -QtBinDirectory E:\QT6\6.10.2\mingw_64\bin `
  -OutputDirectory .\build\package
```

脚本只删除输出目录中同版本、同名称的旧便携目录、ZIP 和校验文件。它通过 CMake 的
`Portable` 安装组件收集五个可执行程序、CLI 文档、最小 operation 示例和 handshake 工程，
再调用所选 Qt Kit 的 `windeployqt.exe` 收集运行库。
