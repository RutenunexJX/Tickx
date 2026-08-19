# Wave Workbench 长期目标

更新时间：2026-08-19

## 目标

ZeroSlack Wave Simulation S12.3“struct、array 和 interface 输入编辑”已完成。Module
Manifest v3 由 Slang 输出结构化 selector，WaveWorkbench 将 packed
struct、固定 unpacked array 和显式 modport interface 映射为可编辑 leaf group；Stimulus
Scenario v3 保存并迁移该身份，runner wrapper 重建原始端口并映射稳定 trace 名。
`wavewidgets` C ABI 继续为 v1；全量 offscreen CTest 为 `91/91`。本机未安装真实
Verilator，外部编译/运行只能由确定性 fixture 验证，不表述为真实 RTL 仿真。

交付一个可独立运行的 Qt 6/C++20 波形桌面应用，使用户能够人工定义与直接编辑 FPGA
预期数字波形；同一场景模型继续支撑 SystemVerilog/SVA/cocotb、文档图、trace 导入及
Expected/Actual 比较，这些派生与比较能力由独立 CLI 和领域模块提供。

该目标已登记为长期 goal，并在第 1030 轮按用户要求结束为 `complete`；用户随后明确追加的
第 1031–1040 轮阶段可用版本也已完成。当前工作是 goal 完成后的追加维护收敛，未重新将长期 Goal 改为
active。阶段 1 至阶段 6、最终加固、Clock
可编辑性与覆盖语义迭代、Lane/Group 属性编辑迭代以及依赖感知删除迭代均已完成并
验收；Lane/Group 显示顺序、Relation condition 求值和画布末尾添加信号入口迭代也已
完成，光标模式增强、Wave Edit 直接波形编辑、画布信号管理、工具收敛、纯波形工作区、
局部边沿预览、桌面吸附控件移除、轻吸附/画布层次/Bus 快捷值、基础操作闭环、任务闭环/
直接编辑/双视角验收、信号名就地重命名、未提交草稿统一收口、快速新增事务隔离与标题拖动取消，
以及持久范围选择、同类型批量赋值、波形 Event 稳定重映射、Relation 依赖一致性、非遮挡固定范围栏、
可见范围 Copy、右键就地 Paste、粘贴结果持续可见、范围原子清除与关系安全撤销、标准范围 Cut 与文本焦点保护，
以及持久范围端点就地修正、范围选择后的同次安全重定向、Bit 写值与清除拍级一致性、拍级选择持久可见、
撤销/重做与 Segment 清除反馈闭环、单信号写值关系安全与一拍 Pulse 一致性、拍级翻转与清除依赖反馈闭环、
重复写值无效果命令收口、Clock Run 结果反馈与空清除收口、快速参数编辑真实结果与空历史收口、单 lane 目标感知 Paste 与空历史收口、多 lane 显式目标 Paste 与可见入口、自描述剪贴板与源删除后 Paste、Paste 完整宽度与 End 原子延长、信号删除影响与恢复反馈、信号重排结果与恢复反馈、锁定光标结果与恢复反馈、活动光标最终测量反馈、持久 Marker 唯一命名、Measure 模式切换状态隔离、只测量边界/选择归属、信号标题目标反馈、右键/重命名取消恢复及轻量参数原位校验与纠错、导出参数原位校验与纠错、项目文件过滤/保存命名、重新打开/结果反馈、脏工程选择/确认顺序、高级 Lane/Group 属性原位纠错、空提交、结构兼容性预检与名称唯一性，以及恢复快照生命周期、崩溃后自动发现、Discard 持久清理、首次保存前 Untitled 自动恢复、最近工程/目录记忆、工程文件拖放收口、Undo/Redo 正式保存点、隐藏项可发现恢复、Group 画布管理闭环、长层级信号名可辨识性、长列表跨视口拖动重排、Wave Edit 水平跨视口拖动、上下文 Fit、Home/End 边界导航、当前信号相邻边沿导航及纵向信号键盘导航与文本焦点隔离、所选信号即时采样值、Bus/Enum 键盘一拍编辑、Enum 声明符号补全、精确边沿取值、Enum 多拍范围原子赋值、键盘时间范围选择、键盘多信号范围调整与键盘范围直达时间轴边界与 Ctrl+A 当前目标完整时间轴选择与 Ctrl+Shift 当前信号真实边沿范围选择，以及 Bus 就近编辑浮层、同步/异步波形编辑、Ctrl 拖动复制、Bus/Enum 连续拍录入、隐式拍跳过、数值草稿步进/最近值循环/留驻提交、画布拍级与可见页导航、Clock 周期键盘编辑、精确拖动时序反馈、Segment 双向相邻复制、显式 Segment 导航与微移、Enum 符号轮换、精确选中/悬浮/指针反馈、编辑光标锚定缩放、`Ctrl+0` 上下文 Fit，以及 Segment 完整值连续录入、光标选段、双向进入、键盘边界调整、退出目标保持、显式范围整块拖动/复制、历史目标回显与动态 Ctrl 意图切换、跨信号范围整块转移、范围转移内容感知预览、Clock 范围批量编辑、键盘时间步长与 Sync/Async 一致性、显式范围端点精确输入、小数物理时间精确输入、显式范围精确宽度、可嵌入自动化/统一 CLI、CLI 名称寻址/紧凑信号定位、Group/Enum 符号刺激建模、CLI 时序关系/阶段标注、CLI 能力发现/Group 生命周期、CLI 整信号安全复制、CLI 安全缩短/显式时间轴截断、固定范围栏 Paste 点击前兼容性预检、信号标题多选直接 Paste 及标题右键 Paste 与逐项映射确认迭代亦已完成。百万 transition 最近指标为 Debug 21 ms / Release 4 ms。
第 781–790 轮的标尺定位保留标题目标、动态 Paste 映射及取消完整恢复迭代亦已完成。
第 791–800 轮的结果范围转标题目标、同目标非相邻连续 Paste 及选择感知 Undo/Redo 迭代亦已完成。
第 801–810 轮的兼容目标 Paste 虚线波形、当前值→预览值及实时标尺跟随迭代亦已完成。
第 811–820 轮的 Paste 提交前 Relation 清理预测、琥珀色风险反馈及可恢复闭环迭代亦已完成。
第 821–830 轮的受影响 Relation 描述、端点摘要及画布定向强调迭代亦已完成。
第 831–840 轮的显式范围 Paste 波形投影、统一右键门禁及无变化抑制迭代亦已完成。
第 841–850 轮的无效果 Paste 提交前判定、全入口禁用及运行时无损跳过迭代亦已完成。
第 851–860 轮的显式范围 Clear/Run 提交前影响预测、风险定位及无效果禁用迭代亦已完成。
第 861–870 轮的显式范围写值影响预测、文本草稿即时反馈及无效果禁用迭代亦已完成。
明确保留范围记录于 `README.md` 和 `PLAN.md`。
本轮修改前的最近完整静默基线为默认、Qt Creator Debug、Qt Creator Release 各 73/73 CTest，顺序执行耗时
分别为 44.78 s、41.69 s、38.83 s。第 371–650 轮已进一步完成
CLI Relation 就绪边沿、既有 Relation/Marker 紧凑查询、损坏 Relation 显式端点修复和损坏
Marker/Relation 快照引用恢复、八类核心结构身份校验、Marker 名称/几何完整性校验及 apply
非退化后置校验门禁，使 validate 直接返回可操作 Marker/Relation 修复引用和结构化 Relation
端点/时序上下文；并继续完成 Event/Lane/Relation ClockDomain、Lane Group、Imported Trace
映射、身份与源引用恢复，以及多 Trace Compare 显式选择、映射失败分类、解析前引用门禁和
多 Scenario CLI 显式目标选择。

当前第 83 轮已完成长信号列表的 `Ctrl+F` 即时查找、前后循环、无结果反馈、显式范围门禁及离屏双视角验收；查找只改变当前选择与纵向可见位置，不修改 Scenario、Undo 或 Saved。
当前第 84 轮进一步完成 `Ctrl+G` 精确时间跳转、整数时间单位与周期输入、非法/越界原位反馈、显式范围门禁及离屏双视角验收；跳转只改变编辑光标和必要的水平可见位置，不修改 Scenario、Undo 或 Saved。
当前第 85 轮进一步完成现有信号一键复制、相邻插入、唯一名称与随机颜色、ClockDomain 独立复制、单步 Undo/Redo、显式范围及文本焦点门禁和离屏双视角验收；复制保留信号属性与波形但重新生成 lane/segment 身份，不复制 Event、Relation 或 trace mapping。
当前第 86 轮进一步完成可见信号与 Group 一步隐藏、动态 Edit/标题右键入口、显式范围无损门禁、实时恢复计数、专用单步 Undo/Redo 和离屏双视角验收；隐藏不删除模型数据或依赖，既有 `Show hidden items` 继续一次恢复全部隐藏项。
当前第 87 轮进一步完成多个隐藏项按名称单独恢复、主体按钮恢复全部、单隐藏项直达模式、原顺序和水平视图保持、恢复后选中、显式范围无损门禁、专用单步 Undo/Redo 和离屏双视角验收；隐藏项专项使用隔离工程副本，测试中断不再污染源码示例。
当前第 88 轮进一步完成固定 Bus Beat/Segment 编辑栏、进制与最近自定义值、Don’t-care 独立比较语义、默认 Sync 与可切换 Async、Alt 绕过轻吸附、Ctrl 拖动复制、空历史抑制、扩展元数据保留及 offscreen 双视角验收。
当前第 89 轮进一步完成 Bus/Enum 当前编辑拍画布高亮、`Tab` 提交并前进、`Shift+Tab` 提交并后退、Sync/Async 连续拍推进、非法输入原位纠错、首尾边界反馈、空历史抑制及 offscreen 双视角验收。
当前第 90–93 轮进一步完成隐式 X 拍无写入跳过、Bus 数值草稿加减、每 lane 最近值循环及 `Ctrl+Enter` 提交后保持当前 Beat/Segment；全部路径保留目标、焦点、规范化、边界和空历史语义。
当前第 94–96 轮进一步完成画布 `Tab` / `Shift+Tab` 拍级导航、`PageUp` / `PageDown` 可见时间页导航，以及 Clock `G` / `X` / `R` 周期编辑；纯导航不修改模型，Clock 修改保持单步 Undo。
当前第 97–99 轮进一步完成 Segment 移动/缩放/Ctrl 复制与范围拖动的精确起止/宽度反馈、Bus/Enum Segment 右键相邻复制、End 边界无历史提示，以及同步模式真实右键命中；offscreen 自动化和视觉验收均通过。
当前第 100–103 轮进一步完成 `Ctrl+D` 按 Segment/信号上下文分流、前后相邻复制、`Ctrl+Tab` /
`Ctrl+Shift+Tab` 显式 Segment 导航，以及 `Alt+Left` / `Alt+Right` 按 Sync 一拍或 Async 一 tick
微移；复制与微移保持值、扩展语义、Event/Relation 同步和单步 Undo，纯导航保持模型零变化。
当前第 104–107 轮进一步完成 Enum 草稿声明符号轮换、Segment 单击精确状态、非 Bit Segment
悬浮详情和指针下信号/值反馈；同步吸附后的编辑时间与实际指针采样值明确分离。
当前第 108–109 轮进一步完成可见编辑光标锚定 Zoom，以及窗口级 `Ctrl+0` 上下文 Fit；存在显式
范围时满宽适配选区，否则恢复完整 Scenario，全部缩放路径不修改 Scenario、Undo 或 Saved。
当前第 110–113 轮进一步完成已选 Bus/Enum Segment 的 Enter/直接输入、Tab/Shift+Tab 双向连续
显式段录入、首尾目标保持及原值预设元数据零变化。
当前第 114–116 轮进一步完成 `Ctrl+Space` 按编辑光标选段，以及未预选 Segment 时从当前信号和
时间位置用 `Ctrl+Tab` / `Ctrl+Shift+Tab` 进入后/前一显式段；纯选择与导航保持模型零变化。
当前第 117–119 轮进一步完成 `[ ]` 与 Shift 变体按当前编辑单位调整 Segment 左右边界、单步 Undo、
相邻内容约束，以及 Esc 退出 Segment 编辑后保留信号和精确时间并可立即恢复。
当前第 120 轮将上述 Segment 键盘入口收敛为 Edit > Segment 与 F6/F7/F8，移除
Ctrl+Space、Ctrl+Tab、Alt+Left/Right 和方括号的系统/导航冲突；Ctrl+D 固定为整条信号
复制。工具栏持续显示当前目标、关联时钟与实际步长，状态栏恢复为短结果；Bus/Enum 编辑器移至
当前 Beat/Segment 附近，并在 960 px 范围编辑场景中保持无裁切。
当前第 121–140 轮进一步将指针预览与编辑光标分离，补齐标尺单击/拖动查样及取消恢复；显式范围
右键保持目标并集中提供 Copy/Cut/Paste/Clear/Fit/Edit/Close，范围栏可见关闭且安全改选不再需要
无意义的空白点击。Bus/Enum 就近编辑器改为两行，公开 Beat/Segment、前后导航、Apply 与关闭，
有效草稿先提交、无效草稿原位保留；Bit、隐式 X、显式 Segment 和标尺均提供精确悬浮预测，
Timing 文案直接公开 Sync/Async、关联时钟与真实步长。
当前第 141–150 轮进一步统一 Bus/Enum 的可见目标与实际操作范围：单击固定选择一拍，只有实际拖动、
双击或 F6 才进入完整 Segment；Delete/Backspace、快捷预设、直接输入和 Apply 均严格作用于当前 Beat。
就近编辑器新增可见 Clear，Beat 清除只恢复一拍为隐式 X，Segment 清除完整段后自动落到首拍，避免
残留失效 Segment 目标；所有清除、无效果与撤销路径均保留明确目标和反馈，960 px 紧凑布局无裁切。
当前第 151–160 轮进一步消除合法草稿提交后的无效第二次点击：快速新增、Bus/Enum Beat、范围值均在
提交后继续完成同一次左键目标变化，Bus/Enum 同样支持在同一次双击中进入 Segment、在同一次右键中
打开目标菜单。隐式 X 与显式值拍保持相同 Beat 选择；非法 Bus 或多 lane 范围值保留旧目标、草稿、
错误与焦点，零部分写入。Timeline End 因改变坐标映射而继续安全消费提交它的当前点击。
当前第 161–170 轮进一步将范围输入的 Esc 改为两级退出：存在草稿时第一次 Esc 只丢弃输入、恢复
正常样式并保留时间范围、全部 lane 目标与焦点；未修改状态下再次 Esc 才关闭范围。固定栏关闭按钮
继续一步退出，草稿丢弃与范围关闭均不修改 Scenario、Undo、Redo 或 Saved 状态。
当前第 171–180 轮进一步完成所选范围紧邻重复：范围右键与 Edit 菜单一次执行即可将全部目标信号
复制到紧邻后方，不需要 Copy、定位和 Paste 三步，也不污染系统剪贴板。结果范围继续选中，隐式
空白与扩展语义保留，Relation 清理和 End 延长沿用原子恢复语义；专用 `Duplicate range`
Undo/Redo 可准确辨识操作，无效果重复不新增历史且保留真实 Redo。
当前第 181–190 轮进一步完成显式范围正文直接移动与 `Ctrl` 拖动复制：多信号目标保持原子性，
源选区实线和目标虚线同时可见，精确起止/宽度、Sync/Async、轻吸附、`Alt` 绕过、自动滚动、
重叠拒绝、取消恢复、剪贴板隔离、结果持续选择、End 延长及专用 Undo/Redo 均形成闭环。完整移动
Segment 尽量携带 stable ID/Event，部分边界和覆盖目标继续按既有 Relation 规则同步。
当前第 191–200 轮进一步完成编辑历史的可见目标恢复：范围 Move/Copy、紧邻重复、显式目标 Paste
以及 Segment 移动/复制/微移/边界调整在 Undo 时恢复源目标、Redo 时恢复结果目标，并对离屏目标做
最小揭示。范围与非 Clock Segment 拖动期间可随时按下或释放 `Ctrl` 即时切换 Copy/Move，预览、
指针、状态和最终提交一致；方向化状态映射不影响其他 Undo 或无效果历史。
当前第 201–210 轮进一步完成显式范围跨信号整块转移：纵向拖动直接推断兼容目标信号块并保留按下行
偏移，源实线、目标虚线、映射标签、同时间不相交 Copy、红色无效目标、上下边缘自动滚动和取消视图
恢复均在释放前可见。跨信号 Move 使用先读取、统一清理、再写入的两阶段语义，避免重叠链式映射污染；
成功后选中目标块，Undo/Redo 在源块和结果块间准确恢复。
当前第 211–220 轮进一步把范围目标从抽象矩形改为内容感知预览：Bit 高低电平、Bus 数值和语义预设、
Enum 符号及 Clock 覆盖均按将要写入的形状显示；标签避让待写入范围，值级不兼容和重叠 Copy 使用红色
拒绝且不绘制误导内容。预览只截取当前可见目标行的源 Segment，取消路径保持模型、历史和保存状态不变。
当前第 221–230 轮进一步将同类型 Clock 纳入显式范围批量编辑：固定栏直接显示 `N Clock` 和
Run/Gate/Disable，R/G/X 使用相同目标；多 Clock 范围一次原子写入并单步 Undo/Redo，重复结果不产生
空历史。Clock 与其他类型混选时保留范围并拒绝修改，Run 继续沿用 Event/Relation 安全清理与可见反馈。
当前第 231–240 轮进一步统一键盘时间移动与 Timing：Sync 的 Left/Right 及 Shift 变体严格使用
活动信号关联 Clock 的 period、phase 和 active edge，Async 使用 1 tick，未关联 Clock 回退 10 ns；
离网格位置、非整拍 End 和多信号范围均给出确定边界及实际步长反馈，导航保持模型和历史零修改。
当前第 241–250 轮进一步将显式范围接入 `Ctrl+G` 紧凑精确输入：固定范围栏摘要可直接点击，
`Range edge` 显示固定锚点、活动端点和 `Other edge`，支持整数时间单位与 `cycle N`，允许跨锚点但
拒绝空范围。单/多信号目标、未提交范围值草稿、工具栏恢复与过期编辑器关闭均保持确定，端点调整
只修改选择上下文，不修改 Scenario、Undo 或 Saved。
当前第 251–260 轮进一步使物理时间输入支持精确小数：字符串有理换算避免浮点舍入，只有整数 ps
且能被项目 timebase 整除的结果才被接受；Go to、范围端点、Timeline End、Clock 周期、导出与其他
共享参数入口一致生效。tick/cycle 保持整数，亚 timebase 和溢出原位拒绝；显示自动选择最多三位
精确小数的紧凑单位。
当前第 261–270 轮进一步使显式范围可直接输入精确宽度：同一紧凑栏通过可点击模式标签在端点与
宽度间切换，宽度从固定锚点沿当前活动方向计算，支持小数物理时间、整数 tick 和整数 cycle 数；
`Other edge` 原位反转方向。零宽与越过 0/End 的宽度提供方向化上限并保持原选择；1440/960 像素
offscreen 验收确认控件、波形与网格无遮挡，Scenario、Undo 和 Saved 均不变化。
当前第 271–280 轮已将统一 headless CLI 纳入长期迭代：新增可嵌入 `waveautomation`，
建立版本化 inspect、validate 与五类原子编辑 operation；`wave-cli` 支持 JSON stdout/stderr、
稳定退出码、文件/stdin、dry-run、独立输出、原地原子替换和 SHA-256 并发保护。自动生成身份、
颜色与同 tick 事件顺序均可重复，dry-run 与实际结果 SHA 一致；Windows PowerShell 管道、
失败无部分结果、依赖清理和三配置静默回归均通过。
当前第 281–290 轮进一步使 CLI 可直接承担日常定位与批量修改：`inspect --summary` 避免读取
完整波形，`sample` 按 tick、物理时间或周期返回指定信号值；range 与 End 接受精确物理时间/
周期；新增信号删除、重排、常用属性和时钟修改 operation。字段冲突、多时钟歧义、类型错误和
批次中途失败均原子拒绝，三配置 35/35 静默回归和 Codex 用户旅程通过。
当前第 291–300 轮补齐 CLI 局部波形工作流：`window` 返回指定时间/Lane 的裁切 Segment 与
完整依赖上下文，多 Lane 赋值和同/跨 Lane Copy/Move 范围由领域命令原子执行；值断言、默认
覆盖拒绝、显式 overwrite、原地写入备份及备份冲突保护降低误改与恢复成本。三配置 37/37
静默回归和 Codex 定位→编辑→拒绝→恢复用户旅程通过。
当前第 301–310 轮使 CLI 可从空路径独立完成刺激工程：`new` 创建确定性空白工程并可在首次
写盘前接收文件/stdin operations；`set-sequence` 以共享 tick、物理时间或时钟周期步长一次写入
单/多 Lane 重复序列并自动延长 End。结果提供结构化变更摘要和同批次内容后置断言；已有目标、
无时钟周期、非法值与中途失败均保持零覆盖、零半成品。三配置 39/39 静默回归和 Codex
从零创建→采样→窗口→校验→拒绝用户旅程通过。
当前第 311–320 轮进一步消除 CLI 调用前查找稳定 ID 的步骤：新增紧凑 `signals` 查询，命令行
与十二类 operation 的 Scenario、Lane、Clock、ClockDomain 和 Group 引用接受稳定 ID 或唯一的
大小写不敏感名称，同批次新增对象也可立即按名称引用。所有成功报告返回规范稳定 ID；歧义、缺失
与无效查询不猜测且无部分结果。三配置 41/41 静默回归和 Codex 定位→名称预演→写入→采样→窗口
→校验用户旅程通过。
当前第 321–330 轮补齐 CLI 从零建立符号刺激的模型路径：新增 `add-group`，`add-signal` 可创建
带显式位宽/声明映射的 Enum，并让 Clock/Bit/Bus/Enum 通过同批次 Group 名称直接归组；
`update-signal.enumMap` 完整替换声明映射。非法映射、既有值失效及非 Group 归组均原子拒绝，
三配置 42/42 静默回归和 Codex Group→符号序列→映射扩展→采样→拒绝用户旅程通过。
当前第 331–340 轮补齐 CLI 的时序意图路径：Relation 通过 Lane 选择器与精确现有波形边沿
寻址，支持增删改、tick/物理时间/周期延迟和确定性身份；Marker 支持唯一命名的点/区间、
五种用途类型及按名称增删改。内部 Event ID 不暴露，缺失边沿、冲突时钟、非法延迟/几何和
批次中途失败均原子拒绝。三配置 43/43 静默回归和 Codex 关系→标注→修改→窗口→清理→拒绝
用户旅程通过。
当前第 341–350 轮补齐 CLI 外部接入与 Group 管理路径：无工程 `capabilities` 和嵌入 API
共同返回命令、22 类 operation、选择器、时间、值域与安全特性；Group 可按名称修改属性、
重排和删除。删除只解除成员归组并报告影响，成员波形保留；类型误用、名称冲突和批次中途失败
均原子拒绝。三配置 44/44 静默回归和 Codex 能力发现→Group 管理→删除保持→拒绝用户旅程通过。
当前第 351–360 轮将桌面端一键复制语义纳入 CLI：`duplicate-signal` 以一个原子 operation
复制完整 Lane 属性、扩展数据与 Segment，自动生成唯一名称、不同颜色和确定性独立身份；
Clock 同时获得独立 ClockDomain。Event、Relation 与 trace mapping 明确不复制，错误批次不留
半成品。三配置 45/45 静默回归和 Codex Bus→副本再复制→Clock→采样→拒绝用户旅程通过。
当前第 361–370 轮补齐 CLI 时间轴缩短闭环：无内容损失的 End 缩短可直接提交；会裁剪波形的
缩短必须显式设置 `truncate`，否则在写入前报告 Segment/Event/Relation/Marker 影响并原子拒绝。
确认后跨边界内容裁短、越界内容和依赖清理均由可撤销领域命令完成，结果提供六项影响计数；
capabilities 公开同一安全契约。三配置 46/46 静默回归和 Codex 保护→预演→截断→采样→校验
用户旅程通过。
当前第 371–380 轮补齐 CLI Relation 边沿定位闭环：新增只读 `edges`，按 Lane、半开范围和
initial/rising/falling/change 查询精确 waveform-linked 端点，直接返回前后值、动作、有效时钟、
候选计数和 Relation 可用性，不暴露内部 Event/Segment ID。歧义、Clock/Group、非法范围与过滤
均结构化反馈；capabilities 公开第九个命令、四类边沿和能力标志。三配置 47/47 静默回归及
Codex 边沿定位→Relation 预演→写入→窗口确认→校验→源 SHA 复核用户旅程通过。
当前第 381–390 轮补齐既有 Relation 的定位与维护闭环：新增只读 `relations`，按文本、严重性、
Lane 和端点范围返回 Relation ID、源/目标 Lane+tick、实际/声明延迟和端点健康状态，不暴露内部
Event/Segment ID。缺失/重复/脱离波形端点、延迟越界、非法筛选和空结果均有确定反馈；
capabilities 公开第十个命令和查询能力标志。三配置 48/48 静默回归及 Codex
Relation 定位→ID 复用→预演→更新→复查→校验→源 SHA 复核用户旅程通过。
当前第 391–400 轮补齐既有 Marker 的定位与维护闭环：新增只读 `markers`，按稳定 ID、名称、
备注、五种用途类型和半开时间范围返回精确起止、宽度及身份/几何健康状态，不读取完整 Scenario
或扩展载荷。重复 ID/名称、空身份、越界/反向区间及 Point/范围冲突均有稳定问题码；单时钟周期
范围自动推断并公开 Clock ID。三配置 49/49 静默回归及 Codex
Marker 定位→ID 复用→预演→更新→复查→校验→源 SHA 复核用户旅程通过。
当前第 401–410 轮补齐损坏 Relation 的错误恢复闭环：`update-relation` 可用显式 Lane+时刻
替换缺失、重复、脱离波形、越界或歧义端点，健康且未指定的另一端保持不变；双端损坏可在同一
operation 中修复。部分替换、歧义边沿、重复/空 Event ID 原子拒绝；`edges` 公开
`eventIdCount`，capabilities 公开 `relationEndpointRepair`。三配置 50/50 静默回归及 Codex
诊断→预演→显式修复→复查→校验→失败保护→源 SHA 复核用户旅程通过。
当前第 411–420 轮补齐损坏 Marker 的错误恢复闭环：`markers` 返回绑定 Scenario、索引和完整
Marker 快照的 `markerRef`，空/重复 ID、损坏几何可精确改 ID、修正或删除；内容或索引变化后
旧引用确定失效。通用选择器不再让稳定 ID 或唯一名称绕过重复底层身份取第一项；扩展数据参与
引用校验但不被查询暴露。capabilities 公开 `markerRepairReference`，三配置 51/51 静默回归及
Codex 诊断→引用→预演→改 ID→复查→校验→旧引用/删除保护→源 SHA 复核用户旅程通过。
当前第 421–430 轮补齐损坏 Relation 身份的错误恢复闭环：`relations` 返回绑定 Scenario、索引
和完整 Relation 快照的 `relationRef`，空/重复 ID 可精确改 ID、联合修复端点或删除；稳定 ID
重复时不再静默选择第一项。身份、端点、延迟、时钟、条件、严重性、说明和扩展数据参与引用校验，
但内部 Event ID 与扩展载荷不被查询暴露。capabilities 公开 `relationRepairReference`，三配置
52/52 静默回归及 Codex 诊断→引用→预演→改 ID→复查→校验→旧引用/删除保护→源 SHA 复核
用户旅程通过。
当前第 431–440 轮补齐自动化校验的结构身份闭环：`validate` 同时审计 Project、ClockDomain、
Scenario、Lane、Segment、Event、Relation 与 Marker 的空/重复稳定 ID，以稳定代码、对象类型、
数组路径、命中数和上下文精确定位，并将问题计入 error；身份与语义计数彼此独立。capabilities
公开 `structuralIdentityValidation`，三配置 53/53 静默回归及 Codex 假阳性复现→结构定位→
引用修复→重新校验→源 SHA 复核用户旅程通过。
当前第 441–450 轮补齐自动化批量编辑的后置安全边界：`apply` 对完整源与候选工程同时校验并比较 error
多重指纹；健康工程不能产生无效结果，损坏工程可保存错误数下降且无新增错误的渐进修复。无关
编辑保留既有错误、身份问题增加或显式修复产生新错误时均不返回候选且不写文件。
`validationGuard` 公开源/候选计数、新增/消除错误、显式修复和接受原因；capabilities 公开
`nonRegressiveApplyValidation`，三配置 54/54 静默回归及 Codex 拒绝→渐进修复→最终有效用户
旅程通过。

