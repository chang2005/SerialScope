<p align="center">
  <img src="resources/icons/app-128.png" width="104" alt="SerialScope icon">
</p>

<h1 align="center">SerialScope</h1>

<p align="center">
  跨平台串口调试与数据可视化工具 · <strong>V1.0</strong>
</p>

<p align="center">
  <a href="https://github.com/chang2005/SerialScope">GitHub 项目主页</a> ·
  <a href="https://github.com/chang2005/SerialScope/releases">下载 Releases</a> ·
  <a href="LICENSE">GPL-3.0 许可证</a>
</p>

SerialScope 是一个基于 **Qt 6 / C++17** 的桌面串口调试工具，面向 Windows 优先开发，
同时保持 Linux / macOS 可编译。它提供完整的串口参数配置、ASCII/HEX 双模式收发、
异步数据日志、CRC 校验、定时/文件发送，以及可解析 ASCII 数值或二进制帧的多通道波形图。

## 功能

### 串口配置

- 自动枚举本机串口，也可手动输入端口名：`COM3`、`/dev/ttyUSB0`、`tty.usbserial-*` 等。
- 已保存但当前未插入的端口会保留为“COMx（未检测到）”，热插拔刷新不会自动切换到其他设备。
- 同名设备会在下拉项和提示中附带描述、序列号或系统位置，便于区分多个同型号适配器。
- 端口提示里显示 **VID:PID**（设备确实提供时才显示，避免出现无意义的 `0000`）。
- 常用波特率：300、600、1200、2400、4800、9600、14400、19200、28800、38400、
  57600、76800、115200、128000、230400、256000、460800、500000、576000、921600、
  1000000、1152000、1500000、2000000、2500000、3000000、3500000、4000000。
- 支持自定义波特率（50–20,000,000）。
- 数据位 5 / 6 / 7 / 8；校验位 无 / 奇 / 偶 / 标记 / 空格；停止位 1 / 1.5 / 2。
- 流控：无 / 硬件 RTS-CTS / 软件 XON-XOFF。
- **DTR / RTS 控制线**可手动置位与复位，常用于复位设备或切换收发方向。
- **自动重连**：串口意外断开或设备复位后自动按相同参数重新打开（间隔 2 秒）。
- 打开串口期间锁定参数栏，避免驱动握手时显示的参数与实际连接参数不一致。
- 参数栏在窗口变窄时自动换行，不会把控件挤扁。
- 串口参数、窗口布局、主题和发送历史自动保存。

### 接收与发送

- 接收区支持 **ASCII / HEX** 双视图，编码可选 UTF-8 / GBK / ASCII。
- 时间戳、自动滚动、暂停、清空、保存显示内容、发送回显。
- **过滤**：按关键词（纯文本，大小写不敏感）或正则表达式筛选，命中片段高亮显示。
- **控制字符可视化**：把 `\r` `\n` `\t` 显示为 `<CR>` `<LF>` `<TAB>`，便于诊断换行符问题。
- 发送区支持文本和 HEX；HEX 输入容错空格、逗号和 `0x` 前缀，非法输入会阻止发送。
- **ASCII / HEX 一键互转**。
- **快捷发送**：把常用指令存成快捷条目，点击即发。
- 行尾可选 无 / CRLF / LF / CR。
- 附加校验：CRC16-Modbus / CRC16-CCITT / Sum8 / Xor8 / LRC8。
- 定时发送、最近 50 条发送历史、分块文件发送与取消。
- **自动回复规则**：收到匹配数据后自动回复预设内容，用于模拟从机设备。
- **轮发序列**：可编辑多组数据，按组顺序、组内顺序轮流发送，全部发完自动回到第一组循环。
  - 两种触发方式：**定时**（按组间隔逐条发送）与 **应答后**（发出一条后等设备应答，收到数据才发下一条）。
  - 序列配置随窗口状态一起保存，下次启动仍在。
- 高波特率下按 30 ms 批量上报，避免逐字节刷新拖慢界面。

### 数据日志

- 独立线程异步写入，不阻塞串口读取。
- 每行包含时间戳、方向（RX/TX）和 HEX 数据。
- 有界队列防止磁盘变慢拖垮程序，溢出时在日志中插入缺口标记并在状态栏告警。
- 支持手动记录和“打开串口后自动记录”。
- 默认日志目录：`文档/SerialScopeLogs`，可在保存对话框中更改。

### 波形图

两种数据源，可在波形面板中切换：

