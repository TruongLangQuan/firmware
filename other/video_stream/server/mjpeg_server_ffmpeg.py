#!/usr/bin/env python3
import subprocess
import time
import threading
from typing import Optional

from flask import Flask, Response, request
from yt_dlp import YoutubeDL

DEFAULT_URL = "https://youtu.be/SEhWSXW0aCc"
TARGET_W = 240
TARGET_H = 135
TARGET_FPS = 10
JPEG_QUALITY = 8  # ffmpeg scale: 2-31 (lower is better)

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

    if cached and time.time() - last_resolve < 300:
        return cached

    stream_url = resolve_stream_url(url)
    if stream_url:
        with _state_lock:
            _state["stream_url"] = stream_url
            _state["last_resolve"] = time.time()
    return stream_url


def spawn_ffmpeg(url: str, seek_to: Optional[float] = None) -> subprocess.Popen:
    cmd = [
        "ffmpeg",
        "-loglevel",
        "error",
        "-reconnect",
        "1",
        "-reconnect_streamed",
        "1",
        "-reconnect_delay_max",
        "2",
    ]
    if seek_to is not None and seek_to > 0:
        cmd += ["-ss", str(seek_to)]
    cmd += [
        "-i",
        url,
        "-vf",
        f"scale={TARGET_W}:{TARGET_H}",
        "-r",
        str(TARGET_FPS),
        "-f",
        "mjpeg",
        "-q:v",
        str(JPEG_QUALITY),
        "-",
    ]
    return subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)


def iter_jpegs(pipe):
    buf = bytearray()
    while True:
        chunk = pipe.read(4096)
        if not chunk:
            break
        buf.extend(chunk)
        while True:
            start = buf.find(b"\xff\xd8")
            end = buf.find(b"\xff\xd9")
            if start == -1 or end == -1 or end < start:
                break
            jpg = bytes(buf[start:end + 2])
            del buf[:end + 2]
            yield jpg


def generate_frames():
    proc = None
    while True:
        with _state_lock:
            seek_to = _state.get("seek_to")
        if proc is None or proc.poll() is not None:
            if proc:
                proc.kill()
            url = get_stream_url()
            if not url:
                time.sleep(1.0)
                continue
            proc = spawn_ffmpeg(url, seek_to)
            with _state_lock:
                if seek_to is not None:
                    _state["pos_sec"] = float(seek_to)
                    _state["seek_to"] = None
            if not proc.stdout:
                time.sleep(1.0)
                continue

        for jpg in iter_jpegs(proc.stdout):
            with _state_lock:
                _state["pos_sec"] = float(_state.get("pos_sec", 0.0)) + (1.0 / float(TARGET_FPS))
            yield (
                b"--frame\r\n"
                b"Content-Type: image/jpeg\r\n"
                b"Content-Length: " + str(len(jpg)).encode() + b"\r\n\r\n" + jpg + b"\r\n"
            )
        time.sleep(0.5)


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
