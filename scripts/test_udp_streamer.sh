#!/usr/bin/env bash
# ==============================================================================
# G2G-Bench Test Streamer (Low-Latency UDP RTP H.264 Streamer)
# Streams videotestsrc with real-time timestamp overlay to 127.0.0.1:5004
# ==============================================================================

PORT=${1:-5004}
WIDTH=${2:-1280}
HEIGHT=${3:-720}
FPS=${4:-60}

echo "[G2G-Bench] Starting test H.264 RTP stream to 127.0.0.1:${PORT} (${WIDTH}x${HEIGHT}@${FPS}fps)..."
echo "[G2G-Bench] Press Ctrl+C to terminate the test stream."

gst-launch-1.0 -v \
    videotestsrc is-live=true pattern=ball ! \
    "video/x-raw,framerate=${FPS}/1,width=${WIDTH},height=${HEIGHT}" ! \
    timeoverlay valignment=bottom halignment=right font-desc="Sans Bold 36" ! \
    x264enc tune=zerolatency speed-preset=ultrafast bitrate=3000 key-int-max=30 bframes=0 ! \
    rtph264pay config-interval=1 pt=96 ! \
    udpsink host=127.0.0.1 port=${PORT} sync=false
