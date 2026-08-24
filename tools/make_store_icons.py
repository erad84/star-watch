#!/usr/bin/env python3
"""Scale the watch menu icon to Rebble/Pebble store sizes."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "resources" / "images" / "menu_icon.png"
OUT_DIR = ROOT / "store"


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"not a PNG: {path}")
    w = int.from_bytes(data[16:20], "big")
    h = int.from_bytes(data[20:24], "big")
    return w, h


def main() -> None:
    try:
        from PIL import Image
    except ImportError as exc:
        raise SystemExit("Pillow is required: pip install Pillow") from exc

    if not SRC.exists():
        raise SystemExit(f"missing {SRC}")

    src_w, src_h = png_size(SRC)
    print(f"source {SRC} {src_w}x{src_h}")
    im = Image.open(SRC).convert("RGBA")
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for size, name in ((80, "icon-80.png"), (144, "icon-144.png")):
        factor = max(1, size // max(src_w, src_h))
        stepped = im.resize((src_w * factor, src_h * factor), Image.Resampling.NEAREST)
        canvas = Image.new("RGBA", (size, size), (0, 0, 0, 255))
        ox = (size - stepped.width) // 2
        oy = (size - stepped.height) // 2
        canvas.paste(stepped, (ox, oy))
        out = OUT_DIR / name
        canvas.save(out, "PNG")
        w, h = png_size(out)
        print(f"wrote {out} {w}x{h} (x{factor} then centered)")


if __name__ == "__main__":
    main()
