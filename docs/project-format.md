# Wave Workbench 工程格式

当前 schema 版本：`1`

## 目录

```text
<project-root>/
  project.wave.json
  traces/
  generated/
  exports/
```

`project.wave.json` 是持久化事实源。`traces/` 可存放被工程引用的外部 trace，
`generated/` 和 `exports/` 仅存放可重新生成的派生产物。工程内路径优先相对于工程
根目录；移动整个目录不应改变引用文本。

## 整数编码

所有可能超过 JavaScript 安全整数范围的值均编码为十进制字符串：

```json
{
  "timebase": {
    "picosecondsPerTick": "1"
  },
  "durationTick": "220000",
  "startTick": "80000"
}
```

加载器兼容绝对值不超过 `2^53 - 1` 的旧式 JSON number，但拒绝非整数或不能安全表示的
number。单位换算必须能够整除当前 timebase，否则返回错误，不执行取整。

## 根对象

主要字段：

- `schemaVersion`：整数，当前为 1。
- `projectId`：稳定 ID。
- `name`：显示名称。
- `timebase.picosecondsPerTick`：每个 tick 对应的皮秒数。
- `clockDomains`：clock domain 数组。
- `scenarios`：场景数组。
- `importedTraces`：外部 trace 引用数组。
- `linkedResources`：跨应用文件或稳定 ID 引用。
- `exportSettings`：导出设置对象。

未知字段在加载时按 JSON 值保留，并在再次保存时恢复。已知字段始终由当前模型覆盖，
避免扩展字段伪造核心状态。

## ClockDomain

```json
{
  "id": "clock-main",
  "name": "clk",
  "periodTick": "10000",
  "phaseTick": "0",
  "dutyNumerator": "1",
  "dutyDenominator": "2",
  "activeEdge": "rising",
  "resetRelation": "reset_n == 0"
}
```

duty cycle 使用有理数而非浮点数。分子必须大于 0 且小于分母。

## Scenario

```json
{
  "id": "scenario-handshake",
  "name": "Request / acknowledge",
  "durationTick": "220000",
  "lanes": [],
  "events": [],
  "relations": [],
  "markers": []
}
```

关系通过稳定 ID 连接。数组顺序仅用于默认显示，不作为语义主键。

## Lane 与 Segment

Lane 的 `kind` 为 `clock`、`bit`、`bus`、`enum`、`transaction`、`event` 或
`group`。主要字段包括 `id`、`name`、`width`、`signed`、`radix`、`enumMap`、
`clockDomainId`、`color`、`height`、`visible`、`groupId` 和 `segments`。

`group` lane 是稳定 ID 容器，不包含 Segment 或 Event。普通 lane 的 `groupId` 必须
引用同一 Scenario 中 `kind=group` 的 lane；group 改名不改变成员关系。桌面属性编辑
保留 lane ID，并在提交前验证现有 Segment/Event、clock domain 和 group 引用，因此
不兼容的类型或位宽变更不会部分写入工程。

删除普通 lane 时，同一 Scenario 中归属于该 lane 的 Event 及引用这些 Event 的 Relation
同步删除；仅在项目其他 Scenario 不再使用该 lane ID 时，ImportedTrace 中对应映射才会
移除。删除 group 不删除成员，而是清空成员的 `groupId`。上述变更作为同一 command
撤销或重做。

`lanes` 数组顺序是画布、Signals/Groups 导航和图形导出的显示顺序。桌面端上移/下移
命令直接调整该数组并保留所有稳定 ID 引用；保存和重新加载不会重排。SystemVerilog、
SVA 和 cocotb 的生成计划在使用前按 stable ID 确定化，因此显示顺序不改变验证行为。

Segment 使用半开区间 `[startTick, endTick)`：

```json
{
  "id": "segment-data-payload",
  "startTick": "80000",
  "endTick": "150000",
  "value": "0x35"
}
```

同一 lane 的 segment 不得重叠。相邻且值相同的 segment 在规范化时自动合并。bit 值为
`0`、`1`、`X`、`Z`；bus 值保留 radix 前缀及 X/Z pattern；enum 可使用
`enumMap` 中的符号。

Clock lane 不复制逐周期 segment，而是通过 `clockDomainId` 引用参数化时钟。其
`segments` 仅表示覆盖参数化时钟的局部半开区间：

- `gated`：区间内保持逻辑低电平。
- `disabled`：区间内驱动未知值 X。

无 segment 覆盖的时间仍由 ClockDomain 的 period、phase 和 duty 决定。旧工程中的空
Clock segment 数组保持原有语义，因此该能力不要求提升 schema 版本。保存时只写规范值
`gated` 和 `disabled`；加载及编辑接口同时接受 `gate`、`disable` 别名并立即规范化。

## ImportedTrace

完整 VCD/CSV 内容及 transition 索引不写入 JSON。工程只保存可解析引用：

```json
{
  "id": "trace-handshake-actual",
  "path": "traces/handshake_actual.vcd",
  "format": "vcd",
  "offsetTick": "0",
  "signalMapping": {
    "lane-request": "tb.dut.req"
  }
}
```

