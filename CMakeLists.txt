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
        Header/Utilities.h
        Header/VulkanValidation.h
        Header/VulkanRenderer.cpp
        Header/VulkanRenderer.h
        Header/Mesh.h
        Header/Mesh.cpp
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



add_custom_command(TARGET Vulkan_CPP_Learning POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Vulkan_CPP_Learning>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/shaders/vert.spv
                ${CMAKE_SOURCE_DIR}/shaders/frag.spv
                $<TARGET_FILE_DIR:Vulkan_CPP_Learning>/shaders
        COMMENT "Copying shaders (vert.spv, frag.spv) next to executable"
)


