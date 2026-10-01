# Tickx 自动化 API 与统一 CLI

## 分层

`waveautomation` 是可嵌入其他 Qt/C++ 应用的无界面静态库。它直接接收和返回
`wave::Project` 与 `QJsonObject`，复用桌面端的命令、校验和依赖清理语义。

`wave-cli` 是该库的薄命令行入口，面向 Codex、脚本和 CI。它只通过文件、
stdin、stdout、stderr 和退出码通信，不创建窗口，也不读取桌面端设置。

当前契约版本：

- 输出：`wave-workbench.cli/v1`
- 编辑批次：`wave-workbench.operations/v1`
- 能力发现：`wave-workbench.capabilities/v1`

与三个协议标识对应的 JSON Schema 使用 Draft 2020-12，并随源码保存在
`src/waveautomation/schemas/automation/v1/`：

- `capabilities.schema.json`：能力发现文档；
- `report.schema.json`：原生命令成功或失败报告的稳定顶层；
- `operation-batch.schema.json`：批次顶层及 33 个 operation 的字段定义。

Schema 内的 `$id` 以及 `capabilities.schemaRef`、`reportSchemaRef`、
`operationBatch.schemaRef` 均使用发布包中的相对路径 `schemas/automation/v1/...`。发布或嵌入时
应保持该目录结构；源码目录中的附加前缀不属于协议标识。兼容的 v1 实现可以增加报告字段和
capabilities 元数据，但不会删除或改变既有必填字段的语义。破坏性变更必须使用新的协议与
Schema 目录版本。

## 命令

```text
wave-cli capabilities [--pretty]
wave-cli new <output.wave.json> [--name=NAME] [--scenario-name=NAME] \
  [--duration=TIME] [--timebase-ps=N] [--project-id=ID] [--scenario-id=ID] \
  [--operations=FILE|-] [--dry-run] [--pretty]
wave-cli inspect <project.wave.json> [--scenario=SELECTOR] [--summary] [--pretty]
wave-cli signals <project.wave.json> [--match=TEXT] [--kind=KIND] \
  [--exact] [--limit=N] [--scenario=SELECTOR] [--pretty]
wave-cli sample <project.wave.json> --at=TIME [--scenario=SELECTOR] \
  [--clock=SELECTOR] [--lane=SELECTOR ...] [--pretty]
wave-cli window <project.wave.json> --start=TIME --end=TIME \
  [--scenario=SELECTOR] [--clock=SELECTOR] [--lane=SELECTOR ...] [--pretty]
wave-cli edges <project.wave.json> [--start=TIME] [--end=TIME] \
  [--edge=initial|rising|falling|change] [--limit=N] \
  [--scenario=SELECTOR] [--clock=SELECTOR] [--lane=SELECTOR ...] [--pretty]
wave-cli markers <project.wave.json> [--match=TEXT] [--exact] \
  [--kind=point|interval|phase|error|note] [--start=TIME] [--end=TIME] \
  [--limit=N] [--scenario=SELECTOR] [--clock=SELECTOR] [--pretty]
wave-cli relations <project.wave.json> [--match=TEXT] [--exact] \
  [--severity=information|warning|error] [--start=TIME] [--end=TIME] \
  [--limit=N] [--scenario=SELECTOR] [--clock=SELECTOR] \
  [--lane=SELECTOR ...] [--pretty]
wave-cli validate <project.wave.json> [--scenario=SELECTOR] \
  [--fail-on-warning] [--pretty]
wave-cli apply <project.wave.json> <operations.json|-> \
  (--output=PATH|--in-place|--dry-run) [--scenario=SELECTOR] \
  [--expect-sha256=HEX] [--backup[=PATH]] [--pretty]
wave-cli generate <wave-generate arguments...>
wave-cli compare <wave-compare arguments...>
wave-cli bridge <wave-bridge arguments...>
```

原生命令的成功结果写入 stdout，失败结果以输出 JSON schema 写入 stderr。除
`--help` 和 `--version` 外，调用方不需要解析自然语言文本；`capabilities` 成功时使用独立的
能力发现 schema。该结构化输出约定不适用于下述三个兼容适配命令。

`generate`、`compare` 与 `bridge` 是兼容适配命令。它们从 `wave-cli` 所在目录启动对应的
`wave-generate`、`wave-compare` 或 `wave-bridge`，原样转发参数、文本输出和退出码；旧可执行文件
继续可直接调用。能力报告的 `compatibilityCommands` 以 `compatibilityAdapter: true`、`legacyExecutable` 和
`structuredOutput: false` 明确标识这三个入口，调用方不得把它们的输出误当作
`wave-workbench.cli/v1`。这三个入口完成迁移后再进行输出协议升级，不在兼容层中静默改变旧契约。

下表为原生命令的退出码。三个兼容适配命令原样返回对应旧工具的退出码：

| 退出码 | 含义 |
|---:|---|
| 0 | 成功 |
| 2 | 命令或参数错误 |
| 3 | 输入文件、工程或 operations JSON 无法读取 |
| 4 | 校验失败、编辑被拒绝或源文件冲突 |
| 5 | 输出文件写入失败 |

## 能力发现

宿主应用或 Codex 应先执行：

```powershell
.\build\wave-cli.exe capabilities --pretty
```

该命令不需要工程文件，也不读取或写入桌面设置。结果机器列出 CLI/工程/输出/批次 schema
版本、全部命令及其读写属性、全部 operation 及类别、各选择器支持的寻址形式、精确时间格式、
可读/可创建 Lane 类型、Marker 类型、Relation 严重性、边沿分类和 dry-run、stdin、原子写入、SHA-256
保护等特性。`scenarioDuration` 另外公开 End 的延长、安全缩短、破坏性缩短授权字段和影响计数
能力；`structuralIdentityValidation` 表示 `validate` 会拒绝核心对象的空或重复稳定 ID，
`markerIntegrityValidation` 表示 `validate` 会使用与 `markers` 查询一致的规则拒绝 Marker
空/重复名称、越界/反向几何和 Point/范围冲突，
`actionableValidationReferences` 表示可修复的 Marker/Relation 校验项会直接返回快照绑定引用
和允许的 operation，无需先执行额外查询，
`structuredRelationValidation` 表示 Relation 语义问题还会返回精确对象路径、端点健康快照、
声明/实际延迟和明确的修复属性，
`structuredWaveformValidation` 表示非法 Bus/Enum Segment 与未定义区间会返回精确对象路径、
Lane/值上下文、半开修复范围及可直接调用的波形 operation，
`structuredEventValidation` 表示越界、目标 Lane 缺失或波形关联错位的 Event 会返回精确对象
路径、紧凑上下文和安全恢复选项，`eventDeletion` 表示可按唯一 Event 稳定 ID 删除损坏项并
清理依赖，`eventLinkRepair` 表示可按关联 Segment 原位恢复 Event 的 Lane、时刻和值，同时保留
Event ID、Relation 和波形，`eventCycleRepair` 表示会拒绝与 Clock 活动边沿不一致的 cycle
元数据，并可在保持当前 Event 时刻、波形和 Relation 不变的前提下清除该陈旧约束，
`eventClockRepair` 表示 Event 显式 ClockDomain 悬空时可采用目标 Lane 的唯一有效时钟，或在
Lane 没有关联时钟时清除该无效显式引用，
`laneClockRepair` 表示 Lane 的 ClockDomain 引用会被独立校验；普通信号可清除缺失引用，
Clock Lane 仅在工程恰有一个 ClockDomain 时自动关联，多时钟或歧义引用不会被猜测，
`laneGroupRepair` 表示 Lane 的分组引用会被独立校验；缺失、指向非 Group、嵌套、自引用或
目标歧义时可直接解除分组，不会猜测替代 Group，
`relationClockRepair` 表示 Relation 的非空 ClockDomain 引用会被独立校验；仅在两端能够
唯一证明共同有效时钟，或两端均无时钟时自动恢复，冲突与歧义不会被猜测，
`traceIdentityValidation` 与 `traceIdentityRepair` 表示 Imported Trace 的空/重复稳定 ID
可通过快照引用精确诊断和修复，`traceReferenceValidation` 与 `traceReferenceRepair` 表示
空源路径或非 VCD/FST/CSV 格式可在读取外部文件前显式诊断和修复，`traceMappingValidation` 与
`traceMappingRepair` 表示本地
可证明的无效 Lane 映射可被诊断并精确删除，
`nonRegressiveApplyValidation` 表示 `apply` 会比较编辑前后的 error 指纹，只接受有效结果或
不引入新错误的渐进式修复，
`relationReadyEdgeQuery` 表示当前二进制支持紧凑 Relation 端点查询，
`relationEndpointRepair` 表示损坏端点可通过显式 Lane/时刻原子替换，
`relationRepairReference` 表示空或重复 Relation ID 可通过快照绑定引用精确恢复，
`compactMarkerQuery` 表示可按身份、用途和时间窗口定位既有 Marker 并读取其健康状态，
`markerRepairReference` 表示可用快照绑定引用恢复无法由稳定 ID 寻址的 Marker，
`compactRelationQuery` 表示可在不读取内部 Event ID 的情况下定位和审计既有 Relation。
调用方可据此协商当前二进制能力，不需要从 `--help` 文本或源码猜测。

