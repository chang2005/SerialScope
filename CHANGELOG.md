# Changelog

All notable changes to SerialScope are documented in this file.

## [Unreleased]

## [1.1.0] — 2026-11-23

### Added

- **接收区过滤**：关键词（纯文本，大小写不敏感）或正则表达式过滤，只显示命中的行，
  命中片段在文本中高亮显示。非法正则自动退回纯文本匹配，不会让过滤静默失效。
- **控制字符可视化**：把 `\r` `\n` `\t` 等不可见字符显示为 `<CR>` `<LF>` `<TAB>`，
  便于诊断换行符不匹配这类通信问题。
- **自动重连**：串口意外断开或设备复位后，按相同端口与参数自动重新打开
  （首次 1 秒后重试，之后每 2 秒一次）；开启后不再弹出打断流程的模态提示框。
- **DTR / RTS 控制线**：手动置位与复位，常用于复位设备或切换收发方向；
  当前电平显示在连接状态上。
- **快捷发送**：把当前输入内容存为快捷条目，点击即发；支持删除，条目自动保存。
- **ASCII / HEX 一键互转**：文本转 HEX，或把 HEX 反解为文本。
- **自动回复规则**：收到匹配的数据后自动回复预设内容，用于模拟从机设备。
  规则可配纯文本或正则匹配，回复内容支持 `\r` `\n` `\t` `\\` `\xNN` 转义，
  也可以直接填 HEX 字节。
- **自适应参数栏**：顶部串口参数栏改为流式布局，窗口变窄时控件自动换行，
  不会被挤成不可用的宽度。
- **VID / PID 显示**：端口提示中显示设备的 VID:PID，设备未提供时隐藏，
  避免出现误导性的 `VID: 0000`。
- **校验算法可选 + 帧尾**：帧解析的校验方式从「只有 CRC16-Modbus 开关」扩展为
  CRC16-Modbus / CRC16-CCITT / Sum8 / Xor8 / LRC8 六选一，并支持固定帧尾
  （位于校验字段之后）。帧内结构统一为 `[帧头][长度/数据][校验][帧尾]`。
  发送区的附加校验同样新增 Xor8 与 LRC8。
- **轮发序列**：多组数据按顺序轮流发送，全部发完自动回到第一组循环。
  支持「定时」（按组间隔逐条发送）与「应答后」（发出一条后等设备应答，
  收到数据才发下一条）两种触发方式；序列配置随设置一起保存。
- **编码转换 / 乱码修复**（“工具 → 编码转换 / 乱码修复…”）：在 UTF-8 / GBK /
  ASCII / Latin-1 之间互转。文本模式选对原始编码与目标编码即可还原；
  HEX 模式粘贴原始字节后逐个编码试解，按可读性评分排序并可一键采纳。

### Changed

- 接收区的过滤与控制字符开关移到独立一行，避免挤占原有工具栏。
- 抽出 `WidgetUtil::repolish()`，替换三处重复的 `repolish` 局部实现。
- 协议配置里的「校验」从复选框改为下拉框，旧配置中的 `verifyCrc16Modbus`
  布尔项在加载时自动迁移为对应的校验算法。

### Fixed

- 过滤器的非法正则不再导致过滤功能静默失效（改为退回纯文本匹配）。
- 乱码修复的候选排序改用「字节是否为合法 UTF-8 序列」作为主判据：
  用错编码解读出的怪字符往往仍是合法字符，只看字符可读性会让
  Latin-1 / ASCII 这样的无意义结果排在前面。

## [1.0.0] — 2026-11-17

First public release.

### Added

- Serial port selection with automatic enumeration, manual entry and hot-plug
  refresh that preserves the current selection.
- Common baud-rate presets from 300 to 4,000,000 plus custom baud rates;
  data bits 5–8, parity none/odd/even/mark/space, stop bits 1/1.5/2 and
  hardware/software flow control.
- ASCII and HEX receive/send modes, UTF-8/GBK/ASCII decoding, timestamps,
  pause, auto-scroll and display export.
- Timed sending, send history, file sending with progress/cancel and optional
  CRC16-Modbus, CRC16-CCITT or Sum8 checksums.
- Asynchronous, bounded-queue logging with RX/TX direction and HEX output.
- Multi-channel waveform view fed either by numeric values in ASCII lines or
  by fields extracted from binary HEX frames.
- CRC toolbox, light/dark themes, connection statistics and persistent
  settings.
- About dialog with version, GitHub project link and GPLv3 license viewer.
- Dedicated Windows executable icon and version metadata.
- Automated core and serial-configuration UI tests.

### Notes

- This release is built and tested on Windows 11 with Qt 6.8.3 and MinGW 13.1.
- The source is platform-neutral and can be built on Linux or macOS with Qt 6.
