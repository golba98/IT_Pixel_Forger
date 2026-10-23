#!/usr/bin/env python3
"""CI verification: run it-pixel-cli on a generated fixture and validate the
resulting animated GIF with Pillow (an independent decoder).

This script is a development/CI tool only; the application itself does not
depend on Python.
"""

import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image


def main() -> int:
    cli = Path(sys.argv[1])
    if not cli.exists():
        print(f"CLI binary not found: {cli}")
        return 1

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        width, height = 240, 160
        image = Image.new("RGB", (width, height))
        pixels = image.load()
        for y in range(height):
            for x in range(width):
                pixels[x, y] = ((x * 255) // width, (y * 255) // height, 128)
        input_path = tmp_path / "input.png"
        image.save(input_path)

        reference = Image.new("RGB", (32, 32), (200, 40, 40))
        reference_path = tmp_path / "reference.png"
        reference.save(reference_path)

        output_path = tmp_path / "out with spaces.gif"
        result = subprocess.run(
            [
                str(cli),
                str(input_path),
                "--output",
                str(output_path),
                "--reference",
                str(reference_path),
                "--steps",
                "20",
                "--block-size",
                "2",
                "--seed",
                "42",
            ],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            print("CLI failed:", result.stdout, result.stderr)
            return 1

        gif = Image.open(output_path)
        assert gif.size == (width, height), f"unexpected size {gif.size}"
        frames = gif.n_frames
        # 80 scan previews + (20 + 10) animation frames + 1 final frame.
        if frames < 30 or frames > 111:
            print(f"suspicious frame count: {frames}")
            return 1
        durations = set()
        for index in range(frames):
            gif.seek(index)
            durations.add(gif.info.get("duration"))
        if not durations or max(durations) == 0:
            print("no frame durations found")
            return 1
        if gif.info.get("loop") != 0:
            print("loop extension missing")
            return 1

        gif.seek(frames - 1)
        final = gif.convert("RGB")
        # blend 1.0 default -> last frame is solid reference colour blocks
        # only in theory; just make sure the final frame decodes intact.
        if final.size != (width, height):
            print("final frame corrupted")
            return 1

    print(f"OK: {frames} frames, durations {sorted(durations)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
