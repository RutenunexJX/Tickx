# `wave-preview/v1` 嵌入波形契约

`wavewidgets` 保持 ABI 版本 1，并新增独立的轻量视图工厂：

```cpp
wavewidgets_create_waveform_view_v1(parent, &view, error, errorCapacity);
wavewidgets_set_waveform_preview_v1(view, json, byteCount, error, errorCapacity);
```

旧的 `wavewidgets_create_simulation_workspace_v1` 及
`wave-workbench.simulation-workspace/v1` 不变。轻量视图发布
`wave-workbench.waveform-view/v1`，其 `wavewidgets.capabilities` 至少包含：

- `wave-preview/v1`
- `generation-replace/v1`
- `waveform-theme/v1`
- `compact-density/v1`
- `source-navigation/v1`

## 所有权与模式

调用方拥有 RTL 解析、符号推导和仿真事实。视图只校验、保存一份当前 payload 并绘制，不读取
RTL、不启动工具链，也不把 Symbolic Preview 标记为 Simulated Result。`mode` 只能是
`symbolic` 或 `simulated`；页面 badge、provenance 和无障碍摘要持续显示该区别。

## 最小 payload

```json
{
  "contract": "wave-preview/v1",
  "generation": 12,
  "mode": "symbolic",
  "timebase": { "unit": "ns", "start": 0, "end": 100 },
  "lanes": [
    {
      "id": "top.data",
      "name": "data[7:0]",
      "kind": "bus",
      "width": 8,
      "provenance": "zeroslack-symbolic",
      "source": {
        "file": "rtl/top.sv",
        "line": 12,
        "column": 3,
        "semanticId": "module:top/signal:data"
      },
      "segments": [
        { "start": 0, "end": 40, "value": "0x00" },
        { "start": 40, "end": 100, "value": "X", "unknown": true }
      ]
    }
  ]
}
```

正式 schema 位于 `schemas/wave-preview/v1/payload.schema.json`。运行时还执行跨字段约束：

- `timebase.end > timebase.start`；
- lane 稳定 ID 唯一，bit/clock 的 width 必须为 1；
- segment 位于 timebase 内，`end > start`，按时间排序且不重叠；
- source file 必须为 workspace-relative 路径；
- 单 payload 最多 8 MiB、512 lanes、200,000 segments；
- JSON integer 限于 IEEE-754 可精确表达范围。

## 更新与交互

首版只接受 generation-tagged 全量替换。已接受 generation 后，小于或等于它的更新会被拒绝，
当前画面不变，错误通过返回码、`lastError` 属性及 `lastErrorChanged` 信号公开。同 timebase 的兼容
更新保留当前 stable lane selection、cursor 和 viewport；身份或范围不兼容时执行可预测重置。

视图支持 system/light/dark 主题、full/compact 密度、Fit、缩放、滚动、lane/cursor 键盘选择。
选中带 source 的 lane 后按 Enter 或双击，视图发出 `sourceNavigationRequested`，但不自行打开文件。
宿主关闭或删除视图即可取消其全部状态，不存在后台任务或跨宿主所有权。


## View lifecycle and presentation

The standalone app, full Simulation Workspace and lightweight Waveform View share rendering components.
Preview never parses RTL or launches tools. Symbolic Preview and Simulated Result retain distinct provenance.
Compatible generation updates preserve stable lane selection, cursor and viewport; malformed, oversized,
unknown-version and stale payloads do not replace valid content. Fit follows widget/splitter size until
explicit user zoom. Host destruction and late trace completions must not access released widgets.
Theme and density changes affect presentation without rebuilding the domain model.
