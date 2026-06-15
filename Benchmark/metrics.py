#!/usr/bin/env python3
"""
TemporalGS evaluation harness (Phase 5).

The TemporalGS contribution is *temporal stability* of splats under TSR/TAA. The most important
metric is therefore how much a rendered sequence flickers / ghosts under motion, and how much the
velocity write reduces it. This script quantifies that.

Capture workflow (see Benchmark/README.md):
  1. In the editor, set a fixed camera fly-through (or use `stat unit` + manual orbit), enable TSR
     (r.AntiAliasingMethod 4), load a splat (TemporalGS.Load).
  2. Record a frame sequence (PNG) twice along the SAME path:
       - baseline:  r.TemporalGS.WriteVelocity 0   -> splats ghost under motion
       - fixed:     r.TemporalGS.WriteVelocity 1   -> splats stay crisp
     e.g. with `HighResShot 1920x1080` per step, or a Movie Render Queue PNG sequence.
  3. Run:
       python metrics.py --baseline <dir> --fixed <dir>

Metrics:
  * temporal_flicker : mean over frames of mean|luma(f_i) - luma(f_{i-1})|  (lower = more stable)
  * psnr/ssim        : vs a high-quality reference sequence, if --ref is given (per-frame mean)

Dependencies: numpy, pillow.  (ssim/lpips optional via scikit-image if installed.)
"""

from __future__ import annotations
import argparse
import sys
from pathlib import Path

import numpy as np

try:
    from PIL import Image
except ImportError:
    sys.exit("Please `pip install pillow numpy` first.")

try:
    from skimage.metrics import structural_similarity as _ssim  # type: ignore
    HAVE_SKIMAGE = True
except Exception:
    HAVE_SKIMAGE = False


IMG_EXTS = (".png", ".jpg", ".jpeg", ".bmp", ".exr")


def list_frames(folder: Path) -> list[Path]:
    files = [p for p in sorted(folder.iterdir()) if p.suffix.lower() in IMG_EXTS]
    if not files:
        raise SystemExit(f"No image frames found in {folder}")
    return files


def load_gray(path: Path) -> np.ndarray:
    img = np.asarray(Image.open(path).convert("RGB"), dtype=np.float32) / 255.0
    # Rec.709 luma
    return img @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)


def load_rgb(path: Path) -> np.ndarray:
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float32) / 255.0


def temporal_flicker(folder: Path) -> float:
    """Mean frame-to-frame luma difference. Lower = more temporally stable."""
    frames = list_frames(folder)
    prev = load_gray(frames[0])
    total, n = 0.0, 0
    for f in frames[1:]:
        cur = load_gray(f)
        if cur.shape != prev.shape:
            raise SystemExit(f"Frame size mismatch at {f.name}")
        total += float(np.mean(np.abs(cur - prev)))
        n += 1
        prev = cur
    return total / max(n, 1)


def psnr(a: np.ndarray, b: np.ndarray) -> float:
    mse = float(np.mean((a - b) ** 2))
    return 99.0 if mse <= 1e-12 else 10.0 * np.log10(1.0 / mse)


def quality_vs_ref(folder: Path, ref: Path) -> tuple[float, float | None]:
    fa, fb = list_frames(folder), list_frames(ref)
    n = min(len(fa), len(fb))
    ps, ss = [], []
    for i in range(n):
        a, b = load_rgb(fa[i]), load_rgb(fb[i])
        if a.shape != b.shape:
            continue
        ps.append(psnr(a, b))
        if HAVE_SKIMAGE:
            ss.append(float(_ssim(a, b, channel_axis=2, data_range=1.0)))
    mean_psnr = float(np.mean(ps)) if ps else float("nan")
    mean_ssim = float(np.mean(ss)) if ss else None
    return mean_psnr, mean_ssim


def main() -> int:
    ap = argparse.ArgumentParser(description="TemporalGS temporal-stability benchmark.")
    ap.add_argument("--frames", type=Path, help="A single frame-sequence directory.")
    ap.add_argument("--baseline", type=Path, help="WriteVelocity=0 sequence (ghosting baseline).")
    ap.add_argument("--fixed", type=Path, help="WriteVelocity=1 sequence (temporal fix).")
    ap.add_argument("--ref", type=Path, help="High-quality reference sequence for PSNR/SSIM.")
    args = ap.parse_args()

    if args.baseline and args.fixed:
        fb = temporal_flicker(args.baseline)
        ff = temporal_flicker(args.fixed)
        print("=== TemporalGS: temporal flicker (lower = more stable) ===")
        print(f"  baseline (WriteVelocity 0): {fb:.5f}")
        print(f"  fixed    (WriteVelocity 1): {ff:.5f}")
        if fb > 1e-9:
            print(f"  flicker reduction:          {100.0 * (fb - ff) / fb:+.1f}%")
        for label, d in (("baseline", args.baseline), ("fixed", args.fixed)):
            if args.ref:
                p, s = quality_vs_ref(d, args.ref)
                extra = f", SSIM {s:.4f}" if s is not None else " (install scikit-image for SSIM)"
                print(f"  {label} quality vs ref: PSNR {p:.2f} dB{extra}")
        return 0

    if args.frames:
        print(f"temporal_flicker({args.frames.name}) = {temporal_flicker(args.frames):.5f}")
        if args.ref:
            p, s = quality_vs_ref(args.frames, args.ref)
            print(f"PSNR {p:.2f} dB" + (f", SSIM {s:.4f}" if s is not None else ""))
        return 0

    ap.print_help()
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