## 不变量

- Scenario、Event、Lane、Segment、Relation 是唯一事实源。
- 时间事实仅使用整数 tick 或显式有理数；不持久化浮点时间。
- 内部关系使用稳定 ID，显示名称不作为主键。
- 领域、时间、验证、生成和比较逻辑不依赖 QtWidgets。
- 生成的 testbench、assertion、图片和报告是单向派生产物。
- 工程使用可读、版本化、可迁移的格式，未知字段不被静默丢弃。
- 外部 trace 保持为工程相对或可解析引用，不复制完整大文件到 JSON。
- 不直接读取其他应用的内部数据库。

## 验收矩阵

| 交付项 | 当前状态 | 证据或缺口 |
|---|---|---|
| 可运行桌面应用 | 完成 | 单一 WaveCanvas 工作区、波形编辑、导出参数原位纠错与恢复快照可运行；trace/Compare/集成由 CLI 和领域模块保留 |
| 示例工程 | 阶段 1 完成 | `examples/handshake/project.wave.json` |
| 自动化测试 | 完成 | 核心 31 组及 54 个 CTest 入口均通过；Wave Edit 长时间轴专项验证右向 Segment 预览/取消/提交/Undo、释放停止、左向 Shift 范围、Fit selection 满宽缩放、Esc 恢复 Fit scenario、完整概览返回、End/Ctrl+Home 边界跳转、缩放保持、End 文本框焦点隔离、Bus/Clock Ctrl+Left/Right 相邻边沿跳转、`0x35`/`X`/`0`/`1` 采样反馈、左侧所选值标签、边界反馈、信号选中提示、Bus/Enum Enter 精确预填/取消/提交/Undo、两条 Enum 范围的共有符号/非法草稿/原子提交/撤销/Esc 清理、显式范围期间 Up/Down 保持、Shift+Up/Down 目标扩缩/草稿门禁/固定栏刷新及 Shift+Home/End 边界选择/无效果/文本焦点隔离及 Ctrl+A 单/多信号完整时间选择/视图保持/输入框全选隔离及 Ctrl+Shift Bus/Clock 真实边沿范围扩缩/无边沿反馈/文本隔离；长列表跨视口拖动专项验证上下边缘持续滚动、真实插入位置、Esc 恢复视口与顺序、上下双向放置、单步 Undo、Up/Down 首末导航、Group 跳过、最小滚动、隐式 Bit `0` 值反馈、End 文本框焦点隔离及 Delete 安全；信号名列专项验证长名称完整提示、分隔线拖动、Esc 恢复、双击自动适配、偏好持久化及无工程脏状态；Group 标题专项验证恢复后选择、F2/双击重命名、右键属性、拖动、Delete、成员解组、逐步 Undo 及回到 Saved 基线；隐藏项专项验证画布末尾/Edit 菜单恢复入口、选择清理、原子 Undo/Redo 及回到保存基线；范围专项验证 Event/Relation 一致性，独立用户旅程验证多草稿门禁、无扩展名保存补全/Open 过滤、最近目录与最近工程、New→Open 650 ns 磁盘回读/成功反馈、当前脏工程最近项安全无操作、700 ns 工程文件直接拖放、脏工程 Open 取消/选定确认顺序与导出两级纠错；Wave Edit 专项继续验证 Bus preset MIME 拖放；高级属性专项验证空名称/大小写重名、其他无效输入、现有 Event 不兼容的结构变更原位纠错与空提交；autosave 专项验证正式保存清理已完成及在途过期快照、更新快照自动恢复、旧/损坏快照回退、Discard 永久清理及首次保存前 Untitled 恢复 |
| README/PLAN/GOAL | 完成 | 根目录三个文档 |
| 工程格式说明 | 阶段 1 完成 | `docs/project-format.md` |
| SystemVerilog/cocotb 示例 | 阶段 3 完成 | `examples/handshake/generated` |
| SVG/PNG/PDF 示例 | 阶段 3 完成 | `examples/handshake/exports` |
| 桌面导出工作流 | 持续迭代 50 完成 | Full/selection/range、Logical width、PNG DPI、PDF span 与附加层；无效范围/跨度原位纠错、草稿/焦点保留及修正后目录选择 |
| 项目文件工作流 | 持续迭代 51/52/53/57/58/59/60/61/62/63 完成 | New 直接空白波形、Open/Save As `*.wave.json` 过滤、无扩展名保存补全、显式扩展名保留、首次保存 Documents/Home、最近目录与最多 5 个最近工程、一键重开及当前脏工程安全无操作、单文件拖入画布直接打开且不影响 Bus preset 拖放、保存→New→Open 磁盘回读及 `Opened <path>` 反馈、脏工程先选目标再确认且取消选择不改当前工程；正式保存清除已完成及仍在途的过期恢复快照；启动/Open 自动采用有效且更新的快照，较旧或损坏快照保留正式工程；Discard 等待并清除在途快照且不重新调度；首次保存前的 Untitled 修改写入隔离快照，无参数启动安全恢复且仍要求 Save As；Undo 返回正式保存点时恢复 Saved、清理已完成/在途快照，Redo 离开保存点重新标记 Unsaved changes |
| VCD 导入示例 | 阶段 4 完成 | `examples/handshake/traces/handshake_actual.vcd` |
| Clock 覆盖与属性编辑 | 持续迭代 1/34/35/49 完成 | gated/disabled、项目级 cycle 重定位、跨输出一致性、拍级 Run、右键周期编辑的结果/无效果反馈以及无效时间原位纠错与草稿保留 |
| Lane/Group 属性编辑 | 持续迭代 2/35/49/54/55/56/64/65 完成 | stable ID、结构校验、右键轻量参数结果/无效果反馈、Redo 保留；完整属性窗口的空名称、大小写不敏感重名、位宽、Enum、clock/group 引用、颜色及会使现有 Segment/Event 失效的结构变更均在关闭前原位纠错并保留草稿/焦点，原值确认不产生脏状态或撤销项；Visible 关闭后提供画布末尾与 Edit 菜单恢复入口，并以单个命令恢复全部隐藏项；可见 Group 标题可右键进入完整属性，F2/双击就地改名 |
| Lane/Group 删除 | 持续迭代 3/40/65 完成 | Event/Relation/trace 依赖清理、Group 解组、确认前准确影响数量、完成/恢复状态及离屏 Undo 回归；Group 可从画布标题直接 Delete，确认成员解组并可原子恢复 |
| Lane/Group 显示顺序 | 持续迭代 4/41/65/67 完成 | 可撤销重排、跨视图顺序一致性、生成确定性、成功/原位结果反馈及 Undo/Redo 恢复提示；Group 标题直接拖动及 Move group 恢复反馈通过；长列表标题拖入上下边缘可持续滚动并更新真实插入位置，Esc 恢复原视口与顺序，跨视口放置仍为单个命令 |
| Relation condition | 持续迭代 5 完成 | Expected/Actual 整数 tick 采样、四态条件、错误定位及报告可见性 |
| 画布信号管理 | 持续迭代 9/14/15/17/35/40/41/47/48/49/64/65/66/67/72/73/74/75 完成 | 三按钮、随机颜色、lane 内联属性、标题左键/右键选择名称/Delete/F2 目标反馈、双击/F2 就地重命名及入口/取消恢复反馈、原处校验、依赖感知删除影响/完成/恢复反馈、快速参数真实结果/空历史抑制与无效值原位纠错、快速新增事务门禁、标题拖动 Esc 取消、跨位置/原位结果反馈及时钟自动关联；隐藏后立即清理不可见选择与 Bus 面板，并持续显示恢复入口；恢复或新增的 Group 可在唯一画布中选择、重命名、编辑、重排和删除；Up/Down 在可见信号间选择并跳过 Group，长列表仅做最小垂直滚动且键盘选择不武装整条信号 Delete；所选信号标题即时显示当前编辑 tick 的采样值，未选信号不增加标签；Bus/Enum 选中与边沿导航反馈公开 Enter 编辑入口，Enum 同时公开声明符号 |
| 信号名列可辨识性 | 持续迭代 66 完成 | 长层级名称中间省略并保留首尾、完整名称悬浮提示、140–480 px 分隔线实时拖动、Esc 恢复、双击按可见名称自动适配及跨窗口偏好恢复；调整列宽不进入 Undo 或工程 dirty，独立 offscreen 专项通过 |
| 跨应用契约 | 阶段 6 完成 | `docs/integration-contracts.md` 与 `wave-bridge` |
| 光标模式增强 | 持续迭代 7/42/43/44/45/46/47/48 完成 | 活动/临时/锁定光标、Reference/Cursor 正负 Δ 最终结果、采样值、锁定对象创建/选择/移动/删除结果、边界无效果反馈、Undo/Redo 恢复提示、删除后重建唯一命名、Bus 浮层/右键编辑隔离、中断平移清理、标题/Marker 选择互斥以及左键/右键目标切换反馈的离屏回归 |
| Wave Edit 直接编辑 | 持续迭代 8/9/10/13/18/19/20/21/22/23/24/25/26/27/28/29/30/31/32/33/34/36/37/38/39/68/69/70/71/72/73/74/75/76/77/78/79/80/81/82 完成 | Segment 选择/移动/缩放/删除及结果反馈、Bit 单拍/多拍/四态输入与精确一拍 Pulse、拍级 Toggle/Clear/Delete 的依赖提示、重复写值/Clock Run/Paste 无空历史及持久选中框、局部边沿虚线预览、固定非遮挡范围赋值、可见 Copy/Cut/Paste/Clear、自描述且兼容旧格式的单/多 lane 目标感知 Paste、源删除后指定目标恢复、越过 End 时完整宽度原子延长、不兼容拒绝、粘贴结果持久选择、范围端点手柄、安全重定向及关系安全清理；各类 Wave Edit 拖动可在水平边缘持续滚动，Esc 恢复起始视图，释放沿用单命令语义；选中信号后可用 Ctrl+Left/Right 严格跳到 Segment 或 Clock 的相邻真实边沿，状态栏公开入口且导航不产生命令；Up/Down 选择相邻可见信号，显式范围与拖动期间无损阻断；所选信号标题与导航状态同步显示采样值，拖动预览期间隐藏未提交模型值；选中 Bus/Enum 后 Enter 在精确 tick 打开一拍值编辑，Enum 提供声明符号补全并隐藏 Bus 专用按钮，Esc 零修改取消，提交保持单命令 Undo；同类型 Enum 多拍选择使用固定范围栏和共有符号补全原子赋值；Shift+Left/Right 从当前光标创建并调整显式时间范围，保留多 lane 目标且零模型修改；Shift+Up/Down 从活动信号连续扩缩目标、跳过 Group 并即时刷新固定范围栏；Shift+Home/End 将活动时间端直达 0/End 并沿用锚点折叠语义；Ctrl+A 将当前单信号或既有多信号目标直接选至完整时间轴并保持光标与视图；Ctrl+Shift+Left/Right 按当前活动信号的真实边沿创建或调整范围；显式范围 Paste 固定使用范围左端点，不受右侧活动端点影响 |
| 持久范围批量编辑 | 持续迭代 18/19/20/21/22/23/24/25/26/27/33/36/37/38/39/76/77/78/79/80/81/82/171–190 完成 | Bit/Bus/Enum 同类型多 lane 整段赋值、Enum 共有声明符号补全、不同 Bus 位宽生成、重复赋值/Paste 无空历史、混合类型保留 Copy/Cut/Paste/Clear、右键目标时间与单/多 lane 目标信号粘贴、schema 2 名称/类型/位宽快照、schema 1 兼容、Paste at End 与完整宽度自动延长、数量/类型/位宽原子校验、两步移动范围、端点就地修正、标题/标尺同次重定向、键盘时间端点步进/边界扩缩与折叠、键盘目标信号扩缩、范围草稿门禁、Ctrl+A 完整时间选择、Ctrl+Shift 真实边沿扩缩、无边沿反馈、文本焦点隔离、显式范围 Paste 左端点与活动端点隔离、紧邻重复、正文整块 Move/Ctrl-Copy 及单步 Undo |
| 范围紧邻重复 | 持续迭代 171–180 完成 | 范围右键与 Edit 菜单将单/多信号所选范围一次写到紧邻后方；源和系统剪贴板保持不变，结果范围自动选中，隐式空白、扩展语义、Relation 清理及 End 延长均包含在专用 `Duplicate range` 单步 Undo/Redo 中；已一致结果不污染历史或清除 Redo，offscreen 自动化与截图通过 |
| 显式范围整块拖动与复制 | 持续迭代 181–190 完成 | 选区正文拖动一次移动全部目标，Ctrl 拖动复制且不改系统剪贴板；源实线/目标虚线、精确标签、Sync/Async/Alt、自动滚动、重叠拒绝、Esc/失键恢复、结果持续选择、stable Segment/Event、Relation 同步、End 延长及专用 Undo/Redo 均通过核心与 offscreen 回归 |
| Undo/Redo 可见目标恢复 | 持续迭代 191–200 完成 | 范围 Move/Copy、紧邻重复、显式目标 Paste 及 Segment 移动/复制/微移/边界调整按历史方向恢复源或结果目标并最小揭示；动态按下/释放 Ctrl 的预览与最终 Move/Copy 命令一致，无关历史不跳转 |
| 跨信号范围整块转移 | 持续迭代 201–210 完成 | 范围可纵向拖到连续兼容信号块，保留按下行偏移；同时间不相交 Ctrl Copy、两阶段重叠 Move、源实线/目标虚线、红色非法目标、类型/位宽/值校验、上下自动滚动、取消视图恢复及方向化 Undo/Redo 均通过核心与 offscreen 回归 |
| 范围转移内容感知预览 | 持续迭代 211–220 完成 | Bit 0/1/X/Z、Bus 数值/多态/Don’t care、Enum 符号及 Clock 覆盖按实际目标形状预览；标签避让、值级拒绝、重叠 Copy 红色阻断、非法目标不绘制内容及仅扫描可见目标行均通过 offscreen 回归 |
| Clock 范围批量编辑 | 持续迭代 221–230 完成 | 纯 Clock 显式范围直接显示 Run/Gate/Disable 与 R/G/X；多 Clock 原子赋值、单步 Undo/Redo、无效果历史抑制、Relation 安全清除、混合类型保护及 offscreen 固定栏视觉验收均通过 |
| 键盘时间步长一致性 | 持续迭代 231–240 完成 | Left/Right 与 Shift 变体按公开 Timing 移动；Sync period/phase/active edge、离网格严格边界、非整拍 End、Async 1 tick、多 Clock 目标、提示与零模型修改均通过 offscreen 回归 |
| 显式范围端点精确输入 | 持续迭代 241–250 完成 | Ctrl+G 与可点击范围摘要打开紧凑端点栏；固定锚点、Other edge、跨锚点、时间单位/cycle、非法值与空范围保护、多信号保持、草稿门禁、工具栏恢复和零模型修改均通过 offscreen 回归 |
| 小数物理时间精确输入 | 持续迭代 251–260 完成 | 字符串有理换算与最多三位紧凑显示覆盖 Go to、范围端点、End、Clock、导出和共享参数；正负值、前导小数、尾随零、亚 timebase、fractional tick、不可整除、边界与溢出均通过核心/offscreen 回归 |
| 显式范围精确宽度 | 持续迭代 261–270 完成 | 端点/宽度模式原位切换、固定锚点和方向、Other edge 反转、小数物理时间/整数 tick/cycle 宽度、零宽/方向边界纠错、目标与状态隔离及 1440/960 px offscreen 回归均通过 |
| 可嵌入自动化层与统一 CLI | 持续迭代 271–280 完成 | `waveautomation` 公开 inspect/validate/apply；`wave-cli` 提供版本化 JSON、五类原子 operation、stdin、dry-run、原子输出、SHA 冲突保护、稳定身份/颜色/结果及结构化退出码，核心/进程/PowerShell 管道和三配置 32/32 回归通过 |
| CLI 定位、采样与信号管理 | 持续迭代 281–290 完成 | 精简 inspect、按时刻/周期采样、精确物理时间 range/End、信号删除/重排/属性修改及 Clock 修改共九类 operation，失败原子性、确定性、依赖清理和三配置 35/35 回归通过 |
| CLI 局部波形、范围转移与恢复保护 | 持续迭代 291–300 完成 | 时间/Lane 窗口及依赖闭包、多信号原子赋值、同/跨 Lane Copy/Move、值断言、默认覆盖拒绝、显式确认、原地备份及备份冲突保护，十一类 operation 和三配置 37/37 回归通过 |
| CLI 从零建工程与多信号时序 | 持续迭代 301–310 完成 | 确定性 new、首次写盘前文件/stdin operations、单/多 Lane 重复序列、tick/物理时间/cycle 步长、End 自动延长、后置断言、变更摘要、已有输出与失败零半成品，十二类 operation 和三配置 39/39 回归通过 |
| CLI 名称寻址与紧凑信号定位 | 持续迭代 311–320 完成 | `signals` 紧凑查询、稳定 ID/唯一名称统一选择器、同批次名称引用、规范 ID 报告、歧义与缺失原子拒绝，十二类 operation 和三配置 41/41 回归通过 |
| CLI Group/Enum 符号刺激建模 | 持续迭代 321–330 完成 | Group 与 Enum 确定性创建、四类信号同批次名称归组、Enum 映射完整替换、符号序列/采样、失效与溢出原子拒绝，十三类 operation 和三配置 42/42 回归通过 |
| CLI 时序关系与阶段标注 | 持续迭代 331–340 完成 | Relation 以 Lane 与精确现有边沿寻址，支持三种延迟单位、增删改、确定性身份和无效果抑制；Marker 支持点/区间、五种用途类型及名称管理；十九类 operation、失败原子性和三配置 43/43 回归通过 |
| CLI 能力发现与 Group 生命周期 | 持续迭代 341–350 完成 | 无工程 capabilities 与嵌入 API 共用版本化能力文档；Group 支持属性修改、重排、成员保持删除、影响报告、名称串联和无效果抑制；二十二类 operation、失败原子性和三配置 44/44 回归通过 |
| CLI 整信号安全复制 | 持续迭代 351–360 完成 | `duplicate-signal` 原子复制完整属性/Segment，唯一名称、不同颜色、独立确定性 Lane/Segment ID，Clock 使用独立 ClockDomain；依赖隔离、同批次串联、失败原子性和三配置 45/45 回归通过 |
| CLI 安全缩短与显式时间轴截断 | 持续迭代 361–370 完成 | 空白尾部直接缩短；内容损失需显式 `truncate`，Segment/Marker 裁边、越界 Segment/Event/Marker 删除和 Relation 依赖清理均可撤销；capabilities、影响计数、失败原子性及三配置 46/46 回归通过 |
| CLI Relation 就绪边沿查询 | 持续迭代 371–380 完成 | `edges` 按 Lane/范围/四类边沿返回前后值、动作、时钟、候选计数与可用性；不暴露 Event/Segment ID，歧义及非法类型结构化拒绝，capabilities 与三配置 47/47 回归通过 |
| CLI 既有 Relation 紧凑查询 | 持续迭代 381–390 完成 | `relations` 按文本/严重性/Lane/范围返回规范 ID、两端 Lane+tick、实际延迟与端点问题；缺失/重复/脱离波形端点可诊断且不暴露内部 ID，capabilities 与三配置 48/48 回归通过 |
| CLI 既有 Marker 紧凑查询 | 持续迭代 391–400 完成 | `markers` 按身份/备注/用途/范围返回规范 ID、精确起止、宽度与健康问题；重复身份、非法几何、半开边界和单时钟周期推断均有进程契约，capabilities 与三配置 49/49 回归通过 |
| CLI 损坏 Relation 显式端点修复 | 持续迭代 401–410 完成 | `update-relation` 以完整 Lane+时刻修复缺失/重复/脱离/越界/歧义端点，保留健康另一端；双端修复、修复报告、Event ID 可寻址性、失败原子性和三配置 50/50 回归通过 |
| CLI 损坏 Marker 快照引用恢复 | 持续迭代 411–420 完成 | `markerRef` 以索引+完整快照摘要精确修复或删除空/重复 ID Marker；支持 `newId`、几何修复、过期引用保护、扩展隔离、单步 Undo/Redo 和三配置 51/51 回归 |
| CLI 损坏 Relation 快照引用恢复 | 持续迭代 421–430 完成 | `relationRef` 以索引+完整快照摘要精确修复或删除空/重复 ID Relation；支持 `newId`、端点联合修复、过期引用保护、扩展隔离、单步 Undo/Redo 和三配置 52/52 回归 |
| CLI 核心结构身份校验 | 持续迭代 431–440 完成 | `validate` 拒绝八类核心对象的空/重复稳定 ID，返回稳定代码、精确数组路径、作用域计数与独立身份/语义汇总；假阳性修复、source SHA 保护及三配置 53/53 回归通过 |
| CLI 非退化后置校验门禁 | 持续迭代 441–450 完成 | `apply` 拒绝保留或引入 error 的普通编辑，允许无新增错误的渐进恢复；错误指纹、结构化门禁报告、失败零输出、两阶段修复及三配置 54/54 回归通过 |
| Event/Relation 一致性 | 持续迭代 19/24/30/31/32 完成 | 同 tick Event ID 稳定重映射、消失边沿依赖精确清理、无关 Relation 保留、范围、Segment Clear、单信号写值与拍级 Toggle/Clear 的完整 Scenario Undo/Redo 及用户可见依赖提示 |
| 工具栏收敛 | 持续迭代 9/10/13/14/20/21/24/25/37/46/69 完成 | 无模式直接编辑；常驻仅保留临时 Measure、缩放与单一上下文 Fit，显式范围时同一动作改为 Fit selection、清除后恢复 Fit scenario，不新增按钮；Measure 内不暴露波形编辑菜单；范围操作按选择临时显示且不覆盖画布；Copy/Cut/Paste/Clear 直接可见，混合选择不显示无效赋值控件，960 像素无裁切 |
| 纯波形桌面布局 | 持续迭代 10 完成 | 无 Project/Inspector/Scenario Dock，WaveCanvas 直接占据中央区，离屏布局回归通过 |
| 直接时间交互 | 持续迭代 11/12/13/14/26/27/39/45/68/69/70/71/72/77/78/79/80/81/82/231–270 完成 | 7 像素轻吸附、范围端点就地拖动、标尺同次清除范围并定位、Alt 临时绕过、平移/方向键、模式切换取消未释放平移与吸附提示、可撤销 End 直接输入及 Paste 自动延长 End/滚入新尾部，非法缩短原处反馈；放大时间轴上的 Wave Edit 拖动在左右边缘以实时吸附和预览持续跨视口推进；显式范围通过同一 Fit 动作满宽聚焦，Esc 后一键返回完整场景；Home/End 与 Ctrl 变体在保持缩放和选择的同时跳到时间轴边界，文本焦点不被劫持；Ctrl+Left/Right 按当前信号跳到严格相邻的 Segment/Clock 边沿，无边沿时原位提示；Up/Down 保持时间与水平视图并最小滚动到相邻信号，画布内文本焦点不泄漏；Left/Right 与 Shift 变体按公开 Timing 移动，Sync 使用关联 Clock 的 period/phase/active edge、Async 使用 1 tick、无关联 Clock 回退 10 ns，边界原位反馈且范围值文本焦点不被劫持；Shift+Up/Down 调整相邻目标信号并做最小纵向滚动；Shift+Home/End 直达时间轴边界、保留多信号目标且不劫持文本选择；Ctrl+A 选择当前目标的完整时间轴并保持光标、缩放和视口；Ctrl+Shift+Left/Right 复用当前信号真实边沿精确扩缩范围；显式范围可由 Ctrl+G 或范围摘要精确输入活动端点或固定锚点方向上的精确宽度；小数物理时间按字符串有理数精确输入并紧凑显示，tick/cycle 保持整数；显式范围粘贴使用范围左端点，无范围时保留光标或右键定位 |
| 画布视觉层次 | 持续迭代 12/29/66/73 完成 | 较浅中性背景、透明选中态、选中区域网格可见性、脱离 hover 的 Bit 拍级持久选中框，长层级名称首尾辨识效果、所选信号青色采样值标签与名称宽度隔离均通过离屏截图验收 |
| Bus/Enum 快捷值 | 持续迭代 12/13/14/45/46/74/75/88/89/120 完成 | 隐式 X、当前 Beat/Segment 附近的就地编辑浮层、HEX/BIN/DEC/OCT、每 lane 最近自定义值、0/X/Z/Don't care、直接值输入、非法值纠错、旧 Reserved 兼容、Undo/Redo；Don’t-care 具有独立纹理和 Expected/Actual 通配语义，普通 X 不变；选中 Bus/Enum 后 Enter 精确预填当前值，当前 Beat 在画布中以透明琥珀框显示；Tab/Shift+Tab 连续提交并前后移动，Enum 提供声明符号补全并隐藏 Bus 专用预设 |
| Bus/Enum 连续拍录入 | 持续迭代 89 完成 | Beat 编辑中 Tab 提交并前进、Shift+Tab 提交并后退、Enter 提交并结束；Sync 按时钟拍推进，Async 保留非对齐偏移；非法值不离开当前拍，首尾边界保持原位并给出方向提示；重复确认不产生空历史 |
| Bus/Enum 高效录入 | 持续迭代 90/91/92/93 完成 | 未修改的隐式 X 可直接前后跳过且不实体化；Up/Down 按进制增减确定数值，Ctrl+Up/Down 循环每 lane 最近值，Ctrl+Enter 提交后保持当前目标；非法值、边界、焦点、Undo/Redo 与 Saved 语义均通过 offscreen 回归 |
| 编辑目标稳定性与就地反馈 | 持续迭代 121–170 完成 | 被动悬浮不再移动编辑光标；标尺单击/拖动/取消、范围右键保留与可见关闭、同次安全改选、Bus/Enum Beat/Segment/导航/Apply/关闭及无效草稿保护均通过 offscreen 回归；单击严格选择 Beat，实际拖动、双击或 F6 才进入 Segment；Beat Delete/Clear、快捷预设与直接输入保持一拍目标，Segment Clear 后回落首拍；快速新增、Bus/Enum 与范围值合法草稿可同次继续，非法草稿原位保留，范围输入以两级 Esc 先丢草稿再关闭选择，End 坐标变化仍安全消费当前点击；Bit/隐式 X/显式 Segment 悬浮与 Timing 文案直接公开即将操作的目标和步长，960 px 编辑器与范围栏无裁切 |
| 同步/异步波形编辑 | 持续迭代 88 完成 | 默认 Sync 以关联时钟有效边沿和周期为最小编辑单位，无关联时钟回退 10 ns；Async 允许任意整数 tick 并保留 7 像素轻吸附，Alt 临时绕过；模式切换安全取消未提交拖动 |
| Segment Ctrl 拖动复制 | 持续迭代 88 完成 | 非 Bit、非 Clock Segment 以 Ctrl+拖动显示保留源段的虚线 Copy 预览，释放后单命令提交并支持 Undo/Redo 及 Event/Relation 同步；相同范围不产生空历史 |
| 画布连续键盘导航 | 持续迭代 94/95 完成 | Tab/Shift+Tab 在当前信号前后移动一拍，PageUp/PageDown 按可见时间跨度翻页；目标高亮、最小滚动、首尾反馈、文本焦点隔离及 Scenario/Undo/Saved 零变化通过 offscreen 回归 |
| Clock 周期键盘编辑 | 持续迭代 96 完成 | 选中 Clock 后 G/X/R 对当前完整周期 Gate/Drive X/Run；周期选择、单步 Undo/Redo、覆盖清理、重复 Run 空历史抑制及提示通过 offscreen 回归 |
| 精确拖动时序反馈 | 持续迭代 97/98 完成 | Segment 移动/缩放/Ctrl 复制以及范围创建/端点修正期间，画布与状态栏直接显示精确起点、终点和 width；标签视口约束、网格可见和零模型预览通过 offscreen 验收 |
| Segment 相邻复制 | 持续迭代 99 完成 | Bus/Enum 右键将完整 Segment 一步复制到紧邻下一段；值与扩展语义保留、结果选中、Event/Relation 同步、单步 Undo/Redo、相同目标空历史、End 边界反馈及同步模式真实点击命中通过 offscreen 回归 |
| Segment 键盘闭环 | 持续迭代 100–120 完成 | Edit > Segment 提供选中、前后导航、前后复制、移动及四种边界调整；F6/F7/F8 对应选中/前一段/后一段，Enter/直接输入及 Tab/Shift+Tab 连续改值，Esc 保留信号和时间。Ctrl+D 固定复制整条信号；Ctrl+Space、Ctrl+Tab、Alt+Left/Right 和方括号不再劫持系统或常用导航。上下文、边界、元数据、选择、模型零变化或单步 Undo 均通过 offscreen 回归 |
| Segment 即时可理解性 | 持续迭代 105/106/107 完成 | 单击状态、悬浮提示和 Wave Edit 指针状态公开信号、值、精确起止与宽度；实际指针采样不受同步边界吸附误导，QHelpEvent 与鼠标路径均验证零模型修改 |
| Enum 声明符号轮换 | 持续迭代 104 完成 | Enum 编辑框 Up/Down 按声明顺序循环符号并公开当前位置；只改草稿、不提前写入模型或历史，非法草稿与焦点语义保持 |
| 编辑光标锚定缩放与上下文 Fit | 持续迭代 108/109 完成 | 可见编辑光标作为 Zoom 锚点并保持屏幕位置，离屏回退视口中心；`Ctrl+0` 在显式选区与完整 Scenario 间复用单一 Fit 动作，缩放、选择、Scenario、Undo 与 Saved 语义通过 offscreen 回归 |
| 未提交草稿安全 | 持续迭代 16/17/49/50/53/54/55/56/57/58/59/60/61/78 完成 | Save/New/Open/Close/Export/点击门禁、Bus→End 条件顺序、快速新增有效先提交/无效原位阻断、轻量参数、完整 Lane/Group 属性及 Export 无效值/其他字段/焦点原位保留、Open 文件选择取消不触发丢弃且保留当前编辑、交错命令防御恢复与双击/Bus 控件隔离；未提交 Bus/Enum 范围值阻止键盘改变目标信号并恢复焦点 |
| 基础操作闭环 | 持续迭代 13/14/16/17/22/24/25/27/28/29/30/31/32/33/34/35/36/37/38/39/40/41/42/43/44/45/46/47/48/49/50/51/52/53/54/55/56/57/58/59/60/61/62/63/64/65/66/67/68/69/70/71/72/73/74/75/76/77/78/79/80/81/82 完成 | 无配置空白工程、内联创建、默认直接编辑、统一草稿与快速新增事务门禁、信号删除前准确影响与完成/恢复反馈、Bit 拍级写值/Toggle/Clear/Delete/Pulse 的依赖反馈、同值写入/Clock Run/快速参数/Paste 无空 Undo/Redo 损失、轻量参数/Export 原位纠错、项目保存/Open 命名、回读反馈与脏工程确认顺序闭环及持久选择、可见且目标感知的范围 Copy/Cut/Paste/Clear、源信号删除后的快照恢复与两步 Undo、Paste 越过 End 的完整写入/可见反馈/单步 Undo、标题/标尺无死首击、标题键盘目标及取消恢复反馈、文本焦点保护、Bus/Enum 键盘一拍编辑、Enum 声明符号提示、多拍范围原子赋值、键盘时间范围选择、键盘多信号范围调整、键盘边界范围选择与 Ctrl+A 当前目标完整时间选择与 Ctrl+Shift 当前信号真实边沿范围选择、显式范围 Paste 起点与可见反馈一致、Measure 状态/编辑边界/选择归属及用户旅程均通过离屏回归 |
| 可见信号即时查找 | 持续迭代 83 完成 | `Ctrl+F` 临时紧凑栏按可见非 Group 信号的名称/稳定 ID 大小写不敏感匹配；即时首项、Enter/Shift+Enter 与上下按钮循环、结果计数、无匹配原位反馈、最小垂直滚动、水平视图保持、显式范围门禁、关闭后选择保留及重开查询恢复均通过 offscreen 专项 |
| 精确时间直接跳转 | 持续迭代 84 完成 | `Ctrl+G` 临时紧凑栏接受整数 ps/ns/us/ms/tick 与 `cycle N`；精确光标定位、最小水平揭示、选中信号/缩放保持、非法与越界原位反馈、显式范围及草稿门禁、Bus/Enum 浮层隔离、关闭后结果保留和重开预填均通过 offscreen 专项 |
| 现有信号一键复制 | 持续迭代 85 完成 | Edit/标题右键/`Ctrl+D` 将非 Group 信号完整复制到原信号正下方；唯一名称、不同随机颜色、属性与波形一致、lane/segment ID 独立、ClockDomain 独立、Event/Relation 不复制、显式范围和文本焦点门禁、单步 Undo/Redo 及状态反馈均通过 offscreen 专项 |
| 可见项一步隐藏 | 持续迭代 86 完成 | Edit 动态动作与标题右键将信号或 Group 一步隐藏；无模态窗口、显式范围无损门禁、不可见选择清理、实时 `Show N hidden items` 与恢复说明、专用 Hide lane/group 单步 Undo/Redo、Group 复制入口隔离及 Saved 基线回归均通过 offscreen 专项 |
| 隐藏项按名单独恢复 | 持续迭代 87 完成 | 多个隐藏项时由同一底部按钮的箭头菜单按名称恢复一个信号或 Group，主体仍恢复全部；单隐藏项保持直达，原顺序/水平视图/光标保持、恢复后选中、显式范围门禁、剩余计数和单步 Undo/Redo 均通过 offscreen 专项 |
| 修改文件清单 | 完成 | 最终交付报告及 git 状态 |
| 构建和测试结果 | 完成 | `PLAN.md` 最终验证证据 |
| 性能测试结果 | 完成 | 百万 transition 可见范围基准记录于 `PLAN.md` |
| 已知限制和后续计划 | 完成 | `README.md` 与 `PLAN.md` |

