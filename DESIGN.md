# SerialScope — 架构设计

> 版本：V1.0（元数据 1.0.0）
>
> 技术栈：Qt 6.8 LTS (Widgets / SerialPort / Core5Compat) + C++17 + CMake。
>
> 开发环境：Windows（MinGW 13.1），代码不依赖平台特有 API，可直接在 macOS / Linux 编译。

## 1. 目标功能

| 模块 | 功能 |
| --- | --- |
| 串口配置 | 端口枚举 + 手动输入、断开的已保存端口保留、常用波特率与自定义值、数据位 5–8、五种校验位、三种停止位、流控、异步打开状态、打开/关闭 |
| 接收区 | ASCII / HEX 双视图；编码 ASCII / UTF-8 / GBK；时间戳；自动滚动；暂停；清空；保存显示内容；显示发送回显 |
| 发送区 | ASCII / HEX 双模式；HEX 输入校验；行尾（无 / CRLF / LF / CR）；附加校验（CRC16-Modbus / CRC16-CCITT / Sum8）；定时发送；发送历史；发送文件（分块） |
| 日志 | 独立线程异步落盘，时间戳 + 方向 + HEX，带丢失告警；自动记录；保存显示内容 |
| 波形图 | 自绘高性能曲线控件；ASCII 行内数字解析（多通道）；HEX 二进制帧字段解析（帧头/长度域/大小端/字段表）；滚动窗口、暂停、清空、导出 CSV、通道显隐 |
| 工具 | CRC 计算工具箱（CRC8 / CRC16-Modbus / CRC16-CCITT / CRC32 / Sum8）、统计状态栏、配置持久化、深/浅色主题 |
| 关于与许可 | 自定义关于对话框：V1.0、GitHub 标志与项目链接、GPLv3 许可证查看器 |
| 品牌 | 多尺寸应用图标、Windows EXE 图标与版本资源 |

## 2. 目录结构

```
SerialScope/
├── CMakeLists.txt
├── DESIGN.md / README.md / CHANGELOG.md
├── LICENSE                         # GPL-3.0-only
├── THIRD-PARTY-NOTICES.md
├── src/
│   ├── main.cpp                    # 应用元信息、多尺寸图标
│   ├── core/                       # 无 UI 依赖，可单测（lib serial_core）
│   │   ├── SerialConfig.h
│   │   ├── SerialWorker.h/.cpp     # QSerialPort 工作线程
│   │   ├── DataLogger.h/.cpp       # 异步日志（独立线程 + 有界队列）
│   │   └── parser/
│   │       ├── HexCodec.h/.cpp          # HEX 解析/格式化、编码转换（含 GBK）
│   │       ├── AsciiValueParser.h/.cpp  # ASCII 行数字提取
│   │       ├── DataFrameParser.h/.cpp   # 帧状态机（帧头/长度/CRC/重同步）
│   │       └── HexFieldExtractor.h/.cpp # 帧字段 → 数值通道
│   ├── utils/
│   │   ├── CrcCalculator.h/.cpp
│   │   └── SettingsManager.h/.cpp
│   └── ui/
│       ├── MainWindow.h/.cpp       # 布局、线程装配、信号路由、热插拔
│       ├── SerialConfigPanel.h/.cpp
│       ├── ReceivePanel.h/.cpp
│       ├── SendPanel.h/.cpp
│       ├── WaveformPanel.h/.cpp    # 波形数据源/解析调度、图例、导出
│       ├── PlotWidget.h/.cpp       # QPainter 自绘曲线（降采样绘制）
│       ├── Theme.h/.cpp            # 浅色/深色 QSS + 调色板
│       └── dialogs/
│           ├── AboutDialog.h/.cpp       # 版本、GitHub、许可证
│           ├── CrcToolDialog.h/.cpp
│           └── HexFieldConfigDialog.h/.cpp
├── resources/
│   ├── app.qrc                     # 图标、GitHub 标志、许可证资源
│   ├── icons/                      # 多尺寸 PNG、ICO、主题化 GitHub 标志
│   └── SerialScope.rc.in           # EXE 图标与 VERSIONINFO 模板
├── LICENSES/                       # LGPL-3.0、Octicons MIT
├── tests/
│   ├── CMakeLists.txt
│   ├── test_core.cpp               # Qt Test：CRC / HEX / ASCII 解析 / 帧状态机 / 字段提取
│   └── test_serial_config.cpp      # Qt Test：串口参数选项、端口输入/刷新/持久化
├── tools/
│   └── make_assets.cpp             # 一次性图标生成工具（不参与应用构建）
└── scripts/
    └── deploy_windows.bat          # 构建、windeployqt、Release ZIP
```

