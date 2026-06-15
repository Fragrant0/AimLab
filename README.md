# OpenGL FPS Aim Lab Demo

一个基于 C++17 / OpenGL 3.3 的第一人称射击交互 Demo，定位为技术美术 / 游戏客户端开发作品集项目。项目围绕“可展示的实时渲染能力”和“可玩的客户端交互闭环”展开：场景支持 JSON 配置、多地图切换、HDR 天空盒与 IBL 预计算、PBR 模型渲染、高度图地形、生态点缀生成、射击命中判定、粒子、准星、计分、命中文字与屏幕震动反馈。

## 项目定位

这个项目不是单一 shader 或单一玩法练习，而是一个小型实时渲染与 FPS 交互框架。它适合在作品集中展示两类能力：

- 技术美术方向：PBR/IBL 管线、高度图地形、HDR 环境光、材质与光照表现、场景配置化、程序化生态布置。
- 游戏客户端方向：模块化 C++ 架构、OpenGL 资源管理、输入与相机、射线命中、对象生命周期、UI/HUD、粒子系统、命中反馈、运行时地图切换。

## 技术栈

- 语言与标准：C++17
- 图形 API：OpenGL 3.3 Core Profile
- 窗口与输入：GLFW
- OpenGL 函数加载：GLAD
- 数学库：GLM
- 模型加载：Assimp
- 图片 / 字体：stb_image, stb_truetype
- 配置解析：nlohmann/json
- 工程环境：Visual Studio 2022, MSBuild, Debug|x64

## 主要模块

| 模块 | 文件 | 展示价值 |
| --- | --- | --- |
| 游戏主循环与调度 | `main.cpp`, `include/game_manager.h`, `src/game_manager.cpp` | 初始化、输入、更新、渲染、资源清理的完整客户端框架 |
| 资源管理 | `include/resource_manager.h`, `src/resource_manager.cpp` | Shader/Texture/Cubemap/HDR/IBL 资源加载、缓存与释放 |
| 地图配置 | `include/map_manager.h`, `src/map_manager.cpp`, `resources/maps_config.json` | 用 JSON 驱动天空盒、地形、出生点、靶区和场景参数 |
| 高度图地形 | `include/terrain.h`, `src/terrain.cpp` | 支持 8/16-bit 高度图、三次插值采样、法线重建、GPU 网格上传 |
| PBR 与 IBL | `shaders/pbr_model.*`, `shaders/irradiance.fs`, `shaders/prefilter.fs`, `shaders/brdf.fs` | GGX、Fresnel、预滤波环境贴图、BRDF LUT、Reinhard tone mapping |
| 程序化生态 | `include/ecology_system.h`, `src/ecology_system.cpp` | 按海拔、坡度、间距生成植被，并驱动飞鸟和地面动物状态 |
| 射击与命中 | `include/raycast.h`, `src/raycast.cpp`, `include/target_manager.h` | 从相机方向发射射线，检测最近靶球并计算命中精度 |
| 粒子系统 | `include/particle_system.h`, `src/particle_system.cpp` | 固定容量对象池、free list 分配、GPU instancing、billboard 粒子 |
| 武器表现 | `include/weapon.h`, `src/weapon.cpp` | 第一人称武器模型、后坐力、枪口火光、射速冷却 |
| UI 与字体 | `include/ui_renderer.h`, `src/ui_renderer.cpp`, `include/font_renderer.h`, `src/font_renderer.cpp` | 2D 正交投影 HUD、准星、分数、combo 条、位图字体批量提交 |
| 反馈系统 | `include/hit_feedback.h`, `src/hit_feedback.cpp`, `include/screen_shake.h`, `src/screen_shake.cpp` | 命中文字、颜色分级、淡出曲线、屏幕震动与旋转扰动 |

## 技术亮点

### 1. HDR 天空盒到 IBL 的实时预计算链路

`ResourceManager::LoadHDRSkybox` 会加载 HDR 等距柱状图，将其转换为 cubemap，并进一步生成 irradiance map、prefilter map 和 BRDF LUT。PBR shader 中使用这些贴图组合漫反射环境光与粗糙度相关的镜面反射，实现比普通天空盒更完整的环境光照展示。

可展示点：

- HDR equirectangular map -> cubemap
- irradiance convolution
- GGX importance sampling prefilter
- BRDF integration LUT
- PBR shader 中的 Cook-Torrance / Fresnel / tone mapping / gamma correction

### 2. 高度图地形与运行时高度查询

`Terrain` 支持从高度图生成网格，自动计算法线并上传 VAO/VBO/EBO。运行时通过高度图采样得到世界坐标高度，用于玩家贴地、生态生成和动物移动。采样逻辑对较大高度图使用三次插值，对小图 fallback 到 bilinear，保证地形运动与视觉表面更连续。

可展示点：

- 8-bit / 16-bit heightmap loading
- grid mesh construction
- face normal accumulation
- cubic interpolation height sampling
- player clamp to terrain

### 3. 配置化场景与多地图切换

