# OpenGL 实时渲染与 FPS 交互作品集展示

> 本文档是一个预留了视频和截图占位符的作品集模板。您可以直接将此文档作为您个人网站、GitHub 首页或求职演示的 Markdown 框架。

---

## 🎥 项目演示视频

<!-- PLACEHOLDER: 推荐放置 60-90 秒的 Bilibili 嵌入链接、YouTube 嵌入链接，或者本地 mp4 视频地址 -->
<!-- 
示例格式：
<video src="images/portfolio_demo.mp4" width="100%" controls poster="images/01_ocean_overview.png"></video> 
-->

> 💡 **视频录制脚本建议：**
> 1. **Ocean 场景开场 (0-20s)**：演示水下扭曲波动、红光吸收后处理、动态和静态鱼类 PBR 模型，展示水下环境光。
> 2. **Nature 场景过渡 (20-40s)**：展示夕阳暖色 HDR 天空盒、高低起伏的山地地形，以及生态树木布置。
> 3. **Crater Base 场景高潮 (40-60s)**：展示星空穹顶、陨石坑地形、停泊的飞船与悬浮的 UFO 模型。打开 Debug Overlay 调整点光源强度，体现强对比的科幻霓虹氛围。
> 4. **FPS 射击闭环 (60-80s)**：进行靶球射击，展示后坐力、枪口火光、命中粒子、连击数 (Combo) 提示、Hit Marker 以及屏幕震动反馈。
> 5. **调试面板与线框模式 (80-90s)**：按 `F1` 开启调试面板，现场演示关闭 IBL、切换 PCSS/PCF 阴影、调节曝光，并按 `SPACE` 切换线框模式展示地形网格。

---

## 📌 项目一句话定位

这是一个基于 **C++17** 和 **OpenGL 3.3 Core Profile** 的模块化实时渲染与第一人称射击 (FPS) 交互框架。项目围绕 **技术美术 (TA)** 与 **游戏客户端开发** 两大方向设计，打通了从 HDR 预计算、PBR/IBL 材质、PCSS 软阴影、多风格后处理到射击反馈闭环的完整链路。

---

## 🎨 场景渲染与美术风格展示

项目通过数据驱动的地图配置文件 `resources/maps_config.json` 驱动三张完全不同主题、光照与渲染管线设置的场景：

### 1. Ocean (深海海底场景)
* **设计重点**：水下风格化后处理、海底沙丘地形、水下漫反射/环境色吸收。
* **运行时内容**：水下扭曲波动、深海红光衰减、绿蓝点光源、 instanced 海草与珊瑚生态、动态游动的鱼类 PBR 模型。

<!-- PLACEHOLDER: 海底场景整体效果截图 -->
![Ocean Map Overview](images/01_ocean_overview.png)
*(占位符：请将海底场景全景截图命名为 `01_ocean_overview.png` 并存放在 `docs/images/` 目录下)*

---

### 2. Nature (落日峡谷场景)
* **设计重点**：暖色夕阳 HDR 环境光照、高耸的陡峭峡谷地形。
* **运行时内容**：落日 HDR 天空盒、IBL 环境光贴图、BoomBox 录音机 PBR 模型、程序化树木/植被生态。

<!-- PLACEHOLDER: 峡谷场景整体效果截图 -->
![Nature Map Overview](images/02_nature_overview.png)
*(占位符：请将自然场景全景截图命名为 `02_nature_overview.png` 并存放在 `docs/images/` 目录下)*

---

### 3. Crater Base (科幻陨石坑基地)
* **设计重点**：高反差霓虹点光源、大体量科幻模型、高对比度阴影。
* **运行时内容**：纯净夜空 HDR 天空盒、陨石坑地形、低光照强反差月光、霓虹地带（蓝、品红、橙色点光源）、停泊的战机与悬浮的 UFO 模型。

<!-- PLACEHOLDER: 科幻基地场景整体效果截图 -->
![Crater Base Map Overview](images/03_crater_base_overview.png)
*(占位符：请将基地场景全景截图命名为 `03_crater_base_overview.png` 并存放在 `docs/images/` 目录下)*

---

## 🛠️ 核心技术模块与亮点