## 3. 线程模型与数据流

```
UI 线程 (MainWindow)
 ├─ SerialConfigPanel / ReceivePanel / SendPanel / WaveformPanel / PlotWidget
 ├─ hotplug QTimer(2s) → QSerialPortInfo::availablePorts()
 ├─ 日志 QThread ← DataLogger（有界队列，250ms 落盘）
 └─ 串口 QThread ← SerialWorker（QSerialPort，30ms 批量上报）
```

接收链路（一次 chunk）：

```
SerialWorker::onReadyRead
  → m_rxPending 聚合
  → 30ms flush：dataReceived(QByteArray)  [跨线程 queued]
      → ReceivePanel::appendRx（增量解码显示，可暂停）
      → WaveformPanel::feed（始终解析，暂停仅冻结绘图）
      → DataLogger::appendRx（线程安全队列）
```

发送链路：`SendPanel::dataToSend → SerialWorker::sendData`（queued，线程安全）。

### 关键决策

1. **30ms 批量刷新**：高波特率下避免每个字节触发一次信号，UI 事件循环稳定。
2. **增量解码**：接收区按 UTF-8/GBK 用 `QTextCodec::ConverterState` 跨 chunk 解码，避免多字节字符被拆断。
3. **UI 与日志解耦**：日志写入独立线程 + 有界队列（16 MB），磁盘慢时丢弃最旧条目并告警，绝不阻塞串口读取。
4. **解析器状态机**：帧解析在 UI 线程增量执行（30ms 一次，批量），支持噪声、半包、粘包、CRC 失败重同步。
5. **波形绘制降采样**：按像素列取 min/max 包络，绘制点数与点数总量解耦；通道数据有上限（每通道 20 万点，超限滚动淘汰）。
6. **配置持久化**：`QSettings`（Windows 注册表，其他平台 ini），保存几何、串口参数、显示选项、发送历史、波形配置。
7. **端口选择语义**：下拉框可编辑，列表项以规范端口名作为 itemData，描述/序列号只用于显示与
   区分同型号设备；2 秒热插拔刷新在列表未变化、弹出菜单打开或输入框聚焦时不重建控件。已保存
   但当前未枚举到的端口会作为“未检测到”条目保留，不会被自动切换到其他设备；用户手动清空后
   也不会被刷新悄悄恢复。
8. **异步打开状态**：点击打开后配置栏立即进入 pending 状态并禁止修改，避免驱动握手期间显示的
   参数与实际打开的 `SerialConfig` 不一致；成功/失败后重新同步并恢复可编辑。波特率只在输入
   完成或下拉选择后提交，避免把 `2`、`25` 这样的中间输入写入设置。
9. **应用品牌与资源**：应用图标由 Qt 绘制为 16–256 px 多尺寸 PNG，打包为多帧 ICO 作为
   Windows EXE 资源；运行时通过 `QIcon` 加载整套 PNG，避免依赖 ICO 图像插件。Windows
   版本资源由 CMake 配置 `SerialScope.rc.in` 生成。
10. **关于与许可**：关于对话框使用自定义 `QDialog`，按当前主题选择深/浅色 GitHub 标志；
    GPLv3 全文通过 Qt 资源内置，离线也可查看。程序采用 GPL-3.0-only，Qt 以 LGPLv3
    动态链接方式使用。

## 4. 模块接口（要点）

