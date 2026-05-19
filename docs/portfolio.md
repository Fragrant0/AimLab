# OpenGL FPS Aim Lab Demo 作品集说明

## 项目一句话

这是一个基于 C++17 / OpenGL 3.3 的实时渲染与 FPS 交互 Demo。项目围绕技术美术和游戏客户端两个方向展开：一方面展示 PBR/IBL、HDR 环境光、PCSS 软阴影、后处理和地图光照配置；另一方面展示资源管理、渲染模块拆分、输入/射击/HUD/粒子反馈等客户端工程能力。

## 可展示产出

### 1. 三套可切换场景

项目通过 `resources/maps_config.json` 驱动三张地图：

| 地图 | 展示重点 | 运行时内容 |
| --- | --- | --- |
| Space | 冷色 HDR、岩石地形、科幻模型 | SciFi Helmet、蓝橙点光、偏冷 IBL |
| Nature | 自然 HDR、高度图地形、生态对象 | Avocado PBR 模型、程序化树木、自然暖光 |
| City | 城市场景、点光源、PBR 展示模型 | BoomBox、冷暖点光、克制 bloom |

每张地图都可以独立配置 HDR、地面纹理、高度图、主光方向、点光源、后处理参数、PBR 展示模型和目标区域。展示时可以用 `1/2/3` 切换地图，说明这是数据驱动的场景系统，而不是写死的单场景 Demo。

### 2. PBR + IBL 模型展示

项目支持外部下载的 glTF/FBX/OBJ PBR 模型接入。模型加载模块会处理 mesh、材质贴图、切线空间和包围盒，PBR shader 支持：

- Albedo / Normal / Metallic / Roughness / AO / ARM / Emissive
- Cook-Torrance BRDF
- GGX 法线分布
- Fresnel-Schlick
- HDR skybox 生成的 irradiance map、prefilter map 和 BRDF LUT
- 按地图变化的 IBL 漫反射和镜面环境光

展示价值：能说明模型不是单纯贴图显示，而是接入了较完整的实时 PBR 光照流程。

### 3. 阴影系统与 PCSS 软阴影

阴影系统由 `ShadowMapper` 和 `ShadowPassRenderer` 管理，当前产出包括：

- 4096 分辨率 shadow map
- 32-bit float depth texture
- camera-centered light frustum
- texel snapping 减少阴影游动
- PCF / PCSS 运行时切换
- shadow bias 运行时调节
- 天空盒旋转时同步主光方向，避免天空光照和阴影方向脱节

展示价值：不仅有阴影，而且考虑了稳定性、软边、调参和天空盒联动这些技术美术常见痛点。

### 4. 后处理与风格控制

后处理管线使用 HDR scene color FBO，当前包括：

- Exposure
- ACES tone mapping
- Gamma
- Contrast / Saturation
- Vignette
- Chromatic aberration
- Film grain
- Bright-pass bloom
- 全分辨率 bloom blur

不同地图的后处理参数在 `maps_config.json` 中配置。运行时 debug overlay 可以开关后处理和 bloom，并调节曝光、bloom 强度、阈值和半径。

### 5. FPS 交互闭环

项目不是纯渲染窗口，而是有完整的第一人称交互：

- WASD 移动和鼠标视角
- 武器模型和后坐力
- 枪口火光
- 射线命中检测
- 靶球生命周期管理
- 分数、combo、hit marker
- 命中粒子和屏幕震动
- HUD 与调试面板

展示价值：适合游戏客户端方向，能说明渲染系统与玩法反馈已经形成闭环。

### 6. Debug Overlay

按 `F1` 打开中文调试面板，可展示运行时调参能力：

- `F2` 后处理 on/off
- `F3` IBL on/off
- `F4` PCSS / PCF 切换
- `F5` Bloom on/off
- `TAB` 选择参数
- `Left / Right` 调整参数
- `Shift` 加速调节
- `R` 重置

可调项：

- 曝光
- Bloom 强度
- Bloom 阈值
- Bloom 半径
- Shadow bias

面板还显示当前地图的主光强度、环境光强度、点光源数量、IBL 漫反射强度、IBL 镜面强度、主光方向和天空旋转角。

展示价值：面试时可以现场切换 IBL、PCSS 和后处理，让对方直接看到每个系统对画面的影响。

## 技术亮点拆解

### 数据驱动地图系统

核心文件：

- `resources/maps_config.json`
- `include/map_manager.h`
- `src/map_manager.cpp`
- `src/map_resource_loader.cpp`

亮点表达：

> 我把地图的 HDR、地形、灯光、后处理、展示模型和出生点抽象进 JSON 配置，运行时切图会卸载旧资源、加载新资源并重建地形/生态。这让场景调参和美术迭代不依赖重新编译。

### PBR 资源加载与材质识别

核心文件：

- `include/model.h`
- `include/mesh.h`
- `shaders/pbr_model.fs`
- `src/pbr_prop_renderer.cpp`

亮点表达：

> 模型加载支持常见 PBR 贴图通道，并且在缺失材质声明时会尝试按贴图命名规则做 fallback。这样下载的 PBR 模型可以更快接入场景，适合作为技术美术展示流程。

### HDR 到 IBL 预计算

核心文件：

