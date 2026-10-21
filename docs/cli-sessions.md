# GUI 会话 CLI

Tickx 0.15.4 源码新增 `wave-cli session`，复用 SuiteApp `suite-app/v1`。
构建和便携目录需使用 SuiteApp SDK/Runtime 1.0.2 或更高版本；1.0.1 的 Windows 异步响应可能被截断。
无 SDK 的构建继续支持原有离线命令，`capabilities.session.available` 为 false。

`wave-cli window` 仍查询文件中的波形数据。正在打开的画布由 `session view` 控制。
命令不会启动或随机选择用户窗口。

```powershell
.\wave-cli.exe session --help
.\wave-cli.exe session schema --pretty
.\wave-cli.exe session list --pretty
.\wave-cli.exe session status --session=<list 返回的 sessionId>
```

默认 Runtime endpoint 为 `suiteapp.runtime.v1`。`Tickx.exe --suite-runtime=<endpoint>`
和 `wave-cli session ... --runtime=<endpoint>` 可连接同一个隔离 Runtime。
`Tickx.exe --settings-dir=<directory> <project.wave.json>` 使用独立界面设置；
这些是公开启动参数。测试和示例应使用自己创建的项目及设置目录。

## 目标和版本

除 list/schema/help 外必须显式提供窗口的 sessionId。ID 在窗口生命周期内稳定，
重启后重新发现。每个窗口注册独立 `wave.session.<UUID>` provider，
不会覆盖其他窗口。现有 `wave` provider 保留为单一兼容入口，不能作为多窗口选择机制。

status 返回真实主窗口状态：

| 字段 | 含义 |
| --- | --- |
| modelSha256 | 当前内存 Project 序列化结果的 SHA-256，包含未保存修改 |
| loadedSha256 | 此窗口实际采用的文件字节 SHA-256 |
| diskSha256 | 查询时磁盘文件的 SHA-256 |
| dirty / activeEditor | 未保存修改、活动编辑器或拖动 |
| adoption | current、pending、conflict、invalid、ignored、missing、unreadable 或 unsaved |
| view | 实际光标、可见起止、选择、场景、信号显示和分组折叠状态 |

current 只表示文件版本已采用；内存仍可能 dirty。不可用的文件摘要为 null。
状态枚举最多列出前 1000 个场景和信号，并同时返回总数。

## 视图

`session view --session=<id> --args=view.json`：

```json
{
  "scenarioId": "main",
  "cursor": "25 ns",
  "start": "10 ns",
  "end": "50 ns",
  "laneIds": ["lane-request", "lane-data"],
  "range": {"start": "20 ns", "end": "30 ns"}
}
```

时间沿用离线解析器，接受 tick 字符串或物理时间表达式。所有时间值必须使用字符串。
start/end、zoom、pan 三组选项互斥。zoom 为 in/out/fit，pan 为带符号时间字符串。
返回 visibleStart/visibleEnd 为实际像素量化后的边界，不保证与请求逐 tick 相同。
单独移动光标会沿用 GUI 的“确保光标可见”行为；同时指定范围时范围优先。

laneIds 可以引用信号或分组；分组会展开为其可见成员。range 为 null 清除范围选择。
markerIds、relationIds 选择稳定对象，groups 接受 `[{"id":"group-core","collapsed":true}]`。
relationsVisible 控制关系覆盖层。持久化的隐藏/显示属于模型编辑，通过 apply 的
update-signal / update-group 的 visible 字段修改，支持撤销。
dryRun 只校验。整个视图请求先校验，再改变画布；无效对象或参数不会留下部分视图变更。

## 在线编辑和保存

```powershell
$state = (.\wave-cli.exe session status --session=$id | ConvertFrom-Json).result.state
.\wave-cli.exe session apply --session=$id --scenario=main --ops=operations.json --dry-run
.\wave-cli.exe session apply --session=$id --scenario=main --ops=operations.json --expect-model-sha256=$($state.modelSha256)
$state = (.\wave-cli.exe session status --session=$id | ConvertFrom-Json).result.state
.\wave-cli.exe session save --session=$id --expect-model-sha256=$($state.modelSha256) --expect-sha256=$($state.loadedSha256)
```

apply 调用与离线 apply 相同的 applyAutomationBatch，并作为一次撤销事务提交。
undo/redo 同样要求最新 expectedModelSha256。dryRun 允许省略内存版本，但已提供的版本仍会校验。
已有 dirty 文档只有在调用方明确提供当前内存版本且没有活动编辑时才可继续在线编辑。
save 必须提供目标文件版本；新文件用 `--expect-sha256=missing`。
不会覆盖尚未采用的外部更新。落盘仍使用原有 QSaveFile 原子保存。
保护属于乐观版本检查，不能排除不遵守此协议的其他进程在检查与替换之间再次写入。

## 同路径更新采用

```powershell
$before = (.\wave-cli.exe session status --session=$id | ConvertFrom-Json).result.state
$write = .\wave-cli.exe apply project.wave.json external.operations.json --in-place --scenario=main --expect-sha256=$($before.diskSha256) | ConvertFrom-Json
.\wave-cli.exe session refresh --session=$id --expect-sha256=$($write.resultSha256) --wait-ms=3000
```

refresh 复用文件监听和安全采用流程，不要求换文件名。open 同一路径也进行版本检查。
dirty 返回 conflict；编辑器或拖动未结束时返回 pending；无效文件返回 invalid，保留原模型。
ignored 保留用户通过 GUI 作出的忽略选择。没有强制丢弃或强制覆盖命令。
先保存/撤销本地修改或在 GUI 中解决冲突，然后刷新。用 loadedSha256 确认实际采用的版本。
`--wait-ms` 只轮询采用状态，不替用户处理冲突。

## 输出与限制

新增 session 命令的成功和失败均以单个 JSON 对象写 stdout；退出码：
0 成功，2 用法，3 输入/传输，4 拒绝，5 输出失败，6 尚未采用。
6 的 ok=true 仅表示刷新请求已处理，必须检查 adoption。
既有离线命令和兼容适配器的输出、退出码维持原约定。
apply/save/undo/redo 遇到传输失败时可能已经执行，先查询状态，不能无条件重试。

输入上限 512 KiB、批次 4096 个操作、选择 1000 个 ID；等待最多 30 秒。
在线批次报告超过 512 KiB 时在提交前拒绝，需拆分批次或改用离线 apply。
--args 和 --ops 支持 UTF-8 JSON 文件或 stdin（-），不能同时读取同一个 stdin。
相对路径以 CLI 调用者工作目录为基准。Runtime 协议限制单条消息为 1 MiB；
过大的状态或批次报告需要缩小工程/批次，传输失败不得被当作成功。

JSON Schema 随包位于 schemas/automation/v1/session-request.schema.json 和
session-response.schema.json。前者的 $defs 按子命令描述 --args 内容。
完整示例见 examples/cli-session/run.ps1，能力范围与已知 GUI 专属入口见交付覆盖表。
