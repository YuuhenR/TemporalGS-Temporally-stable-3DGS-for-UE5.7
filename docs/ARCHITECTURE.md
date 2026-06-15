# TemporalGS —— 架构

本文说明插件的结构,以及最关键的——**如何为 alpha-blended splat 定义逐像素运动矢量**(项目的核心技术贡献)。

## 1. 模块

| 模块 | 加载阶段 | 依赖 | 职责 |
|---|---|---|---|
| `TemporalGSRenderer` | `PostConfigInit` | Renderer, RHI, RenderCore | `SceneViewExtension`、RDG passes、HLSL shaders、渲染线程 proxy 注册表、映射 shader 目录 `/TemporalGS`。 |
| `TemporalGSRuntime` | `Default` | `TemporalGSRenderer` | `UGaussianSplatAsset`、`.ply` 加载器、`UGaussianSplatComponent`/`AGaussianSplatActor`;构建并注册 render proxy。 |

渲染层刻意保持*较底层*,**不**依赖运行层,因此依赖图无环:Runtime → Renderer。

## 2. 数据模型

`UGaussianSplatAsset` 每个高斯存储:

| 字段 | 类型 | 说明 |
|---|---|---|
| Position | `float3` | 世界/局部均值 |
| Scale | `float3` | `.ply` 里是 log-scale,加载时 exp() |
| Rotation | `float4` | 四元数(加载时归一化) |
| Opacity | `float` | sigmoid(存储值) |
| SH (DC) | `float3` | 基础颜色(0 阶)。高阶可选(Phase 4)。 |
| *Prev position* | `float3` | 动态/4D 时填入;静态时等于 Position。 |

GPU 侧这些变成 `StructuredBuffer`。静态场景缓冲只上传一次;动态/4D 时每帧刷新 position(及 `PrevPosition`)缓冲。

## 3. 渲染线程流程

所有工作挂在 `FTemporalGSSceneViewExtension` 上:

```
PreRenderViewFamily_RenderThread
   └─（每个 proxy）上传/刷新缓冲;缓存本帧 view + jitter

PostRenderBasePassDeferred_RenderThread          【此处 GBuffer + velocity + depth 已绑定】
   ├─ Pass 1  GSProjectCS    : Gaussians → 2D 均值、2D 协方差(conic)、深度、
   │                            每高斯屏幕空间 velocity
   ├─ Pass 2  GSSortCS       : 按深度排序(per-tile keys;bitonic/radix)
   └─ Pass 3  GSRasterizePS  : tile 内 front-to-back alpha 混合 → Scene Color（+ 代表性
                               深度),并把逐像素运动矢量(mode = CVarTGSVelocityMode）
                               在 WriteVelocity==1 时写入 GBufferVelocity
```

之所以 hook 在**基础 pass 之后**:这是第一个 velocity 目标与场景深度都可写、且对不透明几何做半透明式合成有明确定义的时机。

## 4. alpha-blended splat 的逐像素 velocity(最难的部分)

TAA/TSR 想要**每像素一个运动矢量**,但一个 splat 像素是 `N` 个高斯的 front-to-back 混合,权重 `w_i = α_i · T_i`(其中 `T_i = Π_{j<i}(1 − α_j)` 是透射率)。没有单一表面,所以我们给出三种定义(CVar `r.TemporalGS.VelocityMode`):

设 `p_i^curr` 为高斯 `i` 本帧投影屏幕位置(**不含** TAA jitter),`p_i^prev` 为用**上一帧** view-projection(动态 splat 还用上一帧世界位置)的投影位置。每高斯屏幕 velocity:`v_i = p_i^prev − p_i^curr`(UE 约定:velocity 指向当前→上一帧,以便取历史)。

- **Mode 0 —— 透射率加权**:`v_pixel = (Σ_i w_i v_i) / (Σ_i w_i)`。平滑,但深度不连续处的运动会糊。
- **Mode 1 —— 主导高斯**:`v_pixel = v_k`,`k = argmax_i w_i`。边界锐利,最贴近 TAA 的单表面假设。
- **Mode 2 —— median-transmittance 表面(默认)**:取累计透射率首次越过 `0.5` 的高斯(等效"前表面"),用其深度重建世界点,再经上一帧 view-projection 重投影。对静态场景最"像表面",作为稳健默认。

> **为什么这是贡献**:为 splat *定义* 并 *消融* `v_pixel`、并把它接进引擎 velocity buffer,是已有 3DGS 工作都没做的。Mode 2 是推荐默认;Mode 0/1 用于量化取舍(见 `docs/ROADMAP.md` Phase 5)。

### velocity 编码
匹配引擎的 velocity 编码,使 TSR 原生消费:复用 UE 对不透明/WPO 几何所用的 `EncodeVelocityToTexture(clipCurr, clipPrev)` 映射(NDC 增量,scale/bias 进 velocity 目标)。velocity 必须**去 jitter**——编码前先去掉 jitter(见 §5)。

## 5. TAA jitter 对齐

引擎每帧给投影加一个子像素 jitter(Halton 序列)。splat **光栅化**必须施加*相同*的 jitter(`InView.TemporalJitterPixels`),使 splat 与 mesh 采样落在同一子像素网格;否则时序累积器看到 splat 和 mesh 不一致,产生双重边缘。但 **velocity** 用*去 jitter* 的位置计算(jitter 是渲染期扰动,不是真实运动)。CVar `r.TemporalGS.ApplyJitter` 用于演示其影响。

## 6. 动态 / 4D

对可变形/4D splat,运行层每帧提供当前帧位置与上一帧位置(类比 UE 在 `t` 与 `t−1` 求值 World Position Offset 得到正确运动矢量)。`GSProjectCS` 用真实上一帧世界位置得到的 `p_i^prev`,而非仅相机上一帧。帧间高斯增删用稳定的每高斯 id 处理,保证 prev/curr 对应。

## 7. 合成与深度

splat 写一个**代表性深度**(Mode-2 表面深度),用于对不透明 mesh 的软遮挡,以及 TSR 的 parallax 启发式(它依赖 depth+velocity 较早完成)。从 global shader 经 `SV_Depth` 写引擎主深度较为棘手;v0 写一个供 velocity/parallax 用的私有深度,颜色用类半透明步骤合成,可选的引擎侧路径(源码版)在 Phase 6 探索。mesh→splat 硬遮挡 / GBuffer 集成是另一个方向(见 `research/` 中 D03/D09 的设计笔记)。
