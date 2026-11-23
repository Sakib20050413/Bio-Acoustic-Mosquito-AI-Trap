#!/usr/bin/env python3
"""
Mobile-to-Antigravity Camera & Video Bridge (iPhone / Android)
Supports:
  1) 1-Tap Live Camera Photo (single photo -> latest.jpg)
  2) 1-Tap Short Video Clip (camera video -> latest_video.mp4/.mov)
  3) Multi-Photo Gallery Upload (multiple photos at once)
"""

import datetime
import html
import http.server
import mimetypes
import re
import socket
import socketserver
from pathlib import Path

PORT = 8765
ROOT_DIR = Path(__file__).resolve().parent.parent
CAPTURE_DIR = ROOT_DIR / "hardware_captures"
DEMO_DIR = ROOT_DIR / "demo_audio"
CAPTURE_DIR.mkdir(parents=True, exist_ok=True)
DEMO_DIR.mkdir(parents=True, exist_ok=True)

ALLOWED_IMG_EXTS = {".jpg", ".jpeg", ".png", ".webp", ".heic"}
ALLOWED_VID_EXTS = {".mp4", ".mov", ".m4v", ".webm"}
ALLOWED_EXTS = ALLOWED_IMG_EXTS | ALLOWED_VID_EXTS


def get_lan_ip() -> str:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


LAN_IP = get_lan_ip()
MOBILE_URL = f"http://{LAN_IP}:{PORT}"


