# Wave Workbench 跨应用接口

当前接口 schema 版本：`1`

接口仅使用显式 JSON 文件、CLI 参数和 URI。任何命令都不读取 ZeroSlack、Private Frame
Workbench 或 Pinloom 的内部数据库。

## ZeroSlack

### 导出工作区描述

```powershell
wave-bridge describe project.wave.json workspace.json
```

`workspace.json` 包含 Project/Scenario 稳定 ID、整数 `durationTick`、signal 定义和可调用
命令模板。ZeroSlack 可在指定 workspace 调用：

```powershell
wave-generate project.wave.json <workspace>
```

生成物是单向派生产物，不回写场景事实源。

### 导入信号清单

```powershell
wave-bridge import-signals project.wave.json signals.json output.wave.json
```

输入格式：

```json
{
  "schemaVersion": 1,
  "sourceProjectId": "zeroslack-project-stable-id",
  "signals": [
    {
      "stableId": "rtl-signal-stable-id",
      "name": "ready",
      "kind": "bit",
      "width": 1,
      "signed": false,
      "clockDomainId": "clock-main"
    }
  ]
}
```

已有稳定 ID 不重复创建。若已有 lane 的 kind/width 冲突，则保留 Wave Workbench 中的定义并
输出诊断。命令始终写到显式输出工程，不覆盖输入文件。

## Private Frame Workbench

```powershell
wave-bridge link-frame project.wave.json frame-reference.json output.wave.json
```

输入格式：

```json
{
  "schemaVersion": 1,
  "frameProjectId": "frame-project-id",
  "frameId": "frame-id",
  "sampleId": "sample-id",
  "encodedBytes": "35A7",
  "contentHash": "sha256:...",
  "summary": "WRITE payload 0x35, CRC 0xA7",
  "sourcePath": "relative/or/absolute/reference.json",
  "scenarioId": "scenario-id",
  "laneId": "transaction-lane-id",
  "segmentId": "transaction-segment-id"
}
```

后三个目标字段可全部省略；若提供则必须同时提供。目标必须是 Transaction lane 中的
Segment。目标失效时，资源仍以
`frameProjectId/frameId/sampleId`、encoded bytes、hash 和 summary 保留，并输出
unresolved 诊断。

## Pinloom

```powershell
wave-bridge pinloom-entry project.wave.json exports pinloom-entry.json
```

输出包含 Project/Scenario 稳定 ID、工程路径、导出文件清单、字节数及
`pinloom://archive?...` URI。桌面端 File > Export and Open in Pinloom 使用同一格式；
只有用户在确认框选择 Open in Pinloom 后才交由操作系统打开 URI。Pinloom 不存在或未注册
URI handler 时，JSON 归档条目仍保留。

## Wave Workbench URI

```text
waveworkbench://open?project=<path>&scenario=<stable-id>&lane=<stable-id>&tick=<integer>
waveworkbench://compare?project=<path>&scenario=<stable-id>&lane=<stable-id>&tick=<integer>
```

`project` 必填。`scenario`、`lane`、`tick` 可选；`tick` 是工程整数 tick。`compare`
动作打开 Expected/Actual 分屏并在 trace 后台加载完成后运行比较。等价命令行形式：

```powershell
wave-workbench --uri="<waveworkbench URI>"
```

未知 action、缺失 project、非法 tick 或不存在的 scenario 会明确失败，不回退到其他
工程或显示名称匹配。
