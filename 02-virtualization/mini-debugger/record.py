# Render animation.html frame by frame into animation.gif
# run: docker run --rm -v "$PWD":/w -w /w mcr.microsoft.com/playwright/python:v1.47.0-jammy \
#        bash -c "pip install -q playwright==1.47.0 pillow && python3 record.py"
import io
from pathlib import Path
from PIL import Image
from playwright.sync_api import sync_playwright

FPS = 12
HOLD_MS = 1800  # pause on each step after the last hop arrives

html = Path(__file__).with_name("animation.html").resolve().as_uri()
frames, durations = [], []

with sync_playwright() as p:
    page = p.chromium.launch().new_page(viewport={"width": 720, "height": 560}, device_scale_factor=1)
    page.goto(html)
    steps = page.evaluate("capture.steps")
    hop_ms = page.evaluate("capture.HOP_MS")
    for n in range(steps):
        move_ms = page.evaluate(f"capture.hops({n})") * hop_ms
        for ms in range(0, move_ms, 1000 // FPS):
            page.evaluate(f"capture.frame({n}, {ms})")
            frames.append(Image.open(io.BytesIO(page.locator("main").screenshot())).convert("RGB"))
            durations.append(1000 // FPS)
        page.evaluate(f"capture.frame({n}, {move_ms})")
        frames.append(Image.open(io.BytesIO(page.locator("main").screenshot())).convert("RGB"))
        durations.append(HOLD_MS)

out = [f.quantize(colors=64, dither=Image.Dither.NONE) for f in frames]
out[0].save("animation.gif", save_all=True, append_images=out[1:], duration=durations, loop=0, optimize=True)
print(f"{len(out)} frames -> animation.gif")