每个 `operations[]` 项还提供：

- `schemaRef`：指向 `operation-batch.schema.json#/$defs/<op>`；
- `required`：无条件必填字段，始终包含 `op`；
- `optional`：该 operation 接受的其余字段；条件必填关系由所引用 Schema 的说明和运行时校验
  共同约束；
- `idempotent`：相同目标状态重复提交是否保持同一工程结果；
- `dangerous`：该操作是否可能删除工程内容或结构；
- `mutatesProject`：操作成功时是否可能改变工程，`assert-value` 为 `false`。

调用方应使用这些字段生成操作表单、过滤只读调用和在 `dangerous=true` 时要求显式确认，不能
根据 operation 名称自行推断风险。字段列表用于发现，最终接受条件仍以对应 JSON Schema 和
原子批次校验结果为准。

未知参数以退出码 2 和结构化 `usage` 错误返回。公共 Qt/C++ API
`describeAutomationCapabilities()` 返回同一文档，因此嵌入式宿主与独立 CLI 不会维护两份
能力清单。

## 从零创建工程

`new` 创建一个包含单个空白 Scenario 的可编辑工程，不需要先启动桌面端或手写工程 JSON。
默认 timebase 为 1 ps/tick、End 为 200 ns、Scenario 名为 `Waveform`；省略工程名时从输出文件名
推导。输出路径缺少 `.wave.json` 时会自动补全，已存在的路径始终拒绝覆盖。

```powershell
.\build\wave-cli.exe new .\stimulus `
  "--name=Handshake stimulus" "--duration=80 ns" `
  --operations=.\tests\fixtures\cli-new-operations.json --pretty
```

`--operations=FILE` 可在首次写盘前完成信号创建和波形生成；`--operations=-` 从 stdin 读取同一
批次格式。任一 operation 失败时不生成半成品。`--dry-run` 返回完全相同的稳定 ID、
`resultSha256`、校验结果和变更摘要，但不创建文件。自动工程 ID 由工程名与 timebase 确定，
Scenario ID 再由工程 ID、Scenario 名与 End 确定；跨系统需要固定身份时使用 `--project-id`
与 `--scenario-id`。

## 读取与校验

`inspect` 输出工程、timebase、ClockDomain、Scenario、Lane、Segment、Event、
Relation、Marker、导入 trace 和链接资源。`--scenario` 只过滤返回的 Scenario，
不会删除工程级上下文。稳定 ID、源文件绝对路径与 `sourceSha256` 可直接用于后续编辑。
完整输出还公开每个 Imported Trace 的 `traceIndex`、快照绑定 `traceRef` 和
`signalMapping`；`--summary` 只保留映射计数，不复制映射表。
只需要定位信号时使用 `--summary`：它保留工程、时钟、Scenario 与 Lane 的稳定 ID、类型、
宽度和计数，但省略 Segment、Event、Relation、Marker 与扩展载荷，避免自动化调用读取整份
波形。

所有已有对象的 `SELECTOR` 都接受稳定 ID 或大小写不敏感的完整名称。稳定 ID 精确匹配优先；
名称必须唯一，重名时命令明确拒绝并要求改用稳定 ID，不会猜测。成功结果始终返回规范稳定 ID，
因此交互调用可直接写 `req`、`clk`，后续程序仍可使用 `lane-request`、`clock-main` 串联。
Scenario 选择还要求最终稳定 ID 非空且唯一。工程只有一个 Scenario 时可以省略
`--scenario`；工程包含多个 Scenario 时省略即拒绝，不会默认采用数组首项。同一规则也用于
`wave-generate`、`wave-compare`、`wave-bridge import-signals` 和
`wave-bridge pinloom-entry`；其中 Generate 的旧位置 Scenario ID 仅作为兼容别名保留。

`signals` 是面向定位的最小查询。`--match` 对信号名称或稳定 ID 做大小写不敏感的包含匹配，
`--exact` 改为完整匹配且必须与 `--match` 同时使用；`--kind` 接受 Lane 类型，`--limit`
限制 1–1000 条，默认为 50。结果只返回名称、身份、类型、位宽、显示属性、关联时钟/组及
`segmentCount`；Enum 还返回声明映射，但不返回 Segment 或其他波形正文。`matchCount`、
`returnedCount` 和 `truncated` 明确说明是否仍有未返回结果。

所有可能超过 JSON 安全整数范围的 tick、周期、相位、延迟和计数字段均以十进制字符串
输出。扩展字段会还原为原始 JSON 值，不会暴露内部编码字符串。

`sample` 返回指定时刻的信号值，不返回工程正文。重复 `--lane` 可限制信号；未指定时返回
全部非 Group 信号。Clock 返回当时的 `0`/`1`/`X`，Bit 空白返回隐式 `0`，Bus/Enum 空白
返回隐式 `X`。结果同时给出解析后的 `atTick`、格式化时间、Segment ID 和显式/隐式标志。

`window` 只返回 `[start, end)` 内与所选 Lane 相交的 Segment，并给出窗口首尾值、裁切边界、
关联 ClockDomain、局部 Event、相关 Relation 端点和相交 Marker。Relation 的另一端即使不在
所选 Lane 中，也以 `laneSelected=false` 的上下文 Event 返回，因此调用方不需要再读取完整
工程才能判断一次修改会影响哪些依赖。

`edges` 是建立或修改 Relation 前的最小查询。它只返回非 Clock、非 Group 信号上实际存在的
waveform-linked Event，并按精确时刻、Lane 顺序稳定排序；不返回内部 Event ID 或 Segment ID。
重复 `--lane` 可按稳定 ID 或唯一名称缩小范围，`--start`/`--end` 可独立省略，默认覆盖
`[0, Scenario End)`。`--edge` 支持：

- `initial`：tick 0 的初始波形端点；
- `rising`：Bit 从 `0` 到 `1`；
- `falling`：Bit 从 `1` 到 `0`；
- `change`：Bus、Enum 等其他变化，以及 Bit 的非二态变化。

每项结果给出 Lane 身份/名称/类型/位宽、精确 tick 与格式化时间、前值、当前值、Event 动作、
有效 ClockDomain、说明、`candidateCount`、`eventIdCount` 和 `relationEndpoint`。
`relationEndpoint=true` 表示该 Lane/时刻恰好对应一个波形 Event，且该 Event 具有唯一非空
稳定 ID，可直接写入 `sourceLaneId` + `sourceAtTick` 或
`targetLaneId` + `targetAtTick`。同一 Lane/时刻存在多个候选时不会猜测：
`candidateCount > 1`、Event ID 重复/缺失时均为 `relationEndpoint=false`，顶层
`ambiguousEndpointCount` 同步计数。
`--limit` 为 1–10000，默认 200；`matchCount`、`returnedCount` 与 `truncated` 区分总命中和
实际返回数量。显式选择 Clock 或 Group 会结构化拒绝，因为它们不能作为当前 Relation 端点。

`markers` 是维护既有 Marker 的紧凑查询。它按起点、终点和 Marker ID 稳定排序，直接返回
`markerId`、名称、用途类型、备注、精确起止、持续宽度及格式化时间，不返回扩展载荷或完整
Scenario。`--match` 对稳定 ID、名称和备注做大小写不敏感的包含匹配；`--exact` 改为完整匹配
且必须与 `--match` 同时使用。`--kind` 接受 `point`、`interval`、`phase`、`error`、`note`，
`--limit` 为 1–1000，默认 100。

`--start`/`--end` 可独立省略，范围使用 `[start, end)`：点 Marker 的位置必须落在范围内，
区间 Marker 只需与范围相交。两端接受 tick、精确物理时间和 `cycle N`；`--clock` 可显式指定
周期时钟，工程只有一个 ClockDomain 时自动推断并在结果中返回规范 `clockId`。完整查询不应用
时间裁剪，因此越界或反向的损坏 Marker 仍可被定位和清理。

