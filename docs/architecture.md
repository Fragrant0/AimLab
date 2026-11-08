# 实现与取舍

本文说明当前实现中的模块职责与技术边界，便于从运行效果追踪到代码。

## 一帧如何产生

1. `main.cpp` 计算帧间隔，处理窗口事件；`GameManager` 接收输入，更新玩法与场景状态。
2. `ShadowPassRenderer` 渲染方向光深度图：地形、实例化生态、PBR 道具使用各自适合的绘制路径。
3. 场景写入 `PostProcessRenderer` 的 RGBA16F 颜色缓冲，配合深度 / 模板附件处理可见性。
4. 地形、生态和 PBR 道具之后绘制靶球、天空盒、粒子与第一人称武器。
5. 全屏阶段处理水下扭曲、像素化（可叠加）、曝光、ACES、色彩调整和 gamma；HUD 在最后绘制。

入口：[main](../src/main.cpp)、[GameManager](../src/game_manager.cpp)、[后处理](../src/post_process_renderer.cpp)。

## PBR 与 Split-Sum IBL

`pbr_model.fs` 使用 Cook–Torrance 微表面模型，结合 GGX 分布、几何遮蔽项与 Fresnel。模型加载器读取 glTF / OBJ 材质，并支持独立贴图或打包的遮蔽、粗糙度和金属度贴图。

环境光预计算发生在地图资源加载阶段，而非每帧执行：

| 数据 | 当前尺寸 | 用途 |
| --- | --- | --- |
| HDR environment cubemap | 512² / 面 | 环境采样与天空盒 |
| Irradiance cubemap | 32² / 面 | 漫反射环境光 |
| Prefilter cubemap | 128² / 面，预计算并采样 5 级 mip（0–4） | 用粗糙度选择镜面环境光层级 |
| BRDF LUT | 256² | Split-Sum 镜面积分近似 |

这项取舍将积分成本集中在加载时；代价是同步切换地图可能停顿。后续可离线烘焙 IBL 数据或缓存已加载环境。

源码：[ResourceManager](../src/resource_manager.cpp)、[PBR](../shaders/pbr_model.fs)、[irradiance](../shaders/irradiance.fs)、[prefilter](../shaders/prefilter.fs)、[BRDF LUT](../shaders/brdf.fs)。

## 阴影为什么需要搜索与滤波

方向光使用 4096² 深度纹理。接收点先变换到光空间，再与采样深度比较；法线相关偏移用于缓解自阴影。

PCF 路径在邻域比较多次深度并平均。PCSS 风格路径先进行 16 次遮挡物搜索，再根据平均遮挡深度和接收点的距离关系，使用经验系数调整 16 次 Poisson PCF 的滤波半径。它呈现接触处较硬、远离遮挡物更软的外观，但没有完整的物理面积光源模型。

阴影偏移是在 acne 与 peter-panning 之间权衡。现有投影范围固定，远处精度、投影随相机移动的稳定性仍需改进；不能仅凭一张截图断言消除了闪烁。

源码：[ShadowMapper](../src/shadow_mapper.cpp)、[ShadowPassRenderer](../src/shadow_pass_renderer.cpp)、[terrain.fs](../shaders/terrain.fs)。

## CPU / GPU 的职责边界

| 系统 | CPU | GPU |
| --- | --- | --- |
| 粒子 | 固定容量对象池、free list、位置与寿命更新、整理活跃实例 | 实例化 billboard 绘制，一批活跃粒子使用一次 draw call |
| 生态 | 根据地形布置对象、更新场景动画与实例数据 | 分组实例化；植被与地面动物参与阴影阶段，鸟类暂不投影 |
| 靶球 | 生成、复用、射线碰撞与收缩状态 | 单位球网格按当前半径变换 |
| 地形 | 读取高度图、生成顶点和法线、查询世界坐标高度 | 上传后的网格绘制与光照 |

粒子上限为 2,000，没有 compute shader 或 transform feedback 模拟。减少 draw call 不等于消除 CPU 更新、实例上传、透明片元覆盖或对象分配成本。

源码：[ParticleSystem](../src/particle_system.cpp)、[EcologySystem](../src/ecology_system.cpp)、[Terrain](../src/terrain.cpp)。

## 射击与计分

相机位置与朝向形成归一化射线，对活跃靶球求交，选择最近的正向交点。球面交点用于粒子和命中提示；射线到球心的垂直距离用于中心精度评分：

```text
offset = targetCenter - rayOrigin
distanceToCenterLine = length(cross(offset, normalizedRayDirection))
precision = clamp(1 - distanceToCenterLine / targetRadius, 0, 1)
```

中心命中趋近 1，边缘趋近 0。使用“球面交点到球心的距离”会恒等于半径，无法区分中心和边缘。计分还叠加距离与连击奖励；当前射线查询未包含场景遮挡。

源码：[Raycast](../src/raycast.cpp)、[GameManager::Shoot](../src/game_manager.cpp)、[ScoreSystem](../src/score_system.cpp)。

## 配置与资源生命周期

`MapManager` 在启动时读取 `resources/maps_config.json`，先在局部容器中校验完整配置，再提交地图列表。缺失文件、非法 JSON、错误字段类型、零光照方向或不可用的地形网格参数会明确报错，不再退回过期的默认资源。资源路径在实际加载时验证，未选中的地图可以在尝试切换时报告缺失资源。