`resources/maps_config.json` 描述地图名称、天空盒类型、HDR 路径、地形模式、地板贴图、玩家出生点、靶区范围等。运行时按 `1/2/3` 切换地图，切换时卸载旧资源、加载新资源、重建地形和生态系统，并重置分数与命中反馈。

这部分可以强调“玩法参数和美术场景参数从代码中解耦”，适合技术美术岗位展示工具化意识。

### 4. 程序化生态点缀

`EcologySystem` 根据地形高度和坡度进行生态分布：低海拔区域更容易生成阔叶树和花丛，中高海拔生成针叶树和高山草甸，同时用间距检测避免过密堆叠。飞鸟以轨道运动和翅膀相位驱动，地面动物有 Idle / Walk 状态、游走半径和坡度限制。

可展示点：

- terrain-aware placement
- altitude / slope based rule set
- deterministic random seed
- simple agent state update
- procedural scene dressing

### 5. FPS 交互反馈闭环

射击行为由 `Weapon`, `Raycast`, `TargetManager`, `ScoreSystem`, `ParticleSystem`, `HitFeedback`, `ScreenShake` 串起来：点击射击后触发武器后坐力和枪口火光，射线命中靶球后计算距离、精度、combo 与分数，同时生成爆炸粒子、命中文字、命中准星和屏幕震动。

这条链路适合展示客户端工程能力：输入、状态、判定、表现、UI 和资源状态恢复都在一个可玩的闭环里。

### 6. 粒子对象池与 GPU Instancing

粒子系统预分配 `MAX_PARTICLES = 2000`，用 free list 复用粒子槽位，避免射击时频繁 new/delete。渲染时把活跃粒子打包成 instance buffer，通过 `glDrawArraysInstanced` 绘制 billboard quad，并用 additive blending 表现爆炸亮度。

可展示点：

- fixed-capacity particle pool
- free-list allocation
- dynamic instance buffer
- camera-facing billboard
- alpha / size lifetime curve

## 演示路线

推荐录制 60-90 秒视频，按下面节奏走：

1. 进入 `Nature` 地图，展示 HDR 天空盒、起伏地形和生态点缀。
2. 按 `SPACE` 切换 wireframe，展示高度图网格和地形结构。
3. 移动镜头靠近地形，说明玩家高度会跟随地形采样。
4. 射击靶球，展示命中粒子、浮动分数、combo、hit marker、屏幕震动和武器后坐力。
5. 按 `1/2/3` 切换地图，展示 JSON 配置化场景切换。
6. 回到 PBR 模型或 HDR 场景，说明 IBL 预计算和 PBR shader 的渲染链路。

## 操作说明

- `WASD`：移动
- `Mouse`：视角控制
- `Left Mouse`：射击
- `SPACE`：切换线框模式
- `1/2/3`：切换地图
- `ESC`：退出

## 构建与运行

项目使用 Visual Studio 2022 工程文件：

- Solution：`opengl_try.sln`
- Project：`opengl_try.vcxproj`
- 推荐配置：`Debug|x64`
- 自动构建脚本：`vibe_build.bat`

命令行构建：

```powershell
.\vibe_build.bat
```

当前 `build_output.log` 显示 Debug|x64 已构建成功，存在一个 `LNK4098` 运行库冲突警告，不影响当前可执行文件生成，但正式展示前可以进一步统一 Debug/Release 的运行库配置。

## 作品集包装建议

### 面向技术美术

标题可以写：

> OpenGL Real-time Rendering Demo: HDR IBL, PBR, Heightmap Terrain and Procedural Ecology

重点讲：

- 我实现了 HDR 环境贴图到 IBL 贴图的预计算流程，并在 PBR shader 中使用 irradiance、prefilter 和 BRDF LUT 支撑环境光照。
- 我用高度图生成可查询地形，并基于高度和坡度规则自动布置生态对象，使场景构建更接近工具化流程。
- 我把地图、天空盒、地形、靶区参数放入 JSON，降低调参和展示不同场景的成本。

### 面向游戏客户端

标题可以写：

> C++ OpenGL FPS Interaction Prototype with Modular Runtime Systems

重点讲：

- 我拆分了 GameManager、ResourceManager、MapManager、Terrain、Weapon、Particle、UI、Score 等系统，形成完整的客户端运行时结构。
- 我实现了从输入到射线判定、计分、粒子、HUD 和屏幕震动的命中反馈闭环。
- 我使用对象池和 GPU instancing 优化粒子生命周期与绘制路径，避免高频交互下的频繁分配。

## 后续可增强方向

- 补充录屏或 GIF，并在 README 顶部放一张主视觉截图。
- 增加一个材质调参面板，用于实时调整 roughness、metallic、exposure 和 muzzle flash 参数。
- 将生态对象改为 mesh instancing，进一步提高视觉丰富度和技术展示力度。
- 统一 Debug/Release 运行库配置，消除 `LNK4098` 警告。
- 为 City 地图替换专用城市 HDR 资源，形成更明确的多场景对比。