每项结果的 `idCount`、`nameCount`、`addressable`、`valid` 和 `issues` 直接说明是否可由稳定 ID
安全寻址，以及是否存在空身份、重复 ID/名称、越界/反向几何或 Point/范围冲突。`markerRef`
是与当前 Scenario 顺序及 Marker 完整快照绑定的不透明短期引用；即使稳定 ID 为空或重复，也能
精确引用该项进行修复或删除。Marker 内容或其前方索引变化后旧引用立即失效，不会落到其他项。顶层
`validCount`、`addressableCount` 与 `issueCount` 对全部命中项计数，即使结果受 `limit` 截断也
不会把异常静默隐藏。

`relations` 是维护既有 Relation 的紧凑查询。它按最早端点时刻和 Relation ID 稳定排序，
直接返回 `relationId`、`relationRef`、身份诊断、源/目标 Lane 与精确时刻、前后值、边沿类型、
Event 动作、声明延迟范围、实际端点延迟、时钟、条件、严重性和说明；不返回
`sourceEventId`、`targetEventId`、扩展载荷或 linked Segment ID。`observedDelayTick` 是目标
时刻减源时刻，`timingWithinRange` 表示它是否位于声明的最小/最大延迟内。

重复 `--lane` 筛选任一端位于指定 Lane 的 Relation；`--start`/`--end` 筛选任一端落在半开区间
内的 Relation，并支持与 `edges` 相同的精确时间格式。`--match` 对 Relation ID、condition 和
description 做大小写不敏感的包含匹配；`--exact` 改为完整匹配且必须与 `--match` 同时使用。
`--severity` 接受 `information`、`warning`、`error`，`--limit` 为 1–1000，默认 100。

每个端点给出 `resolved`、`eventIdCount`、`candidateCount`、`relationEndpoint` 和 `issue`。
正常端点的 `issue=null`；缺失 Event/Lane、空或重复 Event ID、脱离波形的 Event、越界时刻和歧义
waveform edge 均返回稳定问题代码。两个端点都可按 Lane/时刻重新寻址时
`endpointsReady=true`。每项的 `idCount`、`addressable` 与 `identityIssues` 说明稳定 ID 是否
为空或重复；`relationRef` 与当前 Scenario 顺序和 Relation 完整快照绑定，即使 ID 不可寻址也能
精确修复或删除该项。损坏的一端必须同时显式提供 Lane 与精确时刻，健康且未指定的另一端保持
不变；缺少任一字段、替换边沿歧义或 Event ID 重复时原子拒绝，不猜测旧位置。顶层
`readyCount`、`addressableCount`、`identityIssueCount` 与 `endpointIssueCount` 对全部命中项
计数，即使返回结果被 `limit` 截断也不会隐藏异常。

```powershell
.\build\wave-cli.exe inspect .\project.wave.json --summary --pretty
.\build\wave-cli.exe signals .\project.wave.json `
  "--match=req" --kind=bit --exact --pretty
.\build\wave-cli.exe sample .\project.wave.json "--at=80 ns" `
  --lane=req "--lane=data[7:0]" --pretty
.\build\wave-cli.exe window .\project.wave.json `
  "--start=70 ns" "--end=130 ns" `
  --lane=req "--lane=data[7:0]" --pretty
.\build\wave-cli.exe edges .\project.wave.json `
  "--start=70 ns" "--end=130 ns" --edge=rising `
  --lane=req --lane=ack --pretty
.\build\wave-cli.exe markers .\project.wave.json `
  "--match=request" --kind=phase `
  "--start=70 ns" "--end=130 ns" --pretty
.\build\wave-cli.exe relations .\project.wave.json `
  "--match=within" --severity=error `
  "--start=70 ns" "--end=130 ns" --lane=req --pretty
.\build\wave-cli.exe sample .\project.wave.json "--at=cycle 8" `
  --clock=clk --lane=req
