# Wave Workbench 跨应用接口

当前 Module Manifest schema 版本：`3`（兼容读取 `1`、`2`）

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
wave-generate project.wave.json <workspace> --scenario=<稳定 ID 或唯一名称>
```

工程只有一个 Scenario 时可省略 `--scenario`；工程包含多个 Scenario 时必须明确选择。
manifest 中的 Generate/Compare 命令模板包含该选择器占位，不再暗示数组首项。生成物是单向
派生产物，不回写场景事实源。

### 导入 Wave Simulation Module Manifest v1/v2/v3

```powershell
wave-bridge import-module module-manifest.json output.wave.json
```

该命令严格读取 ZeroSlack 输出的版本化 Module Manifest，并创建一个可直接在
Wave Workbench 中打开的未保存仿真目标工程。它不读取 ZeroSlack 内部数据库，也不重新解释
SystemVerilog：

- 唯一 clock candidate 转换为可见 Clock lane 和显式 ClockDomain；多个候选保持
  `ambiguous`，不自动选择。
- 唯一 reset candidate 标注在对应输入 lane；多个候选保持 `ambiguous`。
- `input` 创建 stimulus lane，整个默认时段有一个可见的零值 Segment，可立即编辑。
- `output` 创建无驱动的 watch lane；`inout` / `ref` 同时保留 stimulus 和 watch 意图。
- enum 名称和值映射到 Enum lane；完整声明、方向、源码位置、类型 identity 和 manifest
  identity 保存在扩展字段中。
- v2 的 `observationScope` 区分完整 module 与选中 `always` 的初始观察范围；范围不改变
  编译目标。`observations` 携带语义 ID、access path、类型和源码位置，内部信号作为
  watch lane 导入。
- `always` 范围下，stimulus input 始终可见，已观察的 output 和内部信号可见，其他
  output 默认隐藏但不从工程模型删除。同一声明的不同成员按 access path 保持独立。
- v3 的 `editableLeaves` 由 ZeroSlack 的 Slang elaboration 生成。packed struct member、
  固定 unpacked array element 和显式 modport interface member 作为分组 leaf lane 导入；
  selector、成员方向、enum map、packed bit offset、source/storage index 和稳定 trace 名均
  保存在扩展字段中。WaveWorkbench 不重新解析 SystemVerilog。
- 结构化事实缺失或形态不安全时跳过该输入并返回明确 warning，不退回到按总位宽扁平化。
  当前 runner 仅重建无构造端口的 interface，且拒绝 `inout/ref`、动态数组和宽度超过
  64 bit 的 leaf。

候选状态和端口角色均写入工程扩展字段，输入默认 Segment 直接进入工程 JSON，不存在隐藏的
激励生成逻辑。ZeroSlack 正式入口仍通过该显式文件契约调用独立 runner，两个进程不共享
内部数据库或可变对象。

### 保存和恢复 Stimulus Scenario v3

```powershell
wave-bridge export-stimulus module.wave.json stimulus.json `
  --scenario=<稳定 ID 或唯一名称>
wave-bridge import-stimulus module-manifest.json stimulus.json restored.wave.json
```

`stimulus.json` 是独立于 `.wave.json` 的便携契约。它只保存用户可见的仿真意图，不保存
结果波形或绝对路径：

- Module Manifest identity、schema、workspace identity 和 module/instance target；
- Scenario 稳定 ID、名称、整数 tick timebase 和 duration；
- port/leaf 的名称、方向、宽度、signed、canonical type、declaration shape、结构化 selector
  和原始顺序；
- stimulus/watch 角色、显示顺序、分组、可见性、radix 和 enum map；
- bit、bus、enum、reset 的显式连续 Segment；
- clock 的 period、phase、duty、edge、初始值及显式 override Segment；
- reset 的 active level、同步属性及由可见 Segment 表达的 assert/deassert 区间。
- Marker、当前信号、游标位置和可见时间跨度；这些视图信息仍以端口名称和 tick 表达，
  不依赖窗口像素或绝对路径。

tick 使用十进制字符串，避免 JSON number 在跨语言实现中丢失 int64 精度。正式 schema 位于
`schemas/stimulus/v3/stimulus-scenario.schema.json`；v1/v2 仍可读取和恢复。解析器还执行 schema 难以完整表达的
约束，包括端口身份唯一、显示顺序唯一、stimulus 全时段覆盖、Segment 不重叠、值符合
lane 类型，以及内容哈希 identity 校验。

恢复不重新解释 SystemVerilog，而是先用当前 Module Manifest 建立工程，再应用保存的场景：

- Manifest identity 未变化时，端口契约必须精确一致；任何缺失、类型冲突或新增端口均视为
  契约损坏并拒绝恢复。
