#!/usr/bin/env python3
import time
import threading
from typing import Optional

import cv2
from flask import Flask, Response, request
from yt_dlp import YoutubeDL

DEFAULT_URL = "https://youtu.be/SEhWSXW0aCc"
TARGET_W = 240
TARGET_H = 135
TARGET_FPS = 10
JPEG_QUALITY = 50

app = Flask(__name__)

_state_lock = threading.Lock()
_state = {
    "url": DEFAULT_URL,
    "stream_url": None,
    "last_resolve": 0.0,
    "seek_to": None,
    "pos_sec": 0.0,
}


def resolve_stream_url(youtube_url: str) -> Optional[str]:
    ydl_opts = {
        "quiet": True,
        "no_warnings": True,
        "format": "best[ext=mp4]/best",
    }
    with YoutubeDL(ydl_opts) as ydl:
        info = ydl.extract_info(youtube_url, download=False)
        if "url" in info and info["url"]:
            return info["url"]
        # Fallback: pick best progressive format
        formats = info.get("formats") or []
        for fmt in reversed(formats):
            if fmt.get("url") and fmt.get("vcodec") != "none":
                return fmt["url"]
    return None


def get_stream_url() -> Optional[str]:
    with _state_lock:
        url = _state["url"]
        cached = _state.get("stream_url")
        last_resolve = _state.get("last_resolve", 0.0)

    # Refresh every 5 minutes to avoid expired URLs
    if cached and time.time() - last_resolve < 300:
        return cached

    stream_url = resolve_stream_url(url)
    if stream_url:
        with _state_lock:
            _state["stream_url"] = stream_url
            _state["last_resolve"] = time.time()
    return stream_url


def open_capture() -> Optional[cv2.VideoCapture]:
    stream_url = get_stream_url()
    if not stream_url:
        return None
    cap = cv2.VideoCapture(stream_url)
    if not cap.isOpened():
        cap.release()
        return None
    return cap


def generate_frames():
    cap = None
    frame_interval = 1.0 / float(TARGET_FPS)

    while True:
        with _state_lock:
            seek_to = _state.get("seek_to")

        if seek_to is not None:
            if cap is None or not cap.isOpened():
                cap = open_capture()
            if cap is not None and cap.isOpened():
                cap.set(cv2.CAP_PROP_POS_MSEC, float(seek_to) * 1000.0)
            with _state_lock:
                _state["seek_to"] = None

        if cap is None or not cap.isOpened():
            cap = open_capture()
            if cap is None:
                time.sleep(1.0)
                continue

        start = time.time()
        ok, frame = cap.read()
        if not ok or frame is None:
            cap.release()
            cap = None
            time.sleep(0.5)
            continue

        frame = cv2.resize(frame, (TARGET_W, TARGET_H), interpolation=cv2.INTER_AREA)
        pos_ms = cap.get(cv2.CAP_PROP_POS_MSEC)
        if pos_ms and pos_ms > 0:
            with _state_lock:
                _state["pos_sec"] = float(pos_ms) / 1000.0
        ok, buf = cv2.imencode(
            ".jpg",
            frame,
            [int(cv2.IMWRITE_JPEG_QUALITY), JPEG_QUALITY],
        )
        if not ok:
            continue

        jpg = buf.tobytes()
        yield (
            b"--frame\r\n"
            b"Content-Type: image/jpeg\r\n"
            b"Content-Length: " + str(len(jpg)).encode() + b"\r\n\r\n" + jpg + b"\r\n"
        )

        elapsed = time.time() - start
        sleep_for = frame_interval - elapsed
        if sleep_for > 0:
            time.sleep(sleep_for)


@app.route("/stream")
def stream():
    return Response(
        generate_frames(),
        mimetype="multipart/x-mixed-replace; boundary=frame",
    )


@app.route("/set")
def set_url():
    url = request.args.get("url")
    if not url:
        return "missing url", 400
    with _state_lock:
        _state["url"] = url
        _state["stream_url"] = None
        _state["last_resolve"] = 0.0
        _state["seek_to"] = None
        _state["pos_sec"] = 0.0
    return "ok", 200


@app.route("/seek")
def seek():
    try:
        delta = float(request.args.get("delta", "0"))
    except ValueError:
        return "bad delta", 400
    with _state_lock:
        pos = float(_state.get("pos_sec", 0.0))
        target = max(0.0, pos + delta)
        _state["seek_to"] = target
    return "ok", 200


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, threaded=True)
