# TemporalGS

[English](README.md) · 中文

一个 UE5.7 插件,把 3D Gaussian Splatting 接入引擎的 TSR/TAA,解决 splat 在相机运动时的拖影问题。

3DGS 本身渲染没问题,但放进 UE 后会被当作半透明物体绘制,不写入运动矢量(velocity),于是 TSR 在做时序累积时就会糊。这个插件实现了一套自己的 splat 光栅化器:在 TSR 之前把 splat 绘制进 scene color,为其写入逐像素 velocity,并与场景深度做遮挡测试,使 TSR 能够正确地重投影,而不是留下残影。光栅化部分从零编写,没有 fork 任何现成渲染器。

## 使用

仓库本身就是一个 UE5.7 工程。用 UE5.7 打开 `TemporalGS.uproject`(首次打开会提示编译 C++)。准备一个 INRIA 格式的 `.ply` 放到 `Content/Splats/`,然后在控制台执行:

```
TemporalGS.Load                 加载并自动缩放显示
r.AntiAliasingMethod 4          开启 TSR
r.TemporalGS.WriteVelocity 0    关闭 velocity(拖影)
r.TemporalGS.WriteVelocity 1    开启 velocity(清晰)
```

其它参数:`Sigma`(splat 大小)、`OpacityScale`(密度)、`Occlude`(场景遮挡)、`Mode`、`Debug`,完整列表见 `docs/指令速查.md`。

如果只需要插件,把 `Plugins/TemporalGS` 拷贝到你自己 C++ 工程的 `Plugins/` 目录即可。

## 说明

- 在静态场景、且背景有纹理时,velocity 带来的差别并不明显——因为背景本身已经把相机的运动矢量写入了缓冲,splat 会沿用背景的速度。它在天空背景、快速运动以及动态内容下才会明显。
- 动态 / 4D 的接口已经预留,但需要 4D 数据集才能实际运行。
- 排序目前在 CPU 上逐帧进行,大场景会有一定的 CPU 开销。已经做了持久化缓冲、radix 排序,以及相机静止时跳过排序的优化。

## 测试数据

`.ply` 文件较大,未包含在仓库中。可从 [INRIA](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/) 或 HuggingFace 上的 [Voxel51/gaussian_splatting](https://huggingface.co/datasets/Voxel51/gaussian_splatting) 下载,放入 `Content/Splats/`。

## 环境

UE5.7,在 Windows / DX12 上测试。MIT 许可证。
