# G2G-Bench (Glass-to-Glass Low-Latency Benchmark Tool)

[![Ubuntu 22.04 / 24.04](https://img.shields.io/badge/Platform-Ubuntu%2022.04%20%7C%2024.04-orange.svg)](https://ubuntu.com)
[![Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![Dual Engine](https://img.shields.io/badge/Engine-GStreamer%20%7C%20Pure%20C%2B%2B-brightgreen.svg)]()
[![Zero Buffer](https://img.shields.io/badge/Latency-Zero--Buffer-red.svg)]()

> **G2G-Bench** 是一套專為無人機 FPV 圖傳、遠端遙控、機器人視訊與車載環景系統設計的高精確度 **Glass-to-Glass (G2G，鏡頭採光到螢幕顯色)** 端到端延遲量測與驗證基準工具。

---

## 1. 核心亮點 (Key Features)

* ⏱️ **高頻基準源 (1ms 刷新精度)**：
  * 使用 Linux `CLOCK_MONOTONIC_RAW` 原生單調時鐘，不受 NTP 對時跳變影響。
  * 支援毫秒級（`MM:SS.mmm`）格式顯示，螢幕最高更新率即時渲染。
  * 支援全域快捷鍵快速操作（空白鍵暫停/啟動、R 鍵快速歸零、F 鍵凍結比對）。

* ⚡ **雙核心收流引擎架構 (Dual-Engine Architecture)**：
  * **Engine A (GStreamer 極限低延遲管線)**：支援 UVC (`v4l2src`)、UDP-RTP (`udpsrc`)、RTSP (`rtspsrc`)，強制設定 `latency=0`, `drop-on-latency=true`, `sync=false`, `appsink max-buffers=1, drop=true`。
  * **Engine B (Pure C++ Native 最小直連引擎)**：
    * **UVC 模式**：直接透過 Linux System Call (`open`, `ioctl`, `mmap`) 請求 2 個緩衝區，一進一出無額外 Queue。
    * **UDP 模式**：原生 POSIX Non-blocking Socket 接收 RTP 封包，自行解析 RFC 6184 H.264 NAL / FU-A 單元，搭配 `libavcodec` low-delay 旗標（`AV_CODEC_FLAG_LOW_DELAY`, 單執行緒即時解碼）。
    * **RTSP 模式**：採用原生 `libavformat` 零緩衝配置（`nobuffer`, `low_delay`, `probesize=32k`, `analyzeduration=0`）直連 TCP/RTSP 串流，繞過 GStreamer 框架調度開銷。
    * **無框架負擔**：完全繞過 GStreamer Element 與 Message Bus 調度開銷，作為極致基準線。

* 🚀 **Zero-Buffer 理念 (零緩衝隊列)**：
  * 收流執行緒與 UI 渲染執行緒之間採用 **Lock-Free 原子三緩衝置換（Triple-Buffer Atomic Swap）**。
  * 渲染端永遠只取「最新到達的單一幀」，舊幀一律強制丟棄，徹底杜絕播放器內部佇列積壓導致的 Buffer Bloat。
  * Direct OpenGL Texture 更新，渲染開銷 < 2ms。

* 📸 **量測輔助功能 (Freeze & Dump Snapshot)**：
  * **瞬時凍結 (Freeze Frame)**：一鍵同步凍結左側基準碼錶與右側回傳視訊畫面，肉眼即可直接讀取 G2G 毫秒延遲差。
  * **單鍵快照存檔 (Dump PNG)**：將凍結畫面一鍵儲存為自帶時間戳的 PNG 圖檔（存放於 `captures/`），便於撰寫測試報告。

* 📊 **即時診斷資訊 (Real-Time Diagnostics)**：
  * 實時顯示 Ingestion (Capture) FPS、Render FPS、Dropped Frame 累計計數、VSync 垂直同步開關狀態。

---

## 2. 系統架構 (System Architecture)

```
+---------------------------------------------------------------------------------------+
|                                    G2G-Bench UI                                      |
+-------------------------------------------+-------------------------------------------+
|          左側：高精準度基準碼錶             |          右側：回傳視訊鏡像 (Loopback)     |
|         CLOCK_MONOTONIC_RAW               |               OpenGL Texture              |
|              00:14.382                    |                 00:14.261                 |
+-------------------------------------------+-------------------------------------------+
|                                  Lock-Free FrameQueue                                 |
|                       (Atomic Triple-Buffer / Zero Queue Bloat)                       |
+---------------------------------------------------------------------------------------+
|  Engine A: GStreamer Pipeline             |  Engine B: Pure Native C++                |
|  - appsink (drop=true, sync=false)        |  - Linux V4L2 ioctl mmap (2 buffers)      |
|  - rtpjitterbuffer (latency=0)            |  - POSIX Non-blocking UDP Socket          |
|  - rtph264depay + avdec_h264              |  - RFC 6184 FU-A + libavcodec Low-Delay   |
+-------------------------------------------+-------------------------------------------+
```

---

## 3. 環境依賴 (Dependencies)

支援作業系統：**Ubuntu 22.04 LTS / 24.04 LTS (x86_64, aarch64)**

### 系統必要套件
在編譯前，請確認安裝以下相依套件：
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    pkg-config \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev
```

> **注意**：GLFW 與 Dear ImGui 已由專案 `CMakeLists.txt` 透過 CMake `FetchContent` 自動管理與編譯，不需手動安裝系統版本。

---

## 4. 建置與編譯 (Build Instructions)

```bash
# 1. 進入專案根目錄
cd g2g_analyzer

# 2. 建立 build 資料夾並產生 Makefile
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# 3. 進行編譯 (支援多核加速)
make -j$(nproc)
```

編譯完成後，執行檔將產生於 `build/g2g_bench`。

---

## 5. 操作指南 (Usage Guide)

### 啟動程式
```bash
./build/g2g_bench
```

### UI 介面與控制說明 (UI Controls)
1. **Stopwatch Panel (左側基準碼錶)**：
   * 點擊 **[Start] / [Pause]** 按鈕切換計時/暫停狀態，**[Reset]** 重置。
   * **快捷鍵 (Shortcuts)**：
     * `Space` (空白鍵)：切換 Start / Pause
     * `R` 鍵：碼錶 Reset
     * `F` 鍵：切換 Freeze / Resume
2. **Video Panel (右側視訊鏡像)**：
   * 顯示鏡頭反拍或圖傳回傳之畫面。
   * 點擊 **[Freeze Compare]**：左側基準碼錶與右側畫面同時瞬間定格，便於目視比對時間讀數。
   * 點擊 **[Save PNG Snapshot]**：將定格畫面匯出至 `captures/` 目錄中。
3. **Configuration & Diagnostics Panel (底部配置面板)**：
   * **Input Source**：切換 `V4L2 / UVC Camera`、`UDP-RTP Stream`、`RTSP Stream`。
   * **V4L2 / UVC 規格下拉選單**：
     * **Device**：自動列舉系統已連接的攝影機裝置（如 `/dev/video0 (HD User Facing)`），附帶 `[Rescan]` 按鈕隨插即掃。
     * **Profile (Format/Res/FPS)**：**單一整合選單**，直接列出硬體真實支援的有效規格組合（例如 `MJPG - 1280x720 @ 30 fps`、`YUYV - 1280x720 @ 10 fps`、`YUYV - 640x480 @ 30 fps` 等），杜絕選取到無效或無法協商之參數組合。
   * **Stream Engine**：
     * `(*) GStreamer Pipeline (Engine A)`
     * `( ) Pure C++ Native Direct IO (Engine B)`
     * 可隨時自由切換，系統將在 1 秒內無縫重建視訊管線。
   * **Enable VSync**：勾選可防畫面撕裂；取消勾選可解鎖渲染上限以測試極限更新率。

---

## 6. 本地測試與回環驗證 (Test & Verification)

若您目前手邊沒有實體 UVC 攝影機或外部硬體圖傳設備，專案提供了本地 UDP 低延遲回環測試腳本：

### 步驟 1：啟動測試推流器 (產生帶有實時時間戳的 H.264 RTP 串流)
開啟另一個終端機視窗，執行：
```bash
./scripts/test_udp_streamer.sh 5004 1280 720 60
```
該腳本會使用 GStreamer 產生即時動畫，並在畫面右下角疊加即時碼錶，透過 UDP RTP H.264 推送到本機 `127.0.0.1:5004`。

### 步驟 2：啟動 G2G-Bench 接收與驗收
在主終端機啟動：
```bash
./build/g2g_bench
```
1. 底部面板輸入源選擇 **[UDP-RTP 串流]**，連接埠為 `5004`。
2. 預設 **GStreamer Pipeline** 模式下，右側將立即呈現流暢畫面，Capture FPS 達 60 FPS。
3. 切換為 **Pure C++ Native Direct IO** 模式，畫面將在 1 秒內無縫切換至原生 Socket + libavcodec 低延遲解碼。
4. 按下快捷鍵 `F` 進行凍結，檢驗左側碼錶與右側視訊是否同時精確定格。

### RTSP 串流測試方法
專案亦提供了本地 RTSP 回環測試腳本（以 FFmpeg 在背景建立低延遲 RTSP 服務端）：
1. 開啟另一個終端機執行：
   ```bash
   ./scripts/test_rtsp_streamer.sh 8554 30 1280 720
   ```
2. 在 G2G-Bench 底部面板切換至 **[RTSP Stream]**，輸入框填寫 `rtsp://127.0.0.1:8554/live`，按下 **[Enter]** 鍵或點擊 **[Connect / Apply]**，即可直接連線並顯示即時畫面。

---

## 7. 驗收標準對照 (Acceptance Criteria)

* ✅ **場景 1 (UDP 串流零延遲接收與渲染)**：
  * UDP-RTP 串流於 500ms 內穩定渲染。
  * 畫面更新率與推流訊號相符（偏差 < 2 FPS）。
  * 採用 Lock-free Triple-buffer，無緩衝區積累。
* ✅ **場景 2 (切換 Native C++ 模式)**：
  * UI 點擊切換引擎，於 1 秒內完成管線重建。
  * 原生直連繞過 GStreamer element 開銷，CPU 佔用極低。

---

## 8. 目錄結構 (Project Structure)

```
g2g_analyzer/
├── CMakeLists.txt              # CMake 建置配置
├── README.md                   # 產品說明與使用手冊
├── doc/
│   └── requirement_pdr.md      # 產品需求規格書 (PRD)
├── scripts/
│   └── test_udp_streamer.sh    # 本地低延遲 UDP RTP 測試推流腳本
├── third_party/
│   └── stb_image_write.h       # PNG 快照存檔函式庫
└── src/
    ├── main.cpp                # 程式進入點與主迴圈
    ├── core/
    │   ├── Types.hpp           # 訊號源、引擎與畫格定義
    │   ├── Stopwatch.hpp / cpp # CLOCK_MONOTONIC_RAW 高頻碼錶
    │   └── FrameQueue.hpp      # Lock-free 零緩衝原子交換緩衝區
    ├── engine/
    │   ├── IVideoEngine.hpp    # 雙引擎統一抽象介面
    │   ├── GstVideoEngine.hpp / cpp    # Engine A (GStreamer)
    │   └── NativeVideoEngine.hpp / cpp # Engine B (Pure Native C++)
    ├── render/
    │   ├── GLTexture.hpp / cpp # Direct OpenGL 紋理更新
    │   └── UIRenderer.hpp / cpp# Dear ImGui 分割畫面與參數面板
    └── utils/
        ├── FpsTracker.hpp      # 滑動視窗 FPS 統計
        └── ImageDumper.hpp / cpp# PNG 快照匯出工具
```

---

## 9. 授權 (License)

本工具為專業視訊延遲評估軟體，遵循內部工程開發與開源使用規範。
