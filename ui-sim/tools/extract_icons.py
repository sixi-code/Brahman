# -*- coding: utf-8 -*-
"""
extract_icons.py —— 从 Brahman 固件的 LVGL 位图资源 (*.c) 中提取真实图标并导出 PNG。

支持格式：LV_IMG_CF_ALPHA_1BIT / ALPHA_4BIT / TRUE_COLOR / TRUE_COLOR_ALPHA
(color depth = 16, LV_COLOR_16_SWAP = 0)

alpha 类图标导出为「黑色 + alpha 通道」的 RGBA PNG，前端用 CSS mask-image 重着色，
与固件里 lv_obj_set_style_img_recolor() 的行为一致。

用法：python tools/extract_icons.py
"""
import os
import re
import sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC_DIR = os.path.join(os.path.dirname(ROOT), "ware", "GUI", "GUI_Icons")
OUT_DIR = os.path.join(ROOT, "assets", "icons")

RE_DSC = re.compile(
    r"const\s+lv_img_dsc_t\s+(\w+)\s*=\s*\{(.*?)\n\};", re.S
)
RE_CF = re.compile(r"\.header\.cf\s*=\s*(LV_IMG_CF_\w+)")
RE_W = re.compile(r"\.header\.w\s*=\s*(\d+)")
RE_H = re.compile(r"\.header\.h\s*=\s*(\d+)")
RE_DATA = re.compile(r"\.data\s*=\s*(\w+)")
RE_ARRAY = re.compile(r"(?:\w[\w\s]*?)\b(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\};", re.S)


def parse_bytes(block: str):
    """把 C 数组里的 0x.. 十六进制字面量解析成 bytes。"""
    out = bytearray()
    for tok in re.findall(r"0[xX]([0-9a-fA-F]{1,2})", block):
        out.append(int(tok, 16))
    return bytes(out)


def decode_alpha1(data, w, h):
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = img.load()
    stride = (w + 7) // 8
    for y in range(h):
        for x in range(w):
            idx = y * stride + (x >> 3)
            if idx >= len(data):
                continue
            if (data[idx] >> (7 - (x & 7))) & 1:
                px[x, y] = (0, 0, 0, 255)
    return img


def decode_alpha4(data, w, h):
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = img.load()
    stride = (w + 1) // 2
    for y in range(h):
        for x in range(w):
            idx = y * stride + (x >> 1)
            if idx >= len(data):
                continue
            b = data[idx]
            v = (b >> 4) if (x & 1) == 0 else (b & 0x0F)
            px[x, y] = (0, 0, 0, v * 17)
    return img


def _rgb565(v):
    r = ((v >> 11) & 0x1F) * 255 // 31
    g = ((v >> 5) & 0x3F) * 255 // 63
    b = (v & 0x1F) * 255 // 31
    return r, g, b


def decode_true(data, w, h, alpha=False):
    mode = "RGBA" if alpha else "RGB"
    img = Image.new(mode, (w, h))
    px = img.load()
    step = 3 if alpha else 2
    for y in range(h):
        for x in range(w):
            i = (y * w + x) * step
            if i + 1 >= len(data):
                continue
            v = data[i] | (data[i + 1] << 8)   # little-endian RGB565
            c = _rgb565(v)
            if alpha:
                a = data[i + 2] if i + 2 < len(data) else 255
                px[x, y] = (c[0], c[1], c[2], a)
            else:
                px[x, y] = c
    return img


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    total = 0
    manifest = []

    for fname in sorted(os.listdir(SRC_DIR)):
        if not fname.endswith(".c"):
            continue
        path = os.path.join(SRC_DIR, fname)
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            src = f.read()

        # 先建 数组名 -> bytes 映射
        arrays = {m.group(1): parse_bytes(m.group(2)) for m in RE_ARRAY.finditer(src)}

        for m in RE_DSC.finditer(src):
            name, body = m.group(1), m.group(2)
            cf = RE_CF.search(body)
            w = RE_W.search(body)
            h = RE_H.search(body)
            d = RE_DATA.search(body)
            if not (cf and w and h and d):
                continue
            cf, w, h = cf.group(1), int(w.group(1)), int(h.group(1))
            arr = d.group(1)
            data = arrays.get(arr, b"")
            if not data:
                print("  ! 数据缺失:", name, arr)
                continue

            if cf == "LV_IMG_CF_ALPHA_1BIT":
                img = decode_alpha1(data, w, h)
            elif cf == "LV_IMG_CF_ALPHA_4BIT":
                img = decode_alpha4(data, w, h)
            elif cf == "LV_IMG_CF_TRUE_COLOR":
                img = decode_true(data, w, h, alpha=False)
            elif cf == "LV_IMG_CF_TRUE_COLOR_ALPHA":
                img = decode_true(data, w, h, alpha=True)
            else:
                print("  ? 未支持格式:", name, cf)
                continue

            out = os.path.join(OUT_DIR, name + ".png")
            img.save(out)
            manifest.append({"name": name, "file": name + ".png",
                             "w": w, "h": h, "cf": cf, "src": fname})
            total += 1

    # 生成 JS 清单，前端可直接读
    js = "// 由 tools/extract_icons.py 自动生成 —— 固件位图资源清单\n"
    js += "export const ICON_MANIFEST = " + \
        __import__("json").dumps(manifest, ensure_ascii=False, indent=1) + ";\n"
    with open(os.path.join(OUT_DIR, "manifest.js"), "w", encoding="utf-8") as f:
        f.write(js)

    print(f"共导出 {total} 个图标 -> {OUT_DIR}")
    for it in manifest:
        print(f"  {it['name']:<20} {it['w']:>4}x{it['h']:<4} {it['cf']}")


if __name__ == "__main__":
    sys.exit(main())