1. **ASCII 行数值**
   - 按 `\n` / `\r` 切分数据行。
   - 自动提取每行中的全部十进制数字，依次映射到 CH1…CH8。
   - 支持负数、小数和科学计数法；同一行的多个值共享同一时间戳。

2. **HEX 二进制帧字段**
   - 自定义帧头、固定帧长或长度字节模式、长度偏移与补偿。
   - 校验算法可选：CRC16-Modbus / CRC16-CCITT / Sum8 / Xor8 / LRC8，校验失败自动重同步。
   - 支持固定**帧尾**（位于校验字段之后），逐字节匹配。
   - 帧内结构约定为 `[帧头][长度/数据][校验][帧尾]`。
   - 字段表配置通道名称、字节偏移和类型：int8 / uint8 / int16 / uint16 /
     int32 / uint32 / float32。
   - 支持大端 / 小端字节序。

波形显示支持 5 / 10 / 30 / 60 秒滚动窗口、自动量程、通道显隐、暂停、清空和 CSV 导出。
绘制使用按像素列的 min/max 包络降采样，长时间高采样率采集仍可保持流畅。

### 工具与界面

- CRC 工具箱：CRC8、CRC16-Modbus、CRC16-CCITT、CRC32、Sum8、Xor8。
- **编码转换 / 乱码修复**：在 UTF-8 / GBK / ASCII / Latin-1 之间互转，修复编码不匹配造成的乱码。
  - 文本模式：粘贴乱码文本，选对「原始编码」与「目标编码」即可还原。
  - HEX 模式：粘贴原始字节，工具逐个编码试解并按可读性评分排序，点一下即采纳。
  - 判据用的是「字节是否为合法 UTF-8 序列」——UTF-8 自描述，GBK 的中文字节对一定不是合法 UTF-8，
    比单看解码后字符是否乱码可靠得多（用错编码解出来的怪字符往往仍是合法字符）。
- 深色 / 浅色主题。
- 连接状态、TX/RX 总字节数和实时速率统计。
- 关于对话框显示版本、GitHub 项目链接和 GPLv3 许可证。

## 下载与运行