HTML_PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>Project 2 Hardware Camera Bridge</title>
<style>
  body { font-family: system-ui, -apple-system, sans-serif; margin: 0; padding: 16px; background: #0f172a; color: #f8fafc; }
  .card { max-width: 580px; margin: 0 auto; background: #1e293b; border: 1px solid #334155; border-radius: 14px; padding: 18px; }
  h1 { font-size: 1.15rem; margin: 0 0 8px; }
  .url { background: #0f172a; padding: 10px 12px; border-radius: 8px; font-family: monospace; color: #38bdf8; word-break: break-all; margin: 8px 0 14px; font-size: 0.9rem; }
  .btn { display: block; width: 100%; box-sizing: border-box; text-align: center; background: #10b981; color: #052e16; font-weight: 700; padding: 14px; border-radius: 10px; border: 0; font-size: 1rem; cursor: pointer; margin-bottom: 10px; }
  .btn2 { background: #38bdf8; color: #082f49; }
  .btn3 { background: #f59e0b; color: #451a03; }
  input[type=file] { display: none; }
  .msg { margin: 10px 0; padding: 10px; background: #064e3b; border-radius: 8px; color: #a7f3d0; font-size: 0.9rem; }
  .busy { display: none; margin: 10px 0; padding: 10px; background: #1e3a8a; border-radius: 8px; color: #bfdbfe; font-size: 0.9rem; }
  .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-top: 12px; }
  .thumb { background: #0f172a; padding: 6px; border-radius: 8px; border: 1px solid #334155; }
  .thumb img, .thumb video { width: 100%; border-radius: 6px; display: block; }
  .thumb div { font-size: 0.72rem; color: #94a3b8; margin-top: 4px; word-break: break-all; }
</style>
<script>
  function submitForm(id) {
    var b = document.getElementById('busyMsg');
    if (b) b.style.display = 'block';
    document.getElementById(id).submit();
  }
</script>
</head>
<body>
  <div class="card">
    <h1>Project 2 (CSE 326) — iPhone Hardware Eye &amp; Live Demo</h1>
    <div>Open this URL on iPhone Safari (same Wi-Fi/Router):</div>
    <div class="url">__MOBILE_URL__</div>
    <a class="btn" style="background:#a855f7;color:#ffffff;text-decoration:none;" href="/demo">Open Real Mosquito Live Demo Audio Player</a>

    <form method="POST" action="/upload" enctype="multipart/form-data" id="camForm">
      <label class="btn" for="photoCam">Take 1 Photo (Camera)</label>
      <input id="photoCam" name="photo" type="file" accept="image/*" capture="environment" onchange="submitForm('camForm');" />
    </form>

    <form method="POST" action="/upload" enctype="multipart/form-data" id="vidForm">
      <label class="btn btn3" for="vidCam">Record Short Video (3-10s Camera)</label>
      <input id="vidCam" name="photo" type="file" accept="video/*" capture="environment" onchange="submitForm('vidForm');" />
    </form>

    <form method="POST" action="/upload" enctype="multipart/form-data" id="multiForm">
      <label class="btn btn2" for="photoMulti">Upload Photos / Videos (Gallery)</label>
      <input id="photoMulti" name="photo" type="file" accept="image/*,video/*" multiple onchange="submitForm('multiForm');" />
    </form>

    <div id="busyMsg" class="busy">Uploading to PC... please wait a few seconds.</div>
    __STATUS_HTML__
    __GALLERY_HTML__
  </div>
</body>
</html>
"""


def _guess_extension(headers_text: str) -> str:
    fn_match = re.search(r'filename="([^"]+)"', headers_text, re.IGNORECASE)
    if fn_match:
        ext = Path(fn_match.group(1)).suffix.lower()
        if ext in ALLOWED_EXTS:
            return ext
    ct_match = re.search(r"Content-Type:\s*([^\r\n;]+)", headers_text, re.IGNORECASE)
    if ct_match:
        ctype = ct_match.group(1).strip().lower()
        if ctype == "video/quicktime":
            return ".mov"
        if ctype.startswith("video/"):
            return ".mp4"
        if ctype == "image/png":
            return ".png"
        if ctype == "image/heic":
            return ".heic"
    return ".jpg"


class BridgeHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/demo" or self.path.startswith("/demo?"):
            wavs = sorted(DEMO_DIR.glob("*.wav"))
            cards = []
            labels = {
                "01_Aedes": ("Aedes aegypti (Dengue / Chikungunya)", "Expect: AEDES (DENGUE)! -> FAN ON 5.0s", "#ef4444"),
                "02_Anopheles": ("Anopheles dirus (Malaria)", "Expect: ANOPH (MALARIA) -> FAN ON 5.0s", "#f97316"),
                "03_Culex": ("Culex quinquefasciatus (Filariasis/JE)", "Expect: CULEX (FILAR.) -> FAN ON 5.0s", "#eab308"),
                "04_Pollinator": ("Pollinator Honeybee (~205 Hz)", "Expect: BEE SAFE -> FAN OFF (0%)", "#10b981"),
                "05_Male": ("Male Mosquito (~840 Hz)", "Expect: MALE IGN -> FAN OFF (0%)", "#38bdf8"),
                "06_ESC50": ("ESC-50 Car Horn + Birds + Chainsaw", "Expect: NOISE IGN -> FAN OFF (0%)", "#94a3b8"),
            }
            for w in wavs:
                title, exp, col = w.stem, "Real Peer-Reviewed Test Clip", "#38bdf8"
                for pref, info in labels.items():
                    if w.name.startswith(pref):
                        title, exp, col = info
                        break
                cards.append(
                    f'<div style="background:#0f172a;border-left:4px solid {col};padding:12px;border-radius:10px;margin-bottom:12px;">'
                    f'<div style="font-weight:700;font-size:0.98rem;">{html.escape(title)}</div>'
                    f'<div style="font-size:0.8rem;color:#cbd5e1;margin:3px 0 8px;">{html.escape(exp)}</div>'
                    f'<audio controls playsinline preload="auto" style="width:100%;" src="/demo_audio/{w.name}?v=41"></audio>'
                    f'<div style="margin-top:6px;"><a href="/demo_audio/{w.name}?v=41" download style="color:#38bdf8;font-size:0.8rem;">Download WAV (44.1 kHz)</a></div>'
                    f'</div>'
                )
            body = (
                '<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
                '<title>Project 2 Live Demo Audio Hub v4.1</title>'
                '<style>body{font-family:system-ui,-apple-system,sans-serif;margin:0;padding:16px;background:#0f172a;color:#f8fafc;}'
                '.card{max-width:580px;margin:0 auto;background:#1e293b;border:1px solid #334155;border-radius:14px;padding:18px;}</style></head>'
                '<body><div class="card"><a href="/" style="color:#38bdf8;text-decoration:none;font-size:0.9rem;">&larr; Back to Camera Upload</a>'
                '<h1 style="font-size:1.15rem;margin:10px 0 6px;">WingBeat-TinyNet v4.1 — Real Q1 Demo Audio (44.1 kHz)</h1>'
                '<p style="font-size:0.82rem;color:#94a3b8;margin:0 0 14px;">Hold iPhone speaker 5–10 cm from INMP441 mic (75–90% volume) and tap Play:</p>'
                + "".join(cards)
                + '</div></body></html>'
            ).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if self.path.startswith("/demo_audio/"):
            fname = Path(self.path.split("?")[0].replace("/demo_audio/", "", 1)).name
            target = DEMO_DIR / fname
            if target.exists() and target.is_file():
                data = target.read_bytes()
                total = len(data)
                rng_hdr = self.headers.get("Range")
                if rng_hdr and rng_hdr.startswith("bytes="):
                    try:
                        part = rng_hdr.split("=", 1)[1].split(",")[0].strip()
                        s_str, e_str = part.split("-", 1)
                        start = int(s_str) if s_str else 0
                        end = int(e_str) if e_str else total - 1
                        end = min(end, total - 1)
                        chunk = data[start:end + 1]
                        self.send_response(206)
                        self.send_header("Content-Type", "audio/wav")
                        self.send_header("Accept-Ranges", "bytes")
                        self.send_header("Cache-Control", "no-store")
                        self.send_header("Content-Range", f"bytes {start}-{end}/{total}")
                        self.send_header("Content-Length", str(len(chunk)))
                        self.end_headers()
                        self.wfile.write(chunk)
                        return
                    except Exception:
                        pass
                self.send_response(200)
                self.send_header("Content-Type", "audio/wav")
                self.send_header("Accept-Ranges", "bytes")
                self.send_header("Cache-Control", "no-store")
                self.send_header("Content-Length", str(total))
                self.end_headers()
                self.wfile.write(data)
                return
            self.send_error(404)
            return

        if self.path.startswith("/img/") or self.path.startswith("/media/"):
            prefix = "/img/" if self.path.startswith("/img/") else "/media/"
            fname = Path(self.path.split("?")[0].replace(prefix, "", 1)).name
            target = CAPTURE_DIR / fname
            if target.exists() and target.is_file():
                data = target.read_bytes()
                ctype, _ = mimetypes.guess_type(target.name)
                if not ctype:
                    ctype = "video/quicktime" if target.suffix.lower() == ".mov" else "application/octet-stream"
                self.send_response(200)
                self.send_header("Content-Type", ctype)
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)
                return
            self.send_error(404)
            return

        status = ""
        if self.path.startswith("/?ok="):
            cnt = html.escape(self.path.split("=", 1)[1])
            status = f'<div class="msg">Saved {cnt} file(s)! Tell Antigravity in chat ("deko" / "check") to inspect.</div>'

        files = sorted(
            [
                p
                for p in CAPTURE_DIR.iterdir()
                if p.is_file() and p.name.startswith("capture_") and p.suffix.lower() in ALLOWED_EXTS
            ],
            key=lambda p: p.stat().st_mtime,
            reverse=True,
        )[:6]
        gallery = ""
        if files:
            items = []
            for f in files:
                if f.suffix.lower() in ALLOWED_VID_EXTS:
                    media_tag = f'<video src="/media/{f.name}" controls playsinline preload="none"></video>'
                else:
                    media_tag = f'<img src="/media/{f.name}" alt="{f.name}" />'
                items.append(f'<div class="thumb">{media_tag}<div>{f.name}</div></div>')
            gallery = f'<div class="grid">{"".join(items)}</div>'

        page = (
            HTML_PAGE.replace("__MOBILE_URL__", MOBILE_URL)
            .replace("__STATUS_HTML__", status)
            .replace("__GALLERY_HTML__", gallery)
        ).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(page)))
        self.end_headers()
        self.wfile.write(page)

    def do_POST(self):
        if self.path != "/upload":
            self.send_error(404)
            return
        ctype = self.headers.get("Content-Type", "")
        m = re.search(r"boundary=([^;]+)", ctype)
        if not m:
            self.send_error(400, "Missing multipart boundary")
            return
        boundary = m.group(1).strip().strip('"').encode("utf-8")
        length = int(self.headers.get("Content-Length", "0"))
        chunks = []
        rem = length
        while rem > 0:
            chunk = self.rfile.read(min(rem, 65536))
            if not chunk:
                break
            chunks.append(chunk)
            rem -= len(chunk)
        body = b"".join(chunks)
        if len(body) < length:
            print(f"[WARN] Incomplete upload rejected ({len(body)}/{length} B)", flush=True)
            self.send_error(400, "Incomplete upload - please wait until upload finishes")
            return
        parts = body.split(b"--" + boundary)

        ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        saved = []
        idx = 1
        for part in parts:
            if b'name="photo"' in part and b"\r\n\r\n" in part:
                raw_headers, payload = part.split(b"\r\n\r\n", 1)
                if payload.endswith(b"\r\n"):
                    payload = payload[:-2]
                if payload and len(payload) > 100:
                    ext = _guess_extension(raw_headers.decode("utf-8", errors="replace"))
                    saved_name = f"capture_{ts}_{idx:02d}{ext}"
                    (CAPTURE_DIR / saved_name).write_bytes(payload)
                    if ext in ALLOWED_VID_EXTS:
                        (CAPTURE_DIR / f"latest_video{ext}").write_bytes(payload)
                    else:
                        (CAPTURE_DIR / "latest.jpg").write_bytes(payload)
                    saved.append(saved_name)
                    print(f"[CAPTURED] {CAPTURE_DIR / saved_name}", flush=True)
                    idx += 1

        if not saved:
            self.send_error(400, "No image or video received")
            return
        self.send_response(303)
        self.send_header("Location", f"/?ok={len(saved)}")
        self.end_headers()


class ReusableTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == "__main__":
    print("=== Project 2 Camera & Video Bridge Running ===", flush=True)
    print(f"Open on iPhone Safari (same router/Wi-Fi): {MOBILE_URL}", flush=True)
    print(f"Captures saved to: {CAPTURE_DIR}", flush=True)
    with ReusableTCPServer(("0.0.0.0", PORT), BridgeHandler) as httpd:
        httpd.serve_forever()