### 1. 物理光照 (PBR) 与基于图像的光照 (IBL)
系统实现了完整的 **Cook-Torrance BRDF** 光照模型，支持常见的 PBR 贴图通道：Albedo, Normal, Metallic, Roughness, AO/ARM, Emissive。
* **IBL 预计算**：在运行时加载 HDR 全景图并转换为 Cubemap，利用 GPU 预计算并渲染出 **Irradiance Map (漫反射环境光)**、**Prefilter Map (镜面反射环境光)** 和 **BRDF Integration LUT (查找表)**。
* **视觉呈现**：PBR 展示模型在不同场景（如深海的冷 teal 色与峡谷的暖橙色）下能呈现高度吻合的反射光泽与环境反射。

<!-- PLACEHOLDER: PBR模型开启/关闭IBL对比图 -->
![PBR IBL Comparison](images/04_pbr_ibl_on_off.png)
*(占位符：请将模型在 IBL 开启与关闭下的对比图命名为 `04_pbr_ibl_on_off.png` 存放在 `docs/images/` 目录下)*

---

### 2. 阴影系统与 PCSS (百分比渐进软阴影)
阴影通道由 `ShadowMapper` 和 `ShadowPassRenderer` 驱动，支持 4096 分辨率的深度图渲染：
* **动态软化**：实现 **PCSS (Percentage-Closer Soft Shadows)**。通过搜索遮挡物计算遮挡深度，动态确定 PCF 过滤半径，完美实现“近处阴影硬、远处阴影软”的真实半影效果。
* **阴影抗抖动**：支持 **Texel Snapping** 算法，避免相机移动时阴影边缘出现像素游动。
* **光源联动**：光源方向与天空盒旋转角实时同步，保证阴影朝向与穹顶光照完全契合。

<!-- PLACEHOLDER: PCF与PCSS阴影软度对比图 -->
![Shadow Softness PCSS vs PCF](images/05_pcss_shadow.png)
*(占位符：请将 PCF 与 PCSS 的效果对比图命名为 `05_pcss_shadow.png` 存放在 `docs/images/` 目录下)*

---

### 3. 后处理滤镜与电影级色彩分级
后处理管线采用高动态范围 (HDR) FBO，集成多种后期滤镜：
* **色彩分级**：支持曝光缩放 (Exposure)、对比度 (Contrast) 调节、饱和度 (Saturation) 微调、暗角 (Vignette)、色散 (Chromatic Aberration) 与胶片颗粒 (Film Grain)。
* **ACES Tone Mapping**：使用电影级 ACES 曲线映射高动态颜色，解决天空盒及强光源过曝截断问题，使光影过渡更加平滑。
* **Bloom (光晕效果)**：明亮提取 (Bright-pass) 后进行高斯模糊 (Gaussian Blur) 并叠加，呈现物体的高光溢出质感。
* **水下扭曲与红光吸收**：专门为 Ocean 地图设计，在屏幕空间实现了水流的正弦波动畸变，并随着相机深度吸收红光分量，模拟真实深海视效。

<!-- PLACEHOLDER: 后处理效果/水下效果展示图 -->
![Underwater Post Process Effect](images/06_underwater_postprocess.png)
*(占位符：请将水下后处理或 Bloom 对比图命名为 `06_underwater_postprocess.png` 存放在 `docs/images/` 目录下)*

---

### 4. 数据驱动的场景与高度图地形
* **配置驱动**：通过 `resources/maps_config.json` 控制地图名称、HDR 天空盒、地表材质、地形参数、主光源方向、点光源数组、PBR 装饰模型及位置坐标。
* **高度图重建**：支持 8-bit/16-bit 灰度高度图加载。着色器根据网格顶点位置采样高度并重新计算像素法线；C++ 端则缓存高度数据，为角色及生态对象贴地提供运行时高度查询支持。
* **程序化生态系统 (Ecology)**：系统根据地形的海拔、坡度及间距因子，自动通过 GPU Instancing 渲染海草、珊瑚、树木等生态网格，保证场景细节丰富的同时维持高帧率。

<!-- PLACEHOLDER: 地形网格线框模式截图 -->
![Wireframe Terrain Network](images/07_wireframe_terrain.png)
*(占位符：请将 F1 面板或线框网格截图命名为 `07_wireframe_terrain.png` 存放在 `docs/images/` 目录下)*

