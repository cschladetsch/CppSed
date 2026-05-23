#!/usr/bin/env python3
from __future__ import annotations

import csv
import html
import math
import sys
from pathlib import Path


def fmt_seconds(value: float) -> str:
    return f"{value:.4f}s"


def fmt_ratio(value: float) -> str:
    return f"{value:.3f}x"


def escape(text: str) -> str:
    return html.escape(text, quote=True)


def ratio_color(ratio: float) -> str:
    if ratio < 0.95:
        return "#1f9d55"
    if ratio > 1.05:
        return "#c0392b"
    return "#9a7d0a"


def ratio_label(ratio: float) -> str:
    if ratio < 1.0:
        return f"{(1.0 / ratio):.2f}x faster"
    return f"{ratio:.2f}x slower"


def bar_width(ratio: float, max_ratio: float) -> float:
    return max(8.0, 280.0 * ratio / max_ratio)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: render_chart.py INPUT.csv OUTPUT.svg", file=sys.stderr)
        return 1

    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])

    with input_path.open(newline="", encoding="utf-8") as fh:
        rows = list(csv.DictReader(fh))

    if not rows:
        raise SystemExit("no benchmark rows found")

    parsed = []
    for row in rows:
        ratio = float(row["ratio"])
        parsed.append(
            {
                "name": row["name"],
                "fastsed_avg": float(row["fastsed_avg"]),
                "fastsed_best": float(row["fastsed_best"]),
                "sed_avg": float(row["sed_avg"]),
                "sed_best": float(row["sed_best"]),
                "ratio": ratio,
            }
        )

    max_ratio = max(item["ratio"] for item in parsed)
    max_ratio = max(max_ratio, 1.0)

    width = 1640
    header_h = 132
    row_h = 52
    footer_h = 56
    height = header_h + row_h * len(parsed) + footer_h

    parts: list[str] = []
    parts.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">')
    parts.append('<rect width="100%" height="100%" fill="#f4f1ea"/>')
    parts.append('<rect x="28" y="24" width="1584" height="84" rx="18" fill="#1f2933"/>')
    parts.append('<text x="56" y="60" fill="#f8fafc" font-family="DejaVu Sans, Arial, sans-serif" font-size="34" font-weight="700">fastsed vs GNU sed</text>')
    parts.append('<text x="56" y="92" fill="#cbd5e1" font-family="DejaVu Sans, Arial, sans-serif" font-size="18">Average wall-clock time across benchmark cases. Ratio is fastsed / sed.</text>')

    columns = {
        "name": 52,
        "fastsed": 420,
        "sed": 620,
        "ratio_text": 810,
        "ratio_bar": 1030,
    }

    y0 = header_h
    parts.append(f'<rect x="36" y="{y0}" width="1568" height="38" rx="10" fill="#d9e2ec"/>')
    parts.append(f'<text x="{columns["name"]}" y="{y0 + 25}" fill="#102a43" font-family="DejaVu Sans, Arial, sans-serif" font-size="18" font-weight="700">Benchmark</text>')
    parts.append(f'<text x="{columns["fastsed"]}" y="{y0 + 25}" fill="#102a43" font-family="DejaVu Sans, Arial, sans-serif" font-size="18" font-weight="700">fastsed avg/best</text>')
    parts.append(f'<text x="{columns["sed"]}" y="{y0 + 25}" fill="#102a43" font-family="DejaVu Sans, Arial, sans-serif" font-size="18" font-weight="700">GNU sed avg/best</text>')
    parts.append(f'<text x="{columns["ratio_text"]}" y="{y0 + 25}" fill="#102a43" font-family="DejaVu Sans, Arial, sans-serif" font-size="18" font-weight="700">Ratio</text>')
    parts.append(f'<text x="{columns["ratio_bar"]}" y="{y0 + 25}" fill="#102a43" font-family="DejaVu Sans, Arial, sans-serif" font-size="18" font-weight="700">Comparison</text>')

    for idx, item in enumerate(parsed):
        y = y0 + 42 + idx * row_h
        bg = "#fffdf8" if idx % 2 == 0 else "#f8f6f1"
        color = ratio_color(item["ratio"])
        label = ratio_label(item["ratio"])
        width_px = bar_width(item["ratio"], max_ratio)

        parts.append(f'<rect x="36" y="{y - 28}" width="1568" height="48" rx="8" fill="{bg}"/>')
        parts.append(f'<text x="{columns["name"]}" y="{y}" fill="#102a43" font-family="DejaVu Sans, Arial, sans-serif" font-size="18">{escape(item["name"])}</text>')
        parts.append(
            f'<text x="{columns["fastsed"]}" y="{y}" fill="#102a43" font-family="DejaVu Sans, Arial, sans-serif" font-size="18">{fmt_seconds(item["fastsed_avg"])} / {fmt_seconds(item["fastsed_best"])}</text>'
        )
        parts.append(
            f'<text x="{columns["sed"]}" y="{y}" fill="#102a43" font-family="DejaVu Sans, Arial, sans-serif" font-size="18">{fmt_seconds(item["sed_avg"])} / {fmt_seconds(item["sed_best"])}</text>'
        )
        parts.append(f'<text x="{columns["ratio_text"]}" y="{y}" fill="{color}" font-family="DejaVu Sans, Arial, sans-serif" font-size="18" font-weight="700">{fmt_ratio(item["ratio"])}</text>')
        parts.append(f'<rect x="{columns["ratio_bar"]}" y="{y - 16}" width="300" height="22" rx="11" fill="#e5e7eb"/>')
        parts.append(f'<rect x="{columns["ratio_bar"]}" y="{y - 16}" width="{width_px:.2f}" height="22" rx="11" fill="{color}"/>')
        parts.append(f'<text x="{columns["ratio_bar"] + 316}" y="{y}" fill="#334e68" font-family="DejaVu Sans, Arial, sans-serif" font-size="17">{escape(label)}</text>')

    footer_y = height - 18
    parts.append(f'<text x="40" y="{footer_y}" fill="#52606d" font-family="DejaVu Sans, Arial, sans-serif" font-size="16">Green rows favor fastsed. Red rows favor GNU sed.</text>')
    parts.append('</svg>')

    output_path.write_text("\n".join(parts), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