`signalMapping` 的键是 Expected lane 稳定 ID，值是解析所得 Actual signal 稳定 ID。
相对 `path` 以 `project.wave.json` 所在目录解析。索引在加载时由源文件重建；修改
`offsetTick` 不复制源文件。后台结果发布前必须匹配 project ID、trace ID 和 generation。

## Event、Relation 与 Marker

版本 1 已为后续阶段定义以下持久化字段：

- Event：`id`、`laneId`、`timeTick`、`action`、`value`、`expectedResult`、
  `clockDomainId`、可选 `cycle`、`description`、`linkedSegmentId` 和
  `waveformLinked`。
- Relation：`id`、`sourceEventId`、`targetEventId`、`minimumDelayTick`、
  `maximumDelayTick`、`clockDomainId`、`condition`、`severity`、`description`。
- Marker：`id`、`name`、`startTick`、`endTick`、`kind`、`note`。

`waveformLinked` 为 true 时，Event 与 `linkedSegmentId` 指向的 Segment 共同属于同一
Scenario 聚合；任何一侧的编辑必须通过 command 同步另一侧。

`cycle` 表示参数化 ClockDomain 上的逻辑周期索引，`timeTick` 保存该周期按当前
period、phase 和 active edge 解析后的整数时刻。修改 ClockDomain 时，引用它的 cycle
Event 与关联 Segment 边界在同一 command 中批量重定位。Clock lane 的局部覆盖只改变
信号值，不改变逻辑周期时间轴；覆盖已经影响可见边沿时，生成器按 `timeTick` 调度该
Event，避免门控导致边沿计数漂移。

### Relation condition 语义

`condition` 为空时 Relation 无条件生效。非空时使用以下确定性表达式子集：

```text
expression  := or-expression
or          := and ("||" and)*
and         := unary ("&&" unary)*
unary       := "!" unary | primary
primary     := true | false | "(" expression ")" | lane [comparison literal]
comparison  := "==" | "!=" | "===" | "!=="
```

- lane 首先按 stable ID 精确查找，未命中时按唯一 display name 精确查找。含空格、
  运算符或保留字 `true`/`false` 的显示名使用反引号包围。
- literal 可为无空格 token、单引号字符串或双引号字符串。`true` 和 `false` 作为比较值
  时分别等价于 `1` 和 `0`。
- 省略 comparison 时 lane 必须是 1-bit digital lane，语义为 `lane == 1`。
- bit、bus、enum 和 clock 按 lane width/radix/enum map 归一化后执行精确
  `0/1/X/Z` 比较；`===` 与 `==`、`!==` 与 `!=` 在此确定性子集中语义相同。
- Validation 在 source Event 的预期整数 tick 采样；relation-only Compare 在匹配到的
  Actual source transition 整数 tick 采样。Compare 的 X wildcard、ignore-X 和 bus mask
  不作用于 condition。
- false 表示该 Relation 对当前 source 不适用。语法错误、重复显示名、缺失 lane、Group
  引用、无采样值、非法 literal、Actual 映射缺失或宽度冲突均为错误，并记录表达式字节
  偏移及可用的 lane ID。
- 所有操作数都会被解析和采样；无效引用不会因 `&&`/`||` 的短路结果而被隐藏。

该字段仍保持普通 JSON 字符串，因此 schemaVersion 不变。SVA 生成器只转换可以无损
表达的无 condition Relation；不会把该运行时条件语言近似翻译为 assertion。

## 迁移

加载流程先读取 `schemaVersion`，再按相邻版本迁移到当前版本。当前支持 schema 0 到 1：

- `id` 迁移为 `projectId`。
- `timebase.baseUnitPs` 迁移为 `timebase.picosecondsPerTick`。
- `duration/start/end` 迁移为相应 `*Tick` 字段。
- `displayName` 迁移为 `name`。
- 缺失的 Project、Scenario、Lane、Segment ID 生成稳定 ID。

高于当前版本的工程会被拒绝，避免旧应用覆盖未知的新语义。

## 写入安全

保存使用 `QSaveFile`：内容先写入同目录临时文件，全部写入成功后再原子提交。禁用直接写入
回退，提交失败时不覆盖原文件。创建工程目录时同时建立 `traces/`、`generated/` 和
`exports/`。

## 自动恢复快照

桌面端在修改后 1.5 秒启动后台快照，目标为工程文件同目录的
`<project-file>.autosave`。同目录布局保证 snapshot 中的相对 trace 和 linked resource
路径保持原工程语义。快照仍使用完整 schema 和 `QSaveFile`，但只复制 `Project` 模型；
外部 VCD/CSV 数据不进入 JSON。

每次调度递增 generation。后台任务完成时，只有 generation 与当前文档一致的结果才显示
为最新状态；若编辑期间已有任务运行，则任务完成后立即调度最新快照。File > Open 可载入
`.autosave`，应用将去除后缀作为原工程保存目标并把恢复内容标记为未保存状态。
