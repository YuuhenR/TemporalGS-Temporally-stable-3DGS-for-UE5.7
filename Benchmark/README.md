# TemporalGS — Evaluation harness (Phase 5)

The whole point of TemporalGS is **temporal stability under TSR/TAA**. This harness quantifies the
ghosting-vs-fixed difference that the velocity write produces.

## The core experiment (before / after)

1. Open the project, `TemporalGS.Load` a scene, enable TSR: `r.AntiAliasingMethod 4`.
2. Pick a **repeatable camera motion** (a Sequencer/Movie-Render-Queue camera path is ideal; a fixed
   orbit also works). The path must be identical for both captures.
3. Capture a PNG frame sequence twice along that path:
   - **baseline:** `r.TemporalGS.WriteVelocity 0` → splats ghost/smear under motion.
   - **fixed:** `r.TemporalGS.WriteVelocity 1` → splats stay crisp.
   Use Movie Render Queue (PNG sequence) or `HighResShot` per frame into two folders.
4. Compare:
   ```
   python metrics.py --baseline path\to\baseline --fixed path\to\fixed
   ```
   Expected: **lower temporal_flicker for `fixed`**, reported as a "flicker reduction %".

## Other knobs worth sweeping

- `r.TemporalGS.Sigma`, `r.TemporalGS.OpacityScale` — visual density.
- `r.TemporalGS.Occlude 0/1` — scene-depth occlusion on/off.
- `r.TemporalGS.Mode 0/1` — disc vs anisotropic conic.
- `r.TemporalGS.Debug 1` — solid-magenta visibility test.

## Metrics

- **temporal_flicker** — mean frame-to-frame luma difference; the primary temporal-stability number.
- **PSNR / SSIM** vs a high-quality reference sequence (pass `--ref`), if you render a slow
  super-sampled ground truth along the same path.

## Reproducible reference (optional)

For a ground-truth-style reference, render the same path at high resolution with TSR off and heavy
super-sampling (`r.ScreenPercentage 200`), then pass it via `--ref`.

Dependencies: `pip install numpy pillow` (and optionally `scikit-image` for SSIM).
