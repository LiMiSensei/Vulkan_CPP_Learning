cmake_minimum_required(VERSION 4.3)
project(VulkanTest)

set(CMAKE_CXX_STANDARD 20)

set(BUILD_SHARED_LIBS OFF)

# 添加源码库文件
add_subdirectory(Library/glfw-3.5.1)
add_subdirectory(Library/glm-1.0.3)

# Assimp：加载 FBX / OBJ / glTF 等模型文件（源码方式一起编译）
set(ASSIMP_BUILD_TESTS OFF)             # 默认 ON，会连带编 googletest，很慢
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
set(ASSIMP_BUILD_SAMPLES OFF)
set(ASSIMP_INSTALL OFF)                 # 以 subdirectory 方式使用，不需要安装规则
set(ASSIMP_WARNINGS_AS_ERRORS OFF)      # 不让第三方代码的警告变成编译错误
add_subdirectory(Library/assimp-6.0.5)

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


#可执行文件（单文件渲染，不再编译 Header 目录）
add_executable(VulkanTest
        ${IMGUI_SOURCES}
        main.cpp
        Header/Other.h
        Test/TestRender.h
        Test/TestRender.cpp
        Test/Test_ImguiVulaknBackend.cpp
        Test/Test_ImguiVulaknBackend.h
        Test/GUI_ShaderSystem.cpp
        Test/GUI_ShaderSystem.h
        Test/GUI_Hierarchy.cpp
        Test/GUI_Hierarchy.h
        Test/GUI_Mesh.cpp
        Test/GUI_Mesh.h
)

#编译器
target_include_directories(VulkanTest PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${VULKAN_SDK}/Include
        "$ENV{VULKAN_SDK}/Include/dxc"
)

#链接器
target_link_libraries(VulkanTest PRIVATE
        glfw
        glm::glm
        Vulkan::Vulkan
        assimp::assimp
        "$ENV{VULKAN_SDK}/Lib/dxcompiler.lib")


#拷贝文件
add_custom_command(TARGET VulkanTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/Library/VulkanTools/dxcompiler.dll"
                "$<TARGET_FILE_DIR:VulkanTest>"
        COMMENT "Copying dxcompiler.dll next to the executable")

add_custom_command(TARGET VulkanTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/shaders
                $<TARGET_FILE_DIR:VulkanTest>/shaders
        COMMENT "Copying shaders to $<TARGET_FILE_DIR:VulkanTest>/shaders")

# 如果Win32编译
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            kernel32.lib user32.lib gdi32.lib
            winspool.lib comdlg32.lib advapi32.lib
            shell32.lib ole32.lib oleaut32.lib uuid.lib)
endif ()


# 源码是 UTF-8（注释里有中文），只给本项目加 /utf-8
if (MSVC)
    target_compile_options(VulkanTest PRIVATE /utf-8)
endif()

