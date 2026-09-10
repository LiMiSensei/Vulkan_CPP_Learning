# Vulkan_CPP_Learning

基于 **GLFW + GLM + Vulkan + Imgui** 的 C++ 渲染学习项目：绘制一个彩色三角形，着色器使用 **HLSL** 并在运行时通过 **DXC** 编译为 SPIR-V。

## 技术栈

| 组件 | 用途 |
| ---- | ---- |
| Vulkan API | 图形渲染 |
| GLFW 3.5.1 | 窗口管理 |
| GLM 1.0.3 | 数学库 |
| HLSL + DXC | 着色器编写与运行时编译（`dxcompiler.dll` → SPIR-V） |
| ImGui 1.92.9b | 已集成，预留 UI |

## 环境要求

- Windows
- Vulkan SDK（提供 `dxcompiler.lib` / `dxcompiler.dll`）
- CMake ≥ 4.3
- Visual Studio 2022+（MSVC，C++20）



- Windows
- Vulkan SDK（提供 `dxcompiler.lib` / `dxcompiler.dll`）
- CMake ≥ 4.3
- Visual Studio 2022+（MSVC，C++20）
## 目录结构

```
.
├── main.cpp          # Vulkan 渲染主流程 + DXC 运行时编译 HLSL
├── CMakeLists.txt    # 构建配置（依赖、POST_BUILD 拷贝运行库）
├── shaders/          # HLSL 着色器源码（运行时读取编译）
│   ├── triangle.vert.hlsl   # 顶点着色器
│   └── triangle.frag.hlsl   # 片元着色器
└── Library/          # 本地第三方依赖（glfw / glm / imgui 源码）
```

## 构建

```powershell
cmake -S . -B cmake-build-release-visual-studio -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release-visual-studio --config Release
```

## 运行

```powershell
.\cmake-build-release-visual-studio\Release\Vulkan_CPP_Learning.exe
```

启动后弹出 800×600 窗口并显示一个彩色三角形，窗口可缩放（自动重建交换链）。

## HLSL 编译说明

项目**不在构建时预编译**着色器，而是 `main.cpp` 在运行时调用 DXC 将 `shaders/` 下的 HLSL 源码编译为 SPIR-V（target 为 `vulkan1.0`，profile 为 `vs_6_0` / `ps_6_0`）。

因此 exe 目录需要携带以下文件（`CMakeLists.txt` 的 POST_BUILD 命令已自动拷贝）：

- `dxcompiler.dll` —— DXC 编译器本体
- 新版 VC 运行库（`VCRUNTIME140.dll` / `MSVCP140.dll` 等）—— 本机 System32 中的旧版运行库与新版 `dxcompiler.dll` 不兼容
- `shaders/` 目录 —— 运行时读取的 HLSL 源码
