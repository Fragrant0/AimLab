# OpenGL FPS Aim Lab Demo

一个基于 C++17 / OpenGL 3.3 的第一人称射击交互与实时渲染系统。项目重点实现了 PBR/IBL、HDR 天空盒、高度图地形、PCSS 阴影、后处理、多地图配置化、射击命中反馈和模块化客户端架构。

## 项目特点

这个项目不仅是单一的 Shader 练习，而是一个完整的小型实时渲染与 FPS 交互系统，具备以下核心特点：

- 渲染管线与光影：包含 PBR/IBL 管线、HDR 环境光、高度图地形、PCSS 软阴影、后处理风格化，且支持场景参数配置化。
- 系统设计与交互：采用 C++ 模块化架构，实现了 OpenGL 资源管理、输入与相机控制、射线命中判定、对象生命周期管理、HUD 界面、粒子系统和运行时地图无缝切换。

## 技术栈

- C++17
- OpenGL 3.3 Core Profile
- GLFW / GLAD / GLM
- Assimp
- stb_image / stb_truetype
- nlohmann/json
- Visual Studio 2022 / MSBuild / Debug x64

## 项目结构

```text
include/                  C++ module headers
src/                      C++ implementation files and entry point
shaders/                  GLSL shaders grouped by render pass
resources/maps_config.json Scene, terrain, lighting, post-process, and prop config
resources/objects/        Runtime 3D assets
resources/textures/ground Ground material textures
resources/textures/terrain Terrain surface textures
resources/textures/heightmaps Heightmap data
resources/textures/skybox_hdr HDR environment maps for skybox and IBL
scripts/                  Build and smoke-test helpers
DLL/                      Runtime third-party DLLs
```

## 主要模块

| 模块 | 文件 | 功能职责与技术细节 |
| --- | --- | --- |
| 游戏主循环 | `src/main.cpp`, `include/game_manager.h`, `src/game_manager.cpp` | 初始化、输入、更新、渲染和资源释放的完整客户端框架 |
| 资源管理 | `include/resource_manager.h`, `src/resource_manager.cpp` | Shader、Texture、Cubemap、HDR、IBL 资源加载、缓存与释放 |
| 地图配置 | `include/map_manager.h`, `src/map_manager.cpp`, `resources/maps_config.json` | 用 JSON 驱动天空盒、地形、地表纹理、出生点、灯光和 PBR 摆放物 |
| 高度图地形 | `include/terrain.h`, `src/terrain.cpp` | 8/16-bit 高度图、法线重建、GPU 网格上传、运行时高度查询 |
| PBR 与 IBL | `shaders/pbr_model.*`, `shaders/irradiance.fs`, `shaders/prefilter.fs`, `shaders/brdf.fs` | Cook-Torrance、Fresnel、预滤波环境贴图、BRDF LUT、tone mapping |
| 阴影系统 | `include/shadow_mapper.h`, `src/shadow_mapper.cpp`, `src/shadow_pass_renderer.cpp` | 主光源深度图、软阴影采样、地图光照方向联动 |
| 后处理 | `include/post_process_renderer.h`, `src/post_process_renderer.cpp`, `shaders/post_process.fs` | 屏幕空间滤镜、曝光、色调映射和画面风格控制 |
| 程序化生态 | `include/ecology_system.h`, `src/ecology_system.cpp` | 按地形高度、坡度和间距布置生态对象 |
| 射击反馈 | `include/raycast.h`, `include/target_manager.h`, `include/weapon.h` | 输入、射线判定、分数、粒子、HUD 和屏幕震动闭环 |
| 粒子系统 | `include/particle_system.h`, `src/particle_system.cpp` | 固定容量对象池、free list 分配、GPU instancing |

## 技术亮点

### HDR 天空盒到 IBL

`ResourceManager::LoadHDRSkybox` 会加载 HDR 等距柱状图，将其转换为 cubemap，并生成 irradiance map、prefilter map 和 BRDF LUT。PBR shader 使用这些贴图组合漫反射环境光和粗糙度相关的镜面反射，让模型能在不同地图天空盒下获得不同环境光照。

### 多地图配置化

`resources/maps_config.json` 描述地图名称、HDR 路径、地形模式、地表纹理、主光方向、点光源和 PBR 展示模型。运行时按 `1/2/3` 切换地图，系统会卸载旧资源、加载新资源、重建地形和生态布置。

### 高度图地形

`Terrain` 支持从高度图生成网格，自动计算法线并上传 VAO/VBO/EBO。运行时通过高度图采样获得世界坐标高度，用于玩家贴地、生态生成和对象摆放。

### 射击交互闭环

射击行为由 `Weapon`, `Raycast`, `TargetManager`, `ScoreSystem`, `ParticleSystem`, `HitFeedback`, `ScreenShake` 串联。点击射击后触发后坐力和枪口火光，命中后生成粒子、分数、combo、hit marker 和屏幕震动。

### 渲染架构重构

当前渲染逻辑已经拆分为 `SkyboxRenderer`, `TerrainRenderer`, `PbrPropRenderer`, `ShadowPassRenderer`, `HudRenderer`, `TargetRenderer`, `WeaponViewRenderer`, `ParticleRenderer` 等模块。资源和内部 GL 对象逐步改为 RAII / `std::unique_ptr` 管理，并加入局部 GL 状态 guard，降低状态泄漏风险。

## 操作说明

- `WASD`：移动
- `Mouse`：视角控制
- `Left Mouse`：射击
- `SPACE`：切换线框模式
- `1/2/3`：切换地图
- `ESC`：退出

## 编码约定

项目文本文件统一使用 UTF-8。C++ 编译配置显式启用 `/utf-8`，避免 Visual Studio 或系统代码页把源码按 GB2312/ANSI 误读。