矩阵中的必需交付项均具有实际行为和测试证据；第 82 轮开发视角 Debug/Release 全量构建、26/26 核心测试及 26/26 CTest 均已通过，百万 transition 指标为 Debug 25 ms / Release 7 ms；全部 GUI 路径使用 offscreen。
用户视角确认 data[7:0] 的 50–100 ns `0x35` 复制后，以键盘建立 0–10 ns 目标且活动光标停在 10 ns，固定栏仍公开 0 ps 起点；`Ctrl+V` 后 5 ns 已为 `0x35`、结果范围为 0–50 ns、状态显示 `at 0 ps` 与 `Ctrl+Z`。单步 Undo 精确恢复原波形与 Saved；固定栏、10 ns 活动光标、透明选区、波形和网格均通过离屏截图验收。
第 83 轮开发视角 Debug/Release 全量构建、26/26 核心测试及 26/26 CTest 均已通过，百万 transition 指标为 Debug 20 ms / Release 5 ms；全部 GUI 路径使用 offscreen。用户视角确认 Ctrl+F 在 20 条长列表中输入 `signal_1` 即时定位 `signal_10`，以 1/10 显示结果并可循环到 `signal_19`；Group/隐藏信号不进入结果，无匹配、显式范围门禁、关闭与重开均保留明确状态。紧凑查找栏、所选信号、波形、时间轴与网格通过离屏截图验收。
第 84 轮开发视角 Debug/Release 全量构建、26/26 核心测试及 26/26 CTest 均已通过，百万 transition 指标为 Debug 21 ms / Release 5 ms；全部 GUI 路径使用 offscreen。用户视角确认 Ctrl+G 输入 `375 ns` 后精确定位并只做必要水平滚动，选中 Bus、缩放及模型状态均保持；非法/越界输入原位纠错，`cycle 25` 使用唯一 10 ns 时钟到达 250 ns，显式范围无损阻断。紧凑跳转栏、375 ns 光标、所选信号、波形、时间轴与网格通过离屏截图验收。
第 85 轮开发视角 Debug/Release 全量构建、26/26 核心测试及 26/26 CTest 均已通过，百万 transition 指标为 Debug 19 ms / Release 4 ms；全部 GUI 路径使用 offscreen。用户视角确认右键信号标题可将 `bus` 一步复制为紧邻的 `bus_copy`，属性与波形一致、颜色和稳定身份独立，副本立即选中；Undo/Redo 精确恢复，`Ctrl+D` 复制 Clock 时生成独立 ClockDomain，显式范围无损阻断且 Event/Relation 数量不变。`bus`/`bus_copy` 相邻波形、不同颜色、选中态与状态栏通过离屏截图验收。
第 86 轮开发视角 Debug/Release 全量构建、26/26 核心测试及 26/26 CTest 均已通过，百万 transition 指标为 Debug 19 ms / Release 4 ms；全部 GUI 路径使用 offscreen。用户视角确认 Edit 与标题右键均可将 `req` 一步隐藏，显式范围先得到无损提示，隐藏后选择清空、`Show 2 hidden items` 和恢复说明立即可见，单步 Undo 回到 Saved；Group 右键 `Hide group`、复制入口隔离、Undo 及恢复全部隐藏项同样通过。`req` 消失后的其余波形、时间轴、网格、恢复按钮和状态栏通过离屏截图验收。
第 87 轮开发视角 Debug/Release 全量构建、26/26 核心测试及 26/26 CTest 均已通过，百万 transition 指标为 Debug 24 ms / Release 5 ms；全部 GUI 路径使用 offscreen。用户视角确认两个隐藏项时，底部按钮主体仍一次恢复全部，箭头菜单按名称列出隐藏 Group、`req` 与全部恢复；单独恢复 `req` 后仅该行回到原位置并被选中，Group 仍隐藏，入口变为 `Show 1 hidden item`。显式范围门禁、水平视图与光标保持、剩余计数和单步 Undo/Redo 均通过；菜单、所选行、波形、时间轴、网格及状态栏通过离屏截图验收。
第 88 轮开发视角 Release 与 Qt Creator Debug 构建均通过，两套 26/26 CTest 全部通过；全部 GUI 路径使用 offscreen。用户视角确认 Bus 固定编辑栏明确显示 Beat/Segment、时间范围、进制、直接值、最近值和四个快捷预设，不遮挡画布；默认 Sync 修改按一拍对齐，Async 可偏离时钟边沿；Ctrl 拖动复制期间源段保持可见、目标为虚线 Copy 预览，释放后可单步 Undo/Redo。拖动开始后编辑栏自动收起，无陈旧目标、弹窗、裁切或桌面抢焦点。
第 89 轮开发视角 Qt Creator Debug/Release 构建均通过，两套 26/26 CTest 全部通过，百万 transition 指标为 Debug 20 ms / Release 4 ms；全部 GUI 路径使用 offscreen。用户视角确认输入一个 Bus/Enum 拍值后按 Tab 可连续进入下一拍，Shift+Tab 返回上一拍，Enter 仍用于完成；当前目标由工具栏范围和画布透明琥珀框共同标明。Sync 按拍推进，Async 保留离开同步边界的偏移；非法值、时间轴首尾和重复确认均不会静默跳拍或制造空历史。
第 90–99 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各 26/26 CTest 全部通过，核心入口 26/26，百万 transition 指标为 Debug 20 ms / Release 4 ms；全部 GUI 路径使用 offscreen。用户视角确认隐式 X 跳拍、数值草稿步进、最近值循环、留驻提交、拍级/可见页导航、Clock 周期键盘编辑均无需新增模式或弹窗；Segment 与范围拖动在释放前公开精确起止和宽度，Bus/Enum Segment 可从右键一步复制到下一段并单步撤销，越过 End 时明确提示且不污染历史。连续录入、Ctrl 复制预览和持久范围三张离屏截图确认无遮挡、裁切、桌面抢焦点或视图跳动。
第 100–109 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，核心入口 Debug/Release 各 26/26，百万 transition 指标为 Debug 20 ms /
Release 7 ms；全部 GUI 路径使用 offscreen。用户视角确认 Segment 双向复制、显式 Segment 导航、
按编辑单位微移、Enum 符号轮换、单击/悬浮/指针精确反馈、编辑光标锚定 Zoom 与 `Ctrl+0`
上下文 Fit 均无需新增模式或弹窗。Ctrl 复制、Enum 编辑和 Fit selection 三张离屏截图确认信号值、
精确宽度、固定栏、透明选区、波形与网格清晰，无模态窗口、遮挡、裁切、桌面抢焦点或视图跳动。

第 110–119 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，核心入口 Debug/Release 各 26/26，百万 transition 指标为 Debug 21 ms /
Release 4 ms；全部 GUI 路径使用 offscreen。用户视角确认选中 Segment 后可直接进入完整段改值、
连续前后录入、从编辑光标选段或进入前后段，并以 `[ ]` 和 Shift 变体精确调整左右边界；Esc
退出后信号与时间保持。原值确认、纯导航、边界受阻不污染历史，真实边界修改各为一个 Undo。
固定编辑栏、拍级目标、Bus 显式值/隐式 X、波形和网格通过离屏截图确认清晰，无模态窗口、遮挡、
裁切、桌面抢焦点或视图跳动。

第 120 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过；全部 GUI 路径使用 offscreen。用户视角确认当前目标、关联时钟和真实步长
无需猜测即可读取，Segment 命令从 Edit 菜单可发现，Bus/Enum 编辑浮层跟随当前 Beat/Segment，
960 px 范围工具栏和状态栏均无裁切。离屏截图确认波形、网格、选区和浮层层级清晰。

第 121–140 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过；全部 GUI 路径使用 offscreen。用户视角确认编辑光标不会随被动悬浮漂移，
标尺可直接查样并安全取消，范围右键与关闭不丢目标，Bus/Enum 的范围、导航、提交和关闭均可见，
非法草稿不会被范围切换绕过。离屏截图确认两行 Bus 编辑器、拍级目标、选中行透明背景、波形和
网格无裁切、遮挡或意外视图跳动。

第 141–150 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，耗时分别为 16.98 s、16.83 s、16.52 s；全部 GUI 路径使用 offscreen。
用户视角确认单击 Bus/Enum 始终只编辑一拍，实际拖动、双击或 F6 才进入 Segment；Delete、
Backspace、Clear、快捷预设、直接输入与 Apply 均与可见 Beat/Segment 目标一致。Segment Clear
后自动回到首拍，单步 Undo 可恢复完整段且无效果操作不污染历史。960 px 离屏截图确认 Target、
范围、进制、直接值、最近值、快捷预设、Clear 与 Apply 全部可见，波形和网格无遮挡。

第 151–160 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，耗时分别为 17.61 s、18.38 s、16.62 s；全部 GUI 路径使用 offscreen。
用户视角确认快速新增、Bus/Enum Beat 与多 Bus 范围的合法草稿均可在提交后继续同一次目标点击，
双击和右键同样直达新 Segment 或菜单；隐式 X Beat 具有真实选择，无效单拍或范围值保留原目标、
草稿和修正入口且零部分写入。Timeline End 仍先提交再按新坐标操作。离屏截图确认旧拍 `0x3c`
结果、新隐式 X Beat 目标、编辑器、波形和网格可同时辨认。

第 161–170 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，耗时分别为 17.10 s、17.01 s、16.85 s；全部 GUI 路径使用 offscreen。
用户视角确认非法多 Bus 范围草稿第一次 Esc 只清空输入并保留 20–50 ns 双 lane 选区、输入焦点
和全部操作控件，第二次 Esc 才关闭范围；两步均不修改模型或历史，重新框选后的合法赋值和原子
Undo/Redo 正常。960 px 离屏截图确认选区、手柄、固定栏、状态恢复提示、波形和网格同时清晰。

第 171–180 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，耗时分别为 18.10 s、18.09 s、17.79 s；全部 GUI 路径使用 offscreen。
用户视角确认 req/ack 的 80–100 ns 可由一次范围右键操作重复到 100–120 ns，源范围与系统
剪贴板保持不变，结果继续选中；Relation 清理和恢复路径可见，专用 Undo/Redo 原子恢复。普通
Clock 已一致范围不会产生空历史或破坏上一真实 Redo。离屏截图确认结果选区、手柄、编辑光标、
固定栏、波形、刻度和网格同时清晰。

第 181–190 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，耗时分别为 17.51 s、17.22 s、16.68 s；全部 GUI 路径使用 offscreen。
用户视角确认 req/ack 的 80–100 ns 双信号选区可直接拖动到 120–140 ns，Ctrl 拖动复制到
140–160 ns 时源区间和系统剪贴板保持不变；源实线、目标虚线、Move/Copy 精确标签、重叠拒绝、
Esc 恢复、结果持续选择及单步 Undo/Redo 均可见且一致。完整 Segment/Event 身份、Relation、
无效果历史和 End 延长由核心回归覆盖，三张离屏截图确认刻度、网格与状态反馈无遮挡。

