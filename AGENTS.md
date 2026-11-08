# Repository Guidelines

## 项目结构与模块组织

本项目是基于 C++17 / OpenGL 3.3 的 FPS 练习程序，使用 Visual Studio 2022（v143）构建。

- `src/` 存放实现，`include/` 存放对应头文件；入口为 `src/main.cpp`，主循环由 `GameManager` 组织。
- `shaders/` 按渲染阶段组织 GLSL，顶点与片元着色器分别使用 `.vs`、`.fs`。
- `resources/maps_config.json` 配置地图、灯光和场景；`resources/objects/`、`resources/textures/`、`resources/fonts/` 保存运行资源。
- `scripts/` 提供资源生成与冒烟检查；`DLL/` 保存运行依赖；`x64/`、`tmp/` 为本机构建和日志目录。

新增 C++ 模块时，同时更新 `opengl_try.vcxproj` 和 `.vcxproj.filters`。

## 构建、测试与本地运行

在仓库根目录的 VS 2022 Developer PowerShell 中执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
$env:PATH = "$PWD\DLL;$env:PATH"
.\x64\Debug\opengl_try.exe
```

第一条命令自动定位 MSBuild 并构建 Debug x64；后两条设置运行依赖搜索路径并启动程序。保持工作目录为仓库根目录，以便加载相对路径资源。

本机需配置 GLFW、GLAD、GLM、Assimp 的头文件及链接库；使用 `AIMLAB_INCLUDE_DIRS`、`AIMLAB_LIBRARY_DIRS`（多目录以分号分隔）或个人属性表配置路径，勿提交机器专属路径。Assimp Debug DLL 说明见 `DLL/README.md`。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke_build_run.ps1 -RunSeconds 15
```

该脚本调用 `scripts/build.ps1`，构建并短暂运行程序，检查初始化日志、提前退出和已知错误。需要图形桌面和支持 OpenGL 3.3 的驱动。

## 编码风格与命名

遵循现有 C++ 风格：四空格缩进，块括号另起一行；类和方法使用 `PascalCase`，局部变量使用 `camelCase`，成员使用 `m_` 前缀，文件使用 `snake_case`。采用 RAII 与 `std::unique_ptr` 管理所有权，渲染阶段注意恢复临时 OpenGL 状态。

遵循 `.editorconfig` 与 `.gitattributes`：UTF-8 编码，源码、着色器和 Markdown 使用 LF，VS 工程及 Windows 脚本使用 CRLF。当前没有统一格式化或 lint 工具；保持相邻代码风格，避免无关格式调整。

## 验证要求

当前没有独立单元测试框架、测试命名约定或覆盖率门槛。代码变更至少完成 Debug x64 构建和可用的冒烟检查；日志位于 `tmp/smoke_run.log` 与 `tmp/smoke_run.err.log`。手动验证移动、射击、`1/2/3` 切图、空格线框及退出；渲染修改还需检查阴影、天空盒、HUD 和后处理。

修改初始化、错误传播或资源清理时，构建后运行 `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\check_startup_failures.ps1`。该脚本在 `tmp/` 副本中验证成功启动和故障退出，不改原始 shader。

## 提交与 Pull Request

沿用 Git 历史和 `.gitmessage` 的 Conventional Commits：`feat:`、`fix:`、`docs:`、`refactor:` 等，可带作用域，例如 `fix(shader): restore shadow sampling`。英文标题使用祈使句、控制在 70 字符内且不加句号。

PR 应说明问题、行为变化、验证命令与结果，并关联相关 issue；视觉变更附截图或录屏，注明地图和渲染设置。提交仅包含相关源码、配置和必要资源，排除构建产物、日志及个人 IDE 配置。
