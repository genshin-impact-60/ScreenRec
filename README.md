# ScreenRec

Windows 屏幕录制工具。Qt 6 Widgets + Multimedia，输出 H.264 / AAC 的 MP4。

当前环境：**Qt 6.10.2 + MinGW 64 + CMake**。请用 Qt Creator 打开本目录运行，这样能加载 `plugins/multimedia` 和 FFmpeg 插件。

---

## 功能

- 整屏 / 区域 / 窗口三种模式
- 麦克风、系统声音（WASAPI loopback），可同时开，混成一条音轨
- 暂停、倒计时、多显示器
- 分辨率（原始 / 1080p / 720p）、帧率（24 / 30 / 60）、质量
- 录制中主窗口隐藏，用悬浮条、托盘或全局热键控制
- 默认热键：`Ctrl+Shift+R` 开始，`Ctrl+Shift+S` 停止，`Ctrl+Shift+P` 暂停（可改）
- 设置会记住；停止后可打开文件或目录

默认保存到 `视频/ScreenRec/ScreenRec_时间戳.mp4`。

---

## 使用

1. 选模式：整屏、区域或窗口。
2. 区域模式先点「选择区域」拖出矩形，Enter 确认，Esc 取消。
3. 按需勾选麦克风、系统声音，设置画面参数。
4. 点「开始录制」。有倒计时时主窗口会隐藏。
5. 停止用悬浮条、托盘或热键。

录系统声音时先让电脑里正在出声（浏览器、播放器等），再开始录。

---

## 编译

Qt Creator：Kit 选 **Desktop Qt 6.10.2 MinGW 64-bit**，打开本目录，构建并运行。

命令行示例：

```text
cmake -S . -B build -DCMAKE_PREFIX_PATH=D:/APP/Qt/6.10.2/mingw_64
cmake --build build
```

从 build 目录直接跑 exe 可能找不到 Qt / FFmpeg 插件，优先用 Creator 运行。发布时要用 `windeployqt`，并带上 `plugins/multimedia`。

---

## 结构

```text
src/
  main.cpp                 入口
  ui/                      界面
    mainwindow.*           主窗口和信号连接
    floatingbar.*          录制中悬浮条
    regionselector.*       区域选择遮罩
    countdownoverlay.*     倒计时数字
  core/                    业务
    recordingcontroller.*  录制状态机
    settings.*             QSettings 持久化
    hotkeymanager.*        全局热键
  capture/                 采集
    screencapturesource.*  QScreenCapture 封装
    systemaudiocapture.*   WASAPI 系统内录，可与麦克风混音
```

采集和编码不放在界面类里。`MainWindow` 只发命令、收状态。

状态：`Idle → Countdown → Recording ↔ Paused → Stopping → Idle`。

整屏 / 窗口：`QScreenCapture` 或 `QWindowCapture` → `QMediaRecorder`，分辨率限制交给编码器。  
区域：采集帧 → 裁剪 → `QVideoFrameInput` → 编码器。

---

## 限制

- 不要用定时截图当录像。游戏、全屏、硬件加速窗口可能是黑屏；那是采集后端限制，不是界面问题。
- 麦克风和系统声音不是一回事。系统声走 WASAPI loopback。
- 悬浮条、倒计时、主窗口会尽量用 `WDA_EXCLUDEFROMCAPTURE` 从采集里排除；旧系统或不支持的采集后端仍可能录到它们。
- 区域裁剪是整屏采集再 crop，大分辨率高帧率会吃 CPU。
- 高 DPI 下区域坐标按 `devicePixelRatio` 换算。
- MinGW 下不要走 WinRT 窗口采集；Multimedia + FFmpeg 插件可用。

---

## 以后如果不够用

出现游戏黑屏、1080p60 掉帧、或必须抠掉录制器窗口时，再换 DXGI Desktop Duplication + 自管 FFmpeg。`RecordingController` 对外接口可以保持不变，只换采集和编码实现。