- Manifest identity 已变化时，先匹配同名端口；缺失名称仅在方向、signed、lane kind、
  enum/type 结构形成唯一候选时迁移为重命名，候选不唯一时可用 source order 消歧，
  不做字符串相似度猜测。
- 位宽变化保留顺序、radix、分组和可见性，但丢弃不安全的旧 stimulus，使用当前
  Manifest 默认值；已删除端口报告为 missing，新增端口按当前默认值追加。
- 结构化根端口重命名时，leaf selector 与类型身份形成唯一匹配后迁移既有刺激；自动生成的
  group 使用当前 manifest 的新身份，同时保留用户设置的显示顺序和可见性。用户自建 group
  不参与该自动重写。
- 保存文件移动到其他目录后内容与 identity 不变，恢复只依赖显式传入的当前 Manifest。

仿真结果窗口把默认场景和命名场景原子保存到调用方提供的场景目录：默认场景固定为
`default.json`，命名场景使用稳定 scenario ID 的哈希文件名。结果波形、编译模型和运行
产物仍在独立缓存中，不写入场景目录。

### 导入信号清单

```powershell
wave-bridge import-signals project.wave.json signals.json output.wave.json `
  --scenario=<稳定 ID 或唯一名称>
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
输出诊断。多 Scenario 工程必须显式选择；单 Scenario 工程可省略。选择失败发生在读取
`signals.json` 和写出工程之前。命令始终写到显式输出工程，不覆盖输入文件。

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
wave-bridge pinloom-entry project.wave.json exports pinloom-entry.json `
  --scenario=<稳定 ID 或唯一名称>
```

输出包含 Project/Scenario 稳定 ID、工程路径、导出文件清单、字节数及
`pinloom://archive?...` URI。桌面端 File > Export and Open in Pinloom 使用同一格式；
只有用户在确认框选择 Open in Pinloom 后才交由操作系统打开 URI。Pinloom 不存在或未注册
URI handler 时，JSON 归档条目仍保留。多 Scenario 工程必须显式选择，单 Scenario 工程可
省略；成功输出回显最终 Scenario 名称与稳定 ID。

## Wave Workbench URI

```text
waveworkbench://open?project=<path>&scenario=<stable-id>&lane=<stable-id>&tick=<integer>
waveworkbench://compare?project=<path>&scenario=<stable-id>&lane=<stable-id>&tick=<integer>
```

`project` 必填。`scenario`、`lane`、`tick` 可选；`tick` 是工程整数 tick。`scenario`
若提供，必须唯一精确匹配稳定 ID；桌面端直接选中该波形，不重排工程中的 Scenario 数组。
显式 `scenario` 优先于桌面端为该工程记住的最后波形，并成为本次会话的新选择；因此同一 URI
在不同用户设置下仍打开相同目标。未提供 `scenario` 时，桌面端可恢复该正式工程最后一次唯一
稳定 ID 选择；记忆缺失、目标已删除或身份重复时回到第一项，不影响 URI 校验或工程内容。
正式工程还可按 Scenario 记住最后安全 lane/tick 和可见 tick 跨度；未提供 `lane`/`tick` 时，
普通打开恢复唯一可见非 Group Lane、整数光标位置和与像素无关的时间细节尺度，但不恢复 Beat、
Segment 或时间范围。URI 中有效的显式 `lane`/`tick` 在该恢复之后应用，确定性覆盖本地位置，
保留有效尺度并以显式 tick 重新锚定；若目标是已折叠 Group 的成员，桌面端先展开该 Group，
随后成为普通打开的新安全位置。已记住的 Lane 隐藏、删除
或身份重复时只恢复时间和尺度，不猜测相似名称；缺失或非法跨度不影响 URI 或工程打开。
已有正式路径的工程在选择、定位、缩放或水平视图连续变化停止 400 ms 后更新安全位置，因此
调用方不需要触发 Save 或正常关闭才能获得异常退出恢复；生命周期边界仍同步写入。连续调用只
保留最终 lane/tick/跨度，Untitled 工程和未提交快速新增 Lane 不产生持久身份。加载
`<project>.autosave` 时按 `<project>` 的正式路径查找该位置，使恢复数据和最后安全视图同时
生效，但仍不恢复范围、Segment 或可被 Delete 直接修改的目标。
当前桌面端保持纯波形工作区，`compare` 动作与 `open` 一样打开并定位波形；无界面比较由
`wave-compare` 提供。等价命令行形式：

```powershell
wave-workbench --uri="<waveworkbench URI>"
```

未知 action、缺失 project、非法 tick、不存在或重复的 scenario ID 会明确失败，不回退到
其他工程或显示名称匹配。多 Scenario 工程打开后在工具栏显示当前 `Waveform`；单 Scenario
工程不显示选择器。打开后可用 `Ctrl+PageUp` / `Ctrl+PageDown` 前后切换；该导航不改变
URI、工程数组或 Saved 状态。
