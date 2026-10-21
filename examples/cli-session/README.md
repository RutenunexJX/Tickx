# GUI 会话示例

使用带 SuiteApp SDK/Runtime 1.0.2+ 的本轮构建或便携目录，在 PowerShell 中执行：

```powershell
.\run.ps1 -TickxDirectory C:\Tools\Tickx
```

示例创建独立设置和样例工程，从空工程创建时钟、位信号、总线、分组、序列、标记、关系和两个场景；启动实际 GUI，设置光标/范围；预览并提交在线编辑；保存；对同一路径执行离线更新并核对 GUI 采用摘要；重新打开并导出。

输出保存在当前目录下的新 tickx-session-* 文件夹。窗口保持打开，result.json 记录窗口 ID、Runtime endpoint、采用摘要和视图。
样例不修改已有工程。若目标文件已存在，脚本拒绝覆盖。详细命令契约见 ../../docs/cli-sessions.md。