第 191–200 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，耗时分别为 17.63 s、17.28 s、16.96 s；全部 GUI 路径使用 offscreen。
用户视角确认范围 Move/Copy、紧邻重复、显式多目标 Paste 和 Segment 操作的 Undo 会回到源目标，
Redo 会回到结果目标并最小揭示。两拍显式目标粘贴一拍内容可在 20–40 ns 原目标与 20–30 ns
结果间准确往返；拖动中按下 Ctrl 后提交为 Copy，释放 Ctrl 后提交为 Move，预览、指针、状态和
最终数据一致。三张离屏截图确认恢复选区、动态 Copy 预览、Paste 结果、刻度和网格无遮挡。

第 201–210 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，耗时分别为 18.08 s、17.95 s、17.56 s；全部 GUI 路径使用 offscreen。
用户视角确认 req 的 80–100 ns 可直接纵向 Copy/Move 到 ack，无需重新选择目标；多信号块保留
按下行偏移，重叠链式映射不会污染源数据。类型不同的 data[7:0] 在释放前显示红色拒绝预览且不
修改模型或历史；长列表上下边缘自动滚动，Esc 恢复源选区和原视图。三张离屏截图确认源/目标、
非法目标、映射标签、精确时间、滚动反馈、刻度和网格无遮挡。

第 211–220 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，耗时分别为 18.75 s、18.43 s、18.15 s；全部 GUI 路径使用 offscreen。
用户视角确认 Bit 高低电平、Bus Don’t care、Enum DONE 和 Clock GATED 均以真实目标形状显示，
不再只有抽象矩形；标签移到范围外。Enum 目标缺少 WAIT_ACK 及重叠 Copy 会在释放前红色拒绝且
不绘制误导内容，Esc 后模型、历史和 Saved 保持不变。五张离屏截图确认值、符号、覆盖、边框、
标签、刻度与网格无遮挡。

第 221–230 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，耗时分别为 18.01 s、18.15 s、17.79 s；全部 GUI 路径使用 offscreen。
用户视角确认 24–36 ns 纯 Clock 范围直接显示 `1 Clock`、Run/Gate/Disable 和 R/G/X；多 Clock
X/R/G/Run 均一次作用于全部目标、单步恢复，重复 Gate 不污染历史。Clock/Enum 混选按 G 时模型、
历史、Saved 和选区保持不变。离屏截图确认固定栏、GATED 覆盖、目标虚线预览、刻度与网格无遮挡。

第 231–240 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，耗时分别为 18.13 s、18.39 s、18.33 s；全部 GUI 路径使用 offscreen。
用户视角确认周期 12 ns 的 clk_2 上，Left/Right 与 Shift 变体均按公开 Timing 到 24/36 ns；
离网格位置、2 ns phase、Falling active edge、非整拍 End、双 Clock 目标及 Async 1 tick 均使用
确定的严格边界。导航全程不修改 Project、Undo 或保存状态；离屏截图确认 Timing、范围栏和波形一致。

第 241–250 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，耗时分别为 18.34 s、18.17 s、17.96 s；全部 GUI 路径使用 offscreen。
用户视角确认 0–10 ns 范围可由 Ctrl+G 精确改为 0–37 ns、cycle 5 对应的 0–50 ns，并以
`Other edge` 跨锚点改为 50–70 ns；非法时间、空范围、多 Enum 目标和非法范围值草稿均保持原位
纠错且零模型修改。离屏截图确认紧凑端点栏、透明选区、波形、刻度和网格无遮挡。

第 251–260 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，耗时分别为 18.36 s、18.12 s、17.84 s；全部 GUI 路径使用 offscreen。
用户视角确认 2.5 ns Go to、37.5 ns 范围端点、`.5 ns` End 增量、0.012 us Clock 周期及小数
导出参数均精确生效并紧凑回显；fractional tick、亚 timebase、不可整除和溢出均原位拒绝。
离屏截图确认 2.5 ns 输入、光标标签、所选 Bus、波形、刻度和网格无遮挡。

第 261–270 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
26/26 CTest 全部通过，耗时分别为 18.13 s、18.21 s、17.88 s；全部 GUI 路径使用 offscreen。
用户视角确认范围模式标签可原位切换端点/宽度，25.5 ns、12500 ticks 与 cycle 3 从固定锚点
精确生效，Other edge 反转方向；零宽和方向越界保留选择并显示上限。1440/960 px 离屏截图确认
输入、锚点、方向、操作控件、选区、Bus/Enum/Clock 波形、刻度和网格无遮挡。

第 271–280 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
32/32 CTest 全部通过，耗时分别为 19.10 s、19.24 s、18.55 s；全部 GUI 路径使用 offscreen。
Codex 用户视角确认 inspect 可取得稳定 ID 与 sourceSha256，PowerShell stdin dry-run 不写文件，
随后原子输出的 resultSha256 与预演一致；结果工程包含 230000 tick End、req_cli 和
cli_data=0xa5，validate 为 0 error。冲突或批次失败均无部分写入，源示例保持不变。

第 281–290 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
35/35 CTest 全部通过，耗时分别为 19.59 s、20.22 s、18.90 s；全部 GUI 路径使用 offscreen。
Codex 用户视角先用 summary 定位 8 条 Lane，再以 80 ns 采样得到 request=1、data=0x35；
6-operation dry-run 不写文件，独立输出与预演 SHA 一致。结果为 7 条 Lane、End=230000 tick，
data 已重排到 request 前，55 ns 的 request=1，validate 为 0 error。物理时间和周期无需手算
tick；冲突、歧义、类型错误或中途失败均无部分结果。

第 291–300 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
37/37 CTest 全部通过，耗时分别为 20.00 s、20.97 s、19.42 s；全部 GUI 路径使用 offscreen。
Codex 用户视角用 window 读取 70–130 ns 的 2 条 Lane、2 个局部 Event、1 个 Relation 上下文
端点和 1 条 Relation；六操作批次完成断言、多信号赋值、跨 Lane Copy/Move，预演与输出 SHA
一致，采样结果为 request/ack=1、data_mirror=0x35、request_moved=1、移动源=0，validate 为
0 error。未确认的显式内容覆盖以退出码 4 拒绝；临时工程原地写入的自动备份 SHA 与原始源一致，
磁盘示例始终未变。

长期 Goal 当前保持 active；第 300 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 301–310 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
39/39 CTest 全部通过，耗时分别为 20.32 s、21.13 s、19.37 s；全部 GUI 路径使用 offscreen。
Codex 用户视角从不存在的路径执行 new dry-run 与实际创建，六操作一次建立三条 Lane 和两条
共享周期重复序列，预演与结果 SHA 一致；End 自动延长到 80 ns，request@cycle1=1、
data@cycle2=0x34，局部窗口为 2 条 Lane，validate 为 0 error。已有输出拒绝后 SHA 不变，
非法序列在 operation 1 拒绝且无半成品。

长期 Goal 当前保持 active；第 310 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 311–320 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
41/41 CTest 全部通过，耗时分别为 20.77 s、23.08 s、20.18 s；全部 GUI 路径使用 offscreen。
Codex 用户视角用 `signals` 按 req 精确定位且不读取 Segment 正文，再以 Scenario/Clock/Lane
名称预演并实际写入 ack=1、data=0xa5；预演不写盘，结果 SHA 一致，operation 报告返回规范
lane-request/lane-ack/lane-data。名称采样与局部窗口结果正确，validate accepted=true、0 error。
歧义、缺失、无效 exact/limit 均结构化拒绝且无部分结果。

长期 Goal 当前保持 active；第 320 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 321–330 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
42/42 CTest 全部通过，耗时分别为 21.01 s、23.06 s、20.16 s；全部 GUI 路径使用 offscreen。
Codex 用户视角从空路径一次建立 Control Group、clk、enable 与 2-bit state Enum，三条信号按
名称归组，state 声明 IDLE/BUSY/DONE 并按周期写入；预演和写入 SHA 一致，紧凑查询不含 Segment，
cycle 2 为 DONE。随后完整替换映射加入 ERROR 并写入 cycle 3，结果为 ERROR 且 validate 0 error。
OVERFLOW=4 以退出码 4 在 operation 0 拒绝，无失败输出，源 SHA 与三项声明映射保持不变。

长期 Goal 当前保持 active；第 330 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 331–340 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
43/43 CTest 全部通过，耗时分别为 21.36 s、23.21 s、20.88 s；全部 GUI 路径使用 offscreen。
Codex 用户视角以 Lane 名称和精确现有边沿建立 Relation，以周期表示 10000..20000 tick 延迟，
并创建、改名、扩展 Control window Marker；预演不写盘，实际 SHA 与预演一致，重复更新均为
changed=false，局部窗口返回 1 Relation/1 Marker，validate 0 error，随后可按稳定 ID/名称清理。
先创建 Marker 后引用缺失边沿的批次在 operation 1 以退出码 4 原子拒绝，无失败输出，源 SHA
不变且 Relation/Marker 计数仍为 0。

长期 Goal 当前保持 active；第 340 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 341–350 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
44/44 CTest 全部通过，耗时分别为 21.57 s、23.87 s、20.68 s；全部 GUI 路径使用 offscreen。
Codex 用户视角无工程读取 capabilities，确认 8 个命令、22 类 operation 及四个 Group 操作；
随后从空工程建立 Core/Debug Group 和 req，按名称将 Core 改为 Control 并移动到索引 2。
预演不写盘且与实际 SHA 一致，重复更新/移动 changed=false。删除 Control 报告 1 个成员解除
归组，req 保留且 validate 0 error；把 req 当 Group 删除的批次在 operation 1 以退出码 4
原子拒绝，无 stdout、无失败输出且源 SHA 不变。

长期 Goal 当前保持 active；第 350 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 351–360 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
45/45 CTest 全部通过，耗时分别为 22.21 s、24.50 s、20.93 s；全部 GUI 路径使用 offscreen。
Codex 用户视角从 capabilities 发现 duplicate-signal 后，将 data[7:0] 原子复制为
payload_copy，并按新名称再复制为 payload_copy_copy；clk 复制为 clk_shadow 与独立
clock-shadow。预演不写盘且实际 SHA 一致，Bus/Clock 采样相等，Lane/ClockDomain 分别增至
11/2，Event 14、Relation 1 保持，validate 0 error。Bus 使用 newClockId 的第二项失败批次以
退出码 4 原子拒绝，无 stdout、无失败输出且源 SHA 不变。

长期 Goal 当前保持 active；第 360 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 361–370 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
46/46 CTest 全部通过，耗时分别为 22.35 s、24.73 s、21.21 s；全部 GUI 路径使用 offscreen。
Codex 用户视角先把 handshake End 从 220 ns 无授权缩到 120 ns，进程在 operation 1 以退出码 4
原子拒绝并报告将裁短 6 个 Segment/1 个 Marker、删除 6 个 Segment/4 个 Event，无 stdout 和
失败输出。显式截断预演不写文件，实际 SHA 与预演同为
3eb6d420fd2978c52a51bb75cd756bca6d8a9122f3de073f80d09511ef3aa36a；119 ns 的
req/ack/data/state 为 1/1/0x35/WAIT_ACK，validate 0 error，源 SHA 不变。空白工程从 60 ns
缩至 40 ns 无需 `truncate` 且报告 contentTruncated=false。

长期 Goal 当前保持 active；第 370 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 371–380 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
47/47 CTest 全部通过，耗时分别为 22.99 s、25.95 s、21.61 s；全部 GUI 路径使用 offscreen。
Codex 用户视角按 req/ack 与 70–130 ns 查询上升沿，直接取得
lane-request@80000 和 lane-ack@110000；由这两个结果更新 Relation，预演与写入 SHA 一致，
局部窗口仍返回 1 个 Relation，validate 为 0 error，源工程 SHA 不变。下降沿上限截断、
Bus change、周期范围、Clock/Group、非法过滤和只读源保护均通过进程契约。

长期 Goal 当前已按用户要求暂停；第 380 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 381–390 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
48/48 CTest 全部通过，耗时分别为 23.76 s、26.22 s、22.14 s；全部 GUI 路径使用 offscreen。
Codex 用户视角以 req、within、70–130 ns 直接定位 relation-req-ack，结果给出
lane-request@80000 → lane-ack@110000、30000 tick 实际延迟和健康端点；复用 relationId 将
严重性改为 warning 并更新说明，预演/写入 SHA 一致，重新查询结果正确，validate 0 error，
源工程 SHA 不变。损坏端点、延迟越界、非法筛选和内部 ID 隔离均通过专项契约。

长期 Goal 当前已按用户要求暂停；第 390 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 391–400 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
49/49 CTest 全部通过，耗时分别为 25.26 s、28.32 s、22.98 s；全部 GUI 路径使用 offscreen。
Codex 用户视角以 request、phase、70–130 ns 直接定位 marker-transfer，结果给出
80–150 ns、70000 tick 宽度、addressable=true 和 valid=true；复用 markerId 将名称改为
Transfer window、备注改为 Request acknowledged within expected window，预演/写入 SHA
同为 a87b7d62d80568f221093176864bf836272205a935d386e9bbd743f838ac668e，重新查询结果正确，
validate 0 error，源工程 SHA 不变。重复身份、损坏几何、半开边界、非法筛选和扩展隔离均通过
专项契约。

长期 Goal 已按用户明确指令恢复迭代；第 400 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 401–410 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
50/50 CTest 全部通过，耗时分别为 25.04 s、27.73 s、22.57 s；全部 GUI 路径使用 offscreen。
Codex 用户视角先查询目标 Event 已丢失的 relation-req-ack，得到 target.issue=missing-event；
随后仅以 targetLaneId=ack、targetAt=110 ns 显式替换目标。预演不写盘并报告只修复目标端，
实际 SHA 与预演同为 7f69420c38e934b9a29eb77ca5165879db31674263604ff51a33993470082cd1；
复查 endpointsReady=true、validate 0 error。部分端点替换以退出码 4 拒绝，无失败输出，源工程
SHA 不变；重复/脱离/双端损坏、歧义位置和重复 Event ID 均通过核心与进程契约。

长期 Goal 保持 active；第 410 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 411–420 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
51/51 CTest 全部通过，耗时分别为 25.35 s、28.42 s、22.88 s；全部 GUI 路径使用 offscreen。
Codex 用户视角从两个同 ID 的 marker-transfer 中取得不同 markerRef，直接按 ID 更新原子拒绝；
使用第二项引用改为 marker-transfer-follow-up 后，预演与写入 SHA 同为
24092a613ca527b9381090dd770b638b55ba5475dd77b8e4c979e4d8fa968914。复查两个 ID 各命中
一项且均 addressable，validate 0 error。旧引用在新工程中拒绝且无失败输出，同一引用可在原
损坏工程精确删除第二项；损坏源 SHA 不变。空 ID、损坏几何、ID 冲突、唯一名称绕过、扩展隔离
及领域命令 Undo/Redo 均通过专项契约。

长期 Goal 保持 active；第 420 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 421–430 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
52/52 CTest 全部通过，耗时分别为 23.96 s、29.15 s、22.69 s；全部 GUI 路径使用 offscreen。
Codex 用户视角从两个同 ID 的 relation-req-ack 中取得不同 relationRef，直接按 ID 更新原子拒绝；
使用第二项引用改为 relation-req-ack-follow-up 后，dry-run 结果 SHA 为
9839398b496c9eb18b18bfa795e0e2c6f3b4274042e77abddae9d10503f12931。复查两个 ID 各命中
一项且均 addressable、目标关系 endpointsReady、validate 0 error。旧引用在新工程中拒绝且无
失败输出，同一引用可在原损坏工程精确删除第二项；损坏源 SHA 保持
b634698c888de4912bb453347b20e938abeee64fe4a46770ca85d5433526abc7。空 ID、端点联合修复、
ID 冲突、扩展隔离及领域命令 Undo/Redo 均通过专项契约。

长期 Goal 保持 active；第 430 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 431–440 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
53/53 CTest 全部通过，耗时分别为 23.96 s、28.96 s、23.12 s；全部 GUI 路径使用 offscreen。
Codex 用户视角复现两个同 ID 的 relation-req-ack 仍被旧 validate 报告为 valid=true 的问题；
当前版本以退出码 4 返回两个 duplicate-stable-id、精确路径和 idCount=2，并将四条既有行为结果
独立计入 semanticIssueCount。使用第二项 relationRef 修复后，dry-run 与写入结果 SHA 同为
b9dbd835ba98009ac6c2b43960229b68e496408c4680736eb7eb0bd35e775d2d；重新校验返回退出码 0、
valid=true、0 identity issue、0 error。损坏源 SHA 保持
b634698c888de4912bb453347b20e938abeee64fe4a46770ca85d5433526abc7。八类身份作用域及健康工程
零回归均通过专项契约。

长期 Goal 保持 active；第 440 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 441–450 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
54/54 CTest 全部通过，耗时分别为 24.49 s、29.84 s、23.66 s；全部 GUI 路径使用 offscreen。
核心门禁同时校验源工程与候选工程，以 error 多重指纹证明候选没有新增错误；健康工程只接受
有效结果，损坏工程允许错误集合缩小或不退化的显式渐进修复。旧 Relation 端点恢复用例中
40 ns→110 ns 违反 10–40 ns 约束的隐藏错误已被门禁捕获，用户旅程改用 80 ns 有效边沿，并
保留“用新错误替换旧错误必须拒绝”的专项断言。

Codex 用户视角对同时含重复 Relation/Marker ID 的工程执行无关重命名，得到退出码 4、
post-validation-failed、reason=invalid-candidate、错误数 4→4、stdout 为空且 written=false。
第一步按 relationRef 修复后 reason=progressive-repair、错误数 4→2、newErrorCount=0，
dry-run/文件 SHA 同为
584183fda4c07b4484d97f498cd7f53ff2475fbab3db9230b59a56561e8dbe78；第二步按 markerRef
修复后 reason=valid-candidate、错误数 2→0，SHA 同为
69fe8be3cdc148246b9fe9799df2c6e3f463e8b4711ea65b38064e369d8ca3b2，最终 validate 为
valid=true、0 error。损坏源 SHA 保持
e9a263322e8a4cac0baed4c8d0f9eeb776bf73b0fcd0586fef880f81b1e47291。

长期 Goal 保持 active；第 450 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 451–460 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
55/55 CTest 全部通过，耗时分别为 25.12 s、30.13 s、23.65 s；核心入口为 32/32，全部 GUI
路径使用 offscreen。`validate` 与 `markers` 现共用 Marker 名称和几何诊断规则；空/重复名称、
起点早于 Scenario、终点早于起点、终点晚于 Scenario 及 Point/范围冲突均返回稳定问题码、
精确属性路径和机器计数。Marker 问题计入 error 和 semantic issue，并由工程级 apply 门禁保护。

Codex 用户视角对含两个大小写等价名称和一个越界 Point 的工程执行查询与校验，两者均报告
5 项问题，`validate` 返回退出码 4。无关 Lane 重命名得到 `reason=invalid-candidate`、错误数
5→5、`written=false`；按 `markerRef` 修复几何后错误数 5→2，dry-run 与写入 SHA 同为
65b376b37adf12722a44099fa6179e446619c5302dfe50e47574b4019e6912a9；再修复重复名称后错误数
2→0，SHA 同为 16f5f3c5973c4b130d283df02b83ae34afe6f54cc9131662f33d3cf4f3bfe932，
最终 `markerIssueCount=0`。示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 保持
5ccea02cb613245c91f5f050f63ed5f9ebfad057a0529ad7c623006cb8754e6e。

长期 Goal 保持 active；第 460 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 461–470 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
56/56 CTest 全部通过，耗时分别为 25.46 s、30.84 s、23.98 s；核心入口为 33/33，全部 GUI
路径使用 offscreen。可恢复的 Marker/Relation 校验问题现直接携带快照绑定 `markerRef` /
`relationRef` 和允许的 update/delete operation；重复身份按数组位置取得不同引用，唯一
Relation 的端点/时序语义问题也可直接修复。已满足及 condition 不适用的信息项不暴露修复入口，
顶层分别统计可修复问题和去重目标。

Codex 用户视角只执行一次 `validate`，从含两个重复 Relation 和一个损坏 Point 的工程直接取得
两个不同 Relation 引用及一个 Marker 引用；返回 `repairableIssueCount=5`、
`repairTargetCount=3`。不执行 `relations` 或 `markers` 查询即联合修复，门禁报告错误数 5→0；
dry-run 不写文件且结果 SHA 与实际文件同为
7977e09a583e3af88a197143d4b069b453d2f533539f2e008ce90f604d1a7857。最终 validate 返回
0 个可修复问题和 0 个修复目标。示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 保持
84c7568bdd03e32ba0b00696bc2b2d9df466ede60488cf7412f12749e0e4283e。

长期 Goal 保持 active；第 470 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 471–480 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
57/57 CTest 全部通过，耗时分别为 25.20 s、30.81 s、24.31 s；核心入口为 34/34，全部 GUI
路径使用 offscreen。Relation 语义问题现返回对象索引、精确 `path`/`paths`、明确
`repairProperties` 和与紧凑查询共用的 `relationContext`；其中包含无内部 Event ID 的源/目标
端状态、声明/实际延迟、时钟、condition 和时序结果。缺失端点、延迟违例、非法 condition、
时钟冲突和观察窗口不足均具有专项矩阵。

Codex 用户视角只执行一次 `validate`，从目标 Event 缺失的工程直接读到
`scenarios[0].relations[0].targetEventId`、`target-endpoint`、源端 `req@80 ns` 已就绪、
目标端 `missing-event`、声明延迟 10000–40000 tick 和 `relationRef`。不执行 `relations`
查询即补到 `ack@110 ns`，门禁报告错误数 1→0；dry-run 不写文件且结果 SHA 与实际文件同为
2391211fe2b5e75d77f267bdcc223d05391bc868d944ac4d986ebe159962affc。最终 validate 返回
0 error。示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 保持
4f55c0524b17099927fd8b8c74d13a613aaca11b44e9ced4eab100dfe0c397b9。

长期 Goal 保持 active；第 480 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 481–490 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
58/58 CTest 全部通过，耗时分别为 25.91 s、31.45 s、24.37 s；核心入口为 35/35，全部 GUI
路径使用 offscreen。非法 Bus/Enum Segment 与未定义区间现返回对象索引、精确 `path`、紧凑
`waveformContext`、半开 `repairRange`、明确 `repairProperties` 和可执行的波形 operation。
读取器允许值语义错误进入 `validate`，但继续拒绝负时间、空区间和重叠等结构损坏。

Codex 用户视角只执行一次 `validate`，从 `data[7:0]=0x1ff` 的磁盘工程直接读到
`scenarios[0].lanes[5].segments[1].value`、8-bit Bus、原值、Segment ID、80–150 ns 范围及
`set-range`/`clear-range`。不执行 `sample` 或 `window` 查询即按返回范围恢复 `0x35`，门禁
报告错误数 1→0；dry-run 不写文件且结果 SHA 与实际文件同为
8d98d67b0d9b45d01f6885c95df41c8daed26d4004027412989d839998c5f9ac。最终 validate 返回
valid=true、0 error。示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 保持
177c0a67e2c394ab94a84e1694dbedc4c638c1ca429576a12de6c3a1ec382133。

长期 Goal 保持 active；第 490 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 491–500 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
59/59 CTest 全部通过，耗时分别为 28.03 s、34.01 s、25.37 s；核心入口为 36/36，全部 GUI
路径使用 offscreen。Event 的 Lane 缺失与 Scenario 半开区间越界现返回对象索引、精确
`path`/`paths`、紧凑 `eventContext`、明确 `repairProperties` 和可执行恢复项；可表示的正向
越界可延长 End，负时间及最大 tick 不产生错误或溢出的延长建议。`delete-event` 只接受
`validate` 明确标为可删除且 ID 唯一的 Event，健康对象和歧义 ID 均原子拒绝。

Codex 用户视角只执行一次 `validate`，从 Lane 被改为 `lane-missing` 的 `event-ack-high`
直接读到 `scenarios[0].events[9].laneId`、110 ns、expect、值 1、Lane/Segment 未解析及
`delete-event`。不查询内部 Event/Segment 数组即完成恢复，门禁报告错误数 1→0，并删除
1 个 Event、1 个 Relation、0 个 Segment；dry-run 不写文件且结果 SHA 与实际文件同为
151a4608d11f4578b88132f97edb352a0d74ac35b96ba810aa33c9a700d6449b。最终 validate 返回
valid=true、0 error，示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 保持
dad522496d48fd204f24ae1407765dede0843a7e414c7062f0d73ad701afd6e9。健康 Event 和重复 ID
删除均返回退出码 4、不产生输出且保持源文件不变。