- `src/resource_manager.cpp`
- `shaders/hdr_to_cubemap.fs`
- `shaders/irradiance.fs`
- `shaders/prefilter.fs`
- `shaders/brdf.fs`

亮点表达：

> HDR 天空盒加载后会转换成 cubemap，并预计算 irradiance、prefilter 和 BRDF LUT。PBR shader 使用这些贴图产生随地图变化的环境光照。

### 阴影稳定性

核心文件：

- `src/shadow_mapper.cpp`
- `src/shadow_pass_renderer.cpp`
- `shaders/pbr_model.fs`
- `shaders/terrain.fs`
- `shaders/plane.fs`

亮点表达：

> 阴影不是只做一张深度图，还做了 camera-centered light frustum、texel snapping、PCF/PCSS 切换、shadow bias 调参和天空盒旋转同步主光方向。

### 渲染模块拆分

核心文件：

- `src/game_manager.cpp`
- `src/terrain_renderer.cpp`
- `src/pbr_prop_renderer.cpp`
- `src/post_process_renderer.cpp`
- `src/hud_renderer.cpp`
- `src/weapon_view_renderer.cpp`
- `src/particle_renderer.cpp`

亮点表达：

> 项目早期渲染逻辑集中在 GameManager，后续拆成多个 renderer，让每个模块负责一个明确的渲染域。这样方便维护，也能减少 OpenGL 状态互相污染。

## 推荐展示视频脚本

建议录制 60-90 秒视频。

### 片段 1：项目整体

镜头：Nature 地图开场，移动视角看地形、树木、天空盒和目标球。

讲解：

> 这是一个 OpenGL 实时渲染与 FPS 交互 Demo。场景、灯光、HDR、地形和后处理都由地图配置驱动。

### 片段 2：PBR + IBL

镜头：靠近地图中的 PBR 展示模型，按 `F1` 打开 debug overlay，按 `F3` 切换 IBL。

讲解：

> PBR 模型接入了 HDR 生成的 IBL。关闭 IBL 后，模型只剩基础环境和直接光；打开后可以看到来自天空盒的漫反射和镜面环境光。

### 片段 3：PCSS 软阴影

镜头：观察模型或树木投影，按 `F4` 在 PCF / PCSS 之间切换，调节 shadow bias。

讲解：

> 阴影支持 PCF 和 PCSS 切换。PCSS 会根据 blocker 和 receiver 的深度关系改变过滤半径，让阴影边缘更柔和。

### 片段 4：后处理

镜头：切到 City 地图，打开 debug overlay，切换后处理和 bloom。

讲解：

> 后处理包含曝光、ACES tone mapping、暗角、色散、颗粒和 bloom。不同地图可以配置不同的视觉风格。

### 片段 5：FPS 交互

镜头：射击靶球，展示命中分数、combo、粒子、hit marker、屏幕震动和武器反馈。

讲解：

> 项目不是纯 shader 展示，还实现了 FPS 输入、射线命中、目标生命周期和反馈闭环。

## 截图清单

建议放到 `docs/images/`。

| 文件名 | 内容 |
| --- | --- |
| `01_nature_overview.png` | Nature 地图全景，展示 HDR、地形和生态 |
| `02_pbr_ibl_on_off.png` | PBR 模型 IBL on/off 对比 |
| `03_pcss_shadow.png` | PCF / PCSS 阴影边缘对比 |
| `04_city_postprocess.png` | City 地图后处理风格 |
| `05_debug_overlay.png` | 中文 debug overlay 和调参项 |
| `06_wireframe_terrain.png` | 线框模式展示高度图地形网格 |
| `07_hit_feedback.png` | 射击命中、分数、粒子和 hit marker |

## 构建与验证

构建：

```powershell
.\vibe_build.bat
```

Smoke run：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\smoke_build_run.ps1 -RunSeconds 15
```

当前 smoke 主要验证：

- 程序可启动
- 地图配置可加载
- HDR / IBL 预计算路径可运行
- PBR 模型可加载
- 字体系统可初始化

## 已知限制与后续优化

当前项目已经可以作为作品集展示，但还有几个可以继续加分的方向：

- 增加截图自动保存或简单 replay camera，方便稳定录制展示视频。
- 增加 shadow debug view，直接显示 shadow map 和 light frustum。
- 进一步优化 PCSS 采样核，从 16 sample 提升到 32/64 sample 或分层采样。
- 为 PBR 模型加入材质通道可视化，例如 albedo/normal/metallic/roughness 单独查看。
- 将后处理拆成更完整的多级 mip bloom，获得更接近商业引擎的 bloom 质感。
- 增加 GPU timer query，展示各 pass 的耗时。

## 面试时可以强调的结论

这个项目的重点不是“某一个 shader 写完了”，而是把多个实时渲染模块组织成一个可运行、可切换、可调参、可交互的 Demo：

- 技术美术角度：展示 PBR、IBL、HDR、PCSS、后处理、地图光照配置和调参面板。
- 游戏客户端角度：展示 C++ 模块化、资源管理、输入、射线命中、HUD、粒子、武器反馈和 smoke 验证。
- 工程角度：展示从原型到重构的过程，包括编码统一、结构整理、renderer 拆分、状态 guard 和本地 git 提交记录。
