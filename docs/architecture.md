# 实现与取舍

AimLab 将第一人称交互与实时渲染组织为独立模块。本文介绍各模块如何协作，以及渲染质量、运行成本和资源管理之间的选择。

## 帧循环与渲染顺序

`main.cpp` 负责窗口事件和帧计时，`GameManager` 组织输入、状态更新与渲染。一帧按以下顺序执行：

1. 处理输入，更新相机、靶球、粒子与生态动画。
2. 渲染方向光深度图，收集地形、植被、地面动物和 PBR 道具的遮挡信息。
3. 将场景写入 RGBA16F 颜色缓冲，绘制地形、生态、PBR 道具、靶球、天空盒、粒子和第一人称武器。
4. 执行水下扭曲、像素化、曝光、ACES 色调映射、色彩调整与 gamma 校正。
5. 绘制 HUD、计分和命中反馈。

模拟时间步限制在 0.1 秒以内，避免长帧引起过大的状态变化；FPS 使用实际帧间隔统计。最小化窗口时等待事件，恢复后重置时钟。

入口：[主循环](../src/main.cpp)、[GameManager](../src/game_manager.cpp)、[后处理](../src/post_process_renderer.cpp)。

## PBR 与环境光照

PBR shader 使用 Cook–Torrance 微表面模型，结合 GGX 分布、几何遮蔽项和 Fresnel。模型加载器支持 glTF / OBJ 材质，以及独立或打包的遮蔽、粗糙度和金属度贴图。

Split-Sum IBL 将环境光积分转为预计算贴图与运行时采样：

| 数据 | 尺寸 | 用途 |
| --- | --- | --- |
| HDR environment cubemap | 512² / 面 | 天空盒和环境预计算输入 |
| Irradiance cubemap | 32² / 面 | 漫反射环境光 |
| Prefilter cubemap | 128² / 面，预计算并采样 mip 0–4 | 按粗糙度选择镜面环境光层级 |
| BRDF LUT | 256² | 镜面 BRDF 积分近似 |

环境图相关预计算随地图加载执行，BRDF LUT 在地图之间共享。这减少了逐帧积分成本，同时将一部分开销集中到加载阶段。

天空盒、irradiance 法线和 prefilter 反射方向采用一致的环境空间旋转。直射光和点光仍在世界空间计算，主方向光是否跟随天空盒由地图配置决定。

源码：[资源预计算](../src/resource_manager.cpp)、[PBR shader](../shaders/pbr_model.fs)、[irradiance](../shaders/irradiance.fs)、[prefilter](../shaders/prefilter.fs)、[BRDF LUT](../shaders/brdf.fs)。

## 方向光与软阴影

方向光使用单张 4096² 深度纹理。接收点变换到光空间后，与深度纹理中的遮挡信息比较；法线相关偏移用于缓解自阴影。

PCF 使用邻域深度比较的平均值平滑边缘。PCSS 风格路径先进行 16 次遮挡物搜索，再根据平均遮挡深度、接收深度和光源参数估计半影宽度，最多执行 16 次 Poisson PCF 采样。半影使用经验近似，以固定采样预算呈现接触处较硬、远离遮挡物更软的效果。

深度偏移需要在 acne 与 peter-panning 之间调整。阴影投影覆盖相机附近的固定范围，纹理精度、覆盖距离和投影稳定性之间存在取舍。

源码：[ShadowMapper](../src/shadow_mapper.cpp)、[阴影阶段](../src/shadow_pass_renderer.cpp)、[地形 shader](../shaders/terrain.fs)。

## CPU / GPU 任务划分

| 系统 | CPU | GPU |
| --- | --- | --- |
| 粒子 | 对象池与 free list、位置和寿命更新、整理活跃实例 | 实例化 billboard 绘制 |
| 生态 | 依地形布置对象、更新运动和实例数据 | 分组实例化；植被与地面动物参与阴影阶段 |
| 靶球 | 生成、复用、碰撞与收缩状态 | 单位球网格按当前半径变换 |
| 地形 | 读取 8 / 16 位高度图、生成顶点与法线、查询高度 | 网格绘制与光照 |

粒子池容量为 2,000，活跃粒子合并为一次实例化绘制。CPU 更新、实例数据上传和透明片元覆盖仍产生开销，性能分析需要分别观察这些阶段。

地形保留 CPU 高度数据，供玩家贴地、目标生成和生态布置复用，代价是同时维护 CPU 数据和 GPU 网格。