长期 Goal 保持 active；第 500 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 501–510 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
60/60 CTest 全部通过，耗时分别为 27.83 s、35.03 s、25.20 s；核心入口为 37/37，全部 GUI
路径使用 offscreen。waveform-linked Event 的 Lane、时刻或值与唯一关联 Segment 不一致时，
`validate` 现返回 `event-waveform-mismatch`、对象索引、精确 `paths` 及不暴露内部 Segment ID
的 `eventContext`。`repair-event-link` 从唯一有效 Segment 原位恢复 Event，并保留稳定 Event ID、
action、metadata、Relation 与波形；健康对象、重复 Event ID、重复关联、丢失或非法目标均原子拒绝。

Codex 用户视角只执行一次 `validate`，从被改为 `reset` Lane、111 ns、值 0 的
`event-ack-high` 直接读到 `.laneId`、`.timeTick`、`.value` 三条路径，以及真实目标
`ack`、110–150 ns、值 1 和 `repair-event-link`。不查询内部 Segment ID 即完成恢复，门禁报告
错误数 1→0、更新 1 个 Event，新增/删除 Event、Relation、Segment 均为 0，原 Relation 端点保持
不变；dry-run 不写文件且结果 SHA 与实际文件同为
2391211fe2b5e75d77f267bdcc223d05391bc868d944ac4d986ebe159962affc。最终 validate 返回
valid=true、0 error，示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 保持
d433922a8f57bdb6397c11c2e96a44ba13c79c77d19f21458ba5cf6e4f16bb85。健康关联与重复 Event ID
修复均返回退出码 4、不产生输出且保持源文件不变。

长期 Goal 保持 active；第 510 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 511–520 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
61/61 CTest 全部通过，耗时分别为 28.10 s、34.31 s、25.63 s；核心入口为 38/38，全部 GUI
路径使用 offscreen。带 cycle 的 Event 现必须严格落在唯一有效 ClockDomain 的 active edge；
负 cycle、时钟缺失/歧义、周期计算溢出和实际时刻冲突均返回 `event-cycle-mismatch`、精确路径
及预期时刻上下文。`clear-event-cycle` 只清除陈旧周期元数据，保留当前时刻、ClockDomain、
稳定 ID、Relation、Segment 与波形；健康对象和歧义 ID 均原子拒绝。Event–Segment 关联修复
同时保留仍正确的 cycle，并清除因目标时刻改变而失效的 cycle。

Codex 用户视角只执行一次 `validate`，从当前 110 ns、却携带 cycle 8 的 `event-ack-high`
直接读到 `.cycle`、`.timeTick`、`clock-main` 和预期 80 ns，并取得唯一
`clear-event-cycle`。不查询内部 Clock/Relation 数组即完成恢复，门禁报告错误数 1→0、更新
1 个 Event，新增/删除 Event、Relation、Segment 均为 0，Relation 端点仍为 `ack@110 ns=1`；
dry-run 不写文件且结果 SHA 与实际文件同为
2391211fe2b5e75d77f267bdcc223d05391bc868d944ac4d986ebe159962affc。最终 validate 返回
valid=true、0 error，示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 保持
d62ff724c4c63e101be03dec4f9a1be2fafa92eea9baefe7c4ab292a69be68ff。健康 Event 与重复 Event ID
清理均返回退出码 4、不产生输出且保持源文件不变。

长期 Goal 保持 active；第 520 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 521–530 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
62/62 CTest 全部通过，耗时分别为 29.14 s、34.34 s、25.80 s；核心入口为 39/39，全部 GUI
路径使用 offscreen。Event 的非空显式 ClockDomain 现必须唯一存在；缺失或歧义时返回
`event-clock-domain-invalid`、精确路径、当前/Lane 时钟匹配数和明确恢复动作。
`repair-event-clock` 在 Lane 时钟唯一有效时采用它，Lane 无时钟或不存在时清除无效显式引用，
并按替换后的时钟保留仍正确的 cycle、清除陈旧 cycle；Lane 时钟也无效、当前引用歧义、健康对象
和 Event ID 歧义均不暴露或拒绝恢复。

Codex 用户视角只执行一次 `validate`，从 ClockDomain 被改为 `clock-missing` 的
`event-ack-high` 直接读到 `.clockDomainId`、当前匹配数 0、Lane `clock-main` 匹配数 1 和
`use-lane-clock`，并取得唯一 `repair-event-clock`。不查询内部 Clock/Relation 数组即完成恢复，
门禁同时消除 Event 引用错误和 Relation 时钟冲突，错误数 2→0；只更新 1 个 Event，新增/删除
Event、Relation、Segment 均为 0，Relation 端点仍为 `ack@110 ns=1`。dry-run 不写文件且结果
SHA 与实际文件同为
2391211fe2b5e75d77f267bdcc223d05391bc868d944ac4d986ebe159962affc。最终 validate 返回
valid=true、0 error，示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 保持
27b0640ab61948d6205d460ec1c3d3cffdc5f9b7dbbca4e3cd6f644683c99bb9。健康 Event 和重复 Event ID
修复均返回退出码 4、不产生输出且保持源文件不变。

长期 Goal 保持 active；第 530 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 531–540 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
63/63 CTest 全部通过，耗时分别为 29.20 s、35.55 s、26.53 s；核心入口为 40/40，全部 GUI
路径使用 offscreen。Lane 的 ClockDomain 引用现具有独立完整性校验：Clock Lane 必须引用唯一
存在的时钟域，普通 Lane 的非空引用必须有效，Group 不得携带时钟引用。
`repair-lane-clock` 对普通 Lane 清除悬空引用，对 Clock Lane 只采用工程中唯一的 ClockDomain；
多时钟、歧义 ClockDomain、健康对象和重复 Lane ID 均不暴露或拒绝恢复。修复保留波形、Event、
Relation 与 Segment，并只清理已无法解释的继承 cycle。

Codex 用户视角只执行一次 `validate`，从 ClockDomain 被改为 `clock-missing` 的 Lane ack
直接读到 `scenarios[0].lanes[4].clockDomainId`、当前匹配数 0、`clear-lane-clock` 和唯一
`repair-lane-clock`。不查询内部 Clock/Event/Relation 数组即完成恢复，门禁报告错误数 1→0；
只更新 1 个 Lane，Event、Relation、Segment 均无新增或删除，Relation 端点仍为
`ack@110 ns=1`。dry-run 不写文件且结果 SHA 与实际文件同为
b531034fc14e8066fade31a9807ea3e8f38476d2dec9eeda4c60f6e7b05f62c0。最终 validate 返回
valid=true、0 error，示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 保持
d2763dd0ea7bb5b7bf1667668df3303dc5293bc454156d77f768c0f4194d400b。健康 Lane 和重复 Lane ID
修复均返回退出码 4、不产生输出且保持源文件不变；多时钟 Clock Lane 只诊断，不猜测目标。

长期 Goal 保持 active；第 540 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 541–550 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
64/64 CTest 全部通过，耗时分别为 29.11 s、35.71 s、26.68 s；核心入口为 41/41，全部 GUI
路径使用 offscreen。Lane 的 Group 引用现具有独立完整性校验：非空 `groupId` 必须唯一指向
同一 Scenario 中另一条 Group Lane，Group 不得嵌套，普通 Lane 不得自引用或指向非 Group。
`repair-lane-group` 只解除无效分组，不按名称、类型或顺序猜测替代目标；波形、Event、Relation、
Segment、ClockDomain、显示属性与扩展载荷均保持不变。健康对象和重复待修 Lane ID 拒绝恢复；
重复 Group 目标可渐进式解除分组，但不会选择其中任何一个。

Codex 用户视角只执行一次 `validate`，从 `groupId` 被改为 `group-missing` 的 Lane ack
直接读到 `scenarios[0].lanes[4].groupId`、目标匹配数 0、`clear-lane-group` 和唯一
`repair-lane-group`。不查询内部 Group/Lane/Event/Relation 数组即完成恢复，门禁报告错误数
1→0；只更新 1 个 Lane，Event、Relation、Segment 均无新增或删除，Relation 端点仍为
`ack@110 ns=1`。dry-run 不写文件且结果 SHA 与实际文件同为
5e9c033153daaa608d18502a5dadf647d624eb9241ecde6fc9ffb8c1f5e9f39f。最终 validate 返回
valid=true、0 error，示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 保持
d74c0aba9272d823c5eef80b700eb2a20287d492c1a17ede5b85dc41c2e4f7eb。健康 Lane 和重复 Lane ID
修复均返回退出码 4、不产生输出且保持源文件不变。

长期 Goal 保持 active；第 550 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 551–560 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
65/65 CTest 全部通过，耗时分别为 30.14 s、37.01 s、27.14 s；核心入口为 42/42，全部 GUI
路径使用 offscreen。Relation 的非空显式 ClockDomain 现必须唯一存在；缺失或歧义时返回
`relation-clock-domain-invalid`、精确路径、Relation/端点时钟匹配数和明确恢复动作。
`repair-relation-clock` 只在当前引用缺失、两端时钟上下文均可判定且不冲突时执行：采用唯一
共同端点时钟，或在两端均无时钟时清空无效引用。当前引用歧义、端点时钟悬空/歧义、两端冲突、
健康对象和重复 Relation ID 均不暴露或拒绝自动恢复。修复只更新 Relation ClockDomain，保留
端点、延迟、condition、描述、Event、Lane、Segment 与波形。

Codex 用户视角只执行一次 `validate`，从 ClockDomain 被改为 `clock-missing` 的
`relation-req-ack` 直接读到 `scenarios[0].relations[0].clockDomainId`、当前匹配数 0、两端
唯一 `clock-main`、`use-endpoint-clock` 和唯一 `repair-relation-clock`。不查询内部
Clock/Event 数组即完成恢复，门禁将悬空引用及其引发的时钟冲突同时消除，错误数 2→0；只更新
1 个 Relation，Event、Segment 和依赖删除数均为 0，端点仍为
`req@80 ns=1 → ack@110 ns=1`。dry-run 不写文件且结果 SHA 与实际文件同为
2391211fe2b5e75d77f267bdcc223d05391bc868d944ac4d986ebe159962affc。最终 validate 返回
valid=true、0 error，示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 保持
8b8e76da72927363a74a41b29fe2c9d2f0d5f04b0761ffcc8ecf24dbdc850714。健康 Relation 与重复
Relation ID 修复均返回退出码 4、不产生输出且保持源文件不变；冲突或歧义端点时钟只诊断，
不猜测目标。

长期 Goal 保持 active；第 560 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 561–570 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
66/66 CTest 全部通过，耗时分别为 30.62 s、36.81 s、27.31 s；核心入口为 43/43，全部 GUI
路径使用 offscreen。Imported Trace 映射键现必须指向工程中存在的非 Group Lane，Actual
signal ID 必须非空；损坏时返回 `trace-mapping-invalid`、精确映射路径、Trace/Lane 匹配数、
问题码、外部信号未校验边界和明确清理动作。`repair-trace-mapping` 只删除已证明无效的单条
映射，保留 Trace 路径、格式、偏移、其他映射、扩展载荷和工程波形；健康映射、歧义 Trace ID
和过期快照均原子拒绝。完整 `inspect` 公开映射表与快照绑定 `traceRef`，精简模式仍只返回计数。

Codex 用户视角只执行一次 `validate`，从加入 `lane-stale → tb.dut.removed` 且清空
`lane-data` Actual ID 的磁盘工程直接读到两项问题及唯一清理入口。按 `traceRef` 和唯一
`traceId` 完成两项恢复后，门禁报告错误数 2→0、映射数 7→5；其余 5 条有效映射、Trace 配置、
Event、Relation、Segment 与波形均保持不变。dry-run 不写文件且结果 SHA 与实际文件同为
594e45027b2155a4832df60731978605bc7c2f295a0836fc8dc26d64fb256da4。最终 validate 返回
valid=true、0 error，示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 保持
493dd28ddc30ab43f1487483d3439b9d5fc195527ba392cc3c1f2991abee9337。健康映射修复不产生输出；
重复 Trace ID 可由快照引用精确区分，工程变化后的旧引用不会留下部分删除。外部 VCD/CSV
信号未加载时不猜测替代映射。

长期 Goal 保持 active；第 570 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 571–580 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
67/67 CTest 全部通过，耗时分别为 29.46 s、37.41 s、27.14 s；核心入口为 44/44，全部 GUI
路径使用 offscreen。Imported Trace 已纳入工程级稳定身份校验：空或重复 ID 返回精确数组
路径、ID 匹配数和逐项不同的快照绑定 `traceRef`。`repair-trace-identity` 必须使用该引用，
可显式指定唯一新 ID，或确定性生成 `trace-auto-*`；修复只更新目标 ID，保留路径、格式、偏移、
映射和扩展载荷。桌面端活动 Trace 查找只接受唯一 ID，身份损坏时不再加载首个命中。

`wave-compare` 在存在多条 Imported Trace 时要求显式 `--trace-id`，重复、空或缺失 ID 均在
读取外部文件前拒绝。Compare 报告现以 `unmapped-signal` 表示没有配置 Lane 映射，以
`missing-signal` 表示配置的 Actual signal 已不在加载后的 Trace 中，并在后一种结果中保留
目标 ID。Codex 用户视角从重复 `trace-handshake-actual` 的磁盘夹具一次 `validate` 直接得到
两个修复引用；按第二项修复后错误数 2→0，生成 ID 为
`trace-auto-7b07b7dde44a16d2`。重复夹具 SHA 为
c4dd187064b688bfd9e69e534a4050431a3ef158b65688ac34993aa332ead52f，恢复结果 SHA 为
8401e107f4d109a3e750a66a8666c9a48130903f7707d9a36da170a3cdb152d8，示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4。健康身份、冲突新 ID 和
过期快照均原子拒绝；多 Trace 不再因数组顺序生成错误 Compare 报告。

长期 Goal 保持 active；第 580 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 581–590 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
68/68 CTest 全部通过，耗时分别为 8.19 s、10.43 s、7.02 s（`--parallel 4`）；核心入口为
45/45，全部 GUI 路径使用 offscreen。Imported Trace 的路径现必须非空，格式必须大小写不敏感
地为 VCD/CSV；`validate` 按 Trace 聚合返回 `trace-reference-invalid`、全部无效字段路径、
`empty-path`/`unsupported-format`、支持格式、文件系统未校验边界和快照绑定 `traceRef`。
`repair-trace-reference` 必须显式补充路径/格式至少一项，且最终引用整体有效才提交；修复只更新
路径/格式，保留 ID、偏移、映射、扩展载荷与工程波形。健康引用、部分修复和过期快照均原子拒绝。

`wave-compare` 在解析前分别说明空路径、未知格式和目标不是文件。Codex 用户视角从 path 为空、
format 为 fst 的磁盘夹具一次 `validate` 直接得到一项聚合问题；回传其 `traceRef`、明确 VCD
路径和格式后，结构错误数 1→0，只更新一条 Trace。dry-run 不写文件且结果 SHA 与实际文件同为
5d3d5a8f2e88a3fe67450cd56cc463bd22b52a55a9136c344e12ac02da3fec6a；修复后可直接完成 Compare。
示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4，损坏夹具 SHA 保持
a45b427274c1643c8d887c86b21c54f64c565ee216e81183cbe5706058a0be25。只修复路径、继续指定
fst、对健康对象重复修复或使用旧快照均返回退出码 4、不生成输出且不改写源文件。纯波形桌面
工作区未恢复已删除的 Trace/Compare 面板。

长期 Goal 保持 active；第 590 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 591–600 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
69/69 CTest 全部通过，耗时分别为 8.60 s、10.45 s、7.53 s（`--parallel 4`）；核心入口仍为
45/45，全部 GUI 路径使用 offscreen。`wave-cli`、`wave-generate`、`wave-compare`、
`wave-bridge import-signals` 与 `wave-bridge pinloom-entry` 现共享 Scenario 选择门禁：
单 Scenario 可省略选择器，多 Scenario 必须显式使用稳定 ID 或大小写不敏感的唯一完整名称；
空值、不存在、重名、重复稳定 ID 与空身份均不再落到数组首项。选择在读取外部 Trace/信号清单
或写出文件前完成，成功结果回显名称和规范 ID。Generate 的旧位置 Scenario ID 仍兼容。

Codex 用户视角从磁盘示例构造包含 `Request / acknowledge` 与 `Alternative timing` 的双
Scenario 工程。不选择时五个 Scenario 级入口均明确拒绝且不留下输出；选择
`scenario-alternative` 或 `alternative TIMING` 后，Generate 写出七件目标产物，Compare 只写
Alternative 报告，信号导入只使第二个 Scenario 从 8 条 Lane 变为 9 条，Pinloom 的
`scenarioId` 也准确指向第二项。不存在目标、重名名称和重复 ID 全部原子拒绝。双 Scenario
夹具 SHA 为
6071a17b92575bf90aca750d7d766a48ed0ab08d4545dc33e46e4ce1466da38b，示例源 SHA 保持
d13f64028f7879b9ea15119318d51e7e22428edf47368d472ee1f8804047f3b4。

长期 Goal 保持 active；第 600 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 601–610 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
70/70 CTest 全部通过，耗时分别为 9.41 s、10.84 s、8.83 s（`--parallel 4`）；核心入口仍为
45/45，全部 GUI 路径使用 offscreen。桌面端不再固定编辑 `scenarios.front()`：多 Scenario
工程只在工具栏显示紧凑 `Waveform` 选择器，单 Scenario 工程将标签、下拉框和分隔线整组隐藏。
URI 的 Scenario 稳定 ID 解析为初始活动索引，不再旋转模型数组，重复或不存在 ID 明确拒绝。

切换本身不执行命令、不改变 Saved，也不会写入工程；每个波形分别恢复信号选择、时间光标、
显式范围、缩放和滚动位置。命令历史保持工程级真实时间顺序，并记录每个状态所属波形；
Undo/Redo 命中其他波形时自动切换到实际目标，因此工程级 ClockDomain 修改仍有单一正确历史，
用户也不会撤销不可见内容。Codex 用户视角先修改 Request / acknowledge，再修改 Response path：
两次 Undo 分别自动显示并恢复第二、第一波形，回到干净基线；两次 Redo 按原顺序恢复。专项
`wave-scenario-switch-smoke` 同时验证初始第二项不重排、切换零副作用、双波形编辑隔离、上下文
恢复、单波形零额外界面和 1440×900 离屏视觉结果；`wave-uri-smoke` 覆盖显式 Scenario 启动。

长期 Goal 保持 active；第 610 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 611–620 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
70/70 CTest 全部通过，耗时分别为 8.45 s、11.59 s、8.36 s（`--parallel 4`）；核心入口仍为
45/45，全部 GUI 路径使用 offscreen。已保存的多 Scenario 工程现在以规范化工程路径、
Project 稳定 ID 和唯一 Scenario 稳定 ID 记住最后编辑波形；该偏好只存在于编辑器设置，不修改
模型、命令栈、Saved 或工程 JSON。重新打开及 Scenario 数组重排后仍恢复同一身份；目标已删除、
空 ID 或重复 ID 时清除失效记忆并安全回到第一项。Save As 将当前目标绑定到新路径，URI 或
调用方显式初始目标始终优先于本地记忆。

`Ctrl+PageUp` / `Ctrl+PageDown` 可在画布内前后切换波形，首尾不循环并提供明确返回方向；
非法 End 等就地草稿保留文本、错误反馈与焦点并阻止切换，修正后复用同一动作完成导航。单波形
工程继续完全隐藏选择器并禁用导航。Codex 用户视角从 Response path 关闭并重开磁盘工程后直接
恢复该波形，交换数组顺序仍按 ID 命中；删除目标、构造重复 ID 和显式指定首项分别验证安全回退、
消歧显示和显式优先。扩展的 `wave-scenario-switch-smoke` 同时覆盖首尾反馈、零模型/历史/脏状态
变化、双波形上下文、跨波形 Undo/Redo 和 1440×900 离屏视觉结果。

长期 Goal 保持 active；第 620 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 621–630 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
70/70 CTest 全部通过，耗时分别为 9.13 s、11.81 s、8.59 s（`--parallel 4`）；核心入口仍为
45/45，全部 GUI 路径使用 offscreen。每个正式工程的唯一 Scenario 现在按规范化路径、
Project ID 和 Scenario ID 记住最后一条唯一可见非 Group Lane 的稳定 ID 与整数编辑 tick。
正常关闭、波形切换、New/Open 替换前、保存/Save As 和显式外部定位均更新该安全位置；设置
不进入工程模型、命令栈、Saved、内容哈希或 autosave。

恢复只选择信号并定位光标，不恢复 Beat、Segment、显式范围、标题删除武装或草稿；状态明确
显示 `no edit range restored`，恢复后 Delete 为零效果。隐藏、删除或重复 Lane 不猜测替代项，
只恢复时间并说明 `saved signal unavailable`；越界 tick 钳到当前 End。首次进入 Scenario 才
使用磁盘位置，同一会话返回时保留更完整的范围、Segment、缩放和滚动上下文。显式 URI lane/tick
在恢复后覆盖本地位置，并成为下一次正常打开的新位置。

Codex 用户视角验证 Save As→New→Open 直接回到 data Bus 和保存时的精确 tick；双波形工程关闭
重开直接回到 Response path 的 ack@120 ns，首次切换恢复另一波形的 data@180 ns，返回时仍保留
会话内完整范围。隐藏 ack 降级为 120 ns 时间位置；Scenario 重排、显式 req@40 ns 覆盖及下一次
恢复均按稳定身份工作。扩展的 `wave-scenario-switch-smoke` 与 `wave-user-journey-smoke` 覆盖
上述路径，新增 1440×900 离屏截图确认 Waveform、信号高亮、光标、Target、Saved 和安全反馈。

长期 Goal 保持 active；第 630 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 631–640 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
70/70 CTest 全部通过，耗时分别为 8.85 s、11.18 s、8.47 s（`--parallel 4`）；核心入口仍为
45/45，全部 GUI 路径使用 offscreen。逐 Scenario 安全位置新增可选整数 `visibleSpanTick`，
表示与窗口像素无关的可见时间跨度。恢复在最终 viewport 与滚动条布局稳定后以安全 tick 为中心
重建尺度，不保存裸像素滚动量，也不恢复 Beat、Segment、显式范围或标题删除目标。

待恢复尺度在用户主动 Zoom/Fit、文档替换或上下文清理时取消；同一会话已有完整画布上下文继续
优先。旧设置缺少跨度时使用默认视图，非法值单独忽略，超长值钳到当前 End；Lane 隐藏、删除或
身份重复时仍恢复时间与尺度而不猜测信号。显式 URI lane/tick 保留有效尺度并以显式 tick 重新
锚定，随后写为新安全位置。语义跨度反馈和设置写回不会记录首次布局中的临时像素结果。

Codex 用户视角重开 Response path 后直接看到 ack@120 ns 及约 176 ns 局部时间窗，不再退回完整
220 ns 或重复缩放；首次切换恢复 Request / acknowledge 的 data@180 ns 及其独立尺度，返回时
会话范围与真实视口仍优先。Save As→New→Open、隐藏 Lane、Scenario 重排和显式 req@40 ns 重锚
均保持 Saved、零额外 Undo。更新后的 1440×900 离屏截图同时确认局部刻度、光标、信号高亮、
Target、Saved 和 `no edit range restored`。

长期 Goal 保持 active；第 640 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 641–650 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
70/70 CTest 全部通过，耗时分别为 9.85 s、11.88 s、8.16 s（`--parallel 4`）；核心入口仍为
45/45，全部 GUI 路径使用 offscreen。已有正式路径的工程现在会在选择、定位、缩放及水平视图
连续变化停止 400 ms 后，去抖写入当前 Scenario 的安全 Lane、整数 tick 和语义可见跨度；
Save、切换、替换工程与正常关闭仍同步写入。该行为不修改模型、Saved、Undo、工程 JSON 或
autosave generation。

快速新增信号仍处于就地命名/参数草稿时，新的调度与先前已在途的计时器都不会保存临时 Lane；
Untitled 工程也不创建正式位置身份。加载较新的 `<project>.autosave` 时按 `<project>` 正式路径
恢复该安全视图，使异常退出后的数据与最后工作位置同时返回。恢复继续排除 Beat、Segment、显式
范围及标题删除目标，状态栏同时报告 snapshot、resumed 和 `no edit range restored`。