---

### 5. 第一人称射击 (FPS) 交互闭环
项目不仅是渲染展示，还集成了 Aim Lab 式的射击交互闭环：
* **物理反馈**：实现了第一人称相机移动、碰撞约束、武器模型绘制、枪口火光 (Muzzle Flash) 粒子、开火后坐力 (Recoil)。
* **命中检测**：利用射线检测 (Raycasting) 对靶球包围盒进行精准命中判定。
* **交互特效**：命中后触发屏幕抖动 (Screen Shake)、靶球破碎粒子特效、准心伤害红边提示 (Hit Marker)、击碎音效/视觉反馈，并实时更新 Combo 及得分。

<!-- PLACEHOLDER: 命中靶球反馈截图 -->
![FPS Shooting Hit Feedback](images/08_hit_feedback.png)
*(占位符：请将射击命中瞬间的粒子和特效截图命名为 `08_hit_feedback.png` 存放在 `docs/images/` 目录下)*

---

### 6. Debug 交互控制台与模块化重构
按 `F1` 键可调出运行时中文参数面板，支持热键操作以在面试或展示时直观展示画面变化：
* `F2`：开关屏幕空间后处理 (Post-Processing)
* `F3`：开关基于图像的环境光照 (IBL)
* `F4`：在 PCF 与 PCSS 软阴影模式之间进行切换
* `F5`：开关光晕提取与高斯模糊 (Bloom)
* `SPACE`：开关线框模式以查看地形和模型网格
* `1 / 2 / 3`：热切换地图场景
* `TAB` & `← / →`：手动微调当前的曝光、Bloom 强度/半径/阈值、Shadow Bias 参数并实时查看画面效果。

<!-- PLACEHOLDER: Debug 控制台调参界面截图 -->
![Debug Overlay Showcase](images/09_debug_overlay.png)
*(占位符：请将参数调整界面的截图命名为 `09_debug_overlay.png` 存放在 `docs/images/` 目录下)*

---

## 📂 核心代码展示

### ACES 电影级色调映射与色彩调整
以下为 [post_process.fs](file:///d:/course/over-learn/OpenGL/project/opengl_try/shaders/post_process.fs) 的后处理核心片段，展示了曝光调节、ACES 映射、对比度、饱和度及暗角的组合实现：

```glsl
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomTexture;

// 画面分级参数
const float exposure = 0.62;
const float contrast = 1.03;
const float saturation = 1.02;
const float vignette = 0.08;

// ACES Tone Mapping 拟合函数
vec3 ACESFilm(vec3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    vec3 color = texture(sceneTexture, TexCoords).rgb;
    
    // 1. 曝光缩放
    color *= exposure;
    
    // 2. 电影级 ACES Tone Mapping 映射到 [0,1]
    color = ACESFilm(color);
    
    // 3. 对比度调整 (以 0.5 为基准进行线性拉伸)
    color = (color - 0.5) * contrast + 0.5;
    
    // 4. 饱和度调整 (利用亮度系数灰度化后混合)
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, saturation);
    
    // 5. 屏幕边缘暗角 (Vignette) 遮罩
    vec2 uv = TexCoords - 0.5;
    float dist = dot(uv, uv);
    color *= (1.0 - dist * vignette);
    
    FragColor = vec4(color, 1.0);
}
```

---

## 📈 项目收获与反思

* **渲染状态管理**：通过实现 `GLStateGuard` 类，对混合深度测试、面剔除、写入遮罩等 transient OpenGL 状态进行局部保护，避免了复杂的后处理和 UI 流程破坏主管线的 PBR/阴影绑定状态。
* **内存与资源安全**：摒弃了裸指针，项目整体基于 C++ **RAII** 规范，使用 `std::unique_ptr` 进行 Shader、Mesh、FBO 的生命周期自治，消除了渲染器切换导致的 VRAM 泄漏。
* **数据驱动意识**：不仅用配置控制资产路径，还将物理常量（如光源半角、材质曝光系数）提炼到 JSON 配置文件中，大幅度缩短了美术和逻辑调优的等待时间。
