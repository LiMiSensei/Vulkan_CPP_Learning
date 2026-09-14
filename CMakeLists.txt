cmake_minimum_required(VERSION 4.3)
project(Vulkan_CPP_Learning)

set(CMAKE_CXX_STANDARD 20)

# UTF-8 encoding for MSVC
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")


# 添加源码库文件
add_subdirectory(Library/glfw-3.5.1)
add_subdirectory(Library/glm-1.0.3)

# 查找系统SDK
find_package(Vulkan REQUIRED)


# 宏 设置imgui相关文件
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/Library/imgui-1.92.9b)
set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        
)


#可执行文件
add_executable(Vulkan_CPP_Learning
        ${IMGUI_SOURCES}
        main.cpp
        Test/test1.cpp
        Header/VulkanRenderer.cpp
        Header/VulkanRenderer.h
        Header/Utilities.cpp
        Header/Utilities.h
)

#编译器
target_include_directories(Vulkan_CPP_Learning PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends ${VULKAN_SDK}/Include)

# 链接器
target_link_libraries(Vulkan_CPP_Learning PRIVATE glfw glm::glm Vulkan::Vulkan ${DXCOMPILER_LIB})
# 如果Win32编译
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            kernel32.lib user32.lib gdi32.lib
            winspool.lib comdlg32.lib advapi32.lib
            shell32.lib ole32.lib oleaut32.lib uuid.lib)
endif ()


#if (MSVC)
#    # 源码为 UTF-8（无 BOM），按 UTF-8 读取，避免中文注释触发 C4819 警告
#    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
#    # GLEW 只提供了 Release 静态库：
#    # LNK4099 缺少 glew32s.pdb（Release 库不带调试信息）
#    # LNK4098 Release 库的 CRT 与 Debug 构建不同（链接时无害）
#    # 两者均不影响功能，忽略即可
#    target_link_options(${PROJECT_NAME} PRIVATE /ignore:4098,4099)
#endif ()


# 设置某些禁用
# set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
# set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
# set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)


# ---------------------------------------------------------------------------
# POST_BUILD：把运行所需文件拷贝到 exe 目录
#   1) dxcompiler.dll —— main.cpp 通过 DXC 运行时编译 HLSL
#   2) 新版 VC 运行库 —— 本机 System32 的 VCRUNTIME/MSVCP 只有 14.27，
#      新版 dxcompiler.dll 依赖更新的导出，会初始化失败，需随程序携带
#   3) shaders 源码目录（HLSL 文件，运行时编译读取）
# ---------------------------------------------------------------------------

# 从 VS redist 中取最新版本的 VC 运行库目录
# get_filename_component(_msvc_bin_dir ${CMAKE_CXX_COMPILER} DIRECTORY)   # .../VC/Tools/MSVC/<ver>/bin/Hostx64/x64
# file(GLOB _vcrt_files "${_msvc_bin_dir}/../../../../../../Redist/MSVC/*/x64/*/VCRUNTIME140.dll")
# list(SORT _vcrt_files)
# list(POP_BACK _vcrt_files _vcrt140)
# get_filename_component(VCRT_DIR ${_vcrt140} DIRECTORY)
# if(NOT EXISTS ${VCRT_DIR}/VCRUNTIME140.dll)
#     message(FATAL_ERROR "VC runtime DLLs not found under MSVC redist")
# endif()

# add_custom_command(TARGET Vulkan_CPP_Learning POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VULKAN_SDK}/bin/dxcompiler.dll $<TARGET_FILE_DIR:Vulkan_CPP_Learning>/dxcompiler.dll
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VCRT_DIR}/VCRUNTIME140.dll $<TARGET_FILE_DIR:Vulkan_CPP_Learning>/VCRUNTIME140.dll
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VCRT_DIR}/VCRUNTIME140_1.dll $<TARGET_FILE_DIR:Vulkan_CPP_Learning>/VCRUNTIME140_1.dll
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VCRT_DIR}/MSVCP140.dll $<TARGET_FILE_DIR:Vulkan_CPP_Learning>/MSVCP140.dll
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VCRT_DIR}/MSVCP140_1.dll $<TARGET_FILE_DIR:Vulkan_CPP_Learning>/MSVCP140_1.dll
#     COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:Vulkan_CPP_Learning>/shaders
# )
# DXC (HLSL -> SPIR-V)：main.cpp 运行时编译 HLSL 依赖的库
# if (NOT DEFINED VULKAN_SDK)
#     set(VULKAN_SDK $ENV{VULKAN_SDK})
# endif ()
# find_library(DXCOMPILER_LIB dxcompiler HINTS ${VULKAN_SDK}/bin ${VULKAN_SDK}/Lib)
# if (NOT DXCOMPILER_LIB)
#     message(FATAL_ERROR "dxcompiler.lib not found in Vulkan SDK (${VULKAN_SDK})")
# endif ()