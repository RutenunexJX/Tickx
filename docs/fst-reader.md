# FST 按需读取

Tickx 默认构建 `wave-wellen-reader` 辅助程序，并将其与独立应用和
`wavewidgets` 共享库放在同一目录。辅助程序使用 Wellen 0.25.6 读取标准 FST；项目不包含
自定义 FST 解析器。

导入分为两个阶段：先读取 scope、信号名、位宽、时间范围等元数据，不解码 transition；
随后只为当前可见信号和场景映射信号按批读取 transition。切换可见信号会继续按需加载，
取消或加载新 trace 时，旧 generation 的结果不能合并到当前数据。

默认门禁为最多 1,000,000 条信号元数据、每批 64 个信号、单次响应 256 MiB、120 秒超时。
当前只消费数字 bit-vector；real/string 等非数字变量会给出 warning 并跳过。VCD 仍是短时
模块仿真的默认输出格式。

不需要 FST 时可用 `-DWAVEWORKBENCH_ENABLE_WELLEN=OFF` 构建。该模式仍保留 FST 数据契约，
但界面和嵌入式能力列表不会宣称 FST reader 可用。

Wellen 项目：<https://github.com/ekiwi/wellen>。Wellen 及仓库中复制的配对 VCD/FST 测试
fixture 使用 BSD 3-Clause License；fixture 来源与用途记录在
`tests/fixtures/traces/README.md`。
