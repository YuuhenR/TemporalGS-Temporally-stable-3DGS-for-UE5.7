# TemporalGS —— 路线图与状态

图例:✅ 完成（已编译） · 🔬 待你视觉验证 · 🟡 已铺架构/待数据 · ⬜ 计划中

## Phase 0 —— 仓库与构建骨架  ✅
模块接线、shader 目录映射、`SceneViewExtension` 注册（修了 GEngine 时序）、控制台变量、文档。

## Phase 1 —— 数据层  ✅
`UGaussianSplatAsset` + INRIA `.ply` 加载器 + 组件/Actor + 线程安全 proxy 注册表。
`TemporalGS.Load` / `TemporalGS.Clear` 控制台命令(自动缩放到可见)。**已在本机验证:741,883 高斯加载正确。**

## Phase 2 —— splat 光栅化器  ✅
GPU 上传 → 各向异性椭圆 conic splat（投影半轴法,稳健）→ 深度排序(CPU,far→near)→ 预乘 alpha 混合 → 真彩色。
`r.TemporalGS.{Mode,Sigma,OpacityScale,Debug}` 可调。**已在本机验证:train 场景柔和 3DGS 外观正确渲染。**

## Phase 3 —— 逐像素 velocity + jitter(课题核心)  ✅ 编译 / 🔬 待你视觉验证
- splat 从"tonemap 后"移到 **`PrePostProcessPass`(TSR 之前)**,画进 **SceneColor**(参与 TSR)。
- **MRT**:RT0=颜色,RT1=`GBufferVelocityTexture` 写逐像素运动矢量(最近 splat 胜,dominant 模式)。
- velocity 用**引擎约定**(`VelocityCommon.ush:Calculate3DVelocityBase`)计算,jitter 自动去除;上一帧矩阵自缓存。
- **对 `SceneDepthTexture` 深度测试**=场景遮挡(splat 被地形挡住)。
- `r.TemporalGS.WriteVelocity 0/1` = ghosting 前/后对比开关;`r.TemporalGS.Occlude 0/1`。
- **待验证**:WriteVelocity 0 拖影、1 清晰(课题的决定性对比);遮挡正确性。

## Phase 4 —— 动态 / 4D  🟡 架构就绪,待 4D 数据
velocity 已是通用形式:`prevClip = mul(prevWorldPos, PrevViewProj)`。静态场景 `prevWorldPos==worldPos`(纯相机运动)。
做 4D 只需:① 4D 数据格式(时变位置/形变场)② 每帧刷新 `GpuPrevPositions` 缓冲 ③ shader 用每高斯上一帧世界位置。
train 是静态场景,无 4D 数据,故此阶段为架构预留;接入 4D-GS / Deformable-3DGS 数据后即可启用。

## Phase 5 —— 评测工具  ✅
`Benchmark/metrics.py` + `Benchmark/README.md`:temporal flicker(时序稳定性主指标)+ PSNR/SSIM。
核心实验:同一相机轨迹下 `WriteVelocity 0`(baseline) vs `1`(fixed),量化 ghosting 降低百分比。

## Phase 6 —— 打磨与延展  ⬜（性能优化,功能不缺)
当前每帧 CPU 排序 74 万高斯 + 全量重传 buffer(~47MB/帧)→ 转视角会卡。优化项:
- **GPU 排序**(取代 CPU 排序)+ **持久化 buffer**(只传一次,不每帧重传)→ 实时流畅。
- 相机变化阈值触发重排序(静止时不排)。
- 高阶 SH(视角相关颜色)、velocity 的 median-transmittance 模式(更准的每像素运动)。
- VR 立体时序、桥接到独立的 mesh↔splat 逐片元遮挡。

---

### 已实现并编译通过的:Phase 0–3(渲染器 + 课题核心)+ Phase 5(评测)
**唯一待办的视觉验证**:Phase 3 的 ghosting 前后对比(见 `docs/BUILD_AND_RUN.md` / `Benchmark/README.md`)。