```

`validate` 输出机器可读的 `issues` 和 information/warning/error 汇总。除波形值、Relation
端点、时序和 condition 语义外，它还审计 Project、ClockDomain、Scenario、Lane、Segment、
Event、Relation 与 Marker 八类核心对象的稳定 ID。空 ID 返回 `missing-stable-id`，同一作用域
重复返回 `duplicate-stable-id`；每项包含 `objectKind`、数组索引 `path`、`stableId`、
`idCount` 以及可用的 `scenarioIndex`、`laneIndex` 和对象索引，调用方不必从错误文本反推位置。

`identitySummary` 返回 `valid`、问题数、受影响对象数、空 ID 数及重复 ID 对象数；
Marker 还使用与 `markers` 查询相同的名称和几何规则。空名称、大小写不敏感的重复名称、
起点早于 Scenario、终点早于起点、终点晚于 Scenario，以及 Point 的起止不一致分别返回
`empty-name`、`duplicate-name`、`start-before-scenario`、`end-before-start`、
`end-after-scenario` 和 `point-has-range`。每项包含 `objectKind=marker`、精确属性 `path`、
`markerId`、名称计数、起止 tick 和用途类型。

`markerSummary` 返回 `valid`、问题数、受影响 Marker 数、名称问题数和几何问题数；
`markerIssueCount` 可与 `markers.issueCount` 直接核对。`identityIssueCount` 与
`semanticIssueCount` 区分结构和全部语义问题，Marker 完整性问题同时计入 semantic issue 和
error，因此不会再让查询已判定损坏的工程得到 `valid=true`。`valid` 表示没有 error，
`accepted` 表示同时满足本次 warning 策略。存在 error 时返回退出码 4；指定
`--fail-on-warning` 后，warning 或加载迁移警告也会使 `accepted=false` 并返回退出码 4。
查询和校验保持只读，`sourceSha256` 可用于后续修复批次保护。

值语义错误不会在工程读取阶段抢先终止：例如超出位宽的 Bus 值或不存在的 Enum 符号会保留原文，
由 `validate` 返回 `invalid-bus-value`。负时间、空区间和 Segment 重叠仍属于结构损坏，继续在
加载边界拒绝。

对于可由现有 operation 恢复的问题，`validate.issues[]` 直接包含 `markerRef` 或
`relationRef`，以及对应的 `repairOperations`。Marker 名称/几何问题和 Marker/Relation
空或重复稳定 ID 均按精确数组索引生成引用；稳定 ID 唯一的 Relation 端点、时序或 condition
问题也会直接给出 `relationRef`。`relation-satisfied` 和 `relation-not-applicable` 只是状态
信息，不会伪装成修复入口。顶层 `repairableIssueCount` 统计具有直接修复入口的问题项，
`repairTargetCount` 按引用或 Lane+半开范围去重；一个 Marker 同时有三个几何问题时分别保留
三个诊断，但只计为一个修复目标。引用仍绑定本次校验时的完整快照，工程变化后会原子失效。

Relation 身份或语义问题还包含 `objectKind=relation`、Scenario/Relation 索引、首要 `path`、
全部相关 `paths`、`repairProperties` 和 `relationContext`。`relationContext` 与
`relations` 查询中的单项使用同一构造：包括源/目标端 `resolved`、Lane、精确时间、边沿、值、
候选数和问题码，声明最小/最大延迟、实际延迟、时序是否满足、时钟、condition、严重性和
`endpointsReady`；不暴露内部 Event ID 或扩展载荷。

`repairProperties` 使用 `stable-id`、`source-endpoint`、`target-endpoint`、`delay-range`、
`clock`、`relation-clock`、`condition`、`scenario-duration` 或兜底 `relation`。端点属性对应
`update-relation` 的 Lane+精确时刻字段；`paths` 保留底层受影响属性。延迟违例同时列出最小/
最大延迟路径，观察窗口不足同时列出 Scenario End 与最大延迟路径，调用方不再根据英文 message
猜测应修改哪一侧。

非法 Bus/Enum Segment 包含 `objectKind=segment`、Scenario/Lane/Segment 索引、精确
`.value` 路径，以及紧凑 `waveformContext`。上下文给出 Lane 名称、类型、位宽、signedness、
radix、Segment ID、原值和格式化起止时间，不复制整条波形、Enum 映射或扩展载荷。
`repairRange` 直接提供 `laneId`、`startTick`、`endTick`，`repairOperations` 为
`set-range`/`clear-range`。

未定义区间包含 `objectKind=lane`、`.segments` 路径和同样的 `waveformContext`/
`repairRange`，其中 `undefined=true`，修复操作为 `set-range`。调用方可把范围字段原样写入
operation，不需要先执行 `sample` 或 `window` 推断空洞终点。

目标 Lane 缺失或位于 Scenario 半开区间 `[0, End)` 之外的 Event 包含
`objectKind=event`、Scenario/Event 索引、精确 `.laneId`/`.timeTick` 路径和
`eventContext`。上下文公开稳定 ID、Lane 是否解析、格式化时刻、动作、值、ClockDomain、
是否关联波形、关联 Segment 是否位于当前 Lane、唯一 Segment 所属 Lane/索引/范围/值以及
`linkConsistent`/`linkRepairable`，但不公开易失的 Segment ID 或扩展载荷。Event 的 Lane、
时刻或值与唯一关联 Segment 不一致时另返回 `event-waveform-mismatch` 和全部差异属性路径。
可安全解析且没有第二个 Event 争用同一 Segment 时，`repair-event-link` 原位恢复三个字段；
Lane 缺失和负时间仍可用 `delete-event`。能够以更大 End 表示的正向越界还给出
`set-duration` 与 `minimumDurationTick`。最大 tick 不会产生溢出的延长建议。Event ID 不唯一
或 Segment 归属不唯一时不暴露关联修复入口。

带 `cycle` 的 Event 还必须严格位于其显式 ClockDomain、或目标 Lane 推断 ClockDomain 的活动
边沿。负 cycle、缺少唯一时钟、计算溢出或 `timeTick` 与周期边沿不一致时返回
`event-cycle-mismatch`。`eventContext` 同时给出 cycle、时钟来源、有效 ClockDomain、匹配数、
预期 tick/格式化时间以及当前一致性；精确路径至少包含 `.cycle`，与实际时刻冲突时还包含
`.timeTick`。该问题只提供 `clear-event-cycle`，不会把当前可见波形移动到可能陈旧的周期位置，
也不会把删除 Event 作为默认恢复方式。

Event 的非空 `clockDomainId` 必须唯一解析到工程 ClockDomain；缺失或歧义时返回
`event-clock-domain-invalid` 和精确 `.clockDomainId` 路径。`eventContext` 公开当前匹配数、
Lane 的 ClockDomain 及匹配数、`use-lane-clock`/`clear-event-clock` 恢复动作和替换值。
仅当当前引用确实缺失，且 Lane 为空、Lane 不存在或 Lane 时钟唯一有效时，才提供
`repair-event-clock`；当前引用歧义或 Lane 自身也悬空时只诊断，不声称可安全恢复。该操作保持
Event 时刻、值、波形关联和 Relation 不变，并按替换后的时钟保留仍正确的 cycle、清除陈旧 cycle。

Lane 的 ClockDomain 引用也必须符合 Lane 类型约束：Clock Lane 必须唯一解析到工程
ClockDomain，普通 Lane 的非空引用必须唯一有效，Group 不得携带 ClockDomain。违反时返回
`lane-clock-domain-invalid`、精确 `.clockDomainId` 路径和 `laneContext`；上下文包含 Lane
身份/类型、当前匹配数、工程时钟数、`clear-lane-clock`/`use-only-project-clock` 动作、替换值，
以及继承该 Lane 时钟的 cycle Event 将保留或清理的数量。普通 Lane 的缺失引用可安全清空；
Clock Lane 只在工程恰有一个非空稳定 ID 的 ClockDomain 时采用它。多个候选 ClockDomain、
当前引用歧义或 Lane ID 歧义只诊断，不提供 `repair-lane-clock`。修复保持 Lane 波形、Event、
Relation 与 Segment 身份不变；若清除引用使继承时钟的 cycle 无法继续成立，仅清除对应 cycle
元数据并在报告中给出数量。

Lane 的非空 `groupId` 必须唯一解析到同一 Scenario 中另一条 Group Lane；Group 不允许嵌套，
普通 Lane 也不能指向自身或非 Group Lane。违反时返回
`lane-group-reference-invalid`、精确 `.groupId` 路径和 `laneContext`。上下文公开当前引用、
ID 匹配数、其中 Group Lane 的数量、唯一目标的索引/名称/类型、自引用状态，以及明确的
`clear-lane-group` 动作。由于分组只控制组织和显示、不改变波形或时序事实，目标缺失、类型错误
或歧义时均可通过 `repair-lane-group` 清空无效引用；不会按名称或数组顺序选择替代 Group。
待修 Lane ID 自身不唯一时仍只诊断，操作原子拒绝。

Relation 的非空 `clockDomainId` 必须唯一解析到工程 ClockDomain；缺失或歧义时返回
`relation-clock-domain-invalid`、精确 `.clockDomainId` 路径、现有 `relationContext` 和
`relationClockContext`。后者公开当前匹配数、两端 Event/Lane 匹配数、有效 ClockDomain 与
匹配数、端点时钟上下文是否可判定、是否冲突、恢复动作及替换值。仅当当前引用确实缺失、两端
上下文均可判定且不冲突时提供 `repair-relation-clock`：两端存在一个共同有效时钟时采用它，
两端均无时钟时清空无效引用。当前引用歧义、任一端上游时钟损坏或两端时钟冲突时只诊断；
调用方仍可通过 `update-relation` 显式指定意图。自动恢复只修改 Relation ClockDomain，不改变
端点、延迟、condition、严重性、描述、Event、Lane、Segment 或波形。

Imported Trace 的 `id` 必须非空且在 `importedTraces` 数组内唯一。违反时按工程级结构身份
错误返回 `missing-stable-id` 或 `duplicate-stable-id`、精确 `importedTraces[N].id` 路径、
Trace 索引、ID 匹配数和与完整快照绑定的 `traceRef`。`repair-trace-identity` 只接受该
`traceRef`，避免重复或空 ID 导致选错对象；省略 `newId` 时按快照确定性生成未占用的
`trace-auto-*`，也可显式提供唯一非空 ID。操作只更新 ID，保留路径、格式、偏移、映射和扩展
载荷；健康身份、冲突新 ID 和已变化快照均原子拒绝。

Imported Trace 的 `path` 去除首尾空白后必须非空，`format` 必须大小写不敏感地等于 `vcd`
或 `csv`。违反时每条 Trace 只返回一项 `trace-reference-invalid`，但 `paths`、`problems`
和 `repairProperties` 会同时列出所有无效属性；`traceReferenceContext` 公开规范化格式、
支持格式及 `filesystemVerification=not-performed`。该校验只判断工程内引用结构，不检查文件
是否存在，也不根据扩展名猜测格式。

`repair-trace-reference` 必须使用问题返回的 `traceRef`，并显式提供 `path`、`format` 中至少
一项；省略的属性沿用原值，最终引用必须同时满足路径和格式约束，否则整批拒绝。显式格式会规范
为小写。操作只更新目标 Trace 的路径/格式，保留 ID、偏移、映射和扩展载荷；健康引用与已变化
快照均拒绝。

Imported Trace 的 `signalMapping` 键必须指向工程任一 Scenario 中存在的非 Group Lane，值必须
是非空 Actual signal ID。违反时返回 `trace-mapping-invalid`、精确映射路径、
`traceContext` 和 `traceMappingContext`；上下文公开 Lane 匹配数、Actual ID 是否存在、问题码、
修复动作，以及外部信号尚未校验这一边界。`repair-trace-mapping` 只删除已证明无效的映射项，
不读取或改写 VCD/FST/CSV，也不猜测替代信号。可用唯一 `traceId` 寻址；Trace ID 重复时使用
`validate` 返回的快照绑定 `traceRef`。快照变化后旧引用原子失效，同一 Trace 的多项修复应在
首项后使用唯一 `traceId`，或重新 `validate` 获取新引用。

## 原子编辑

编辑文件格式：

```json
{
  "schema": "wave-workbench.operations/v1",
  "expectedProjectId": "project-wave-workbench-demo",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "add-signal",
      "id": "lane-cli-data",
      "name": "cli_data",
      "kind": "bus",
      "width": 8,
      "clockDomainId": "clock-main"
    },
    {
      "op": "set-range",
      "laneId": "lane-cli-data",
      "startTick": "0",
      "endTick": "10000",
      "value": "0xa5"
    }
  ]
}
```

批次先应用到工程副本。任一 operation 的字段、目标、值或模型约束无效时，整个批次
失败且不产生输出。成功结果包含每项 operation 的 `changed`、结果工程 SHA-256、编辑后校验
报告和 `changes` 摘要。摘要直接列出增删改移的 Lane/Clock ID 以及 Scenario 的时长、
Segment、Event、Relation 和 Marker 计数变化，调用方不必再次读取整份工程才能确认结果。

全部 operation 完成后，嵌入 API 和 CLI 会对整个待写入工程共同执行非退化后置校验，而不只
检查本次选择的 Scenario。健康源工程只能产生
`validation.valid=true` 的候选；损坏源工程可分步恢复，但候选 error 必须不增加、不能出现
新的 error 指纹、身份问题不能增加，并且本步必须消除至少一项 error 或由 operation 明确报告
身份、几何或 Relation 端点修复。无关编辑若原样保留既有 error，会以
`post-validation-failed`、退出码 4 原子拒绝，不返回候选工程，也不写输出文件。
Marker 名称与几何问题使用同一门禁；普通 `update-marker` 只要确实减少问题且不引入新 error，
即使仍有其他 Marker 待修复，也可作为 `progressive-repair` 保存。

`validationGuard.scope=project` 明确这一边界，并公开 `sourceValid`、`candidateValid`、`sourceErrorCount`、
`candidateErrorCount`、`newErrorCount`、`resolvedErrorCount`、`noNewErrors`、
`repairOperationReported`、`progressiveRepair` 和 `reason`。`reason=valid-candidate` 表示
本次结果已无 error；`reason=progressive-repair` 表示结果可以安全保存为恢复进度，但仍须继续
修复，调用方不能把顶层 `ok=true` 等同于最终 `validation.valid=true`。

批次顶层 `scenarioId` 和引用已有对象的 `laneId`、`laneIds`、`sourceLaneId`、
`targetLaneId`、`beforeLaneId`、`afterLaneId`、`clockId`、`clockDomainId`、`groupId`
字段同样接受稳定 ID 或唯一的大小写不敏感名称。字段名为 `*Id` 是为了保持 v1 schema 兼容，
不表示调用方必须先查询 ID。名称会在每项 operation 执行前解析，因此同一批次中新建的信号
也可由后续 operation 按唯一名称引用。operation 报告只返回规范稳定 ID。歧义、缺失或重名
目标会拒绝整个批次，不留下部分结果。`markerId` 同样接受 Marker 稳定 ID 或唯一的大小写
不敏感名称，但解析后的稳定 ID 也必须唯一；身份损坏时改用 `validate` 问题或 `markers`
结果返回的 `markerRef`。
`relationId` 只接受唯一非空 Relation 稳定 ID；身份损坏时改用 `relations` 返回的
`relationRef`，也可直接使用 `validate` 问题中的引用。
`traceId` 只接受唯一 Imported Trace 稳定 ID；重复时改用 `validate` 或完整 `inspect`
返回的 `traceRef`。`repair-trace-identity` 与 `repair-trace-reference` 必须使用快照引用；
`repair-trace-mapping` 还需要精确 `laneId` 映射键。

当前 operation：

| `op` | 必填字段 | 可选字段 | 语义 |
|---|---|---|---|
| `add-group` | `name` | `id`, `color`, `insertionIndex` | 新增可由后续信号引用的 Group |
| `update-group` | `groupId`，至少一个属性 | `name`, `color`, `height`, `visible` | 修改 Group；无效果修改不产生变更 |
| `move-group` | `groupId`，以及一个位置字段 | 无 | 按索引或相邻 Lane 重排 Group |
| `delete-group` | `groupId` | 无 | 删除 Group，保留成员信号并解除其归组 |
| `add-signal` | `name`, `kind` | `id`, `color`, `groupId`, `insertionIndex` 及类型参数 | 新增 `clock`、`bit`、`bus` 或 `enum` |
| `duplicate-signal` | `laneId` | `id`, `name`, `color`, `groupId`, `newClockId` 及一个位置字段 | 原子复制完整信号与波形；Clock 获得独立时钟域 |
| `assert-value` | `laneId`, `value`，以及一个时刻 | `clockId` | 断言当前值；不匹配则拒绝整个批次 |
| `set-range` | 单个 `laneId`/`value` 或 `assignments`，以及一组起止时间 | `clockId` | 原子设置一个或多个 Lane 的半开区间 |
| `set-sequence` | 单个 `laneId`/`values` 或 `sequences`，以及 `start` 与一个步长 | `clockId`, `repeat` | 按共享步长写入一个或多个 Lane 的值序列 |
| `clear-range` | `laneId` 或 `laneIds`，以及一组起止时间 | `clockId` | 清除一个或多个信号的半开区间 |
| `transfer-range` | `mode`、Lane 目标、一组源起止时间和目标时刻 | `clockId`, `overwrite` | 原子复制或移动单/多 Lane 时间范围 |
| `rename-lane` | `laneId`, `name` | 无 | 按稳定 ID 重命名，名称保持唯一 |
| `set-duration` | `durationTick` 或 `duration` | `clockId`, `truncate` | 延长或缩短 Scenario End；会丢失内容的缩短必须显式确认 |
| `repair-event-link` | `eventId` | 无 | 仅修复 `validate` 明确标为可恢复的 Event 波形关联；保留 Event ID、Relation 与 Segment |
| `clear-event-cycle` | `eventId` | 无 | 仅清除 `validate` 明确标为不一致的 Event cycle；保持当前时刻、ClockDomain、波形与 Relation |
| `repair-event-clock` | `eventId` | 无 | 仅修复 `validate` 明确标为可恢复的 Event ClockDomain 悬空引用；优先采用 Lane 的唯一有效时钟 |
| `repair-relation-clock` | `relationId` | 无 | 仅修复 `validate` 明确标为可恢复的 Relation ClockDomain 悬空引用；采用两端唯一共同时钟或清除无时钟引用 |
| `repair-trace-identity` | `traceRef` | `newId` | 修复空或重复 Imported Trace ID；默认确定性生成唯一 ID，只修改所选快照的身份 |
| `repair-trace-reference` | `traceRef`，以及 `path`/`format` 至少一项 | 无 | 修复空 Trace 路径或未知格式；最终引用必须完整有效，不检查文件系统或猜测格式 |
| `repair-trace-mapping` | `traceId` 或 `traceRef`，以及 `laneId` | 无 | 删除 `validate` 已证明无法指向 Lane 或 Actual ID 为空的单条 Trace 映射；不猜测外部信号 |
| `repair-lane-clock` | `laneId` | 无 | 仅修复 `validate` 明确标为可恢复的 Lane ClockDomain 引用；普通 Lane 清除缺失引用，Clock Lane 只采用工程唯一时钟 |
| `repair-lane-group` | `laneId` | 无 | 仅清除 `validate` 明确标为无效的 Lane Group 引用；不选择或创建替代 Group |
| `delete-event` | `eventId` | 无 | 仅删除 `validate` 明确标为可删除且稳定 ID 唯一的 Event，并同步清理关联波形与 Relation |
| `delete-signal` | `laneId` | 无 | 删除信号并执行 Event、Relation 与 trace mapping 依赖清理 |
| `move-signal` | `laneId`，以及一个位置字段 | 无 | 按索引或相邻稳定 ID 重排信号 |
| `update-signal` | `laneId`，至少一个属性 | 见下文 | 修改信号常用属性 |
| `update-clock` | `clockId`，至少一个属性 | 见下文 | 修改时钟域并重定时周期事件 |
| `add-relation` | 源/目标 Lane 与边沿时刻、最小/最大延迟 | `id`, `clockId`, `condition`, `severity`, `description` | 在两个现有波形边沿之间新增时序关系 |
| `update-relation` | `relationId` 或 `relationRef`，至少一个属性 | `newId`、端点、延迟、时钟及描述属性 | 修改现有时序关系；快照引用可修复空/重复身份，损坏端点须以 Lane+时刻完整替换 |
| `delete-relation` | `relationId` 或 `relationRef` | 无 | 按唯一稳定 ID 或快照引用精确删除一项 |
| `add-marker` | `name`，以及点时刻或区间 | `id`, `clockId`, `kind`, `note` | 新增唯一命名的点或区间标注 |
| `update-marker` | `markerId` 或 `markerRef`，至少一个属性 | `newId`、名称、点/区间、`clockId`, `kind`, `note` | 修改标注；快照引用可修复空/重复身份或损坏几何 |
| `delete-marker` | `markerId` 或 `markerRef` | 无 | 按稳定身份或快照引用精确删除一项 |

范围起止时间必须分别二选一：`startTick` 或 `start`，以及 `endTick` 或 `end`。位置字段必须
三选一：`destinationIndex`、`beforeLaneId`、`afterLaneId`。

`set-duration` 可直接延长 End，也可在目标之后没有内容时直接缩短。若缩短会碰到现有内容，
默认以退出码 4 拒绝，并在错误中列出将裁剪或删除的 Segment、Event、Relation 与 Marker 数量；
调用方确认该影响后必须显式设置 `"truncate": true`。确认截断时：

- 与新 End 相交的 Segment 和区间 Marker 保留原 ID、起点和值，只把终点裁到新 End；
- 起点位于新 End 或之后的 Segment、区间 Marker，以及新 End 之后的点 Marker 被删除；
- 位于新 End 或之后的 Event 被删除，引用这些 Event 的 Relation 同步删除；
- 新 End 上的点 Marker 保留，仍在新 End 之前的 Relation 与内容保持不变。

成功报告返回 `direction`、`truncateRequested`、`contentTruncated` 以及六项裁剪/删除计数。
`--dry-run` 可先取得相同影响计数与结果 SHA 而不生成文件。以下操作把 220 ns 场景确认截到
120 ns：

```json
{
  "op": "set-duration",
  "duration": "120 ns",
  "truncate": true
}
```

省略 `truncate` 只允许无内容损失的缩短，避免宿主或 Codex 因误算 End 静默丢失波形。
同批次前面已经完成的编辑也会在拒绝时整体回滚。

`set-range.assignments` 是非空数组，每项包含 `laneId` 和 `value`；所有值先验证，再由一个
领域命令提交。`assert-value` 使用 `atTick` 或 `at`，适合在修改前声明“此处仍应为 0/1/总线值”
之类的内容前置条件，也可放在写入后作为同批次后置条件。断言失败时，位于它之前的 operation
也不会留下部分结果。

`set-sequence` 可使用单 Lane 形式：

```json
{
  "op": "set-sequence",
  "laneId": "lane-request",
  "start": "cycle 0",
  "stepCycles": 1,
  "values": ["0", "1", "1", "0"],
  "repeat": 2
}
```

或使用共享起点和步长的多 Lane 形式：

```json
{
  "op": "set-sequence",
  "start": "cycle 0",
  "stepCycles": 1,
  "sequences": [
    {"laneId": "lane-request", "values": ["0", "1", "1", "0"]},
    {"laneId": "lane-data", "values": ["0x00", "0x12", "0x34", "0x00"]}
  ]
}
```

步长必须三选一：整数 `stepTick`、物理时间 `step` 或整数 `stepCycles`。`stepCycles` 需要唯一
可推断或显式 `clockId`，使用完整时钟周期而不是相位；不同 Lane 的数组可有不同长度，短数组
结束后不再写入。所有单元先完成类型和值校验，成功时自动延长 Scenario End。

`transfer-range` 的 `mode` 为 `copy` 或 `move`。源时间使用
`sourceStartTick`/`sourceStart` 与 `sourceEndTick`/`sourceEnd`，目标使用
`destinationTick`/`destination`。Lane 目标三选一：

- `laneId`：同一 Lane；
- `laneIds`：多个 Lane 各自原位映射；
- `mappings`：由 `{sourceLaneId, targetLaneId}` 组成的数组，可跨兼容 Lane。

默认不覆盖目标区间中的显式 Segment；需要替换已有内容时必须明确设置
`"overwrite": true`。Move 会清除源区间，Copy 保留源区间；两者都保留值和扩展元数据，
并复用 Event/Relation 同步与 End 自动延长语义。

`update-signal` 接受：

- 通用：`name`、`color`、`height`、`visible`、`groupId`。
- Bus/Enum：`width`、`signed`、`radix`。
- Enum：`enumMap`，表示完整替换声明映射而不是增量合并。
- 非 Clock 信号：`clockDomainId`。

Enum 映射替换会按修改后的位宽和有符号属性重新校验全部值；若现有 Segment 或 Event 使用的
声明符号在新映射中失效，整个批次被拒绝，源工程保持不变。解除分组时将 `groupId` 设为空字符串。

`update-group` 的 `groupId` 接受稳定 ID 或唯一名称，可修改 `name`、`color`、`height`
和 `visible`；成功报告返回规范 Group ID、修改后的属性及成员数量。`move-group` 使用与
`move-signal` 相同的位置三选一字段，参照对象可以是任意 Lane。`delete-group` 只删除 Group
本身，成员信号、波形和依赖均保留并解除归组；报告返回 `ungroupedSignalCount` 与
`ungroupedLaneIds`，调用方无需重新扫描整个工程才能解释结果。把普通信号当作 Group、名称冲突、
非法属性或无效位置都会拒绝整个批次。

```json
{
  "schema": "wave-workbench.operations/v1",
  "operations": [
    {"op": "update-group", "groupId": "Control", "name": "Timing"},
    {"op": "move-group", "groupId": "Timing", "afterLaneId": "Debug"},
    {"op": "delete-group", "groupId": "Debug"}
  ]
}
```

`update-clock` 接受 `name`、`periodTick` 或 `period`、`phaseTick` 或 `phase`、
`dutyNumerator`、`dutyDenominator`、`activeEdge` 和 `resetRelation`。改变时钟会复用桌面
命令的周期事件重定时和区间安全检查；不能安全重定时则拒绝整个批次。

`add-signal` 的类型参数：

- Bus：`width`（1–65536）、`signed`、`radix`、`clockDomainId`。
- Enum：必填 `width` 与非空 `enumMap`；可选 `signed`、`radix`、`clockDomainId`。
- Bit：`clockDomainId`。
- Clock：`clockId`、`periodTick` 或 `period`、`phaseTick` 或 `phase`、`dutyNumerator`、
  `dutyDenominator`、`activeEdge`。

所有四类信号均可用 `groupId` 引用 Group。`add-group` 必须位于同批次中引用它的
`add-signal` 之前；引用可写唯一名称或稳定 ID，成功报告统一返回 Group 的稳定 ID。Enum 的
`enumMap` 键为非空声明符号，值必须是可由声明位宽表示的字符串值；CLI 会规范化映射值。

只有一个 ClockDomain 时，新增 Bit/Bus/Enum 默认关联该时钟。省略 `id`、`clockId` 或
`color` 时，CLI 会生成稳定且可重复的身份与可读颜色，因此相同源工程上的 dry-run
和实际执行得到相同结果。同一批次的后续 operation 可直接使用新增信号或时钟的唯一名称；
只有存在同名歧义或跨批次需要固定外部身份时才需要显式提供 `id`。

`duplicate-signal` 默认把副本插在源信号正下方，复制类型、位宽、有符号属性、进制、Enum
映射、关联 Group/ClockDomain、显示高度、扩展属性和全部 Segment 内容。副本强制可见，Lane
和 Segment 使用独立、确定性的 stable ID；默认名称为 `<源名称>_copy`，冲突时依次使用
`_copy_2`、`_copy_3`，默认颜色与源信号不同。可用 `name`、`id`、`color` 或 `groupId`
覆盖相应结果，其中空 `groupId` 表示副本不归组。位置可省略，或在 `insertionIndex`、
`beforeLaneId`、`afterLaneId` 中至多指定一个。

复制 Clock 时会克隆周期、相位、占空比、有效边沿、复位关系和扩展属性，并创建独立
ClockDomain；可用 `newClockId` 固定其身份。普通信号继续关联原 ClockDomain，且不能使用
`newClockId`。Event、Relation 与 imported trace mapping 不随副本复制，避免把新信号误接入
既有行为约束；报告明确返回三个 `copied*Count=0`、Segment 数量、最终位置、颜色、分组和
ClockDomain 身份。Group 不能通过该 operation 复制。

```json
{
  "schema": "wave-workbench.operations/v1",
  "operations": [
    {"op": "duplicate-signal", "laneId": "data[7:0]", "name": "payload_copy"},
    {"op": "duplicate-signal", "laneId": "payload_copy"},
    {
      "op": "duplicate-signal",
      "laneId": "clk",
      "name": "clk_shadow",
      "newClockId": "clock-shadow"
    }
  ]
}
```

以下批次从空工程建立分组、时钟和符号状态序列：

```json
{
  "schema": "wave-workbench.operations/v1",
  "operations": [
    {"op": "add-group", "name": "Control"},
    {
      "op": "add-signal",
      "name": "clk",
      "kind": "clock",
      "period": "10 ns",
      "groupId": "Control"
    },
    {
      "op": "add-signal",
      "name": "state",
      "kind": "enum",
      "width": 2,
      "enumMap": {"IDLE": "0", "BUSY": "1", "DONE": "2"},
      "clockDomainId": "clk",
      "groupId": "Control"
    },
    {
      "op": "set-sequence",
      "laneId": "state",
      "clockId": "clk",
      "start": "cycle 0",
      "stepCycles": 1,
      "values": ["IDLE", "BUSY", "DONE"]
    }
  ]
}
```

## 时序关系与阶段标注

CLI 不提供通用 Event 新增或修改。波形关联 Event 仍由 Segment 编辑命令创建和重映射；让调用方
日常维护 Event ID 会在波形边沿移动后形成易失引用。唯一例外是 `validate` 已确认损坏且稳定 ID
唯一时的六类恢复：

- 问题返回 `repair-event-link` 时，调用方只传回 `eventId`。领域命令从未公开的稳定关联中唯一
  定位 Segment，将 Event 的 Lane、时刻和值恢复为 Segment 当前事实，同时保留 Event ID、
  Relation、动作、描述和扩展载荷。健康 Event、缺失/重复 Segment、同一 Segment 被多个 Event
  争用或不支持的动作均原子拒绝。修复改变时刻时会保留仍与目标活动边沿一致的 cycle，并清除
  已不一致的 cycle，避免下一次修改 Clock 时重新跳回旧位置。
- 问题返回 `clear-event-cycle` 时，只移除已与当前 Event 时刻冲突或无法解析的逻辑周期元数据；
  Event 的 `timeTick`、`clockDomainId`、波形关联、稳定 ID、Relation、动作和扩展载荷均保持不变。
  健康 Event、无 cycle Event 和重复 Event ID 均原子拒绝。
- 问题返回 `repair-event-clock` 时，从问题上下文指定的 Lane 唯一有效时钟恢复显式
  `clockDomainId`；Lane 没有关联时钟或已经不存在时清除悬空显式值。Lane 时钟本身悬空、当前
  ClockDomain ID 歧义、健康 Event 或重复 Event ID 均原子拒绝。
- 问题返回 `repair-lane-clock` 时，普通 Lane 清除不存在的 ClockDomain 引用；Clock Lane
  仅采用工程中唯一的 ClockDomain。操作保持当前波形、Event、Relation 和 Segment 不变；继承
  Lane 时钟且已无法解释的 cycle 元数据会清除并单独计数。多时钟工程、歧义 ClockDomain ID、
  健康 Lane 或重复 Lane ID 均原子拒绝。
- 问题返回 `repair-lane-group` 时，只清除无效 `groupId`。缺失 Group、目标不是 Group、
  Group 嵌套/自引用或目标 ID 歧义均不会触发替代目标推断；当前波形、Event、Relation、
  Segment、ClockDomain 和 Lane 显示属性保持不变。健康 Lane 或重复 Lane ID 原子拒绝。
- 问题返回 `delete-event` 时，可清理该 Event；健康 Event 即使 ID 已知也会被原子拒绝。删除会
  通过共享领域命令移除引用它的 Relation；若其
  关联 Lane 与 Segment 仍有效，也会清理对应波形区间并重新同步派生 Event。报告返回删除的
  Event、Relation、Segment 数量及可能同步产生的 Event 数量。重复 Event ID 以退出码 4 原子
  拒绝。

Relation 的正常创建和修改仍使用“Lane 选择器 + 精确边沿时刻”定位端点，并只接受该时刻恰好
存在一个波形关联 Event 的非 Clock、非 Group 信号。

例如，`validate` 返回 `repairOperations=["delete-event"]` 后可直接使用同一问题中的
`eventId`：

```json
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "delete-event",
      "eventId": "event-ack-high"
    }
  ]
}
```

波形关联错位时使用同样的单字段输入，但操作为：

```json
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "repair-event-link",
      "eventId": "event-ack-high"
    }
  ]
}
```

周期元数据与当前时刻冲突时仍只传回同一问题中的 Event ID：

```json
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "clear-event-cycle",
      "eventId": "event-ack-high"
    }
  ]
}
```

Event ClockDomain 悬空且问题公开安全替换动作时使用：

```json
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "repair-event-clock",
      "eventId": "event-ack-high"
    }
  ]
}
```

Lane ClockDomain 悬空且问题公开安全动作时使用：

```json
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "repair-lane-clock",
      "laneId": "lane-ack"
    }
  ]
}
```

Lane Group 引用无效时同样只传回问题中的 Lane ID：

```json
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "repair-lane-group",
      "laneId": "lane-ack"
    }
  ]
}
```

Relation ClockDomain 悬空且问题公开安全动作时只传回 Relation ID：

```json
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "repair-relation-clock",
      "relationId": "relation-req-ack"
    }
  ]
}
```

Imported Trace ID 为空或重复时，可直接回传对应问题中的 `traceRef`。省略 `newId` 会生成
确定性唯一 ID；若调用方已有稳定命名规则，可显式提供 `newId`：

```json
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "repair-trace-identity",
      "traceRef": "trace-ref-v1:1:<sha256>"
    }
  ]
}
```

操作报告返回旧/新 ID、索引、修复前后引用、原 ID 匹配数和是否自动生成。修复后旧
`traceRef` 必然失效；应使用报告中的新引用或新 ID 串联后续操作。

Imported Trace 的路径为空或格式未知时，回传同一问题中的 `traceRef`，并显式补齐引用：

```json
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "repair-trace-reference",
      "traceRef": "trace-ref-v1:0:<sha256>",
      "path": "traces/handshake_actual.vcd",
      "format": "vcd"
    }
  ]
}
```

若问题同时包含 `empty-path` 与 `unsupported-format`，只修复其中一项会原子拒绝。操作报告
返回旧/新路径、格式、引用、`changedProperties` 和修复前后上下文；新引用可用于后续映射修复。

Imported Trace 映射指向已删除 Lane 时，可直接回传问题中的 `traceRef` 和映射键：

```json
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "repair-trace-mapping",
      "traceRef": "trace-ref-v1:0:<sha256>",
      "laneId": "lane-removed"
    }
  ]
}
```

如果 `traceContext.traceIdMatchCount=1`，也可使用 `traceId`；操作成功后报告删除映射数、
修复前上下文和剩余映射数。健康映射、已变化的 `traceRef` 或不存在的映射键均原子拒绝。

`add-relation` 的源端必须同时提供 `sourceLaneId` 与
`sourceAtTick`/`sourceAt` 二选一，目标端同理使用 `targetLaneId` 与
`targetAtTick`/`targetAt`。最小和最大延迟分别三选一：

- `minimumDelayTick` / `maximumDelayTick`：整数 tick；
- `minimumDelay` / `maximumDelay`：物理时间；
- `minimumDelayCycles` / `maximumDelayCycles`：整数时钟周期。

周期延迟使用完整时钟周期，不使用相位；需要由两个端点的关联时钟唯一推断，或由 `clockId`
明确指定。端点时间中的 `cycle N` 仍表示时钟活动边沿。两个端点关联不同 ClockDomain、端点
不是现有波形边沿、延迟为负数或最大值小于最小值时，整个批次被拒绝。`severity` 接受
`information`、`warning` 或 `error`。省略 `id` 时按完整关系语义生成确定性 ID；重复加入相同
关系或提交无效果修改时返回 `changed=false`。

`update-relation` 以唯一稳定 `relationId`，或 `validate` / `relations` 返回的
`relationRef` 定位对象，
两者必须且只能提供一个。健康端点可只修改 Lane 或时刻，也可只修改延迟、`clockId`、
`condition`、`severity` 或 `description`。若既有端点缺失、Event ID 重复、脱离波形、越界或
不再对应唯一边沿，则该端必须同时提供 `sourceLaneId` +
`sourceAtTick`/`sourceAt` 或 `targetLaneId` + `targetAtTick`/`targetAt`；未损坏且未指定的
另一端原样保留。成功报告除 Relation ID、规范 Lane ID/精确 tick、延迟和时钟 ID 外，还返回
`selectedByRepairRef`、`previousRelationId`、`repairedIdentity`、`repairedSourceEndpoint`、
`repairedTargetEndpoint` 及更新后的 `relationRef`。`newId` 可将空或重复 ID 改为 Scenario 内
唯一的非空 ID；`delete-relation` 同样接受 ID 或引用。

`relationRef` 摘要覆盖 Relation 的身份、两个内部端点引用、延迟、时钟、条件、严重性、说明和
扩展载荷，但查询不会暴露内部 Event ID 或扩展内容。任何这些内容或 Relation 索引变化都会让旧
引用以退出码 4 原子失效，不会误改其他项。调用方应先执行 `validate` 或查询，再以 source SHA
保护的 dry-run/写入批次使用引用。调用方仍不需要持有内部 Event ID。

例如，`validate` 问题已报告 `repairProperties=["target-endpoint"]`，且
`relationContext.target.issue="missing-event"` 时，可直接使用同一问题中的 `relationRef`
把目标替换为 `ack` 的 110 ns 现有边沿：

```json
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "update-relation",
      "relationId": "relation-req-ack",
      "targetLaneId": "ack",
      "targetAt": "110 ns"
    }
  ]
}
```

若查询报告两个 `relation-req-ack` 均为 `addressable=false`，可用目标行的引用同时修复身份和
端点：

```json
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "update-relation",
      "relationRef": "relation-ref-v1:1:...",
      "newId": "relation-req-ack-follow-up",
      "targetLaneId": "ack",
      "targetAt": "110 ns"
    }
  ]
}
```

Marker 有两种几何形式：

- 点：`atTick` 或 `at`，二选一；
- 区间：一组 `startTick`/`start` 与 `endTick`/`end`，每端二选一。

名称在 Scenario 内大小写不敏感唯一。`kind` 接受 `point`、`interval`、`phase`、`error` 或
`note`；省略时按点或区间选择默认类型。`update-marker` 与 `delete-marker` 的 `markerId`
可写稳定 ID 或唯一名称，且同一批次可按刚创建或刚改后的名称继续引用。区间必须非空且位于
Scenario 内；点和区间字段混用会拒绝整个批次。

若 `validate` 或 `markers` 返回 `markerRef`，`update-marker`/`delete-marker` 可直接使用该
引用，但不能同时提供 `markerId`。`update-marker.newId` 用于把空或重复稳定 ID 改为
Scenario 内唯一的非空 ID；报告返回 `selectedByRepairRef`、`previousMarkerId`、
`repairedIdentity`、`repairedGeometry` 以及修改后的新 `markerRef`。引用包含 Marker 扩展载荷
的摘要但不暴露载荷本身；任何相关内容或索引变化都会使旧引用以退出码 4 原子拒绝。调用方应先
在工程内容未变化时，以 source SHA 保护的 dry-run/写入批次使用该引用。
修复后应再次执行 `validate`，以 `markerIssueCount=0` 和 `markerSummary.valid=true` 确认
名称与几何均已恢复，而不是只依据 `update-marker` 的成功退出码判断整个工程健康。

```json
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "update-marker",
      "markerRef": "marker-ref-v1:1:...",
      "newId": "marker-transfer-follow-up"
    }
  ]
}
```

以下批次把 `enable` 与 `state` 的现有边沿关联，并加入阶段标注：

```json
{
  "schema": "wave-workbench.operations/v1",
  "operations": [
    {
      "op": "add-relation",
      "sourceLaneId": "enable",
      "sourceAt": "cycle 1",
      "targetLaneId": "state",
      "targetAt": "cycle 2",
      "minimumDelayCycles": 1,
      "maximumDelayCycles": 2,
      "clockId": "clk",
      "severity": "warning",
      "description": "state changes within two cycles after enable"
    },
    {
      "op": "add-marker",
      "name": "Control window",
      "start": "cycle 1",
      "end": "cycle 4",
      "clockId": "clk",
      "kind": "phase",
      "note": "Complete control interval"
    }
  ]
}
```

如果任一 Relation 端点不是现有边沿，即使前面的 Marker 已通过字段校验，批次仍整体回滚，
不会留下 Marker、Relation 或输出工程。

`start`、`end`、`duration`、`sourceStart`、`sourceEnd`、`destination`、
`sample --at`、`window --start/--end`、`edges --start/--end` 与
`relations --start/--end` 接受以下精确时间：

- `12500 tick` 或 `12500 ticks`；
- `37.5 ns`、`0.012 us`、`250 ps`、`1 ms`；
- `cycle 8`，表示指定时钟活动边沿的第 8 周期。

`update-clock` 的 `period` 与 `phase` 使用上述 tick 或物理时间写法；周期参数应直接表达为
时间，不使用 `cycle N`。

时间转换不使用浮点数，不能被工程 timebase 精确表示的值会被拒绝。周期时间会优先使用显式
`clockId`/`--clock` 选择器，其次使用目标 Lane 的唯一时钟；仍有歧义时不会猜测。`*Tick` 输入可使用
十进制字符串；处于 IEEE-754 安全整数范围内时也接受 JSON 整数。自动化调用应优先使用字符串，
避免其他语言的 JSON 数字精度损失。

## 安全写入流程

先读取源文件：

```powershell
.\build\wave-cli.exe inspect .\project.wave.json --pretty
```

预演 operations：

```powershell
.\build\wave-cli.exe apply .\project.wave.json .\operations.json `
  --dry-run --expect-sha256=<inspect 返回的 sourceSha256> --pretty
```