```cpp
// core/SerialConfig.h
struct SerialConfig { QString portName; qint32 baudRate; /* … Qt 枚举 … */ };

// core/SerialWorker.h
class SerialWorker : public QObject {
    Q_OBJECT
public slots:
    void openPort(const SerialConfig &cfg);
    void closePort();
    void sendData(const QByteArray &data);
signals:
    void portOpened(const QString &name, qint32 baud);
    void portClosed();
    void dataReceived(const QByteArray &chunk);
    void statsUpdated(qint64 txBytes, qint64 rxBytes);
    void errorOccurred(const QString &message);
    void portLost(const QString &message);
};

// core/DataLogger.h —— appendRx/appendTx 线程安全，可跨线程直接调用
class DataLogger : public QObject {
    Q_OBJECT
public:
    void appendRx(const QByteArray &data);
    void appendTx(const QByteArray &data);
public slots:
    void startLog(const QString &path, bool includeTx);
    void stopLog();
signals:
    void started(const QString &path);
    void stopped();
    void errorOccurred(const QString &message);
    void overflow(qint64 droppedBytes);
};

// core/parser/HexCodec.h
enum class TextEncoding { Ascii, Utf8, Gbk };
QByteArray HexCodec::parseHex(const QString &text, QString *error = nullptr);
QString   HexCodec::formatHex(const QByteArray &data, int bytesPerLine = 0);
QString   HexCodec::decode(const QByteArray &data, TextEncoding enc,
                           QTextCodec::ConverterState *state = nullptr);
QByteArray HexCodec::encode(const QString &text, TextEncoding enc);

// core/parser/AsciiValueParser.h —— 行缓冲 + 数字提取（支持小数/负数/科学计数）
class AsciiValueParser {
public:
    void feed(const QByteArray &data);
    void reset();
    std::function<void(const QVector<double> &values)> onValues;
    std::function<void()> onOverflow;
};

// core/parser/DataFrameParser.h —— 帧头搜索 / 半包 / 长度域 / 可选 CRC16-Modbus 校验
struct FrameSpec {
    QByteArray header;                 // 如 AA 55
    enum class LengthMode { Fixed, LengthByte } lengthMode = LengthMode::LengthByte;
    int lengthByteOffset = 2;          // 长度字节在帧内偏移
    int lengthAdjust = 0;              // frameLen = lengthByte + lengthAdjust
    int fixedFrameLength = 16;
    bool verifyCrc16Modbus = false;
    int maxFrameLength = 4096;
};
class DataFrameParser {
public:
    void feed(const QByteArray &data);
    void reset();
    std::function<void(const QByteArray &frame)> onFrame;
    std::function<void(int bytesSkipped)> onResync;
};

// core/parser/HexFieldExtractor.h
enum class FieldType { Int8, UInt8, Int16, UInt16, Int32, UInt32, Float32 };
struct HexField { QString name; int offset; FieldType type; };
struct HexParseSpec { FrameSpec frame; QVector<HexField> fields; bool bigEndian = false; };
QVector<double> HexFieldExtractor::extract(const QByteArray &frame,
                                           const HexParseSpec &spec, bool *ok);
```

## 5. 波形数据源

### 5.1 ASCII 行解析
- 输入按 `\n` / `\r` 切行（带半行缓冲）。
- 每行用正则提取全部数字（`[-+]?\d+(\.\d+)?([eE][-+]?\d+)?`），按出现顺序映射到 CH1…CH8。
- 同一行的多个数值共享同一时间戳（接收时刻），不同行按到达时间推进。
- 超过 8 个数值忽略多余项；十六进制字面量（0x…）不参与解析。

### 5.2 HEX 帧解析
- 帧结构由「协议配置」对话框定义：帧头、长度模式（固定帧长 / 长度字节）、长度字节偏移与补偿、可选 CRC16-Modbus 校验、字节序。
- 字段表定义每个通道的 `名称 / 字节偏移 / 类型`（int8/uint8/int16/uint16/int32/uint32/float32）。
- 状态机对字节流自动搜索帧头、处理半包、CRC 失败丢 1 字节重同步。
- 每帧提取所有字段 → 同一时间戳写入各通道。

