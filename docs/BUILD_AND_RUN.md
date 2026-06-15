# TemporalGS —— 编译与运行

## 前置条件
- **源码编译的 Unreal Engine 5.7**(`UnrealEngine` 仓库,`Development Editor`,Win64/DX12)。
  必须源码版,因为渲染层包含了引擎 `Renderer/Private` 头文件。
- Visual Studio 2022,装好 C++ / 游戏开发 工作负载。
- 支持 DX12 的 GPU。
- Python 3.10+(仅 Phase 5 的 benchmark 脚本需要)。

## 1. 把插件放进一个工程
TemporalGS 是个插件。要在一个 UE 5.7 的 **C++** 工程里用(纯蓝图工程无法编译插件源码):

```
<YourProject>/
  <YourProject>.uproject
  Source/...
  Plugins/
    TemporalGS/            <- 把本仓库 clone 到这里(含 TemporalGS.uplugin 的那层)
```

如果你还没有工程:在 UE 5.7(源码版)编辑器里新建一个空白 **C++** 工程,关掉编辑器,再把 TemporalGS clone 进它的 `Plugins/` 目录。

> 也可以直接把仓库根 `E:\Code\UE\Optimize` 当成插件目录,软链/拷贝到某个 C++ 工程的 `Plugins\TemporalGS` 下。

## 2. 生成工程文件并编译
- 右键 `.uproject` → **Generate Visual Studio project files**
  (必要时用引擎的 `GenerateProjectFiles.bat`)。
- 打开 `.sln`,配置选 **Development Editor**、**Win64**,然后 **Build**。

## 3. 启用插件
- 启动编辑器。**Edit → Plugins → 搜 "TemporalGS" → 勾选启用 → 重启。**
- 确认加载成功:**Output Log** 应出现
  `TemporalGS: mapped shader dir /TemporalGS -> ...` 和 `SceneViewExtension registered.`
- 进入 PIE 后,`LogTemporalGSSVE`(Verbose)会打印节流的心跳——证明渲染线程 hook 是活的。
  (在控制台输入 `Log LogTemporalGSSVE Verbose` 打开。)

## 4. 准备一个 3DGS `.ply`
需要一个 INRIA 格式的已训练 `.ply`(官方 3DGS 代码产出的标准格式)。

**最快**:用仓库脚本下载一个小场景(Tanks&Temples 的 train,7000 迭代,~184 MB):
```powershell
pwsh ./scripts/download_sample_ply.ps1            # 默认 train / 7000 -> Content/Splats/train_7000.ply
pwsh ./scripts/download_sample_ply.ps1 -Scene truck -Iteration 30000
```

或手动直链下载:
```
https://huggingface.co/datasets/Voxel51/gaussian_splatting/resolve/main/FO_dataset/train/point_cloud/iteration_7000/point_cloud.ply
```
可选场景 `train|truck|playroom|drjohnson`,迭代 `7000|30000`(30000 更清晰、更大)。
数据集:<https://huggingface.co/datasets/Voxel51/gaussian_splatting>。
官方全量包(~13.7 GB):<https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/datasets/pretrained/models.zip>。

**避免** `.splat` / `.ksplat` / `*_compressed.ply` 等非标准格式。放到 `Content/Splats/` 下(已被 git 忽略)。*(Phase 1 会加上消费它的加载器 + Actor。)*

## 5. 运行并看到效果 *(Phase 3 起可用)*
1. 放一个 **Gaussian Splat Actor**,设置它的 `.ply`。
2. **开 TSR**:`r.AntiAliasingMethod 4`(TSR)—— 或 TAA `=2`。
3. 点 Play,移动相机。
4. 切换修复开关对比:
   ```
   r.TemporalGS.WriteVelocity 0     // baseline:运动下 splat 拖影/残影
   r.TemporalGS.WriteVelocity 1     // 修复后:运动下清晰
   r.TemporalGS.ApplyJitter 0/1     // 看 jitter 不对齐的伪影
   r.TemporalGS.VelocityMode 0/1/2  // 消融逐像素 velocity 的定义
   ```

## 6. Benchmark *(Phase 5)*
```
python Benchmark/metrics.py --capture <dir> --ref <supersampled_dir>
```
产出 flicker / warped-history 误差 / PSNR-SSIM-LPIPS 与对比表。

## 排错

- **shader 路径报错(找不到 `/TemporalGS/...`)**:插件没映射到 shader 目录——确认插件已启用,且 `Shaders/Private/*.usf` 存在。
- **找不到 `Renderer/Private` 头文件**:你用的是 launcher(二进制)版。请换 UE 5.7 源码版,或把相关代码改走 Renderer 的 public API。
- **回调签名不匹配**:`SceneViewExtension` 的 hook 签名在 UE 小版本间会变;以你这版的 `Engine/Source/Runtime/Engine/Public/SceneViewExtension.h` 为准做对齐。
- **首次编译报 `TemporalGSRenderer` 相关链接/包含错误**:多半是 `TemporalGSRenderer.Build.cs` 里那两条 `PrivateIncludePaths`(指向引擎 `Renderer/Private` 和 `Renderer/Internal`)在你的引擎布局下路径不同——把报错贴给我,我按你的引擎目录修正。

---

### 给我反馈
按上面编译时**任何报错都原样贴给我**(尤其是第一次 Build 的输出),我把它修到能加载、能看到那两行日志,再继续往下写 Phase 1–3。