Codex 用户视角验证在不 Save、不切换、不正常关闭的情况下快速经过 req@40 ns 后停在
data[7:0]@170 ns，并保留约 142 ns 局部时间窗；未提交 + BIT 草稿等待超过去抖周期也不污染
该位置。异常恢复重开后，更新数据、目标信号、170 ns、局部尺度和 Save required 同时可见，
Undo 为空且 Delete 不改变工程。Untitled 恢复没有生成 `waveforms/lastLocation` 键。离屏截图
`build/autosave-smoke-resumed-location.png` 已复核目标、刻度、恢复状态与安全提示。

长期 Goal 保持 active；第 650 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 651–660 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
70/70 CTest 全部通过，耗时分别为 9.29 s、12.37 s、9.13 s（`--parallel 4`）；核心入口仍为
45/45，全部 GUI 路径使用 offscreen。既有 Group 从没有层级含义的空白行收敛为标准可折叠画布
层级：标题 disclosure、Left/Right 和右键 Collapse/Expand 使用同一状态，成员标题缩进并显示
连续层级引导线，Group 标题直接显示成员数和 Expanded/Collapsed。

折叠只存在于逐 Scenario 会话视图，不修改 Lane `visible`、`groupId`、工程 JSON、Saved 或
Undo。若当前成员带有 Beat、Segment 或显式范围目标，折叠会清理破坏性编辑状态并安全指向 Group，
但 disclosure 不武装 Delete。查找、URI 或其他显式定位成员时自动展开；隐藏、删除或失效 Group
会清理折叠状态，使仍可见成员立即恢复。切换 Scenario 分别记住本次会话状态，不跨波形泄漏。

Codex 用户视角验证 data Segment 处于目标时点击 Handshake signals 箭头，五个成员一次收起，
时钟、复位、刻度、工程、Undo 和 Saved 保持不变，按 Delete 为零效果；Right 恢复全部成员。
显式 data 定位会自动展开，折叠后隐藏 Group 不会留下不可见成员。专项
`wave-group-header-smoke` 及 Lane 隐藏/重排/自动滚动、信号标题、Wave Edit、Scenario 切换
回归全部通过。离屏截图 `build/group-collapse-smoke.png` 与
`build/group-collapse-smoke-expanded.png` 已复核折叠标题、层级线、Target、刻度与状态反馈。

长期 Goal 保持 active；第 660 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 661–670 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
71/71 CTest 全部通过，耗时分别为 9.41 s、9.14 s、8.67 s（`--parallel 4`）；核心入口为
46/46，全部 GUI 路径使用 offscreen。Group 成员维护不再要求逐条进入完整 Lane 属性窗口后再
手工重排：信号标题右键提供带当前勾选状态的 `Move to group` 子菜单，`No group` 原位解除成员
关系；将信号标题直接拖到可见 Group 行也会归组，并把信号放到该 Group 最后一个既有成员之后。
归属和顺序只形成一个可 Undo 的 `SetLaneGroupCommand` 状态。

拖放到 Group 与普通上下重排使用不同反馈：Group 整行显示蓝色轮廓、半透明底色和
`Move <signal> into <group>`，普通重排仍显示插入线。折叠 Group 仍可接收信号；释放后自动展开、
显示并选中新成员。重复归入当前 Group 为零变化，不截断 Redo；无效或歧义 Group、非 Group 目标
及 Group 嵌套被拒绝。解除归组不移动信号，Event、Relation、Segment、ClockDomain、波形内容及
工程格式均保持不变。

Codex 用户视角验证先把 reset_n 通过右键一次放入 Handshake signals，并确认其出现在原五个成员
之后；一次 Undo 同时恢复原归属和原顺序。随后收起 Group，将 reset_n 拖到标题行，拖动中即可
确认目标而无需猜测插入线含义；释放后六个成员重新展开且 reset_n 保持选中。右键勾选状态准确，
`No group` 一次解除且不改变顺序；连续 Undo 最终回到 Group 隐藏、reset_n 未分组的 Saved 基线。
离屏截图 `build/group-membership-smoke.png` 已复核拖放目标、折叠状态、时间刻度与信号波形。

长期 Goal 保持 active；第 670 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 671–680 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
72/72 CTest 全部通过，耗时分别为 9.55 s、8.93 s、8.65 s（`--parallel 4`）；核心入口为
47/47，全部 GUI 路径使用 offscreen。`Edit > Add group…` 不再打开包含类型、颜色、高度、
可见性等无关信息的完整 Lane 属性页，只显示名称输入；空名和大小写不敏感的信号/Group 重名
会在原对话框即时阻止，默认名称自动唯一。创建后直接显示空 Group，并明确提示拖入信号、
`Move to group` 和 Ctrl+Z；高级属性仍保留在 Group 标题右键中。

信号标题右键的 Group 子菜单新增 `New group with this signal…`。用户只命名一次，新的 Group
便插入当前信号正上方，当前信号成为首成员并保持选中；即使信号原属其他 Group，也只迁移该信号，
不改变原 Group 的其他成员。`CreateGroupWithLaneCommand` 将新 Group、成员引用和顺序封装为
一个 Undo 状态；空/重复稳定 ID、空名称、非 Group 数据及 Group 嵌套均拒绝。Event、Relation、
Segment、ClockDomain 和波形内容不变。

Codex 用户视角先从 Edit 创建 `Control signals`，对话框只要求名称，空名与已有
`Handshake signals` 均原位给出错误，合法名称一次创建；Undo 精确回到 Saved。随后在只有隐藏
Group 的情况下右键 reset_n，仍能看到 `New group with this signal…`，输入 `Reset controls`
后立即得到“Group 标题 + 缩进 reset_n”层级，状态栏说明首成员、Ctrl+Z 和继续拖入信号。
Undo/Redo 往返后再次 Undo，工程、归属、顺序和 Saved 完全回到基线。离屏截图
`build/group-creation-smoke.png` 已复核新 Group、首成员层级、时间刻度、波形和结果反馈。

长期 Goal 保持 active；第 680 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 681–690 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，耗时分别为 9.95 s、9.38 s、8.84 s（`--parallel 4`）；核心入口为
48/48，全部 GUI 路径使用 offscreen。信号名区域现在支持 `Ctrl+单击` 增减多选和
`Shift+单击` 连选可见非 Group 信号；选择按画布顺序保持，Group 标题与折叠成员不会被意外
纳入。右键任一已选信号仍保留整组选择，Target 和状态栏同时显示数量、活动信号及后续 Group
动作；Esc 可安全退回单信号。

`SetLanesGroupCommand` 将全部所选信号归入已有 Group 或解除归组封装为一个 Undo 状态，只移动
实际需要迁移的信号，并保持原显示顺序；已经属于目标的成员原位保留。`CreateGroupWithLanesCommand`
在最早所选信号位置创建 Group，并把全部所选信号按 Scenario 顺序连续放入。空/重复源、Group
嵌套、无效目标和零变化均不会产生部分结果或污染历史。提交后必要 Group 自动展开，全部选择与
活动信号恢复；重命名、复制、隐藏和删除在多选时明确要求单信号，不执行隐式批量破坏。

Codex 用户视角验证先用 Ctrl 增加并移除 req，再从 reset_n Shift 连选到 ack；三行高亮和
`Target: 3 signals` 无需额外面板即可确认。右键选择 `New group with selected signals…`，
只输入 `Control bundle` 后三条信号按原顺序成为成员且保持选中，一次 Undo 精确恢复原 Group
归属和顺序。随后把同三条信号归入 Handshake signals，反馈明确显示只有 reset_n 迁移，
req、ack 已在目标中；再次一次 Undo 恢复。独立 `wave-group-batch-smoke` 与核心命令测试覆盖
上述路径，离屏截图 `build/group-batch-smoke.png` 已复核多行选择、Group 层级、刻度、波形和
一次 Undo 反馈；既有单信号 Group 创建/维护、折叠、Lane 重排/自动滚动及标题专项均继续通过。

长期 Goal 保持 active；第 690 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 691–700 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，耗时分别为 10.39 s、9.60 s、9.90 s（`--parallel 4`）；核心入口为
49/49，全部 GUI 路径使用 offscreen。标题多选现在可以直接形成一次批量隐藏或批量删除：
右键及 Edit 菜单按数量显示动作，Delete/Backspace 进入同一删除路径。隐藏后底部恢复数量
立即更新；删除前确认框汇总信号、Event、Relation 和 trace mapping 影响，Cancel 不改变模型、
Saved 或选择。重命名和复制继续限定单信号。

`HideLanesCommand` 原子保存并修改每条所选信号的可见性；`RemoveLanesCommand` 原子移除 Lane、
所属 Event、引用 Relation 和不再被其他 Scenario 使用的导入映射。共享 Lane ID 的映射保留；
Undo 只向仍存在的 Trace 合并恢复原映射，不覆盖命令后新导入的 Trace。重复/歧义身份、Group
源及空选择在执行前拒绝。命令历史选择过渡现包含标题选择语义：Undo 恢复全部标题高亮、活动
信号和 tick，Redo 对隐藏或删除结果清除悬空目标，并同步 Target、菜单状态和状态栏。

Codex 用户视角先选择 reset_n、req、ack，通过右键一次隐藏三条信号；Undo 恢复三行选择，
Redo 清除目标，再次 Undo 回到 Saved。随后只选择 req 和 ack，首次 Delete 在依赖预览中取消，
模型和选择保持；再次 Delete 确认后两个信号及依赖一次消失，reset_n 与其他波形不变。Undo
精确恢复 Saved、req/ack 双行高亮和活动 ack，Redo/Undo 往返仍一致。扩展后的
`wave-group-batch-smoke` 与核心批量清理用例覆盖上述路径；离屏截图
`build/group-batch-smoke.png` 已复核 `Target: 2 signals · active ack`、双行高亮、刻度、
波形、Saved 和删除 Undo 反馈。既有 Group、单信号隐藏/删除、标题、重排及波形编辑回归通过。

长期 Goal 保持 active；第 700 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 701–710 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，耗时分别为 10.59 s、9.24 s、9.40 s（`--parallel 4`）；核心入口为
50/50，全部 GUI 路径使用 offscreen。标题多选不再在开始拖动时退化为单信号：无修饰键抓住
任一已选名称，全部选择会按当前 Scenario 顺序组成连续块，普通投放保持每条信号原 Group 归属，
拖到 Group 标题则一次提交混合成员归组。两种投放分别显示带数量的插入线标签和 Group 整行
目标，提交只形成一个 Undo 状态。

新增 `MoveLanesCommand` 在构造阶段验证全部唯一、存在且非 Group 的源，并从原 Scenario 收集
成员，不采用点击顺序。插入槽在移除所选成员后确定性换算，首尾、前后、非连续选择和块内部
零变化使用同一规则；零变化不写历史或截断 Redo。Event、Relation、Segment、ClockDomain、
Lane 内容和 `groupId` 均保持。Esc 取消拖动会恢复视图、保留模型、多选和 Redo；只单击已选
名称而未形成拖动，仍明确收敛为该单信号。

Codex 用户视角验证从 reset_n Shift 连选到 ack，抓住 ack 拖到 state 前，插入线直接显示
`Move 3 selected signals`；释放后三条信号按原画布顺序连续移动，一次 Undo/Redo 往返精确。
再次拖动按 Esc 不改变 Saved 或选择，单击不拖动则只保留所点名称。显示 Handshake signals 后
将同三条信号拖到标题，整行反馈显示目标；释放后状态栏准确报告 1 moved、2 already there，
一次 Undo 恢复。离屏截图 `build/group-batch-drag-preview.png`、
`build/group-batch-group-drop-preview.png` 和 `build/group-batch-smoke.png` 已复核选择、
反馈、刻度、波形与状态；单信号重排/自动滚动、Group、隐藏/删除和 Wave Edit 回归通过。

长期 Goal 保持 active；第 710 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 711–720 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，耗时分别为 9.84 s、9.00 s、9.24 s（`--parallel 4`）；核心入口为
50/50，全部 GUI 路径使用 offscreen。标题多选现在可用 `Alt+↑/↓` 整体移动一个可见行，不再
要求为精确微调重新使用鼠标。Edit 菜单按选择数量显示动作，Target 直接公开快捷键；隐藏 Lane、
隐藏 Group 和折叠成员不会消耗一次按键，顶部或底部边界会禁用对应方向。

批量步移与鼠标整组拖动共用 `MoveLanesCommand`，提交前按 Scenario 验证并归一化唯一、可见、
非 Group 的选择，再按画布实际显示行寻找相邻目标。结果保持相对顺序、每条 Lane 的 `groupId`、
Event、Relation、Segment、ClockDomain 与波形内容，只形成一个 Undo 状态。命令前后标题选择
快照进入历史；Undo/Redo 会恢复全部高亮、活动信号和 Saved。边界调用只反馈当前位置，不创建
状态或截断历史；单信号与 Group 原有 Alt+方向键路径未改变。

Codex 用户视角验证选择 reset_n、req、ack 后按 Alt+Down，隐藏的 Handshake Group 被正确跳过，
data[7:0] 一次出现在选择块上方，三条信号保持原顺序、原归属和共同高亮。Undo 回到 Saved，
Redo 精确恢复。按 Alt+Up 到达可见顶部后动作立即禁用，再次请求只显示边界反馈且一次 Undo
即可回到基线。扩展后的 `wave-group-batch-smoke` 使用真实快捷键并保存
`build/group-batch-keyboard-step.png`；单信号重排、自动滚动、Group、隐藏/删除、整组拖动和
Wave Edit 回归通过。

长期 Goal 保持 active；第 720 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 721–730 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，顺序执行耗时分别为 36.91 s、35.30 s、33.98 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。标题多选现在可直接使用 `Ctrl+D`、Edit 菜单或右键菜单复制；
副本按当前 Scenario 顺序组成连续块并插在最后一个源信号后，动作文字和 Target 显示数量。

新增 `DuplicateLanesCommand` 将全部 Lane 副本及必要 ClockDomain 封装为一个 Undo 状态。
每个副本获得唯一名称、新 Lane/Segment 身份和重新分配的可读颜色，同时保留波形、属性、
扩展字段及 Group 归属；Clock 各自获得独立时钟域，普通信号保持原关联时钟。Event、Relation
和 Imported Trace 映射不会被猜测复制。空、重复、既有或歧义身份、Group 源、无效 Group、
缺失/额外 ClockDomain 和越界插入均在修改前拒绝，不产生部分结果。

Codex 用户视角验证选择 reset_n、req、ack 后按 Ctrl+D，三条 `_copy` 副本一次连续出现并共同
高亮，活动信号对应 `ack_copy`；波形和归组保持，颜色与各自源信号区分，状态栏明确说明依赖
链接未复制。一次 Undo 删除整个副本块并恢复原三条选择和 Saved，Redo 使用稳定身份恢复副本块
及副本选择，再次 Undo 精确回到基线。扩展后的 `wave-group-batch-smoke` 保存
`build/group-batch-duplicate.png`；标题、Edit 菜单、Group、清理、步移、拖动和 Wave Edit
既有回归继续通过。

长期 Goal 保持 active；第 730 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 731–740 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 39.09 s、36.57 s、35.26 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。复制入口现已收敛为一个上下文 `Ctrl+D`：显式时间范围优先重复
到其后，没有范围时继续复制标题单选、多选或当前 Segment 所属信号。Edit 菜单使用同一个动态
动作显示实际目标，旧的独立范围复制动作已移除。

范围重复继续保持多信号内容、隐式空白和 Segment 元数据，并原子处理 End 延长、Relation 清理、
结果范围选择及 Undo/Redo；系统剪贴板不改变。目标已经一致时只推进选择并显示
`no values changed`，不新增历史或截断 Redo。文本输入焦点继续阻止快捷键修改模型；信号标题
右键仍明确表示整信号复制，显式范围存在时保留范围并提示按 Esc。范围状态下的隐藏整项动作也
恢复为可触发的无损说明，不再静默禁用。

Codex 用户视角验证在 req 与 ack 上选定协议片段后直接按 Ctrl+D，两条结果波形一次写到选区
后方并继续高亮，状态栏同时说明目标数量、范围、剪贴板隔离、Relation 清理和 Ctrl+Z。一次
Undo/Redo 精确往返波形、范围、时间轴和依赖；零变化路径保持 Redo。清除范围后，同一快捷键
恢复批量信号复制语义。扩展后的 `wave-wave-edit-smoke` 保存
`build/wave-edit-smoke-duplicate-range-after.png`；画布添加、Edit 菜单、隐藏、批量复制及
全部既有回归继续通过。

长期 Goal 保持 active；第 740 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 741–750 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 37.93 s、35.74 s、34.95 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。固定范围栏现以可见 `Repeat` 取代低频 Cut 按钮，主栏顺序收敛为
Copy/Repeat/Paste/Clear；Cut 功能继续由 `Ctrl+X`、Edit 和选区右键提供。

`RangeEditRepeatButton` 直接复用既有范围重复路径，不引入第二套算法；单/多信号内容、隐式空白、
End 延长、Relation 清理、剪贴板隔离、结果范围和选择感知 Undo/Redo 语义保持。按钮提示同时
公开紧邻重复、剪贴板不变和 `Ctrl+D`；混合类型范围也直接说明 Copy/Repeat/Paste/Clear。
旧 Cut 按钮对象仅保留在“不存在”测试断言中，实际 Cut 的文本焦点隔离、范围清理和撤销均继续
通过。

Codex 用户视角验证框选 req 与 ack 后直接点击 Repeat，两条协议波形一次重复到其后并继续高亮，
状态栏说明剪贴板隔离、Relation 清理和 Ctrl+Z。第一次 960 像素验收捕获文字增宽导致的裁切后，
仅收紧范围栏横向留白和间距；最终 Bus/混合范围的摘要、Repeat、值输入和全部有效预设均完整
可见且中心可命中。`wave-wave-edit-smoke` 保存
`build/wave-edit-smoke-duplicate-range-after.png`；自动滚动、用户旅程、Edit 菜单、专注布局
及全部既有回归继续通过。

长期 Goal 保持 active；第 750 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 751–760 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 37.57 s、35.08 s、33.78 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。固定范围栏的 Paste 现在会在点击前预检 schema、复制宽度、
目标数量、信号类型、Bus/Enum 位宽及 Segment 值域；不兼容时按钮保持可见但禁用，提示直接
给出源→目标配对和修正条件。

新增 `rangePasteAvailability()` 作为固定按钮唯一的只读可用性来源；有效提示提前公开复制
信号数、完整宽度、固定选区左端点、逐信号映射、选区宽度差异和必要的 End 延长。Edit、
`Ctrl+V`、右键及实际命令路径仍保留完整运行时门禁，因而剪贴板或目标状态在提示后变化时仍会
原子拒绝。schema 1 源存在兼容、schema 2 源删除后快照恢复、End 延长和零变化历史抑制均未退化。

Codex 用户视角验证 2 条复制信号只对应 1 个目标时，Paste 原位禁用并明确说明数量差异；改选两个
兼容 Bit 目标后立即启用，提示预告 10 ns 内容落到 20 ns、两项映射和选区宽度差异，点击后两条
结果一次写入并保持选中。Bit→Bus、8-bit→4-bit Bus 和非法 Bit 值分别在点击前给出类型、位宽和
值域原因，波形、Saved 与 Undo 历史保持不变。扩展后的 `wave-wave-edit-smoke` 保存
`build/wave-edit-smoke-multi-target-paste.png`；添加信号、自动滚动、用户旅程、Edit 菜单、
波形专注布局及全部既有回归继续通过。

长期 Goal 保持 active；第 760 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 761–770 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 37.65 s、35.24 s、33.41 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。标题区域的单选、Ctrl 增减和 Shift 连选现在可直接作为编辑光标处
的 Paste 目标，目标优先级固定为显式时间范围、标题选择、单信号目标、无显式目标时的原复制集合。

固定范围栏与标题提示共用目标参数化预检，按可见顺序验证数量、类型、Bus/Enum 位宽和值；标题
数量或配对不兼容时状态栏立即说明原因，执行层再次原子拒绝，不再回写原源信号。Target 摘要公开
活动信号和光标落点，工具提示说明 Ctrl+G 与 Ctrl+V。成功路径继续复用 `PasteRangeCommand`，
结果转为持久范围；Undo 恢复原标题集合与光标，Redo 恢复结果范围。多标题复制、隐藏和删除的
历史反馈同步显示实际恢复数量。

Codex 用户视角验证复制 req/ack 后只选择 reset_n 时立即得到 2→1 数量提示，选择
reset_n/data[7:0] 时得到 ack (bit) → data[7:0] (bus) 类型提示，两次执行均零写入。保留
reset_n、精确定位 0 ps 并加入 req 后，Ctrl+V 按可见顺序一次写入 0–10 ns；结果范围继续可用
Repeat/Clear/批量赋值，一次 Undo/Redo 在标题集合与结果范围之间精确往返。
`wave-wave-edit-smoke` 保存 `build/wave-edit-smoke-header-target-paste.png`；标题批量、添加信号、
自动滚动、用户旅程、Edit 菜单及全部既有回归继续通过。

长期 Goal 保持 active；第 770 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 771–780 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 36.20 s、33.85 s、32.99 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。标题选择的 Paste 不再仅依赖用户记忆 `Ctrl+V`：只要系统剪贴板
包含 Wave 范围，既有信号标题右键菜单顶部即显示目标数量或名称和精确落点。

菜单与标题状态复用同一目标参数化预检。有效动作在提交前显示复制宽度及全部
`source → target` 映射；复制数量不符或 Bit→Bus 等配对不兼容时，动作仍可见但禁用，并显示
具体原因。菜单触发既有 `pasteAtCursor()` 和 `PasteRangeCommand`，执行时再次校验；End 延长、
Relation 清理、零变化抑制及选择感知 Undo/Redo 语义均未分叉。

Codex 用户视角验证复制 req/ack 后只选择 reset_n，右键菜单直接显示 Paste 入口和 2→1 数量
失败；选择 reset_n/data[7:0] 时显示 ack (bit) → data[7:0] (bus) 类型失败。改选
reset_n/req 后，菜单在执行前显示 10 ns、0 ps、req → reset_n、ack → req；单击一次完成
0–10 ns 粘贴，结果范围保持选中，一次 Undo/Redo 在原标题集合与结果范围之间精确往返。
`wave-wave-edit-smoke` 保存并复核 `build/wave-edit-smoke-header-paste-menu.png` 与
`build/wave-edit-smoke-header-target-paste.png`；标题批量、添加信号、自动滚动、用户旅程、
Edit 菜单及全部既有回归继续通过。

长期 Goal 保持 active；第 780 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 781–790 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 37.46 s、33.63 s、32.85 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。标题选择后的标尺单击或拖动不再清空粘贴目标：活动信号、全部
标题集合及可见映射顺序保持，Target、状态栏和 Paste 预检随当前标尺时间同步更新。

标尺交互现在在任何显式范围清理前保存临时选择快照。正常释放只确认新编辑时间，并保持标题目标；
Esc 或异常失去左键会恢复原光标、水平视图和标题集合；从显式范围开始时还恢复原范围、全部信号
和固定范围栏。快照不进入模型、命令栈、autosave 或工程格式，成功 Paste 继续使用既有
`PasteRangeCommand`。

Codex 用户视角验证复制 req/ack 后选择 reset_n/req，先将标尺误拖到 60 ns 并按 Esc，两条目标、
原光标和视图完整恢复；再单击 40 ns，Target 与状态栏显示 req → reset_n、ack → req。标题右键
Paste 一次写入 40–50 ns，结果范围保持选中，Undo/Redo 在原标题集合与结果范围之间精确往返。
显式范围的标尺取消同样恢复范围及固定栏。`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-header-ruler-target.png` 与
`build/wave-edit-smoke-header-target-paste.png`；标题批量、信号标题、光标模式、自动滚动、
用户旅程、Edit 菜单及全部既有回归继续通过。

长期 Goal 保持 active；第 790 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 791–800 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 36.15 s、34.22 s、33.34 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。第一次 Paste 的结果范围在标尺正常重定位时关闭，但保留原有序
信号集合并转换为标题目标；Target、状态栏和逐项映射立即采用新时间，下一次 Paste 不再要求
重新选择信号，也不会回退到复制源。

连续 Paste 继续使用既有 clipboard 预检和 `PasteRangeCommand`。40–50 ns 与 60–70 ns 两次提交
分别进入历史；Undo 依次恢复 60 ns、40 ns 的标题目标，Redo 依次恢复对应结果范围。标尺按 Esc
取消仍恢复原显式范围、固定栏、信号集合、光标与水平视图，转换本身零模型、零命令变化。

