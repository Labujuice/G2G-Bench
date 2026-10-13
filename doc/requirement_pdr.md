# 產品需求規格書（PRD）：G2G-Bench (Glass-to-Glass Low-Latency Benchmark Tool)

| 文件版本 | 狀態 | 撰寫人 | 目標平台 |
| :--- | :--- | :--- | :--- |
| v1.0.0 | Draft for Engineering Review | Senior Technical PM | Ubuntu 22.04 / 24.04 LTS (x86_64 / aarch64) |

---

## 1. 產品概述 (Executive Summary)

### 1.1 背景痛點 (Problem Statement)
在無人機 FPV 圖傳、遠端操控與車載環景系統中，視訊串流的 Glass-to-Glass (G2G，鏡頭採光到螢幕顯色) 延遲直接決定了系統操控穩定性與安全邊界。然而目前業界缺乏一套能在 Ubuntu 上開箱即用、能排除作業系統與播放器多餘緩衝（Display/Buffer Bloat）、且能交叉驗證 GStreamer 框架開銷的精確量測工具。

### 1.2 產品定位與核心目標 (Product Vision & Goals)
打造一套以 C++ 原生開發的輕量化、高效能 G2G 延遲驗證 UI 工具。
* **高頻基準源**：提供 1ms 級別高精確度數位碼錶。
* **雙引擎架構（Dual-Engine）**：支援標準 GStreamer 低延遲 Pipeline，以及可一鍵切換的 Pure C++ Native 最小直連引擎。
* **Zero-Buffer 理念**：採集端最小化 Queue、解碼端無延遲、渲染端 Direct Texture Update，消弭播放器自身引入的非必要延遲。

### 1.3 核心成功指標 (Success Metrics / KPIs)
* **工具內部固有延遲（Self-Overhead）**：碼錶渲染至畫面幀緩衝延遲 < 1ms。
* **播放器端 Buffer 延遲**：在 Zero-Copy 模式下，從接收到 Frame 到 GPU 顯示的軟體處理耗時 < 2ms。
* **量測再現性**：高格率相機反拍測試下，誤差抖動在 ±1 Frame（依相機更新率為準）。

---

## 2. 目標使用者與使用場景 (User Personas & Scenarios)

### 2.1 目標使用者
* **嵌入式影像 / 串流工程師**：需要調校 V4L2 Driver、ISP、H.264/H.265 編碼參數與 RTP 傳輸佇列。
* **無人機 / 機器人系統架構師**：評估不同圖傳模組（Wi-Fi、專用數位圖傳）與不同串流協議（RTSP、Raw RTP）的端到端物理延遲。
* **品管與產測工程師（QA）**：出廠驗收標準化 G2G 測試。

### 2.2 典型量測旅程 (User Journey)
1. 使用者在 Ubuntu 主機啟動本工具，UI 左半部顯示高頻碼錶，並點擊「啟動計時」。
2. 使用者配置輸入訊號來源（如相機 UVC 拍螢幕，或相機接外部板子編碼後以 UDP/RTSP 串流回灌給主機）。
3. UI 右半部實時無緩衝渲染回傳畫面，畫面中能清楚看見攝影機拍到的「左側碼錶鏡像」。
4. 使用者點擊「畫面凍結（Freeze/Snapshot）」或使用高速相機對著螢幕拍照，計算：
   $$\text{G2G Latency} = \text{左側主碼錶當前時間} - \text{右側影像內顯示之碼錶時間}$$
5. 使用者切換「GStreamer 模式」與「Native C++ 模式」，快速量化 GStreamer 引入的框架負載差異。

---

## 3. 規格範圍邊界 (Scope Definition)

### 3.1 In-Scope (本次 MVP 交付)
1. **高頻碼錶子系統**：1ms 刷新精度、Start / Pause / Reset 控制。
2. **雙分割視窗 UI（Side-by-Side）**：左側基準碼錶、右側即時視訊預覽、底部控制與狀態列。
3. **雙核心串流收流引擎**：
   * **Engine A (GStreamer)**：支援 V4L2 (UVC)、RTSP、UDP (Raw RTP/H.264/H.265)，強制開啟零緩衝參數 (`latency=0`, `drop-on-latency=true`, `sync=false`)。
   * **Engine B (Pure C++ Native)**：繞過 GStreamer，純 V4L2 mmap 讀取 / POSIX UDP Socket 接收 + 輕量解碼 + 直接寫入 OpenGL Texture。
4. **即時診斷資訊**：顯示當前 Ingestion FPS、渲染幀率、Drop Frame 計數器。

### 3.2 Out-of-Scope (後續版本迭代)
* 自動化光學 OCR 字元識別與自動算差算法（MVP 階段採人工目視凍結讀數或高格相機標定，避免 OCR 演算法本身開銷干擾量測）。
* 複雜的視訊錄製與回放剪輯功能。
* Windows / macOS 跨平台移植（首版嚴格聚焦 Linux/Ubuntu 系統級優化）。