`MapResourceLoader` 加载地表、天空盒和模型，同地图中相同模型路径只加载一次。切换采用 `MapLoadTransaction`：旧地图缓存暂存，候选纹理、模型、地形和生态全部就绪后，才交换场景并提交地图索引。失败释放候选资源并恢复旧缓存，保留相机、靶场和计分状态；成功后释放旧场景。Shader、BRDF LUT 与共享 cube 几何不随地图事务重复创建。

该方案仍为同步加载，属于运行时切换，不是文件热重载。为了保留失败回退能力，准备阶段会同时占用新旧地图的资源；异步加载、离线 IBL 与显存预算仍是后续工作。`heightmap.grid_size` 限制为整数 `1–2048`，避免无约束地分配地形网格。

Shader 和模型由 RAII / `std::unique_ptr` 持有；模型缓存独占材质纹理，Mesh 仅借用纹理 ID，模型析构去重释放这些纹理。FBO、VAO、VBO 等 GL 句柄有清理入口。清理必须发生在 OpenGL context 销毁前。局部状态 guard 用于恢复深度测试、混合和多边形模式等状态。

模拟使用受限时间步，FPS 统计使用实际帧间隔；最小化窗口时等待事件并重置时钟，避免恢复时产生过大步长。

## 初始化与失败处理

Shader 只有在文件读取、两阶段编译和 program 链接全部成功后才提交 ID；失败抛出带文件路径、阶段与驱动日志的异常，并释放中间对象。IBL 预计算使用局部资源保护，在完整成功后提交输出贴图。

`GameManager` 检查各模块的初始化结果。即使只完成了一部分初始化，清理也会按现有句柄执行；所有 GL 资源在 context 销毁前释放。后处理和阴影 FBO 必须完整才可使用，后处理 resize 先创建新目标，失败保留旧目标。

主运行段捕获失败并返回非零状态；窗口回调在自身边界捕获异常并请求退出，避免 C++ 异常跨越 GLFW 的原生回调边界。

`scripts/check_startup_failures.ps1` 使用物理副本中的小型场景，验证正常基线与 27 种失败：缺失或非法配置、资源缺失、源码读取与 GLSL 编译/链接错误，以及 IBL、字体和后处理晚期失败。它检查明确的退出码与阶段诊断，并保留日志；不修改仓库内的原始资源。

`scripts/check_map_loading.ps1` 构建独立的真实 OpenGL 集成测试。它注入缺失或损坏的纹理、HDR、模型、材质与高度图，验证切图失败时索引、相机和旧缓存不变；同时检查候选纹理与网格已释放、失败后仍可成功切图，以及成功提交后旧资源被删除。

## 可复现的视角检查

`--capture` 通过 `GameManager::PrepareShowcase` 设置地图与地形相对视角，关闭输入，执行固定步长更新，再读取最终的 OpenGL back buffer。图片逐行翻转写入 P6 PPM，读取过程检测 OpenGL 错误。该入口便于检查各地图是否可渲染与比较画面；生态布局可能使用随机生成，不保证逐像素一致，也不替代性能测量。

## 已完成的重要修复

| 项目 | 当前行为 | 验证入口 |
| --- | --- | --- |
| 初始化失败与部分资源清理 | Shader / FBO 失败向上传播，退出前释放已创建的 GL 对象；失败 resize 保留旧目标 | `scripts/check_startup_failures.ps1` |
| 配置与资源失败策略 | 完整校验 JSON；模型、纹理及高度图失败明确报错；切图失败保留可用旧场景 | `scripts/check_startup_failures.ps1`、`scripts/check_map_loading.ps1` |
| 环境反射方向 | 天空盒、irradiance 与 prefilter 使用相同的环境空间旋转，直射光仍在世界空间计算 | `skybox.vs` / `pbr_model.fs` 旋转公式核对、地图截图检查 |

## 优先改进事项

| 优先级 | 问题 | 下一步与验证方式 |
| --- | --- | --- |
| 高 | 射击尚未检测地形与模型遮挡 | 加入场景最近交点查询；用遮挡物前后靶球验证无法穿透命中 |
| 中 | 阴影投影稳定性 | 在固定光空间基准中量化投影，录制相机平移前后阴影边缘对比 |
| 中 | 地面颜色纹理与模型 albedo 的 sRGB 处理不同 | 明确颜色贴图与数据贴图的线性化规则，使用灰卡或固定测试材质对照 |
| 中 | 同步加载停顿、准备阶段资源峰值及性能证据不足 | 记录 CPU / GPU 帧时、IBL 加载耗时、显存峰值与粒子规模，再评估离线预计算、缓存和后台加载 |
| 中 | 本机 GLFW 与 Debug 应用的 CRT 配置不同 | 使用匹配 `/MDd` 的 Debug GLFW，消除 `LNK4098`，并在无个人工程配置的环境验证依赖设置 |
| 后续 | Bloom shader 尚未接通 | 补齐亮部提取、可分离模糊和 HDR 合成，并比较开关效果与 GPU 成本 |

构建与启动检查见 [README](../README.md#快速开始)。自动冒烟检查只验证初始化，不能代替交互与视觉回归。