### 5.3 显示与性能
- 时间轴：相对「图纸起点」的秒数，滚动窗口（5/10/30/60 s）。
- Y 轴自动量程（带 10% 边距），坐标刻度取 1/2/5×10^n 的「漂亮数」。
- 绘制时按像素列做 min/max 包络降采样，60fps 下可稳定绘制 >10 万点。
- 暂停只冻结曲线显示；数据仍继续解析和采集，恢复后立即显示最新窗口。清空同时重置解析器与时间基准。

## 6. 日志格式

```
# SerialScope log 2026-11-17 15:00:00
[2026-11-17 15:00:01.123] RX 01 03 00 00 00 02 C4 0B
[2026-11-17 15:00:01.456] TX AA 55 01 02 03 04
```

- 每行最多 32 字节，超出换行续写（同时间戳，缩进）。
- 记录线程每 250 ms 强制 flush，停止时关闭文件。
- 队列上限 16 MB；溢出丢弃最旧块并通过状态栏告警。

## 7. 关键边界情况

| 场景 | 处理 |
| --- | --- |
| 端口被拔出 / 驱动错误 | `QSerialPort::errorOccurred` → 安全关闭、通知 UI、重置配置面板 |
| 打开失败（占用/权限） | 弹窗显示 `errorString()`，保持断开状态 |
| 系统枚举不到任何串口 | 下拉框留空并显示可手动输入的占位提示；“打开串口”禁用，输入有效端口名后启用 |
| 保存的端口当前未插入 | 作为“xxx（未检测到）”条目保留，不切换到其他设备，也不覆盖保存值；可直接重试打开 |
| 2 秒热插拔刷新与用户操作冲突 | 列表未变化时不重建；弹出菜单打开或端口输入框聚焦时推迟刷新 |
| 打开串口握手较慢 | 配置栏进入“正在打开…”状态并锁定；成功后用驱动回报的端口/波特率回写，失败后恢复编辑 |
| 波特率输入到一半 | 输入框内实时校验并禁用打开；只在下拉选择或编辑结束时提交有效值到设置 |
| 1.5 停止位 | 仅在 Windows 且数据位为 5 时可用；其他组合在打开前给出明确提示 |
| 标记/空格校验 | 保留为可选项，但通过提示说明驱动依赖；macOS 打开前拒绝 |
| 高波特率洪峰 | 30ms 聚合 + 显示区最多 20000 块 + 波形点数上限 |
| UTF-8/GBK 字符跨包 | `ConverterState` 保留半字符状态 |
| HEX 输入非法 | 实时红色边框 + 提示，禁止发送 |
| 发送/接收定时器与关闭竞态 | 定时发送在关闭端口时停止；worker 关闭时先 flush 再发 portClosed |
| 无显示器 CI | 核心库不依赖 Widgets，测试可在 `-platform offscreen` 下运行 |
| 应用退出 | 阻塞关闭串口、停止日志线程、退出计时器，保证文件落盘 |

## 8. 构建与测试

```bash
# Windows (MinGW)
cmake -S . -B build -G Ninja \
      -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/mingw_64 \
      -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

- `test_core` 验证 CRC、HEX、编码、数值/帧解析等纯逻辑。
- `test_serial_config` 用 Qt `offscreen` 平台验证波特率/数据位/校验位/停止位选项、手动端口
  输入、刷新后保留选择、打开中状态和设置持久化。Windows 构建会自动复制测试所需的 Qt
  DLL 与平台插件，避免 PATH 中其他软件携带的旧 Qt 运行时抢先加载。
- 若工程路径含中文导致 `moc` 报 `Invalid argument`，可用目录联接建立 ASCII 路径后从
  联接路径配置构建：
  `mklink /J D:\SerialScope "D:\projects\SerialScope"`。
- Windows 发布：`cmake --build build --target package_release` 生成
  `build/release/SerialScope-1.0.0-win64.zip`，其中包含 Qt 运行库、README、LICENSE 与
  第三方声明。

## 9. 后续路线（未包含在本次实现）

- 多串口标签页、Modbus RTU 主/从模拟、Lua/JS 脚本联动、数据导出（CSV/二进制）、协议表格视图、i18n、安装包。
