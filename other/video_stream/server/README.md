# MJPEG Server (Arch Linux)

This folder provides two MJPEG server implementations for the ESP32-S3 Video Stream module.

## 1) OpenCV version (recommended for simplicity)

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python mjpeg_server.py
```

- Stream URL: `http://<arch-server-ip>:5000/stream`
- Switch video: `http://<arch-server-ip>:5000/set?url=<youtube_url>`
- Seek +/- seconds: `http://<arch-server-ip>:5000/seek?delta=10` or `delta=-10`

## 2) ffmpeg version (lower latency)

Requires `ffmpeg` installed on Arch:

```bash
sudo pacman -S ffmpeg
python mjpeg_server_ffmpeg.py
```

## Notes
- Default YouTube source: `https://youtu.be/SEhWSXW0aCc`
- Frames are resized to 240x135 and encoded as JPEG for the ESP32 client.
- For viewing from anywhere, the server must be reachable over the internet
  (public IP, port-forwarding, or a tunnel like Cloudflare Tunnel).

## Autostart (systemd)
Install the provided service so the stream starts on boot:

```bash
sudo cp mjpeg-server.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now mjpeg-server.service
```

Check status:

```bash
systemctl status mjpeg-server.service
```