写入独立结果：

```powershell
.\build\wave-cli.exe apply .\project.wave.json .\operations.json `
  --output=.\project.edited.wave.json --expect-sha256=<sourceSha256>
```

确认需要替换源工程时：

```powershell
Get-Content .\operations.json -Raw |
  .\build\wave-cli.exe apply .\project.wave.json - `
    --in-place --backup --expect-sha256=<sourceSha256>
```

`--expect-sha256` 在加载前检查源版本；同路径写入还会在保存前再次检查，文件被其他进程
修改时返回 `source-conflict`。写入使用临时文件和原子替换。无效果的 `--in-place`
批次不会重写文件。

`--backup` 仅用于 `--in-place`。它在替换前保存原始字节，自动路径为
`<project-name>.backup-<sourceSha 前 12 位>.wave.json`；也可使用 `--backup=PATH`。已存在且内容
相同的备份会复用，内容不同则返回 `backup-conflict`，源工程不会被写入。`--output` 不允许
指向源工程；替换源文件必须显式使用 `--in-place`，避免误把普通输出当成不可恢复覆盖。

`apply` 的 warning 不阻止输出；error 受上述非退化门禁约束。operation 本身的语法、范围、
类型、依赖和模型不变量始终是硬性约束。损坏工程可按
`relationRef`/`markerRef`/`traceRef` 分步修复，
每一步都必须使错误集合缩小或完成一个不增加任何 error 的显式恢复动作；最终以
`validation.valid=true` 和 `validationGuard.reason=valid-candidate` 确认恢复完成。

