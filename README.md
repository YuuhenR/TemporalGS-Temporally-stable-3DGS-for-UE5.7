# TemporalGS

English · [中文](README.zh-CN.md)

A UE5.7 plugin that brings 3D Gaussian Splatting into the engine's TSR/TAA pipeline and fixes the ghosting you get when the camera moves.

3DGS renders fine on its own, but inside UE it ends up drawn as a translucent object that doesn't write motion vectors (velocity), so TSR smears it during temporal accumulation. This plugin implements its own splat rasterizer: it draws the splats into scene color before TSR, writes per-pixel velocity for them, and depth-tests them against the scene for occlusion, so TSR can reproject them correctly instead of leaving trails. The rasterizer is written from scratch — nothing is forked.

## Usage

The repo is itself a UE5.7 project. Open `TemporalGS.uproject` with UE5.7 (it will offer to compile C++ on first open). Put an INRIA-format `.ply` in `Content/Splats/`, then in the console:

```
TemporalGS.Load                 load + auto-fit to view
r.AntiAliasingMethod 4          enable TSR
r.TemporalGS.WriteVelocity 0    velocity off (ghosting)
r.TemporalGS.WriteVelocity 1    velocity on  (crisp)
```

Other parameters: `Sigma` (splat size), `OpacityScale` (density), `Occlude` (scene occlusion), `Mode`, `Debug` — full list in `docs/指令速查.md`.

If you only need the plugin, copy `Plugins/TemporalGS` into your own C++ project's `Plugins/` folder.

## Notes

- On a static scene with a textured background the velocity makes little visible difference — the background already writes the camera's motion vectors and the splats inherit them. It shows up against sky, under fast motion, and with dynamic content.
- The dynamic / 4D path is in place but needs a 4D dataset to actually run.
- Sorting currently runs on the CPU each frame, which costs some CPU on large scenes. There are persistent buffers, a radix sort, and the sort is skipped while the camera is still.

## Test data

`.ply` files are large and not included in the repo. Download one from [INRIA](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/) or [Voxel51/gaussian_splatting](https://huggingface.co/datasets/Voxel51/gaussian_splatting) on HuggingFace and put it in `Content/Splats/`.

## Requirements

UE5.7, tested on Windows / DX12. MIT licensed.