源码：[粒子](../src/particle_system.cpp)、[生态](../src/ecology_system.cpp)、[地形](../src/terrain.cpp)。

## 射线命中与计分

相机位置与朝向形成归一化射线，对活跃靶球求交，选择最近的正向交点。交点位置用于粒子和命中提示，射线到球心的垂直距离用于精度评分：

```text
offset = targetCenter - rayOrigin
distanceToCenterLine = length(cross(offset, normalizedRayDirection))
precision = clamp(1 - distanceToCenterLine / targetRadius, 0, 1)
```

中心命中趋近 1，边缘趋近 0。计分叠加精度、距离和连击奖励，再由 HUD、屏幕震动与后坐力形成反馈。当前查询范围是靶球，场景几何的遮挡查询属于后续玩法扩展。

源码：[Raycast](../src/raycast.cpp)、[射击流程](../src/game_manager.cpp)、[ScoreSystem](../src/score_system.cpp)。

## 配置驱动的场景切换

`MapManager` 在启动时读取 `resources/maps_config.json`，校验字段类型、数值范围及路径字段的完整性，再提交地图列表。文件解码与 GPU 上传在对应地图加载时进行。`heightmap.grid_size` 限制为整数 `1–2048`，约束地形网格的分配规模。

场景切换通过 `MapLoadTransaction` 分三步处理：

1. 暂存当前地图缓存，加载候选纹理、模型、地形与生态。同地图中相同模型路径只加载一次。
2. 候选场景齐备后，交换场景对象并提交地图索引。
3. 成功时释放旧场景；失败时释放候选资源并恢复旧缓存，保留相机、靶场和计分状态。

Shader、BRDF LUT 与共享 cube 几何跨事务保留。切换同步执行，便于控制 GL context 和资源生命周期；准备阶段同时持有新旧场景，增加资源峰值，并可能造成加载停顿。

源码：[配置解析](../src/map_manager.cpp)、[地图资源加载](../src/map_resource_loader.cpp)、[缓存事务](../include/resource_manager.h)、[场景提交](../src/game_manager.cpp)。

## 资源所有权与错误处理

Shader 和模型通过 RAII / `std::unique_ptr` 持有。模型独占材质纹理，Mesh 借用纹理 ID；模型析构时去重释放纹理。FBO、VAO、VBO 等对象按句柄清理，局部状态 guard 恢复临时修改的 OpenGL 状态。

Shader 在读取、编译和链接成功后保存 program ID。读取失败抛出带路径与阶段的异常，编译或链接失败另附驱动日志；中间对象随作用域释放。IBL 预计算采用相同的资源保护方式，在完整成功后提交输出贴图。

初始化检查模块返回值和 FBO 完整性。调整窗口尺寸时先创建新渲染目标，成功后替换旧目标。主运行段捕获错误并返回非零状态；窗口回调在自身边界捕获异常并请求退出。GL 资源的清理发生在 context 销毁之前。

源码：[Shader](../include/shader.h)、[Model](../include/model.h)、[后处理目标](../src/post_process_renderer.cpp)、[GL 状态保护](../include/gl_state_guard.h)。

## 验证与画面对照

- [启动冒烟检查](../scripts/smoke_build_run.ps1)：构建并启动程序，检查主要模块的初始化。
- [启动失败回归](../scripts/check_startup_failures.ps1)：在隔离场景中验证配置、资源和 shader 错误的诊断及退出行为。
- [地图加载集成测试](../scripts/check_map_loading.ps1)：使用真实 OpenGL context，检查失败回退、候选资源释放、成功提交和后续渲染。
- `--capture`：固定地图、视角与模拟步长，读取 OpenGL back buffer 并输出 PPM，便于进行画面对照。

交互行为、长期运行和性能数据需要单独验证。构建与检查命令见 [README](../README.md#快速开始)。

## 设计边界与扩展方向

射击体验优先补充地形和模型遮挡查询，以场景最近交点决定命中结果。

渲染方面，主要方向是稳定阴影投影、统一颜色贴图的 sRGB 处理，以及接入 Bloom 的提取、模糊和 HDR 合成。当前鸟类参与颜色绘制，阴影阶段覆盖植被和地面动物。

加载与性能优化应先记录 CPU / GPU 帧时、IBL 预计算耗时和资源峰值，再选择环境缓存、离线预计算或后台加载方案。