## 嵌入接口

头文件：`src/waveautomation/include/wave/automation.h`

公开入口：

```cpp
wave::describeAutomationCapabilities();
wave::inspectProjectForAutomation(
    project, scenarioId, wave::AutomationInspectDetail::Summary);
wave::findSignalsForAutomation(project, signalQueryOptions, scenarioId);
wave::parseAutomationTime(project, QStringLiteral("80 ns"), clockId);
wave::sampleProjectForAutomation(project, tick, scenarioId, laneIds);
wave::inspectProjectWindowForAutomation(
    project, startTick, endTick, scenarioId, laneIds);
wave::findWaveformEdgesForAutomation(
    project, edgeQueryOptions, scenarioId, laneIds);
wave::findMarkersForAutomation(
    project, markerQueryOptions, scenarioId);
wave::findRelationsForAutomation(
    project, relationQueryOptions, scenarioId, laneIds);
wave::validateProjectForAutomation(project, scenarioId);
wave::createProjectForAutomation(newProjectOptions);
wave::applyAutomationBatch(project, operationsObject, scenarioId);
```

`createProjectForAutomation` 返回可继续交给宿主或 `applyAutomationBatch` 的空白工程。
`applyAutomationBatch` 不修改传入的 `Project`。成功时返回新的工程副本；失败时不返回部分结果，
并提供失败 operation 的零基索引。宿主应用负责决定保存位置和并发控制。


## Scenario lifecycle operations

The `wave-workbench.operations/v1` batch selects its target through top-level `scenarioId`
(or the CLI Scenario selector). Fields below are operation fields; no nested `scenarioId` is accepted.

| op | Required fields besides op | Optional fields |
|---|---|---|
| create-scenario | name | id, durationTick, duration, insertionIndex |
| duplicate-scenario | none | id, name, insertionIndex |
| rename-scenario | name | none |
| delete-scenario | none | none |
| reorder-scenario | destinationIndex | none |

Indices are zero-based, durations are positive, and stable IDs/names are validated. Deleting the last
Scenario is rejected. GUI commands are undoable; CLI edits use the existing validated atomic batch path.
For an existing Scenario with ID `scenario-main`, a rename batch is:

```json
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-main",
  "operations": [{"op": "rename-scenario", "name": "Reset sequence"}]
}
```

Save as `rename.operations.json` and run `wave-cli apply project.wave.json rename.operations.json --dry-run`
before using `--in-place`. Discover all supported operations with `wave-cli capabilities --pretty`.
