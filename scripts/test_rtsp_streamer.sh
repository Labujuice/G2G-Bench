#!/usr/bin/env bash
# ==============================================================================
# G2G-Bench Test Camera RTSP Streamer
# Streams local camera (/dev/video0) via low-latency RTSP to rtsp://127.0.0.1:8554/live
# ==============================================================================

DEVICE=${1:-/dev/video0}
PORT=${2:-8554}
WIDTH=${3:-1280}
HEIGHT=${4:-720}
FPS=${5:-30}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"
MEDIAMTX_BIN="${PROJECT_ROOT}/third_party/bin/mediamtx"

if [ ! -f "${MEDIAMTX_BIN}" ]; then
    echo "[G2G-Bench] Downloading lightweight RTSP server (mediamtx)..."
    mkdir -p "${PROJECT_ROOT}/third_party/bin"
    curl -sL https://github.com/bluenviron/mediamtx/releases/download/v1.21.2/mediamtx_v1.21.2_linux_amd64.tar.gz | tar -xz -C "${PROJECT_ROOT}/third_party/bin" mediamtx
    chmod +x "${MEDIAMTX_BIN}"
fi

echo "=============================================================================="
echo "[G2G-Bench] Starting local RTSP Server on port ${PORT}..."
echo "[G2G-Bench] Camera Device: ${DEVICE} (${WIDTH}x${HEIGHT} @ ${FPS}fps)"
echo "[G2G-Bench] Target RTSP URL: rtsp://127.0.0.1:${PORT}/live"
echo "[G2G-Bench] Press Ctrl+C to terminate."
echo "=============================================================================="

# Launch MediaMTX in background (RTSP only, other protocols disabled)
MTX_RTSPADDRESS=":${PORT}" \
MTX_WEBRTC=no MTX_HLS=no MTX_RTMP=no MTX_SRT=no MTX_MOQ=no \
MTX_PATHS_ALL_SOURCE=publisher \
"${MEDIAMTX_BIN}" > /dev/null 2>&1 &
MTX_PID=$!

# Trap signals to ensure cleanup of all background jobs
cleanup() {
    echo ""
    echo "[G2G-Bench] Stopping RTSP server and camera capture..."
    kill ${FFMPEG_PID} 2>/dev/null
    kill ${MTX_PID} 2>/dev/null
    wait ${MTX_PID} 2>/dev/null
    echo "[G2G-Bench] Exited cleanly."
}
trap cleanup EXIT INT TERM

# Wait briefly for RTSP server listener
sleep 0.8

# Check if device supports MJPEG for high fps / 720p
V4L2_FMT_ARG=""
if v4l2-ctl -d "${DEVICE}" --list-formats 2>/dev/null | grep -qi "MJPG"; then
    V4L2_FMT_ARG="-input_format mjpeg"
fi

# Optional timestamp overlay
VF_ARG=""
if [ -f "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" ]; then
    VF_ARG="-vf drawtext=fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf:text='%{pts\:hms}':fontsize=36:fontcolor=white:box=1:boxcolor=black@0.6:x=w-tw-20:y=h-th-20"
fi

ffmpeg -hide_banner -loglevel warning \
    -f v4l2 ${V4L2_FMT_ARG} -video_size "${WIDTH}x${HEIGHT}" -framerate "${FPS}" -i "${DEVICE}" \
    ${VF_ARG} \
    -c:v libx264 -preset ultrafast -tune zerolatency -pix_fmt yuv420p \
    -f rtsp "rtsp://127.0.0.1:${PORT}/live" &
FFMPEG_PID=$!

wait ${FFMPEG_PID}