---

## 4. 功能規格與使用者體驗 (Feature Specifications)

### 4.1 UI 佈局規範 (Layout)

```

+-------------------------------------------------------------------------------+
| G2G-Bench v1.0 [Engine: Native C++] [Status: RUNNING]      |
+---------------------------------------+---------------------------------------+
| [基準時間 (Reference Time)]            | [回傳影像 (Loopback Stream)]           |
|                                       |                                       |
|             00:14.382                 |             00:14.261                 |
|            (1ms 刷新率)                |           (相機反拍鏡像)               |
|                                       |                                       |
|  [啟動]   [暫停]   [歸零]              |  [凍結比對 (Freeze)]  [全螢幕]         |
+---------------------------------------+---------------------------------------+
| 參數配置面板:                                                                  |
| - 輸入源: [ V4L2 / UVC v ]  Device: [/dev/video0]  Profile: [1920x1080@60fps] |
| - 協議:   [ UDP-RTP    v ]  Port:   [5004       ]  Payload: [H.264]           |
| - 引擎:   (*) GStreamer Pipeline      ( ) Pure C++ Native Direct IO           |
| - 監控:   Capture FPS: 59.8 | Render FPS: 60.0 | Dropped: 0 | Sync: None      |
+-------------------------------------------------------------------------------+

```

---

### 4.2 核心功能模組清單

#### 模組 1：高精準度基準碼錶 (Precision Stopwatch)
* **規格需求**：
  * 使用 Linux `CLOCK_MONOTONIC_RAW` 或 `std::chrono::steady_clock` 作為底層時基，嚴禁受 NTP 對時跳變影響。
  * 畫面更新頻率至少達螢幕最高更新率（如 60Hz/144Hz/240Hz），數值以 **分:秒.毫秒 (MM:SS.mmm)** 格式呈現。
  * 支援背景常駐與瞬態歸零，支援快捷鍵（空白鍵：暫停/繼續、R：歸零）。

#### 模組 2：視訊輸入與雙引擎抽象層 (Dual-Engine Ingestion Pipeline)
系統必須具備統一的 Video Receiver 介面，底下實作兩個獨立引擎：

* **Engine A: GStreamer 極限低延遲引擎**
  * **UVC 模式**：構建 `v4l2src device=/dev/videoX ! <caps> ! videoconvert ! appsink`，設定 `io-mode=mmap`, `buffer-time=0`, `max-buffers=1`, `drop=true`。
  * **RTSP 模式**：`rtspsrc location=... latency=0 buffer-mode=none drop-on-latency=true ! ...`。
  * **UDP/RTP 模式**：`udpsrc port=... caps=... ! rtpjitterbuffer latency=0 drop-on-latency=true ! ...`。
  * Sink 端設定：所有渲染節點關閉時鐘同步 (`sync=false`)，禁止額外時間戳排程等待。

* **Engine B: Pure Native C++ 最小直連引擎 (Non-Streaming Baseline)**
  * **UVC 模式**：直接以 Linux V4L2 System Call (`open`, `ioctl`) 操作，請求 2 個 mmap 緩衝區 (`VIDIOC_REQBUFS`)，一進一出以先進先出最小佇列運作。
  * **UDP 模式**：使用原生 POSIX Non-blocking Socket，直接收取 RTP 封包。搭配嵌入式輕量解碼庫（如直接調用 `libavcodec` low-delay 旗標或專用解碼模組），將資料直接 DMA/複製到紋理記憶體。
  * **核心原則**：完全拔除 GStreamer 的 Element 連接、Message Bus 與線程切換開銷。

#### 模組 3：零緩衝渲染管線 (Zero-Buffer Render Pipeline)
* **渲染架構**：採用 C++ 搭配 OpenGL (或 Vulkan/EGL) 直接繪製。
* **傳輸優化**：
  * 接收執行緒與渲染執行緒採用單緩衝原子置換（Double/Triple Buffer with lockless atomic swap），渲染端永遠只取「最新到達的單一幀」，舊幀一律強制丟棄，嚴禁 Queue 積累。
  * 支援 PBO (Pixel Buffer Object) 非同步上傳或 DMA-BUF (若硬體支援)，盡可能達成 Zero-Copy 紋理綁定。
  * 關閉渲染後端所有非必要的 Double Buffering 延遲設定。

#### 模組 4：量測輔助功能（Freeze & Capture）
* **瞬時凍結（Freeze Frame）**：點擊後，左側碼錶與右側影像同時「定格在同一個瞬間」，讓工程師無須高速相機即可肉眼比對兩邊時間差。
* **單鍵存檔（Dump Comparison）**：將凍結畫面一鍵儲存為 PNG，檔名帶有時間戳，供測試報告建檔。