Codex 用户视角验证复制 req/ack、选择 reset_n/req 后，先在 40 ns Paste，再从结果范围直接单击
标尺 60 ns 并再次 Paste；全程只复制一次、只选目标一次，两段非相邻结果同时保留，映射始终为
req → reset_n、ack → req。`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-range-ruler-retarget.png` 与
`build/wave-edit-smoke-header-repeated-paste.png`；标题批量、信号标题、光标模式、自动滚动、
用户旅程、Edit 菜单及全部既有回归继续通过。

长期 Goal 保持 active；第 800 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 801–810 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 36.42 s、34.73 s、33.56 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。兼容标题目标在 Paste 前按实际 clipboard Segment 显示半透明虚线
波形、覆盖边界、起止时间和 Ctrl+V 标签；左侧标题显示当前值→预览值，相同值显示等值标记。

预览复用既有 schema、数量、类型、Bus/Enum 位宽、值和目标时间预检，并只构造临时 Lane 副本。
标尺拖动实时更新预览，Esc 恢复原目标与落点；不兼容目标不绘制，提交后随结果范围出现而消失，
Undo 恢复。整个预览生命周期不修改 Scenario、CommandStack、Saved、autosave、稳定 ID 或工程格式。

Codex 用户视角验证复制 req=0、ack=1 后选择 reset_n/req，在 60–70 ns 提交前直接看见两条差异
虚线及 `1→0`、`0→1`；原实线、网格和刻度仍可辨认。拖动与取消、真实 Paste、连续 Paste 及
两步 Undo/Redo 均保持对应预览生命周期。`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-header-paste-preview.png`；自动滚动、标题批量、信号标题、光标模式、
用户旅程、Edit 菜单及全部既有回归继续通过。

长期 Goal 保持 active；第 810 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 811–820 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 36.60 s、34.35 s、33.15 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。标题目标 Paste 预览现在会按真实覆盖后的完整 Segment 拓扑和
Event 复用顺序预测 Relation 清理；无影响时保持蓝色，存在影响时以琥珀色显示准确数量。

预检在临时 Lane 副本中复现范围清除、后写覆盖、归一化、extensions 更新，以及
`linkedSegmentId`、`tick+value`、`tick` 的 Event 复用优先级。画布标签、标题右键 Paste 和
状态栏统一公开 `removes N relation(s)` 与 Ctrl+Z 恢复说明；预检不修改 Scenario、历史、
Saved、autosave、稳定 ID 或工程格式。

Codex 用户视角验证 req/ack 的 80–90 ns 预览在提交前显示琥珀色虚线、`1→0`、`0→1` 和
`removes 1 relation`，原 Relation 连线保持可见；真实 Paste 恰好清理一条关系，一次 Undo 同时
恢复波形、关系、标题目标、80 ns 落点和同一预警。安全落点预测 0 且提交后关系保留。
`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-header-paste-relation-warning.png`；自动滚动、标题批量、信号标题、
光标模式、用户旅程、Edit 菜单及全部既有回归继续通过。

长期 Goal 保持 active；第 820 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 821–830 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 36.88 s、34.06 s、32.83 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。Paste 风险预检现在除数量外还生成 Relation 描述、源/目标信号与
精确端点时间；安全落点返回空影响列表，风险提交与 Undo 的摘要生命周期严格对应。

同一帧只生成一份 Paste 投影并同时驱动波形与 Scenario overlay。受影响 Relation 使用琥珀色
光晕虚线、双端点环和中点删除标记；未受影响连线保留原严重级别样式。画布标签直接显示首条端点，
右键 Paste 与状态栏列出全部关系描述和端点；未增加模式、按钮、持久状态或工程字段。

Codex 用户视角验证 80–90 ns 风险落点可直接读到
`req @ 80 ns → ack @ 110 ns`，并从光晕、虚线、端点环和 × 明确定位唯一受影响连线；
菜单同时显示 `ack must rise within 1..4 cycles after req`。真实 Paste 清理该 Relation，Undo
恢复描述、强调、波形、标题目标和落点；20 ns、60 ns 安全落点无误报。
`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-header-paste-relation-identity.png`；自动滚动、标题批量、信号标题、
光标模式、用户旅程、Edit 菜单及全部既有回归继续通过。

长期 Goal 保持 active；第 830 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 831–840 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 36.27 s、33.71 s、32.83 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。显式时间范围现在与标题目标共享同一 Paste 投影、兼容性门禁、
Event/Relation 影响预测和画布绘制。

显式选区继续显示用户选定宽度，虚线边界与标签独立显示 copied duration；固定 Paste 按钮和范围
右键对数量、类型、位宽、值及 Relation 风险给出一致结果，已知无效动作直接禁用。提交后按
Segment 起止、值和 extensions 判断语义等值，不因内部 ID 不同残留陈旧预览；Undo 返回原目标时
预览恢复。整个预检过程零模型、零历史、零 autosave 变化。

Codex 用户视角验证 40–60 ns 双信号选区在提交前直接显示实际 40–50 ns 覆盖；2→1 数量不匹配时
固定栏和右键同时禁用。req/ack 的 80–100 ns 选区显示 80–90 ns 风险波形及具体 Relation 端点和
琥珀色强调；提交后虚线消失，Undo 恢复原选区与投影。
`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-explicit-range-paste-preview.png` 与
`build/wave-edit-smoke-explicit-range-paste-relation-warning.png`；自动滚动、标题批量、
信号标题、光标模式、用户旅程、Edit 菜单及全部既有回归继续通过。

长期 Goal 保持 active；第 840 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 841–850 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 37.06 s、34.34 s、32.49 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。Paste 投影现在显式公开 `modelChanges`，由最终波形语义差异、
Relation 清理和 End 延长共同决定，内部 Segment ID 不影响结果。

固定范围栏、范围右键、波形右键、标题状态和标题右键对已匹配目标统一禁用 Paste 并给出同一原因。
显式选区宽于 copied duration 时保留中性宽度投影；等宽目标和标题目标不残留陈旧预览。直接
`Ctrl+V` 仍经过执行层语义守卫，只报告跳过，不修改模型、选择、Undo/Redo、Saved 或 autosave。
多信号波形右键保持原复制信号集合，单信号波形右键使用点击信号。

Codex 用户视角验证复制 Clock 的 20–30 ns 并选择 40–60 ns 后，固定栏与范围右键在点击前说明目标
已匹配，画布仍以中性虚线显示实际 copied width 为 40–50 ns。直接分发 Paste 只显示
`Paste skipped`、`already matches copied range` 与 `no values changed`，原选区和既有 Redo 保持；
退出范围后，40 ns 波形右键、Clock 标题状态和标题右键均给出相同禁用原因且无陈旧预览。
`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-explicit-range-paste-preview.png` 与
`build/wave-edit-smoke-paste-no-effect-preflight.png`；七项关联专项和全部既有回归继续通过。

长期 Goal 保持 active；第 850 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 851–860 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 36.09 s、34.28 s、33.52 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。显式范围 Clear/Run 现在会在提交前按真实清除、拆分、归一化、
Event 复用和 Relation 清理语义完成纯投影，并区分安全、风险和无效果三态。

安全动作公开受影响信号数、精确范围和 `No Relation will be removed`；风险动作显示琥珀色
`Clear ⚠N`，并列出准确 Relation 描述及源/目标端点；无效果动作在固定栏和范围右键直接禁用。
范围 Cut 与 Edit > Cut 复用同一影响结果，但隐式源范围仍可复制。Delete 与 Clock `R` 的运行时
兜底不创建命令、不清除 Redo。预检不修改 Scenario、历史、Saved、autosave 或稳定 ID。

Codex 用户视角验证 req/ack 的 80–100 ns 范围在点击前显示 `Clear ⚠1` 和
`req @ 80 ns → ack @ 110 ns`；真实 Clear 恰好删除该 Relation，一次 Undo 同时恢复波形、关系、
选区和预警。Clock 的 40–60 ns 正常范围在点击前禁用 Run，Delete/`R` 只报告无变化且保留 Redo，
Cut 仍可复制并说明不会移除源值。`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-range-clear-relation-warning.png` 与
`build/wave-edit-smoke-range-clear-no-effect-preflight.png`；七项关联专项和全部既有回归继续通过。

长期 Goal 保持 active；第 860 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 861–870 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 36.63 s、34.05 s、32.75 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。显式范围的预设值、Clock Gate/Disable 和 Bus/Enum 文本草稿现在
共用纯候选投影，在点击或 Enter 前区分安全、Relation 风险、无效果和非法输入。

预设按钮显示真实变化信号数；等值按钮直接禁用，风险按钮显示琥珀色 `值 ⚠N` 并列出准确 Relation
描述与端点。Bus/Enum 草稿即时定位具体位宽或符号错误，并公开有效值的影响；执行层对等值快捷键和
Enter 只报告跳过，不修改选择、Undo/Redo、Saved 或 autosave。投影使用隔离 Segment 身份，不消耗
工程稳定 ID；实际提交后的变化数、Relation 删除和 Undo 恢复与预检逐项一致。

Codex 用户视角验证 req/ack 的 80–100 ns 范围在点击前显示 `0 ⚠1`、`1 of 2` 和
`req @ 80 ns → ack @ 110 ns`；真实写值恰好修改 req 并删除该 Relation，重复按 0 无历史，
一次 Undo 恢复波形、关系、选区和预警。不同位宽 Bus 输入 `0xa5` 会在 Enter 前定位
data_small，`0xa` 安全写入，重复值不增加历史。Clock 的 200–210 ns 已禁用范围在点击前禁用
Disable，键盘 X 只报告无变化并保留 Redo。`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-range-value-relation-warning.png` 与
`build/wave-edit-smoke-range-value-no-effect-preflight.png`；八项关联专项和全部既有回归继续通过。

长期 Goal 保持 active；第 870 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 871–880 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 36.91 s、36.03 s、34.09 s；核心入口为 51/51，
全部 GUI 路径使用 offscreen。显式范围 Repeat 现在会在提交前按真实覆盖、拆分、归一化、
Event 复用、Relation 清理和 End 延长语义生成确定性目标投影。

固定 Repeat、范围右键、Edit 菜单和 `Ctrl+D` 共用同一可用性结果。安全动作提前显示源范围、
目标范围、信号数和 Relation 保证；风险动作显示琥珀色 `Repeat ⚠N`，并列出准确 Relation 描述
与端点；无效果动作在可见入口直接禁用。悬浮 Repeat 会在目标位置显示虚线波形，风险 Relation
同步强调，并优先于剪贴板 Paste 预览。只要会延长 End，即使波形语义等值，动作仍保持可用并提前
显示新 End。

执行层对直接 `Ctrl+D` 保留最终守卫：已匹配目标只报告 `Repeat skipped`，源选区、活动时间、
模型、剪贴板、Undo/Redo、Saved 和 autosave 均不变化；成功结果才移动到目标范围并进入一次历史。
实际 Relation 删除和状态栏回显与预检一致，一次 Undo 同时恢复波形、关系、源选区与风险预警。

Codex 用户视角验证 req/ack 的 80–100 ns 范围在点击前显示 `Repeat ⚠1`，目标 100–120 ns 以
琥珀虚线呈现，并标出
`ack must rise within 1..4 cycles after req · req @ 80 ns → ack @ 110 ns`；真实 Repeat 恰好删除
该 Relation，Undo/Redo 精确往返且剪贴板不变。Clock 的 20–30 ns 范围在点击前禁用 Repeat，
目标 30–40 ns 仍以中性虚线解释，`Ctrl+D` 不移动源选区并保留既有 Redo。Clock 的 210–220 ns
范围因目标超出 End 而保持可用，提交前显示 `Extends End to 230 ns`，提交和 Undo 各为一步。
`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-repeat-relation-warning.png` 与
`build/wave-edit-smoke-repeat-no-effect-preflight.png`；八项关联专项和全部既有回归继续通过。

长期 Goal 保持 active；第 880 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 881–890 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 38.59 s、35.97 s、33.72 s；核心入口为 51/51，
八项关联 offscreen 专项为 8/8。显式范围 Move 与 Ctrl+拖动 Copy 现在会在释放前按真实命令顺序
投影源清除、目标覆盖、Segment 归一化、Event 复用、Relation 清理和 End 延长。

安全目标保持蓝色虚线并明确保证不删除 Relation；风险目标改为琥珀色并显示 `Move/Copy ⚠N`、
准确 Relation 描述及源/目标端点，受影响连线同步强调；波形、Relation 与 End 均无变化时使用
中性预览并显示 `no Move/Copy needed`。投影使用隔离 Segment 身份，不修改 Scenario、命令历史、
Saved、autosave、剪贴板或稳定 ID。

执行层在命令创建前复用同一投影。无效果释放只报告 `Move/Copy skipped`，恢复源选区和光标并保留
既有 Redo；成功结果才选择目标并进入一次历史。跨信号映射、Ctrl 按压/释放切换、重叠拒绝、类型
不兼容、纯 End 延长和实际 Relation 删除均与预检及一次 Undo 逐项一致。

Codex 用户视角验证 req/ack 的 80–100 ns 移到 120–140 ns 时，释放前直接显示
`Move ⚠1` 与
`ack must rise within 1..4 cycles after req · req @ 80 ns → ack @ 110 ns`，对应连线呈琥珀色；
释放后恰好删除该 Relation，Undo 恢复波形、关系和源选区。Ctrl+拖到 140–160 ns 显示安全保证；
将已匹配的 20–40 ns 复制到 160–180 ns 或移动空白 Clock 范围只显示跳过，Redo 与剪贴板保持。
复制末尾 210–220 ns 到 220–230 ns 会提前显示并原子提交 End 延长。
`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-range-move-preview.png`、
`build/wave-edit-smoke-range-copy-no-effect-preflight.png` 与
`build/wave-edit-smoke-range-copy-end-extension-preflight.png`；全部既有回归继续通过。

长期 Goal 保持 active；第 890 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 891–900 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 37.35 s、34.62 s、33.57 s；核心入口为 51/51，
八项关联 offscreen 专项为 8/8。单个 Segment 的主体移动、Ctrl+拖动复制和左右边界调整现在
共用同一释放前投影，严格复现相邻 Segment 边界联动、覆盖、拆分、归一化、extensions、
Event 复用和 Relation 清理。

安全预览使用蓝色，只在实际受影响时段叠加最终虚线波形并保证不删除 Relation；风险预览使用
琥珀色 `Move/Copy/Resize ⚠N`，同步显示准确 Relation 描述、源/目标端点和受影响连线；
语义无变化时使用中性色并显示 `no Copy needed`。Ctrl 按下与释放、鼠标连续移动和画布标签
全部读取同一投影，不修改 Scenario、稳定 ID、命令历史、Saved、autosave 或剪贴板。

提交层在创建 Segment 命令前复用投影。无变化 Copy 保留源 Segment、选区、光标、既有 Redo
和剪贴板；成功结果才选择提交后的 Segment 并进入一次历史。`EditSegmentCommand` 也按完整
Scenario 前后状态过滤真正的无效果命令；实际 Relation 删除描述与预检一致，一次 Undo 同时
恢复波形、关系和源目标。

Codex 用户视角验证 data[7:0] Segment 移动时，受影响部分直接显示最终蓝色虚线和精确宽度；
Ctrl+拖动仍落回原同步拍时，释放前显示 `no Copy needed`，释放后源目标、Redo 和剪贴板均不变；
复制跨过被 Relation 引用的边沿时，释放前显示琥珀色目标及
`temporary Segment Copy risk relation · data[7:0] @ 100 ns → ack @ 110 ns`，释放后恰好清理
该 Relation，一次 Undo 完整恢复。`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-segment-move-preview.png`、
`build/wave-edit-smoke-segment-copy-no-effect-preflight.png` 与
`build/wave-edit-smoke-segment-copy-relation-warning.png`。

长期 Goal 保持 active；第 900 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 901–910 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 38.08 s、35.61 s、34.37 s；核心入口为 51/51，
百万 transition 指标为 21 ms；八项关联 offscreen 专项为 8/8。Edit > Segment 的八项精确
操作与 Bus/Enum 右键相邻复制现在共用点击前评估，直接返回目标范围、约束结果、模型是否变化
以及将删除的准确 Relation 集合。

不可用动作显示具体边界或相邻内容原因并提前禁用；语义等值动作显示 `no change` 并不创建命令；
安全动作悬浮显示蓝色最终虚线波形和无 Relation 保证；风险动作显示 `⚠N`、琥珀色最终波形、
受影响连线、准确描述和端点。移向非预览项、关闭菜单、改变目标或刷新模型都会清除临时投影。
投影不修改 Scenario、稳定 ID、命令历史、Saved、autosave 或剪贴板；八项状态只在选择、模型、
Timing 或菜单上下文变化时重新计算，不进入每次指针移动的状态更新路径。

提交层在创建命令前复用同一评估。真实修改范围、Relation 清理数量和描述均与预检一致，一次
Undo 同时恢复波形、关系与源目标；不可用和无效果入口保持选择、Redo、剪贴板及模型不变。
右键菜单从复制项移向其他操作时立即清除旧预览，避免将未提交波形误认为当前动作结果。

Codex 用户视角验证 data[7:0] 的 100–170 ns Segment：悬浮 `Duplicate Segment before`
直接显示 30–100 ns 蓝色最终波形与 70 ns 宽度；加入临时 Relation 后，菜单显示 `⚠1`，
目标与受影响连线改为琥珀色并列出准确端点，点击后恰好删除该关系，一次 Undo 完整恢复。
当相邻目标已与源值一致时，动作在点击前显示 `no change` 并禁用，历史、Redo、剪贴板和选择
均不变化；时间轴末拍的向后复制显示 `unavailable` 及超过 End 的原因。
`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-segment-menu-safe-preview.png`、
`build/wave-edit-smoke-segment-menu-relation-warning.png` 与
`build/wave-edit-smoke-segment-menu-no-effect-preflight.png`。最终右键悬浮收口后，默认构建及
`wave-canvas-add-lane-smoke`、`wave-wave-edit-smoke` 两项 offscreen 针对性回归再次通过。

长期 Goal 保持 active；第 910 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 911–920 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 38.38 s、37.35 s、36.26 s；核心入口为 51/51，
百万 transition 指标为 25 ms。Bus/Enum 的 Beat/Segment 就地编辑器现在会在提交前对文本草稿、
0/X/Z/Don't care 与 Clear 建立确定性候选投影。

非法输入在键入时变红并禁用 Apply 与前后导航；等值草稿显示 `Done` 和 `no command will run`；
安全修改显示蓝色最终虚线波形；会清理依赖的操作显示琥珀色 `⚠N`、准确 Relation 描述和端点。
悬浮预设只临时替换画布预演，离开即恢复当前文本草稿。预演不修改 Scenario、稳定 ID、命令历史、
Saved、autosave 或剪贴板；提交前目标失效时原位阻断，无效果路径不创建命令。

提交层复用同一影响结果。Clear 实际删除的 Relation 数量和完整摘要与预检一致，一次 Undo 同时
恢复波形与依赖；Enum 符号循环、Bus 数值步进和最近值切换在刷新候选后仍保留其操作反馈，无效
草稿阻断目标切换时明确说明目标保持。

Codex 用户视角验证 data[7:0] 的 100–170 ns Segment：输入 `0x2a` 后 Enter 前即可看到蓝色最终
波形；输入既有 `0x35` 显示中性 no change 与 Done，提交不增加历史；输入越位宽 `0x1ff` 时提交
和导航均不可用。危险 Clear 在点击前显示 `Clear ⚠1` 及
`temporary Bus editor clear relation · data[7:0] @ 100 ns → ack @ 110 ns`，点击后恰好删除
该关系，一次 Undo 完整恢复。`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-bus-editor-safe-preview.png`、
`build/wave-edit-smoke-bus-editor-no-effect-preflight.png`、
`build/wave-edit-smoke-bus-editor-invalid-preflight.png` 与
`build/wave-edit-smoke-bus-editor-clear-relation-warning.png`；全部 GUI 路径使用 offscreen。

长期 Goal 保持 active；第 920 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 921–930 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 37.16 s、35.08 s、33.75 s；核心入口为 52/52，
百万 transition 指标为 20 ms，全部 GUI 路径使用 offscreen。Bus/Enum 既有 Beat 编辑框现在
直接识别逗号、分号、空格、Tab 或换行分隔的连续值，最多实时预演 1024 项，不新增编辑模式。

每项沿用当前进制或 Enum 符号校验，错误直接定位到值序号，单独 `X/Z` 扩展到完整 Bus 位宽。
候选投影显示全部目标拍、规范化值、Relation 风险和 End 延长；预设悬浮离开后恢复整段草稿。
等值列表显示 `Done` 且不创建命令。列表期间禁用会造成误解的前后导航和 Beat→Segment 切换；
Segment 中粘贴列表会阻断提交，点击 `Segment` 可切回 Beat 并保留原草稿。

`SetLaneSequenceCommand` 将连续范围写入、End 延长、Event 同步和 Relation 清理收口为一次原子
历史；提交后选中完整结果范围，一次 Undo/Redo 精确恢复波形、End、依赖和选择。预演不修改
Scenario、稳定 ID、命令历史、Saved、autosave 或剪贴板，超长、非法、失效和溢出目标原位阻断。

Codex 用户视角验证在 data[7:0] 的 110 ns Beat 粘贴
`0x12, 0x34, X, 0x56` 后，110–150 ns 四拍立即显示最终虚线波形和 `Apply 4`；第二项越位宽时
直接标出 `value 2`，三项等值时提交不新增 Undo。210 ns 起的三项列表提前显示 `End → 240 ns`，
提交和恢复各为一步；七拍归零在点击前显示 `Apply 7 ⚠1` 和准确 Relation 描述/端点，实际清理与
Undo 一致。Enum 的 `IDLE, WAIT_ACK, DONE` 使用同一路径，Segment→Beat 切换保留原列表。

`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-bus-sequence-safe-preview.png`、
`build/wave-edit-smoke-enum-sequence-safe-preview.png`、
`build/wave-edit-smoke-bus-sequence-invalid-preflight.png`、
`build/wave-edit-smoke-bus-sequence-no-effect-preflight.png`、
`build/wave-edit-smoke-bus-sequence-end-extension-preview.png`、
`build/wave-edit-smoke-bus-sequence-relation-warning.png` 与
`build/wave-edit-smoke-bus-sequence-segment-blocked.png`。

长期 Goal 保持 active；第 930 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 931–940 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 39.67 s、36.71 s、36.15 s；核心入口为 52/52，
百万 transition 指标为 22 ms，全部 GUI 路径使用 offscreen。单个 Bit 信号的显式范围栏现在
直接接收紧凑或分隔的 `0/1/X/Z` 模式及 `0b` 前缀，不新增编辑模式。

一个符号对应一个当前编辑拍，实时目标最多 1024 拍；短模式只有在能整除选中拍数时才重复，
目标不会隐式越过选区。
安全草稿显示完整蓝色虚线波形、模式长度、重复次数和范围；拍数不整除、符号过多、非法字符或
非完整拍立即变红并原位阻断。等值模式显示 `no command will run` 且不污染历史；Esc 只丢弃草稿。

Relation 风险按最终 Bit 边沿投影，字段、波形和受影响连线在提交前统一变为琥珀色，并列出准确
描述和端点。提交复用 `SetLaneSequenceCommand`，一次 Undo/Redo 同时恢复波形、Event、Relation
和选区。预演使用隔离 Segment 身份，不修改 Scenario、稳定 ID、Saved、autosave 或剪贴板。

Codex 用户视角验证 req 的 20–60 ns 四拍输入 `0b01` 后立即显示 0101，Enter 只产生一次 Undo；
输入 `010` 直接说明四拍不能整除三符号模式，模型和纠错目标保持；分隔的 `0, 1; X Z` 被识别为
四个准确值。常量模式会删除临时 Relation 时，提交前显示琥珀色最终波形、准确关系摘要和端点，
真实清理与一次 Undo 完全一致。

`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-bit-pattern-safe-preview.png`、
`build/wave-edit-smoke-bit-pattern-invalid-preflight.png`、
`build/wave-edit-smoke-bit-pattern-no-effect-preflight.png` 与
`build/wave-edit-smoke-bit-pattern-relation-warning.png`。

长期 Goal 保持 active；第 940 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 941–950 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 40.42 s、37.15 s、36.54 s；核心入口为 53/53，
百万 transition 指标为 24 ms，全部 GUI 路径使用 offscreen。共享准确拍网格的多个 Bit 信号
现在可在同一个显式范围栏中接收同一 `0/1/X/Z` 模式，不新增编辑模式或确认步骤。