从 [GitHub Releases](https://github.com/chang2005/SerialScope/releases) 下载：

```text
SerialScope-1.0.0-win64.zip
```

解压后进入 `SerialScope-1.0.0-win64` 目录，直接运行 `SerialScope.exe`，无需安装，也不需要
管理员权限。发布包已包含 Qt 运行库和平台插件，可整体复制到其他 Windows 10/11 x64 机器使用。
校验文件 `SHA256SUMS.txt` 随 Release 一并提供，可用
`certutil -hashfile SerialScope-1.0.0-win64.zip SHA256` 核对。

> 当前构建使用 Qt 6.8.3 + MinGW 13.1，已在 Windows 11 上验证。Linux 和 macOS
> 需要从源码构建。

## 快速上手

1. 在顶部“串口”下拉框选择设备，或直接输入端口名。
2. 选择波特率、数据位、校验位、停止位和流控。
3. 点击“打开串口”。
4. 在接收区切换 ASCII / HEX；在发送区输入内容并发送。
5. 在波形面板选择数据源：
   - ASCII 数据可直接发送 `1.0,2.0,3.0\n` 这类文本；
   - 二进制协议点击“协议配置…”填写帧格式和字段表。
6. 需要留存数据时，使用“文件 → 记录日志”。
7. 需要只关注特定内容时，在接收区“过滤”框输入关键词（勾选 `.*` 表示按正则匹配）。
8. 需要模拟从机应答时，使用“工具 → 自动回复规则…”配置匹配与回复内容。
9. 需要按固定顺序发一串命令时，发送区下方“轮发 → 编辑序列…”配置多组数据，
   再选“定时”或“应答后”触发方式并点“开始轮发”。

## 构建

### 依赖

- Qt 6.5+（开发验证版本：Qt 6.8.3 LTS，MinGW 13.1 64-bit）
- CMake 3.21+、Ninja（或 Visual Studio 生成器）
- Qt 模块：Widgets、SerialPort、Core5Compat、Test（测试需要）

### Windows（MinGW）

```bat
git clone https://github.com/chang2005/SerialScope.git
cd SerialScope

cmake -S . -B build -G Ninja ^
  -DCMAKE_PREFIX_PATH=D:\Qt\6.8.3\mingw_64 ^
  -DCMAKE_CXX_COMPILER=D:\Qt\Tools\mingw1310_64\bin\g++.exe ^
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

运行：

```bat
build\SerialScope.exe
```

### Linux / macOS

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/SerialScope
```

Linux 需要系统提供 Qt 6 SerialPort 模块；macOS 可使用 `macdeployqt` 打包应用包。
Windows 的 EXE 图标与版本资源由 `resources/SerialScope.rc.in` 生成。CMake 会自动从
C++ 编译器目录查找 `windres`；如果使用其他 MinGW 布局，可把其 `bin` 目录加入 PATH，
或传入 `-DCMAKE_RC_COMPILER=<路径>/windres.exe`。

### 中文路径

Qt 的 `moc` 在部分 Windows/MinGW 组合下无法处理含中文的工程路径，可能报
`moc: Cannot create ... Invalid argument`。建议将仓库克隆到纯 ASCII 路径，或建立
目录联接后从联接路径构建：

```bat
mklink /J D:\SerialScope "D:\projects\SerialScope"
cmake -S D:\SerialScope -B D:\SerialScope\build ...
```

## 测试

```bash
ctest --test-dir build --output-on-failure
```

三组测试：

- `test_core`：CRC 已知向量、HEX 解析/格式化、UTF-8/GBK 增量解码、ASCII 数值解析、
  帧状态机、字段提取、文本过滤、控制字符转义、快捷发送清单、自动回复规则。
- `test_serial_config`：常用波特率、数据位/校验位/停止位选项、手动端口输入、刷新时
  保留选择、清空端口、打开中状态和设置持久化。
- `test_flowlayout`：流式布局的排布、窄宽度换行与 `heightForWidth` 计算。

UI 测试使用 Qt `offscreen` 平台插件，不需要显示器。Windows 构建会自动复制测试所需的
Qt 运行库与平台插件，避免 PATH 中其他软件的旧版 Qt 抢先加载。

## 打包 Windows 发布包

```bat
cmake --build build --target deploy
cmake --build build --target package_release
```

- `deploy`：复制程序、运行 `windeployqt`，并附带 README、LICENSE 和第三方声明。
- `package_release`：生成 `build\release\SerialScope-1.0.0-win64.zip`。

也可以直接执行：

```bat
scripts\deploy_windows.bat
```

发布前建议在干净机器或虚拟机中解压 ZIP 并运行一次，确认串口驱动和 Qt 插件正常。

## 项目结构

```text
src/
  main.cpp              程序入口、应用元信息与图标
  core/                 串口线程、异步日志、HEX/数值/帧解析（无 UI 依赖）
  utils/                CRC、QSettings 封装
  ui/                   主窗口、配置栏、收发面板、波形与自绘曲线、主题
  ui/dialogs/           CRC 工具箱、帧协议配置、关于对话框
resources/
  app.qrc               应用图标、GitHub 标志、内置许可证资源
  icons/                多尺寸 PNG、Windows ICO、GitHub 标志
  SerialScope.rc.in     Windows EXE 图标与版本资源模板
tests/                  核心库与串口配置 UI 测试
scripts/                Windows 构建/部署脚本
LICENSES/               LGPLv3 与 Octicons MIT 许可证文本
```

## 版本与发布

当前版本：**V1.0**（关于对话框显示 V1.0；Windows 文件版本与发布包使用 `1.0.0`）。

发布流程：

```bash
git tag -a v1.0.0 -m "SerialScope v1.0.0"
git push origin main --tags
```

然后把 `build/release/SerialScope-1.0.0-win64.zip` 上传到对应 GitHub Release。

## 已知限制

- 1.5 位停止位仅 Windows 支持，且要求 5 位数据位；不满足时程序会在打开前提示。
- 标记 / 空格校验依赖驱动：Linux 需要 CMSPAR 支持，macOS 不支持。
- 当前为单串口窗口，不支持多串口标签页。
- 未在真实串口硬件上完成全部波特率/流控组合的压力测试，请以目标设备为准。

## 许可证

Copyright © 2026 SerialScope Contributors。

本项目采用 **GNU General Public License v3.0 only（GPL-3.0-only）** 发布。完整文本见
[LICENSE](LICENSE)。你可以自由使用、修改和分发本软件，但分发衍生作品时必须继续以
GPLv3 开源，并提供对应源代码。

第三方组件与商标说明见 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)。
Qt 6 以 LGPLv3 动态链接方式使用，许可证文本见 [LICENSES/LGPL-3.0.txt](LICENSES/LGPL-3.0.txt)。

## 贡献

欢迎通过 GitHub Issues 提交问题、建议或硬件兼容性报告。提交代码前请确保：

- `ctest --test-dir build --output-on-failure` 全部通过；
- 新增功能附带相应测试或说明；
- 不引入与 GPLv3 不兼容的依赖。