---

## 5. 非功能性需求 (Non-Functional Requirements)

| 維度 | 指標 | 驗收基準 / 說明 |
| :--- | :--- | :--- |
| **程式語言** | C++17 或以上 | 確保極致效能與現代並發支援（Memory barrier / Lock-free atomics）。 |
| **UI 框架** | 輕量原生 UI | 建議優先考慮 **Dear ImGui (OpenGL3 backend)** 或 **Qt6 (QOpenGLWidget)**，確保渲染主循環無額外排程負擔。 |
| **底層相容性** | Linux V4L2 API | 支援 UVC 1.1/1.5 規範，相容 YUYV / MJPEG / NV12 格式輸入。 |
| **記憶體佔用** | 靜態常駐 < 150MB | 嚴格防範長時間運行的 Memory Leak。 |
| **CPU 負載** | 單核 < 20% (原生模式) | Native 模式下不得佔用過多運算資源，以免影響解碼時效。 |

---

## 6. 例外與邊界情境處理 (Edge Cases & Error Handling)

1. **網路封包遺失 (Packet Drop in UDP)**：
   * 當 RTP 丟包造成破圖或解碼失敗時，解碼器不得卡住等待，必須立即跳幀（Skip/Drop），在下一關鍵幀（IDR/I-Frame）到達時立即重繪，並於 UI 狀態列更新「Frame Drop」計數。
2. **攝影機拔除 / 訊號中斷**：
   * 偵測到 V4L2 設備斷開或 RTSP Connection Timeout 時，右側畫面顯示「Signal Lost」，引擎進入重試或待機狀態，不得引發程式崩潰（SIGSEGV）。
3. **螢幕更新率不匹配（VSync 瓶頸）**：
   * 若相機輸出為 120fps 但螢幕僅有 60Hz，渲染循環必須以「最新幀優先」，並提供關閉 VSync 選項，避免繪製執行緒被 Display Server 阻斷。

---

## 7. 驗收標準 (Acceptance Criteria - Gherkin 格式)

```gherkin
場景 1: 驗證 UDP 串流零延遲接收與渲染
  Given 使用者啟動 G2G-Bench，並選擇「UDP-RTP 串流模式」且連接埠設為 5004
  And 串流引擎設為「GStreamer Pipeline」
  When 外部設備開始推送 H.264 RTP 串流
  Then 右側視窗必須在 500ms 內開始穩定顯示畫面
  And 畫面渲染更新率必須與發送端實際訊號幀率相符（偏差 < 2 FPS）
  And 系統記憶體使用量維持恆定，無緩衝區積壓現象

場景 2: 切換 Native C++ 模式
  Given 串流正常播放中
  When 使用者在 UI 面板切換為「Pure C++ Native 引擎」
  Then 視訊管線必須在 1 秒內無縫重建完畢
  And 畫面正常渲染，右側延遲觀測值應小於或等於 GStreamer 引擎模式

```

---

## 8. 風險評估與工程建議 (Risks & Recommendations)

* **X11 / Wayland Compositor 延遲干擾（高風險）**：
* *風險*：Ubuntu 22.04/24.04 預設的 GNOME Wayland 視窗合成器本身會引入 1~2 幀（16~33ms）的桌面合成延遲。
* *建議*：在 PRD 中規範建議以全螢幕獨佔模式（Direct Scanout）運行，或在 X11 Session 下測試以獲得最純粹的物理數值。


* **H.264 解碼器 B-Frame 延遲（中風險）**：
* *建議*：在 UDP/RTSP 模式下，必須明確要求編碼端關閉 B-Frame（Zero-Latency Tuning: `zerolatency`, `bframes=0`），工具端也必須預設開啟解碼器的 Low-Delay 模式。




---

### PM 建議之下一步落地行動

這份 PRD 已具備讓架構師與工程團隊直接進入技術細節（TAD / Architecture Design）的完整條件。接下來您可以直接選擇：

<ElicitationsGroup message="這份規格是否符合您的預期？請選擇下一步推進方向：">
  <Elicitation label="輸出技術架構決策（UI 與解碼庫選型）" query="請以系統架構師視角，評估 UI（Qt6 vs. ImGui）與原生解碼庫的選型建議與系統模組切分。"/>
  <Elicitation label="產生 GStreamer 低延遲 Pipeline 與 Native C++ 核心偽代碼" query="請提供 PRD 中 Engine A (GStreamer 零緩衝 Pipeline) 與 Engine B (Native V4L2 mmap) 的關鍵 C++ 實作範例。"/>
  <Elicitation label="微調 PRD 規格細節" query="我想針對 PRD 的部分功能或介面配置進行補充與調整。"/>
</ElicitationsGroup>

