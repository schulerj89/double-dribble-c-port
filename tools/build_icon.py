#!/usr/bin/env python3
"""Build the repository's multi-resolution Windows icon from user-supplied art."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


ICON_SIZES = (16, 24, 32, 48, 64, 128, 256)


def build_icon(source_path: Path, output_path: Path) -> None:
    source = Image.open(source_path).convert("RGBA")
    background = source.getpixel((0, 0))
    size = ICON_SIZES[-1]
    canvas = Image.new("RGBA", (size, size), background)
    content_height = max(1, round(size * source.height / source.width))
    resized = source.resize((size, content_height), Image.Resampling.NEAREST)
    canvas.alpha_composite(resized, (0, (size - content_height) // 2))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(
        output_path,
        format="ICO",
        sizes=[(size, size) for size in ICON_SIZES],
        bitmap_format="png",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path, help="User-supplied source PNG")
    parser.add_argument("output", type=Path, help="Destination .ico")
    arguments = parser.parse_args()
    build_icon(arguments.source, arguments.output)


if __name__ == "__main__":
    main()