输入阶段为每条目标信号绘制完整最终虚线波形，并显示模式长度、重复次数、拍数、信号数和实际
变化数。不同拍网格、非完整拍、模式不整除和非法字符在提交前原位阻断；最多实时评估 1024 拍和
4096 signal-beats。等值批次显示 `no command will run`，不创建命令或破坏既有 Redo。

`SetLaneSequencesCommand` 在修改前校验全部目标，并以一个 Scenario 前后快照原子提交多条序列；
任一目标失败均无部分写入。全部 Lane 的最终边沿共同决定 Event 同步与 Relation 清理，预演和提交
使用同一投影。一次 Undo/Redo 同时恢复所有波形、Event、Relation 和选区。视觉验收还发现并修复
多行范围绘制中的画刷状态泄漏，第二条信号不再被不透明蓝块覆盖，刻度和预演保持可见。

Codex 用户视角验证 req 与 ack 的 20–60 ns 四拍只输入一次 `01`，两条信号立即显示 0101，
Enter 后同时生效且只产生一次 Undo；再次输入 `01` 不增加历史。常量 `0` 会删除临时 Relation
时，字段、两条最终波形和连线在提交前统一显示琥珀色风险及准确描述，真实清理与一次 Undo 一致。
当两条信号使用不同拍网格时，字段立即变红并明确说明 `different beat grids`，Enter 不改变模型，
草稿和原选区保持可修正。

`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-bit-pattern-multi-safe-preview.png`、
`build/wave-edit-smoke-bit-pattern-multi-relation-warning.png` 与
`build/wave-edit-smoke-bit-pattern-multi-grid-mismatch.png`。

长期 Goal 保持 active；第 950 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 951–960 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 40.93 s、37.80 s、36.96 s；核心入口为 53/53，
百万 transition 指标为 21 ms，全部 GUI 路径使用 offscreen。多 Bit 范围现在可在同一既有字段
中按信号录入不同模式，不新增编辑模式、面板或确认步骤。

不含 `/` 的模式继续广播到全部目标；`01 / 0011` 按画布从上到下映射到两条 Bit 信号。每条模式
继续支持 `0b` 及原有分隔符，并可在整除选中拍数时独立重复。预演直接显示每条最终虚线波形及
`req=01×2; ack=0011×1`。模式数量不等于信号数、空模式、超长或非整除模式立即变红并原位阻断，
逐信号错误会点名目标信号。

逐信号模式与广播模式共用同一个最终 Lane 集合、Event/Relation 影响评估和
`SetLaneSequencesCommand`。任一模式无效时零模型、零历史变化；全部目标等值时显示
`no command will run`，不清除 Redo。成功提交只产生一条历史，一次 Undo/Redo 同时恢复所有
波形、Event、Relation 和选区。核心命令用例现以两条不同序列验证该原子语义。

Codex 用户视角验证 req 与 ack 的 20–60 ns 四拍输入 `01 / 0011` 后立即分别显示 0101 与 0011，
Enter 一次完成，Ctrl+Z 一次恢复；重复输入不增加历史。输入 `01 / 0011 / 0` 时明确说明
`3 patterns for 2 selected Bit signals` 和从上到下映射规则；输入 `01 / 010` 时直接点名 ack 的
三符号模式不能整除四拍。只输入 `01` 时仍广播为两条 0101，旧路径保持兼容。

`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-bit-pattern-per-signal-safe-preview.png` 与
`build/wave-edit-smoke-bit-pattern-per-signal-count-mismatch.png`。

长期 Goal 保持 active；第 960 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 961–970 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 41.29 s、39.14 s、35.22 s；核心入口为 53/53，
百万 transition 指标为 24 ms，全部 GUI 路径使用 offscreen。Bit 模式字段现在支持
`symbol*N` 游程写法，在不增加控件或模式的前提下压缩长连续电平输入。

`0*8` 只重复紧邻的一个 Bit 符号八次，`1/X/Z` 同样适用；计数必须为正十进制且紧邻 `*`。
该语法在原解析层展开，因此不含 `/` 时继续广播，含 `/` 时继续按画布从上到下逐信号映射。
预演、拍网格、独立重复、联合 Relation 评估、等值抑制和一次 Undo 均复用既有路径。单模式
展开仍限制为 1024 符号，所选范围仍受 1024 拍和 4096 signal-beat 门禁保护。

`0*0`、`0*` 与 `0*1025` 分别报告大于零、缺少正十进制计数和超过展开上限；逐信号输入会同时
点名具体目标。所有错误在输入阶段变红并保持草稿、焦点和选区，Enter 不修改模型或历史。

Codex 用户视角验证 req 与 ack 的 20–60 ns 四拍输入
`0*2 1*2 / 1*2 0*2` 后立即分别显示 0011 与 1100 的虚线最终结果，Enter 一次完成，Ctrl+Z
一次恢复；重复输入不增加历史。只输入 `0*2 1*2` 时两条信号仍按原广播语义得到 0011。

`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-bit-pattern-run-length-safe-preview.png` 与
`build/wave-edit-smoke-bit-pattern-run-length-invalid-count.png`。

长期 Goal 保持 active；第 970 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 971–980 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 39.46 s、37.07 s、35.48 s；核心入口为 53/53，
百万 transition 指标为 20 ms，全部 GUI 路径使用 offscreen。Bus/Enum 既有 Beat 序列字段现在
支持 `value*N`，不新增编辑模式、面板或确认步骤。

`0x00*8 0xff*2` 与 `IDLE*4 WAIT_ACK*2` 在解析层展开为普通逐拍值，随后继续使用同一个
`BusEditProjection`、End 预测、Event/Relation 影响评估和 `SetLaneSequenceCommand`。逗号、
分号、空格、Tab、换行及单值输入保持兼容；完整合法 Enum 符号优先于游程解释。展开总量继续
限制为 1024 值。

零计数、缺失计数和超限计数分别给出大于零、正十进制示例和 1024 值上限；错误状态只显示
`Sequence · invalid`，不再把原始 token 数展示为展开结果。非法 Enter 保持字段全选、焦点和
目标且零模型/历史变化；等值游程显示 `no command will run`，不清除已有 Redo。

Codex 用户视角验证 data[7:0] 从 110 ns 输入 `0x12*2 0x34*2` 后立即显示四拍
0x12、0x12、0x34、0x34，一次 Apply/Undo/Redo 精确闭环；`0x35*3` 等值提交不增加历史。
state 输入 `IDLE*2 WAIT_ACK DONE*2` 后显示 IDLE、IDLE、WAIT_ACK、DONE、DONE，一次 Apply
和一次 Undo 完成。输入字段扩至 160–240 px 后完整表达式可见，原 `X (implicit)` 语义及
960 px 浮层包含性保持。

`wave-wave-edit-smoke` 保存并复核
`build/wave-edit-smoke-bus-sequence-run-length-safe-preview.png`、
`build/wave-edit-smoke-bus-sequence-run-length-invalid-count.png` 与
`build/wave-edit-smoke-enum-sequence-run-length-safe-preview.png`。

长期 Goal 保持 active；第 980 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 981–990 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 40.36 s、36.76 s、35.71 s；核心入口为 53/53，
百万 transition 指标为 21 ms，全部 GUI 路径使用 offscreen。Bus/Enum 显式范围字段现在直接
接受整段单值、共享逐拍序列和用 `/` 分隔的逐信号序列，不新增编辑模式、面板或确认步骤。

无列表、`*N` 或 `/` 语义的输入继续整段赋值；序列要求选区由完整拍组成，短序列仅在能整除拍数时
重复，多信号要求共享同一拍网格。每条目标 Lane 独立使用自己的进制、位宽和 Enum 映射校验，
完整合法 Enum 符号仍优先于游程解释。输入时直接显示所有目标的最终虚线波形、变化数、范围和
Relation 风险；非整除、映射数量错误、值非法、超限或拍网格不一致均在提交前原位阻断。

提交复用 `SetLaneSequencesCommand`，所有目标先预检再由一个命令原子写入；一次 Undo/Redo 同时
恢复全部波形、Event、Relation 和选区。等值序列显示 `no command will run`，不增加历史或清除
已有 Redo。非法 Enter 保持草稿全文选中和原选区，第一次 Esc 只丢弃草稿，第二次才关闭范围。

Codex 用户视角验证 data[7:0] 的 20–60 ns 四拍输入 `0x12 0x34` 后立即显示
0x12、0x34、0x12、0x34，一次 Enter/Undo/Redo 精确闭环；输入三个值时明确说明不能整除四拍，
模型和历史不变。state 与 state_next 的五拍输入
`IDLE DONE*2 IDLE*2 / DONE IDLE*2 DONE*2` 后分别预演两组不同结果，一次 Enter 完成两行，
一次 Undo 同时恢复。

`wave-wave-edit-smoke` 与 `wave-wave-edit-autoscroll-smoke` 保存并复核
`build/wave-edit-smoke-bus-range-sequence-safe-preview.png`、
`build/wave-edit-smoke-bus-range-sequence-invalid-length.png` 与
`build/wave-edit-autoscroll-smoke-enum-range-sequence-mapped-preview.png`。首次回归捕获并修复
统一加宽字段导致的最小窗口工具栏裁切；最终 Bus/Enum 字段保持 105 px 最小宽度，在空间充足时
扩展到 200 px。

长期 Goal 保持 active；第 990 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 991–1000 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 39.54 s、37.27 s、36.56 s；核心入口为 53/53，
百万 transition 指标为 20 ms，全部 GUI 路径使用 offscreen。Bus/Enum 显式范围字段末尾现在
提供现有拍值载入动作，使用户不再为修改一两拍而重新抄写整个序列。

载入只接受完整、显式且拍内恒定的值，并要求多信号共享同一拍网格；隐式 X、预设扩展元数据、
拍内变化、非法存量值、`/` 语法冲突及实时规模超限均在写入字段前禁用并解释原因。16 拍以内按拍
展开，较长序列将三个及以上连续同值压缩为 `value*N`；结果再通过既有解析器往返校验，不能精确
还原则不提供载入。多信号文本继续按画布从上到下使用 `/` 分组。

载入状态使用虚线字段样式且保持 `modified=false`，不修改 Scenario、Saved、autosave、历史或
选择；全文反向选择使第一条信号和第一拍优先可见。用户改动任一 token 后自动进入既有逐拍投影、
Relation 预检和 `SetLaneSequencesCommand` 提交路径。第一次 Esc 仅隐藏载入文本并保留范围，
第二次才关闭范围；Undo 后载入入口会按恢复后的真实值重新计算。

Codex 用户视角验证 data[7:0] 的 20–60 ns 四拍一次载入为
`0x00 0x00 0x00 0x00`，只把第二项改为 `0x2a` 即看到对应一拍虚线结果，Enter 一次生效，
Ctrl+Z 一次完整恢复。state 与 state_next 的 50–100 ns 五拍一次载入为两组从上到下的
WAIT_ACK/IDLE 序列，各修改中间一拍后一次提交和一次 Undo 同时作用于两条信号。隐式 X Bus
范围继续保持空字段，载入入口禁用并明确说明不会把隐式值物化为显式 Segment。

`wave-wave-edit-smoke` 与 `wave-wave-edit-autoscroll-smoke` 保存并复核
`build/wave-edit-smoke-bus-range-current-values-loaded.png`、
`build/wave-edit-smoke-bus-range-current-values-one-beat-edit-preview.png` 与
`build/wave-edit-autoscroll-smoke-enum-range-current-values-loaded.png`。定向回归首次捕获并修复
程序清空字段后残留 modified 状态，确保 Esc 和 Undo 后载入入口稳定恢复；三张图确认选区、
刻度、当前值、编辑文本、单拍预演和状态反馈同时可见。

长期 Goal 保持 active；第 1000 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 1001–1010 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 40.46 s、37.98 s、36.24 s；核心入口为 53/53，
百万 transition 指标为 20 ms，全部 GUI 路径使用 offscreen。载入的 Bus/Enum 当前值文本现在
与画布信号和拍范围建立临时、只读的 token 映射。

短序列中的每个 token 直接映射一拍；长序列中由载入器生成的 `value*N` token 映射完整连续游程。
多信号 `/` 文本保留画布从上到下的 Lane 身份。映射与文本在同一序列化步骤生成，记录字符范围、
Lane ID、物理起止、首拍、拍数和总拍数，因此不依赖二次猜测或显示值反查。

用户把光标移入或选中 token 后，画布自动露出目标 Lane 和起始时间，以青色虚线框强调准确区域；
状态栏显示信号名、`beat N of M` 或 `beats N–K of M`、物理范围和值。该动作只改变滚动位置和
临时覆盖层，不改变编辑光标、显式范围、模型、Saved、autosave 或历史。全文仍被选中时不假定
具体 token；开始编辑、Esc 隐藏、关闭范围、切换文档或失去安全载入状态后立即清除映射和覆盖层。

Codex 用户视角验证 data[7:0] 的四拍载入后选中第二个 `0x00`，画布只强调 30–40 ns，
状态显示 `beat 2 of 4`。双 Enum 文本选中第二条信号的第三个 `IDLE` 时，只强调 state_next 的
70–80 ns。全时间轴载入被压缩为
`IDLE*5 WAIT_ACK*5 DONE*90 / IDLE*100`；选中 `WAIT_ACK*5` 后完整强调 state 的 50–100 ns，
并显示 `beats 6–10 of 100`。Esc 隐藏文本后所有 token 定位状态消失而原范围保留。

`wave-wave-edit-smoke` 与 `wave-wave-edit-autoscroll-smoke` 保存并复核
`build/wave-edit-smoke-bus-range-current-values-token-target.png`、
`build/wave-edit-autoscroll-smoke-enum-range-current-values-token-target.png` 与
`build/wave-edit-autoscroll-smoke-enum-range-current-values-compressed-run-target.png`。
三张图确认局部虚线框、整体透明选区、原波形、刻度、字段 token 选择和状态反馈同时可辨识。

长期 Goal 保持 active；第 1010 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 1011–1020 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 40.16 s、38.75 s、37.32 s；核心入口为 53/53，
百万 transition 指标为 33 ms，全部 GUI 路径使用 offscreen。Bus/Enum 序列 token 与画布拍位的
联动现在会在有效草稿的整个编辑过程中持续存在，不再在第一次输入后清空。

解析器在规范化值的同时记录 token 原字段字符范围、展开偏移和展开数量；范围评估再使用真实拍
数组生成目标范围。短模式重复时，一个 token 可对应多个不连续拍；共享序列可对应多个 Lane；
`value*N` 对应其完整连续展开范围。逐信号 `/` 序列继续保留画布从上到下的 Lane 身份。该映射
只在值、拍网格、整除关系和规模门禁全部通过后发布，非法草稿不会留下陈旧目标。

光标或单 token 选择变化会绘制全部目标的青色局部虚线框；状态栏显示目标出现次数、信号数及
紧凑拍号。选择跨越 token 时不作猜测，定位立即消失。定位只改变必要滚动和临时覆盖层，不改变
显式范围、编辑时间、Scenario、Saved、autosave 或历史。最终波形预演、Relation 预检、等值
抑制和 `SetLaneSequencesCommand` 单步 Undo/Redo 路径保持不变。

Codex 用户视角验证 data[7:0] 的 20–60 ns 四拍输入 `0x12 0x34` 后，第一项同时定位第 1、3 拍，
第二项同时定位第 2、4 拍。双 Enum 文本修改后，可从 state 的第三拍连续切换到 state_next 的
第三拍；跨两项选择不会错误定位。共享 Enum 序列中的同一 token 同时框出两条信号。完整 1 us
文本中的 `DONE*5` 准确框出 state 的 50–100 ns，并显示 `beats 6–10 of 100`；Esc 后局部目标
消失而模型与原范围保持不变。

`wave-wave-edit-smoke` 与 `wave-wave-edit-autoscroll-smoke` 保存并复核
`build/wave-edit-smoke-bus-range-sequence-edited-token-target.png`、
`build/wave-edit-autoscroll-smoke-enum-range-edited-values-next-token-target.png` 与
`build/wave-edit-autoscroll-smoke-enum-range-edited-compressed-run-target.png`。
三张图确认多目标局部框、最终虚线波形、透明整体选区、原波形、刻度、字段选择和状态反馈同时可辨识。

长期 Goal 保持 active；第 1020 轮为已验收的阶段可用节点。
本轮未提交、未推送，
迭代过程中未打包。

第 1021–1030 轮开发视角默认、Qt Creator Debug、Qt Creator Release 三套构建均通过，三套各
73/73 CTest 全部通过，执行耗时分别为 40.90 s、40.63 s、39.02 s；核心入口为 53/53，
百万 transition 指标为 23 ms，全部 GUI 路径使用 offscreen。安全载入文本或有效 Bus/Enum
序列草稿中的 token 现在可以从波形预演拍反向定位。

本轮审计发现：用户在有效序列草稿仍打开时单击画布，原路径会先执行焦点离开提交，再处理画布
点击，原本只想确认某一拍的动作可能意外应用整段草稿。现在，普通左键命中映射拍时会优先选择
字段中的对应 token，不提交、不改变 Scenario、Saved、autosave、历史、编辑光标或显式范围。
Ctrl、Shift、Alt 及范围外边界仍保留既有语义。

映射拍悬浮显示手形指针和 token/目标数量提示。点击重复序列中的某一拍会选择共享 token，同时
把被点击的 Lane 和拍旋转为首目标，因此字段选择与用户当前所指位置一致；其他重复目标仍以青色
局部框保留。共享序列从任一目标 Lane 点击都会选择同一 token，并优先显示被点击 Lane；逐信号
`/` 序列只选择对应分组。释放鼠标后字段恢复键盘编辑目标，用户可以直接键入替换。

Codex 用户视角验证 data[7:0] 已载入的 20–60 ns 四拍中，单击第三拍只选择第三个 `0x00`；
修改草稿 `0x12 0x34` 后，单击第三拍选择 `0x12` 且首目标为 40–50 ns，单击第四拍选择
`0x34` 且首目标为 50–60 ns，两个重复目标均继续可见。逐信号 Enum 文本可从 state_next
的 75 ns 拍反向选择第二组 `DONE`；共享 Enum 文本从 state_next 的 85 ns 拍选择末尾
`DONE`，状态明确显示同一 token 影响两条信号上的两个目标。上述操作均未提前提交草稿。

`wave-wave-edit-smoke` 与 `wave-wave-edit-autoscroll-smoke` 保存并复核
`build/wave-edit-smoke-bus-range-current-values-waveform-token-pick.png`、
`build/wave-edit-smoke-bus-range-sequence-waveform-token-pick.png`、
`build/wave-edit-autoscroll-smoke-enum-range-waveform-token-pick.png` 与
`build/wave-edit-autoscroll-smoke-enum-range-shared-waveform-token-pick.png`。
四张图确认字段 token 选择、被点击目标优先、多目标局部框、原波形、刻度、透明范围和状态反馈
同时可辨识。

第 1030 轮为已验收的阶段可用节点。按用户要求，长期 Goal 在本轮完成后结束。
本轮未提交、未推送，
迭代过程中未打包。

第 1031–1040 轮为用户在长期 Goal 结束后另行追加的十轮迭代。开发视角默认、Qt Creator Debug、
Qt Creator Release 三套构建均通过，三套各 73/73 CTest 全部通过，offscreen 串行执行耗时分别为
44.78 s、41.69 s、38.83 s；核心入口为 53/53，1000 lanes / 1,000,000 transitions /
100,000 queries 指标为 23 ms。最终定向 `wave-wave-edit-smoke` 为 1/1，4.28 s。

本轮把活动 QLineEdit 的文本历史与波形 CommandStack 明确分流：字段可见、启用、非只读且存在
对应文本历史时，Edit 菜单及 Ctrl+Z/Ctrl+Y 只撤销或重做草稿；否则继续操作波形命令。载入当前
Bus/Enum 拍值时同时记录 Scenario、Lane 顺序、显式范围、命令状态、原文本和 token spans。
文本 Undo 回到原文或第一次 Esc 时恢复该安全基线；若模型 Undo/Redo 已改变命令状态，即使字段
仍聚焦，也会立即清除旧载入文本、映射、锚点和本地文本历史，避免把旧值显示为当前值。

从波形点击获得的 Lane/拍锚点现在可跨真实逐字符输入和短暂非法中间态持续存在。主目标使用亮色
实线框，受同一 token 影响的其他重复拍或共享 Lane 使用弱化虚线框。Tab/Shift+Tab 按 Lane 与
时间顺序逐个定位目标，首尾不循环，`value*N` 仍为原子 token；Ctrl+Shift+Tab 保留原有语义。
token 点击延迟到未超过标准拖动阈值的释放时生效。未修改载入态仍可直接拖动显式范围；修改态
拖动会原位阻断、保留草稿/锚点/焦点，释放后不残留 Forbidden cursor。

Codex 用户视角验证：在 data[7:0] 四拍范围中将第 3 拍对应 token 逐字符替换后，主目标仍保持
40–50 ns，第 1 拍作为关联目标可见；真实 Ctrl+Z/Ctrl+Y 只恢复或重做文本。Tab 可到第 4 拍，
Shift+Tab 可逐项返回且首尾停止。载入当前值后，文本 Undo/Esc 可恢复原载入值；在字段聚焦时
执行模型 Undo，旧载入值和映射立即消失，Redo 不会复活它们。修改态拖动被拒绝后，字段焦点、
目标锚点和草稿均保持。

离屏证据包括 `build/wave-edit-smoke-bus-range-token-replace-anchor.png`、
`build/wave-edit-smoke-bus-range-token-keyboard-next.png`、
`build/wave-edit-smoke-bus-range-current-values-text-undo-restored.png` 与
`build/wave-edit-autoscroll-smoke-enum-range-shared-token-replace-anchor.png`。

第 1040 轮为已验收的阶段可用节点。长期 Goal 仍保持第 1030 轮的 `complete` 状态；本次仅完成
用户后续明确追加的十轮迭代。本轮未提交、未推送，迭代过程中未打包。

## Goal 完成后的追加可用性收敛

状态：实现与最终三配置静默验收均已完成；长期 Goal 保持 `complete`，未重新开启。

追加维护范围收敛为：

- 桌面 Timing 明确区分关联时钟的 Sync、未关联时钟的 Grid 和 1 tick 的 Async；指针采样与
  操作结果使用不同状态区域。
- Tab 焦点链与活动文本 Undo/Redo 边界收紧；Timeline End 有效草稿提交后继续同一次画布操作。
- Bus 单击收敛为一拍选择，Reserved 同时进入 Beat/Range 预设；范围当前值使用虚拟基线载入
  隐式 X 和有扩展语义的存量内容，未修改拍不实体化或重写。
- Clock 轻量参数补齐 phase、duty、active edge 和 reset/disable；Bus 快速新增与完整属性统一
  1–65536 bit。
- Relation 常规连线默认隐藏且可从 Edit 菜单显示，但编辑风险连线不随之隐藏；工具栏收敛为
  `Measure / Markers`。
- GUI 正式保存增加外部文件 SHA-256 冲突保护，提供 Reload、Save As、Overwrite 和 Cancel。
- `wave-cli` 增加 generate/compare/bridge 文本兼容适配；自动化 v1 公开 Draft 2020-12 的
  capabilities/report/operation-batch Schema 和 33 类 operation 安全元数据；CMake Portable install
  收集五个可执行程序、Schema、CLI 文档、最小 operations 示例和 handshake 工程。
- 最终收敛修复 Timeline End 失焦后的焦点和同次点击时间漂移；Clock duty 大分数完整往返；Reserved
  可见标签与超宽 Bus 载入预算；单信号/批量序列一致的 `preserveExisting`、End 与 Event 同步语义。

本轮不声称已交付以下项目：

- 多 Waveform/Scenario 的新建、复制、删除和重排。现有历史命令持有 `Scenario*`，修改
  `Project::scenarios` 容器前必须先建立稳定身份绑定。
- 浅色背景/主题切换。
- 长信号列表的常驻/粘性添加入口。

最终验收记录：

```text
Default build / offscreen CTest: 76/76 passed, 41.41 sec
Qt Creator Debug build / offscreen CTest: 76/76 passed, 38.61 sec
Qt Creator Release build / offscreen CTest: 76/76 passed, 37.16 sec
Focused desktop and CLI contract tests: 6/6 passed
Desktop interaction: none
Packaging: not run
Commit/push: not run
```

本次维护已完成三套全量构建与 offscreen CTest；未运行可见 GUI，未打包、未提交、未推送。
