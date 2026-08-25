# WaveWorkbench UI and Reusable Waveform View

状态：进行中

日期：2026-08-25

## 组件边界

- 将波形输入模型、timeline mapping、lane layout、绘制、命中测试和交互状态从现有画布拆分为
  可复用模块；独立应用、完整 Simulation Workspace 和轻量 Waveform View 共享实现。
- 保持当前完整 workspace v1 C ABI 和既有 capability。新增能力通过显式探测使用，不改变旧宿主。
- 新增严格 `wave-preview/v1` 数据模型：timebase、lane、segment、unknown/provenance、稳定信号身份、
  便携源码链接和 generation。首版使用 generation-tagged 全量替换与大小/范围门禁。
- Waveform View 不解析 RTL、不启动工具链、不推导符号值。ZeroSlack 是这些事实的唯一所有者。
- Symbolic Preview 与 Simulated Result 共用渲染，但页面标题、状态 badge、lane provenance 和空状态
  始终区分两种模式。

## 视觉与响应式壳

- 定义 light/dark semantic token：应用/画布/面板/raised surface、grid major/minor、lane header、
  bit/bus/clock/unknown、expected/actual/diff、cursor/range/selection、focus、status 和 diagnostic。
- 移除 `TraceCanvas` 等路径中的硬编码浅色颜色。颜色与 DPI 改变后组件无需重建领域模型。
- 主窗口收敛为一行主操作区：Run、Run all、Stop、Compare、Checks；低频 Scenario、Stub、Clock 和
  Source 操作进入分组或 overflow，不重复占据视觉主层级。
- Stimulus/Expected、Actual 和 Review 使用清晰 page header、可调整 splitter、最小尺寸与折叠状态。
- Lane 标题使用稳定对齐、类型/宽度 badge、活动行和键盘焦点；bus 值在可用宽度不足时采用可预测省略。
- Empty、Loading、Stale、Failed、Saved/Dirty 状态在对应区域呈现，状态栏仅保留耐久状态。

## 交互兼容

- 保留现有 waveform 编辑、范围、marker、relation、采样、搜索、缩放、Fit、Undo/Redo、compare、
  checks、batch、trace hierarchy 和 source navigation 行为。
- 可复用组件的 selection/cursor/viewport 状态独立于输入 payload；兼容 generation 更新保留状态，
  不兼容更新按明确规则重置。
- 嵌入组件销毁、宿主关闭、快速 payload 替换和后台 trace 完成不得访问已删除对象或发布旧结果。

## 接口与验收

- 公共 C 入口返回 Qt widget，并提供 capability、set/replace preview payload、theme、selection/source
  navigation 和错误查询所需的稳定 meta-object contract。
- ABI 测试覆盖旧 workspace factory 与新 view factory；畸形、超限、过期和未知版本 payload 明确拒绝。
- UI 自动化覆盖 light/dark、960x720、1440x900、100/125/150/200% scaling、键盘焦点和 splitter。
- 现有全部配置测试及真实共享库的 ZeroSlack 跨仓契约测试通过。
- 形成可复现截图证据，独立提交并推送；不打包。
