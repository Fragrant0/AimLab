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

`MapManager` 在启动时读取 `resources/maps_config.json`。`MapResourceLoader` 加载地表、天空盒和模型，同地图中相同模型路径只加载一次。切换时卸载旧资源、加载新地图并重建地形与生态；这属于运行时切换，不是文件热重载。

Shader 和模型由 RAII / `std::unique_ptr` 持有；模型缓存独占材质纹理，Mesh 仅借用纹理 ID，模型析构去重释放这些纹理。FBO、VAO、VBO 等 GL 句柄有清理入口。清理必须发生在 OpenGL context 销毁前。局部状态 guard 用于恢复深度测试、混合和多边形模式等状态。

模拟使用受限时间步，FPS 统计使用实际帧间隔；最小化窗口时等待事件并重置时钟，避免恢复时产生过大步长。

## 初始化与失败处理

Shader 只有在文件读取、两阶段编译和 program 链接全部成功后才提交 ID；失败抛出带文件路径、阶段与驱动日志的异常，并释放中间对象。IBL 预计算使用局部资源保护，在完整成功后提交输出贴图。

`GameManager` 检查各模块的初始化结果。即使只完成了一部分初始化，清理也会按现有句柄执行；所有 GL 资源在 context 销毁前释放。后处理和阴影 FBO 必须完整才可使用，后处理 resize 先创建新目标，失败保留旧目标。

主运行段捕获失败并返回非零状态；窗口回调在自身边界捕获异常并请求退出，避免 C++ 异常跨越 GLFW 的原生回调边界。

`scripts/check_startup_failures.ps1` 使用物理副本中的小型场景，先验证成功基线，再注入缺失源码、非法 GLSL、链接接口不符，以及 IBL、字体和后处理晚期失败。它检查明确的退出码与阶段诊断，并保留日志；不修改仓库内的原始资源。

## 可复现的视角检查

`--capture` 通过 `GameManager::PrepareShowcase` 设置地图与地形相对视角，关闭输入，执行固定步长更新，再读取最终的 OpenGL back buffer。图片逐行翻转写入 P6 PPM，读取过程检测 OpenGL 错误。该入口便于检查各地图是否可渲染与比较画面；生态布局可能使用随机生成，不保证逐像素一致，也不替代性能测量。

## 优先改进事项

| 优先级 | 问题 | 下一步与验证方式 |
| --- | --- | --- |
| 高 | 模型、纹理与地图配置的失败策略仍需统一 | 明确缺失资源的报错与回退策略，并使切图失败能保留可用场景 |
| 中 | 阴影投影稳定性 | 在固定光空间基准中量化投影，录制相机平移前后阴影边缘对比 |
| 中 | 天空盒旋转未同步到 IBL 方向 | 用相同旋转约定变换环境采样方向，使用高反射模型核对高光与天空位置 |
| 中 | 地面颜色纹理与模型 albedo 的 sRGB 处理不同 | 明确颜色贴图与数据贴图的线性化规则，使用灰卡或固定测试材质对照 |
| 中 | 缺少场景遮挡查询和加载性能证据 | 补齐射击遮挡；分别记录 CPU / GPU 帧时、IBL 加载耗时与粒子规模 |
| 后续 | Bloom shader 尚未接通 | 补齐亮部提取、可分离模糊和 HDR 合成，并比较开关效果与 GPU 成本 |

构建与启动检查见 [README](../README.md#快速开始)。自动冒烟检查只验证初始化，不能代替交互与视觉回归。
