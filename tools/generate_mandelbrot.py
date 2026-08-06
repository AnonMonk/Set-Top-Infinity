#!/usr/bin/env python3

import os
import struct
import zlib

import numpy as np


WIDTH = 768
HEIGHT = 432
LEVELS = 20
CENTER_X = -0.658448
CENTER_Y = -0.466852
START_SPAN = 3.2
ZOOM_FACTOR = 0.64


def png_chunk(kind, data):
    return (
        struct.pack(">I", len(data))
        + kind
        + data
        + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    )


def write_png(path, rgb):
    rows = [b"\x00" + rgb[y].tobytes() for y in range(rgb.shape[0])]
    raw = b"".join(rows)
    header = struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0)
    data = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(raw, 9))
        + png_chunk(b"IEND", b"")
    )
    with open(path, "wb") as output:
        output.write(data)


def render_level(level):
    span_x = START_SPAN * (ZOOM_FACTOR ** level)
    span_y = span_x * HEIGHT / WIDTH
    xs = np.linspace(
        CENTER_X - span_x * 0.5,
        CENTER_X + span_x * 0.5,
        WIDTH,
        dtype=np.float64,
    )
    ys = np.linspace(
        CENTER_Y - span_y * 0.5,
        CENTER_Y + span_y * 0.5,
        HEIGHT,
        dtype=np.float64,
    )
    c = xs[np.newaxis, :] + 1j * ys[:, np.newaxis]
    z = np.zeros(c.shape, dtype=np.complex128)
    active = np.ones(c.shape, dtype=bool)
    smooth = np.zeros(c.shape, dtype=np.float64)
    max_iterations = 145 + level * 13

    for iteration in range(max_iterations):
        z[active] = z[active] * z[active] + c[active]
        magnitude = z.real * z.real + z.imag * z.imag
        escaped = active & (magnitude > 4.0)
        if np.any(escaped):
            absolute = np.sqrt(magnitude[escaped])
            smooth[escaped] = (
                iteration
                + 1
                - np.log2(np.maximum(1.0, np.log(absolute)))
            )
            active[escaped] = False
        if not np.any(active):
            break

    # Ruhige, durchgehend blau-cyanfarbene Palette statt Regenbogenfarben.
    wave = 0.5 + 0.5 * np.cos(smooth * 0.36)
    red = 0.035 + 0.16 * wave
    green = 0.10 + 0.52 * wave
    blue = 0.28 + 0.72 * wave
    brightness = np.clip(smooth / 18.0, 0.18, 1.0)

    rgb = np.stack(
        [red * brightness, green * brightness, blue * brightness],
        axis=2,
    )
    rgb[active] = 0.0
    return np.asarray(np.clip(rgb * 255.0, 0, 255), dtype=np.uint8)


def main():
    output_dir = os.path.join("assets", "mandelbrot")
    os.makedirs(output_dir, exist_ok=True)
    for level in range(LEVELS):
        path = os.path.join(output_dir, "Mandelbrot%02d.png" % level)
        print("Generating", path)
        write_png(path, render_level(level))


if __name__ == "__main__":
    main()
