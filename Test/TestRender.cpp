//
// Created by LiMi on 2026/9/29.
//

// Vulkan 的裁剪空间深度范围是 0~1，必须在包含 glm 之前定义，否则 glm::perspective 会按 OpenGL 的 -1~1 来算
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "TestRender.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "GUI_Mesh.h"
#include "Test_ImguiVulaknBackend.h"
#include "../Header/Other.h"
#include "GLFW/glfw3.h"
#include "glm/gtc/vec1.hpp"

static void DebugLog(VkResult result, const char* message) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(message);
    }
}

void TestRender::初始化函数(GLFWwindow* window)
{
    this->window = window;
    auto 步骤 = [](const char* 名) { std::cerr << "[init] " << 名 << std::endl; };
    try
    {
        步骤("创建实例_1");        创建实例_1();
        步骤("创建表面_2");        创建表面_2();
        步骤("选物理设备_3");       选物理设备_3();
        步骤("创建逻辑设备_4");     创建逻辑设备_4();
        步骤("创建命令池_5");       创建命令池_5();
        步骤("创建交换链_6");       创建交换链_6();

        步骤("选定采样数_7_3");    sampleCount = 选定采样数_7_3();
        std::cerr << "[init] 选定采样数 = " << static_cast<int>(sampleCount) << std::endl;
        步骤("创建渲染通道_7");      创建渲染通道_7();

        //1-描述符集布局（binding 0 = 相机 UBO）：建材质管线要用它
        步骤("创建描述符集布局_8");    创建描述符集布局_8();
        //2-MSAA 颜色附件 & 深度附件（必须早于帧缓冲）
        步骤("创建MSAA颜色附件_12");  创建MSAA颜色附件_12();
        步骤("创建深度附件_13");      创建深度附件_13();
        步骤("创建帧缓冲_14");       创建帧缓冲_14();

        //3-相机 UBO 与它的描述符池 / 描述符集
        步骤("创建UniformBuffer_18"); 创建UniformBuffer_18();
        步骤("创建描述符池_19");        创建描述符池_19();
        步骤("分配相机描述符集_20");     分配相机描述符集_20();

        //4-模型/材质的 GPU 资源（顶点索引缓冲、材质管线）在渲染循环里按需创建，
        //  因为模型是运行时通过 GUI 导入的，初始化这个时间点一个模型都还没有。
        //  见 渲染函数 开头的补建逻辑：shader 为空（未赋材质）的子网格会被跳过。

        步骤("分配绘制命令缓冲_21");    分配绘制命令缓冲_21();
        步骤("创建同步对象_22");       创建同步对象_22();
        步骤("初始化ImGui_23");       初始化ImGui_23();

        初始化完成 = true;   // 全部成功，渲染循环才能开跑
        步骤("初始化全部完成");
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        std::cerr.flush();   // 立刻刷出来，免得崩溃时丢失这条关键信息
    }
}

void TestRender::渲染函数()
{
     auto& cameraGUI = GUI.hierarchy;
    //0-初始化没成功就别跑了：否则拿空的 Vulkan 句柄 / 未创建的 ImGui 上下文继续跑，会直接访问违规
    if (!初始化完成)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    //0.1-窗口最小化就整帧跳过，一条 Vulkan 命令都不发。
    //  这一条必须放在最前面（比 GUI.draw() 还早）。最小化后帧缓冲是 0×0，此时：
    //    - vkAcquireNextImageKHR 会因为表面尺寸变成 0×0 而失败（多数驱动报 OUT_OF_DATE，
    //      有的报 SURFACE_LOST，而 3 那里只把 OUT_OF_DATE 当成"该重建了"，其余一律抛异常）
    //    - 拿 0×0 去重建交换链也没有合法尺寸可建
    //    - ImGui 的 DockSpace 拿到的是 0 尺寸视口
    //  原来只在 0.9 那步查尺寸，可那时 GUI.draw() 已经跑完、取图也已经发出去了，根本来不及。
    //  glfwWaitEvents() 睡在事件上（不空转 CPU），窗口一还原就返回，下一帧尺寸正常自动恢复
    //  这里两个条件都要查：只查帧缓冲尺寸的话，依赖"WM_SIZE 已经被这一帧的 glfwPollEvents 处理过"，
    //  万一某次最小化时尺寸还没来得及更新成 0，这一帧就会带着旧尺寸继续往下走；
    //  GLFW_ICONIFIED 是窗口自己的状态位，最小化那一刻就置上了，更早、更可靠
    const bool 已最小化 = glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE;
    int 帧缓冲宽 = 0;
    int 帧缓冲高 = 0;
    glfwGetFramebufferSize(window, &帧缓冲宽, &帧缓冲高);
    if (已最小化 || 帧缓冲宽 == 0 || 帧缓冲高 == 0)
    {
        std::cerr << "[render] 窗口已最小化(帧缓冲 " << 帧缓冲宽 << "x" << 帧缓冲高 << ")，本帧跳过，等窗口还原" << std::endl;
        glfwWaitEvents();
        return;
    }

    //调试用：前 3 帧逐步打点，定位渲染循环里的崩溃点
    static int 帧计数 = 0;
    const bool 追踪 = (帧计数 < 3);
    ++帧计数;
    if (追踪) std::cerr << "[render] 帧 " << 帧计数 << " 开始（帧缓冲 " << 帧缓冲宽 << "x" << 帧缓冲高 << "）" << std::endl;

    //imgui渲染
    GUI.draw();
    if (追踪) std::cerr << "[render] GUI.draw 完成" << std::endl;

    // 0.5-处理相机操作。必须放在 GUI.draw() 之后：ImGui 的 WantCaptureMouse / WantCaptureKeyboard
    //     要到 NewFrame 之后才有效，靠它们判断输入是不是该交给面板
     cameraGUI.处理相机输入(window);
    cameraGUI.处理灯光信息();   // Shift + 右键拖拽转主光方向；不按 Shift 时里面直接 return，不影响相机

    // 0-模型是运行时通过 GUI 导入的，初始化时还不存在，所以每帧补建一次：
    //   shader 为空（还没赋材质）的跳过；已经建过管线的也不再重复建
    for (auto& model : GUI.meshGUI.models)
    {
        for (auto& mesh : model.subMesh)
        {
            if (mesh.pipeline != VK_NULL_HANDLE) continue;   // 已经建好，跳过
            if (mesh.material.shader.spvVert.empty() || mesh.material.shader.spvFrag.empty())
            {
                continue;                                     // shader 为空，跳过
            }
            上传网格缓冲(mesh);
            创建材质管线(mesh);
        }
    }

    // 0.9-窗口相关处理统一交给 GUI_Hierarchy（和 处理相机输入 一个路子）：
    //     最小化、以及"帧缓冲尺寸和交换链尺寸不一致"都在里面判断。
    //     返回 true 说明该重建交换链了，重建完本帧直接跳过，下一帧用新交换链画，
    //     这样就不会出现"旧尺寸图像被窗口缩放"的拉伸
    if ( cameraGUI.检测尺寸变化(window, swapchain.extent))
    {
        重建交换链();
        return;
    }

    // 1-本帧资源
    FrameResources& frame = frames[currentFrame];

    // 2-等这一帧上一次提交完成
    //   注意：重置栅栏要放到"取图成功"之后。取图可能因交换链过期而提前 return，
    //   那一帧不会提交，栅栏必须保持已发出状态，否则下一帧会等一个永远不会发出的栅栏而死锁
    DebugLog(vkWaitForFences(device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX), "Failed to wait for fence!");

    // 2.5-先把这一帧的相机矩阵写进 UBO（Hierarchy 面板改的就是同一份相机；
    //      aspect 用的是 swapchain.extent，尺寸变化已经在 0.9 处理过了）
    更新相关Uniform();

    // 3-取一张交换链图像
    uint32_t imageIndex = 0;
    VkResult 取图结果 = vkAcquireNextImageKHR(device, swapchain.handle, UINT64_MAX, frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);
    if (取图结果 == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // 交换链已经和表面不匹配（通常就是窗口尺寸刚变）：重建，本帧作罢
        重建交换链();
        return;
    }
    if (取图结果 != VK_SUCCESS && 取图结果 != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("Failed to acquire swapchain image!");
    }

    // 3.5-这一帧确定会提交了，才重置栅栏
    DebugLog(vkResetFences(device, 1, &frame.inFlight), "Failed to reset fence!");

    // 4-重置并开始录制命令缓冲
    DebugLog(vkResetCommandBuffer(frame.commandBuffer, 0), "Failed to reset command buffer!");
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;  // 结构体类型标识
    DebugLog(vkBeginCommandBuffer(frame.commandBuffer, &beginInfo), "Failed to begin command buffer!");

    // 5-开始渲染通道（清屏：颜色 + 深度）
    VkClearValue clearValues[3] = {};                      // 下标要和渲染通道的附件一一对应
    clearValues[0].color = { cameraGUI.camera.clearColor.x, cameraGUI.camera.clearColor.x, cameraGUI.camera.clearColor.x};
    clearValues[1].depthStencil = { 1.0f, 0 };             // 附件 1：深度清成 1.0
    // 附件 2（解析目标）不 clear，占个位就行

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;  // 结构体类型标识
    renderPassInfo.renderPass = renderPass;                           // 使用的渲染通道
    renderPassInfo.framebuffer = swapchain.framebuffers[imageIndex];  // 对应本次取得的图像
    renderPassInfo.renderArea.offset = { 0, 0 };                      // 渲染区域左上角
    renderPassInfo.renderArea.extent = swapchain.extent;              // 渲染区域尺寸
    renderPassInfo.clearValueCount = 3;                               // 3 个附件各占一个清屏值
    renderPassInfo.pClearValues = clearValues;                        // 清屏值
    vkCmdBeginRenderPass(frame.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 6-画模型
    //  pipelineLayout 为空说明这次一个材质都没赋过，整段跳过（否则会绑一个无效布局）
    if (pipelineLayout != VK_NULL_HANDLE)
    {
        // 6.0-视口 / 裁剪矩形是管线里的动态状态，每帧用当前的交换链尺寸设一次。
        //     这样重建交换链就不必把所有材质管线都重建一遍（建管线比设视口贵得多）
        VkViewport viewport = {};
        viewport.x = 0.0f;                                              // 左上角 X
        viewport.y = 0.0f;                                              // 左上角 Y
        viewport.width = static_cast<float>(swapchain.extent.width);    // 视口宽度
        viewport.height = static_cast<float>(swapchain.extent.height);  // 视口高度
        viewport.minDepth = 0.0f;                                       // 深度最小值
        viewport.maxDepth = 1.0f;                                       // 深度最大值
        vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };                     // 左上角偏移
        scissor.extent = swapchain.extent;             // 裁剪范围
        vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

        // 所有 SubMesh 共用同一份相机描述符集（绑定点 0）
        vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                                0, 1, &cameraDescriptorSets[currentFrame], 0, nullptr);

        for (auto& model : GUI.meshGUI.models)
        {
            for (auto& mesh : model.subMesh)
            {
                // shader 为空 / 没建管线的子网格直接跳过
                if (mesh.pipeline == VK_NULL_HANDLE) continue;
                vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh.pipeline);

                const glm::mat4 模型矩阵 = mesh.transform.toMatrix();
                vkCmdPushConstants(frame.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,sizeof(glm::mat4), &模型矩阵);

                const VkDeviceSize vertexOffset = 0;
                vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, &mesh.vertexBuffer, &vertexOffset);
                vkCmdBindIndexBuffer(frame.commandBuffer, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(frame.commandBuffer, mesh.indexCount, 1, 0, 0, 0);
            }
        }
    }

    // 7-把 ImGui 的绘制数据录进命令缓冲（叠在模型上面）
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame.commandBuffer);

    // 8-结束渲染通道和录制
    vkCmdEndRenderPass(frame.commandBuffer);
    DebugLog(vkEndCommandBuffer(frame.commandBuffer), "Failed to end command buffer!");

    // 9-提交：等图像可用，完成后发出渲染完成信号量
    VkSemaphore waitSemaphores[] = { frame.imageAvailable };                 // 要等的信号量
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // 在颜色输出阶段等
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[imageIndex] }; // 完成后通知的信号量

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;               // 结构体类型标识
    submitInfo.waitSemaphoreCount = 1;                              // 等待的信号量数量
    submitInfo.pWaitSemaphores = waitSemaphores;                    // 等待的信号量
    submitInfo.pWaitDstStageMask = waitStages;                      // 等待发生在哪个阶段
    submitInfo.commandBufferCount = 1;                              // 命令缓冲数量
    submitInfo.pCommandBuffers = &frame.commandBuffer;              // 提交的命令缓冲
    submitInfo.signalSemaphoreCount = 1;                            // 发出的信号量数量
    submitInfo.pSignalSemaphores = signalSemaphores;                // 发出的信号量
    DebugLog(vkQueueSubmit(graphics, 1, &submitInfo, frame.inFlight), "Failed to submit draw command buffer!");

    // 10-呈现到屏幕
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;         // 结构体类型标识
    presentInfo.waitSemaphoreCount = 1;                             // 等待的信号量数量
    presentInfo.pWaitSemaphores = signalSemaphores;                 // 等渲染完成再呈现
    presentInfo.swapchainCount = 1;                                 // 交换链数量
    presentInfo.pSwapchains = &swapchain.handle;                    // 交换链
    presentInfo.pImageIndices = &imageIndex;                        // 要呈现的图像下标
    VkResult 呈现结果 = vkQueuePresentKHR(present, &presentInfo);
    // 呈现时才知道交换链过期（比如这帧期间窗口尺寸又变了）：下一帧开始前重建。
    // 只认 OUT_OF_DATE；SUBOPTIMAL 只代表"还能用但不再最优"（窗口被遮挡时可能一直报），
    // 跟着它重建会变成每帧重建，得不偿失——真正的尺寸变化交给 GUI_Hierarchy::检测尺寸变化 去发现
    if (呈现结果 == VK_ERROR_OUT_OF_DATE_KHR)
    {
        重建交换链();
    }
    else if (呈现结果 != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present swapchain image!");
    }

    // 11-推进到下一帧
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void TestRender::清理函数()
{
    if (device == VK_NULL_HANDLE)
    {
        return;   // 设备都没建起来，没什么可清的
    }

    //1-先等设备上的活干完，再动资源
    vkDeviceWaitIdle(device);

    //2-每个子网格的顶点 / 索引缓冲与材质管线
    for (auto& model : GUI.meshGUI.models)
    {
        for (auto& mesh : model.subMesh)
        {
            if (mesh.vertexBuffer != VK_NULL_HANDLE)       vkDestroyBuffer(device, mesh.vertexBuffer, nullptr);
            if (mesh.vertexBufferMemory != VK_NULL_HANDLE) vkFreeMemory(device, mesh.vertexBufferMemory, nullptr);
            if (mesh.indexBuffer != VK_NULL_HANDLE)        vkDestroyBuffer(device, mesh.indexBuffer, nullptr);
            if (mesh.indexBufferMemory != VK_NULL_HANDLE)  vkFreeMemory(device, mesh.indexBufferMemory, nullptr);
            if (mesh.pipeline != VK_NULL_HANDLE)           vkDestroyPipeline(device, mesh.pipeline, nullptr);

            mesh.vertexBuffer = VK_NULL_HANDLE;       mesh.vertexBufferMemory = VK_NULL_HANDLE;
            mesh.indexBuffer = VK_NULL_HANDLE;        mesh.indexBufferMemory = VK_NULL_HANDLE;
            mesh.pipeline = VK_NULL_HANDLE;
        }
    }

    //3-共享管线布局
    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    //4-MSAA 颜色附件与深度附件
    if (msaaImage.view != VK_NULL_HANDLE)   vkDestroyImageView(device, msaaImage.view, nullptr);
    if (msaaImage.image != VK_NULL_HANDLE)  vkDestroyImage(device, msaaImage.image, nullptr);
    if (msaaImage.memory != VK_NULL_HANDLE) vkFreeMemory(device, msaaImage.memory, nullptr);
    if (depthImage.view != VK_NULL_HANDLE)   vkDestroyImageView(device, depthImage.view, nullptr);
    if (depthImage.image != VK_NULL_HANDLE)  vkDestroyImage(device, depthImage.image, nullptr);
    if (depthImage.memory != VK_NULL_HANDLE) vkFreeMemory(device, depthImage.memory, nullptr);

    //5-每帧的相机统一缓冲
    for (FrameResources& frame : frames)
    {
        if (frame.uniformBufferMapped != nullptr)
        {
            vkUnmapMemory(device, frame.uniformBuffer.memory);
            frame.uniformBufferMapped = nullptr;
        }
        if (frame.uniformBuffer.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, frame.uniformBuffer.buffer, nullptr);
        if (frame.uniformBuffer.memory != VK_NULL_HANDLE) vkFreeMemory(device, frame.uniformBuffer.memory, nullptr);
        frame.uniformBuffer.buffer = VK_NULL_HANDLE;
        frame.uniformBuffer.memory = VK_NULL_HANDLE;
    }
}

void TestRender::重建交换链()
{
    //0-窗口最小化时帧缓冲是 0×0，这时候的重建没有意义（也没有合法尺寸可建），直接跳过。
    //  但不能什么都不做就 return：acquire 会一直返回 OUT_OF_DATE，主循环又不等事件，
    //  那样就变成每帧重建一次交换链的死循环（CPU 拉满 + 反复申请显存）。
    //  睡在事件上等窗口还原即可——还原后由 检测尺寸变化 发现尺寸不一致，再正常走一次重建
    int 宽 = 0;
    int 高 = 0;
    glfwGetFramebufferSize(window, &宽, &高);
    if (宽 == 0 || 高 == 0)
    {
        std::cerr << "[resize] 窗口最小化中(帧缓冲 " << 宽 << "x" << 高 << ")，跳过本次交换链重建" << std::endl;
        glfwWaitEvents();
        return;
    }

    //1-先把设备上的活全干完：下面要销毁的图像 / 帧缓冲可能还被上一帧的 GPU 命令引用着
    vkDeviceWaitIdle(device);

    //2-拆掉所有按旧尺寸建的东西
    //  渲染通道只用到格式和采样数，和尺寸无关，不用重建；
    //  材质管线用的是动态视口，也不用重建
    for (VkFramebuffer framebuffer : swapchain.framebuffers)
    {
        if (framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    swapchain.framebuffers.clear();

    for (VkImageView view : swapchain.imageViews)
    {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
    }
    swapchain.imageViews.clear();
    swapchain.images.clear();   // 交换链图像归交换链所有，销毁交换链时会一起释放，这里只清句柄
    if (swapchain.handle != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device, swapchain.handle, nullptr);
        swapchain.handle = VK_NULL_HANDLE;
    }

    //3-MSAA 颜色附件和深度附件也是按旧尺寸建的
    if (msaaImage.view != VK_NULL_HANDLE)   { vkDestroyImageView(device, msaaImage.view, nullptr);   msaaImage.view = VK_NULL_HANDLE; }
    if (msaaImage.image != VK_NULL_HANDLE)  { vkDestroyImage(device, msaaImage.image, nullptr);      msaaImage.image = VK_NULL_HANDLE; }
    if (msaaImage.memory != VK_NULL_HANDLE) { vkFreeMemory(device, msaaImage.memory, nullptr);       msaaImage.memory = VK_NULL_HANDLE; }
    if (depthImage.view != VK_NULL_HANDLE)  { vkDestroyImageView(device, depthImage.view, nullptr);  depthImage.view = VK_NULL_HANDLE; }
    if (depthImage.image != VK_NULL_HANDLE) { vkDestroyImage(device, depthImage.image, nullptr);     depthImage.image = VK_NULL_HANDLE; }
    if (depthImage.memory != VK_NULL_HANDLE){ vkFreeMemory(device, depthImage.memory, nullptr);      depthImage.memory = VK_NULL_HANDLE; }

    //4-按新尺寸重建（顺序和初始化时一致：后三个都依赖 swapchain.extent）
    创建交换链_6();
    创建MSAA颜色附件_12();
    创建深度附件_13();
    创建帧缓冲_14();

    //5-"渲染完成"信号量是每张交换链图像一个。图像数量有可能变（不同尺寸下 minImageCount 会变），
    //   所以按新数量重建，否则渲染时 renderFinishedSemaphores[imageIndex] 会越界读
    for (VkSemaphore semaphore : renderFinishedSemaphores)
    {
        if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, semaphore, nullptr);
    }
    renderFinishedSemaphores.clear();
    renderFinishedSemaphores.resize(swapchain.images.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (VkSemaphore& semaphore : renderFinishedSemaphores)
    {
        DebugLog(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore), "Failed to recreate render finished semaphore!");
    }

    std::cerr << "[resize] 交换链已重建: " << swapchain.extent.width << " x " << swapchain.extent.height
              << "，图像数 " << swapchain.images.size() << std::endl;
}

void TestRender::创建实例_1()
{
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // 结构体类型标识
    appInfo.pApplicationName = "ModelViewer"; // 应用名
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // 应用版本
    appInfo.pEngineName = "None"; // 没用引擎
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0); // 引擎版本
    appInfo.apiVersion = VK_API_VERSION_1_0; // 目标 Vulkan API 版本

    uint32_t glfwExtensionCount = 0; // GLFW 需要的扩展数量
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount); // GLFW 要求的必需实例扩展
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount); // 拷进 vector 方便追加

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; // 结构体类型标识
    createInfo.pApplicationInfo = &appInfo; // 应用信息
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size()); // 启用的扩展数量
    createInfo.ppEnabledExtensionNames = extensions.data(); // 启用的扩展名字
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = 0;

    vkCreateInstance(&createInfo, nullptr, &instance);
    //out: VkInstance
}

void TestRender::创建表面_2()
{
    glfwCreateWindowSurface(instance, window, nullptr, &surface);
    //out:VkSurfaceKHR
}

void TestRender::选物理设备_3()
{
    //先查询有多少物理设备
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    //再存放物理设备
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    //再挑选物理设备
    for (VkPhysicalDevice device : devices)
    {
        //1-设备家族队列不全 跳过
        if (!查找物理设备_家族队列_3_1(device, surface).isComplete()) continue;
        //2-设备缺扩展 跳过
        if (!查找物理设备_扩展_3_2(device))continue;
        //3-无表面格式 无呈现数量 跳过
        if (!查找物理设备_表面和呈现_3_3(device, surface))continue;
        //4-都合格就选择这块
        physicalDevice = device;
    }
}

void TestRender::创建逻辑设备_4()
{
    // 1-取出物理设备家族队列
    indices = 查找物理设备_家族队列_3_1(physicalDevice, surface);

    // 2-要创建的队列：图形家族和呈现家族（同一个家族时只创建一次）
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};
    float queuePriority = 1.0f; // 队列优先级（0.0 ~ 1.0）
    for (uint32_t family : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;    // 结构体类型标识
        queueCreateInfo.queueFamilyIndex = family;                             // 家族下标
        queueCreateInfo.queueCount = 1;                                        // 该家族要几个队列
        queueCreateInfo.pQueuePriorities = &queuePriority;                     // 优先级数组
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // 3-设备支持的特性（暂时全关）
    VkPhysicalDeviceFeatures supportedFeatures = {};

    // 4-逻辑设备创建信息
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;                            // 结构体类型标识
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());   // 队列创建信息数量
    createInfo.pQueueCreateInfos = queueCreateInfos.data();                             // 队列创建信息数组
    createInfo.pEnabledFeatures = &supportedFeatures;                                   // 启用的设备特性

    // 5-启用交换链扩展：不启用的话 vkCreateSwapchainKHR 是空函数指针
    std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());  // 启用的扩展数量
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();                       // 启用的扩展名字

    // 6-获取逻辑设备 和 队列
    DebugLog(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device),"4:__4");   //获取逻辑设备
    vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphics); // 取出图形队列
    vkGetDeviceQueue(device, indices.presentFamily, 0, &present);   // 取出呈现队列
}

void TestRender::创建命令池_5()
{
    // 1-命令池创建信息
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;          // 结构体类型标识
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;     // 允许单独重置命令缓冲（每帧重录）
    poolInfo.queueFamilyIndex = indices.graphicsFamily;                   // 命令池所属的队列家族
    // 2-创建命令池
    DebugLog(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool), "Failed to create command pool!");
}

void TestRender::创建交换链_6()
{
    // 1-查询表面能力
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
    // 2-选定表面格式
    VkSurfaceFormatKHR surfaceFormat = 选定表面格式6_1(physicalDevice, surface);
    // 3-选定呈现模式
    VkPresentModeKHR presentMode =选定呈现模式6_2(physicalDevice, surface);
    // 4-选定交换链分辨率
    VkExtent2D extent = 选定交换链分辨率6_3(window,capabilities);
    // 5-图像数量
    uint32_t imageCount = capabilities.minImageCount + 1;  // 图像数量
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;  // 不超过允许的最大值
    }
    // 6-交换链创建信息
    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR; // 结构体类型标识
    createInfo.surface = surface;                                   // 用于呈现的表面
    createInfo.minImageCount = imageCount;                          // 最少图像数量
    createInfo.imageFormat = surfaceFormat.format;                  // 图像像素格式
    createInfo.imageColorSpace = surfaceFormat.colorSpace;          // 图像色彩空间
    createInfo.imageExtent = extent;                                // 图像分辨率
    createInfo.imageArrayLayers = 1;                                // 图像数组层数
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;    // 用途：颜色附件
    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;   // 共享模式：并发
        createInfo.queueFamilyIndexCount = 2;                       // 共享的家族数量
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };  // 两个家族下标
        createInfo.pQueueFamilyIndices = queueFamilyIndices;        // 共享的家族下标数组
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;    // 共享模式：独占
    }
    createInfo.preTransform = capabilities.currentTransform;        // 表面预变换
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;  // 与窗口系统混合方式
    createInfo.presentMode = presentMode;                           // 呈现模式
    createInfo.clipped = VK_TRUE;                                   // 允许裁剪不可见区域
    createInfo.oldSwapchain = VK_NULL_HANDLE;                       // 旧交换链句柄
    // 7-创建交换链
    DebugLog(vkCreateSwapchainKHR(device,&createInfo,nullptr,&swapchain.handle),"6:__7");
    // 8-获取交换链图像
    DebugLog(vkGetSwapchainImagesKHR(device, swapchain.handle, &imageCount, nullptr),"6:__8");
    // 9-按实际数量分配图像
    swapchain.images.resize(imageCount);
    DebugLog(vkGetSwapchainImagesKHR(device, swapchain.handle, &imageCount, swapchain.images.data()),"6:__9");
    // 10- 记录
    swapchain.format = surfaceFormat.format;              // 记录交换链格式
    swapchain.extent = extent;                          // 记录交换链分辨率
    swapchain.imageViews.resize(swapchain.images.size());   // 为每张图像分配视图

    // 11-创建
    for (size_t i = 0; i < swapchain.images.size(); i++) {
        // 1-图像视图创建信息
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;        // 结构体类型标识
        viewInfo.image = swapchain.images[i];                                       // 基于哪张图像
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;                        // 2D 视图
        viewInfo.format = swapchain.format;                                         // 格式（要和图像一致）
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // 看哪部分（颜色 / 深度 / 模板）
        viewInfo.subresourceRange.baseMipLevel = 0;                       // 从第 0 级 mip 开始
        viewInfo.subresourceRange.levelCount = 1;                         // 只含 1 级
        viewInfo.subresourceRange.baseArrayLayer = 0;                     // 从第 0 层开始
        viewInfo.subresourceRange.layerCount = 1;                         // 只含 1 层
        // 2-创建出来的图像视图
        VkImageView imageView;
        // 3-创建图像视图
        vkCreateImageView(device, &viewInfo, nullptr, &imageView);
        swapchain.imageViews[i] =  imageView;
    }
}

void TestRender::创建渲染通道_7()
{
    // 3 个附件：0 = MSAA 颜色，1 = 深度，2 = 解析目标（交换链图像）
    VkAttachmentDescription attachments[3] = {};

    //1-附件 0：MSAA 颜色（先画到多重采样图像上，再解析到交换链图像）
    attachments[0].format = swapchain.format;                           // 格式与交换链一致
    attachments[0].samples = sampleCount;                               // 多重采样数
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                // 加载时清空
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;          // 解析完就不需要了
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;     // 没有模板
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;   // 没有模板
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;           // 初始布局：未定义
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // 解析源

    //2-附件 1：深度（同样按多重采样数创建，才能和 MSAA 颜色附件配套）
    const VkFormat depthFormat = 选定深度格式7_1(physicalDevice);        // 深度格式
    attachments[1].format = depthFormat;
    attachments[1].samples = sampleCount;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                // 每帧清成 1.0
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    //3-附件 2：解析目标（单采样，就是交换链图像，直接呈现）
    attachments[2].format = swapchain.format;
    attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;            // 内容由解析写入
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;              // 保留下来呈现
    attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;       // 最终布局：直接呈现

    //4-三个附件引用（下标要和上面的附件一一对应）
    VkAttachmentReference msaaRef = {};                                  // MSAA 颜色引用
    msaaRef.attachment = 0;
    msaaRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef = {};                                 // 深度引用
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference resolveRef = {};                               // 解析目标引用
    resolveRef.attachment = 2;
    resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    //5-子通道描述（颜色 + 解析 + 深度）
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;            // 绑定点：图形
    subpass.colorAttachmentCount = 1;                                       // 颜色附件数量
    subpass.pColorAttachments = &msaaRef;                                   // 画到 MSAA 颜色附件
    subpass.pResolveAttachments = &resolveRef;                              // 解析到交换链图像
    subpass.pDepthStencilAttachment = &depthRef;                            // 挂上深度附件

    //6-子通道依赖（颜色输出 + 深度测试阶段）
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;                            // 源子通道：外部
    dependency.dstSubpass = 0;                                              // 目标子通道：0
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;   // 源阶段：颜色输出 / 深度测试
    dependency.srcAccessMask = 0;                                           // 源访问：无
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;   // 目标阶段：颜色输出 / 深度测试
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;// 目标访问：颜色 / 深度写入

    //7-渲染通道创建信息
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;           // 结构体类型标识
    renderPassInfo.attachmentCount = 3;                                         // MSAA 颜色 + 深度 + 解析目标
    renderPassInfo.pAttachments = attachments;                                  // 附件数组指针
    renderPassInfo.subpassCount = 1;                                            // 子通道数量
    renderPassInfo.pSubpasses = &subpass;                                       // 子通道数组指针
    renderPassInfo.dependencyCount = 1;                                         // 依赖数量
    renderPassInfo.pDependencies = &dependency;                                 // 依赖数组指针

    //6-创建渲染通道
    DebugLog(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass), "Failed to create render pass!");
}

void TestRender::创建描述符集布局_8()
{
    // 1-描述符集布局：
    // binding 0 = 相机 UBO（顶点着色器）
    // binding 1 = 反照率贴图（片元着色器）
    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {};  // 两个绑定项
    bindings[0].binding = 0;  // 绑定点 0
    bindings[1].binding = 1;  // 绑定点 1

    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;          // 类型：uniform 缓冲
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;  // 类型：组合图像采样器

    bindings[0].descriptorCount = 1;  // 描述符数量
    bindings[1].descriptorCount = 1;  // 描述符数量

    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;    // 用于顶点着色器
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;  // 用于片元着色器

    bindings[0].pImmutableSamplers = nullptr;  // 不使用不可变采样器
    bindings[1].pImmutableSamplers = nullptr;  // 不使用不可变采样器

    // 2-布局创建信息
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};                         // 布局创建信息
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;  // 结构体类型标识
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());        // 绑定项数量
    layoutInfo.pBindings = bindings.data();                                  // 绑定项数组

    // 3-创建描述符集布局
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
}

void TestRender::加载着色器模块_9()
{
    vertShaderModule = 读取着色器文件(device,"shaders/vert.spv");
    vertShaderModule = 读取着色器文件(device,"shaders/frag.spv");
}

void TestRender::创建贴图采样器_10()
{
    //1-查询设备属性
    VkPhysicalDeviceProperties props;  // 物理设备属性
    vkGetPhysicalDeviceProperties(physicalDevice, &props);

    //2-查询设备特性
    VkPhysicalDeviceFeatures features;  // 物理设备特性
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);

    //3-采样器创建信息
    VkSamplerCreateInfo samplerInfo = {};                               // 采样器创建信息
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;          // 结构体类型标识
    samplerInfo.magFilter = VK_FILTER_LINEAR;                           // 放大过滤：线性
    samplerInfo.minFilter = VK_FILTER_LINEAR;                           // 缩小过滤：线性
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;          // U 方向寻址：重复
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;          // V 方向寻址：重复
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;          // W 方向寻址：重复
    samplerInfo.anisotropyEnable = features.samplerAnisotropy;          // 是否开启各向异性过滤
    samplerInfo.maxAnisotropy = features.samplerAnisotropy ? props.limits.maxSamplerAnisotropy : 1.0f;  // 最大各向异性倍数
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;         // 边界颜色：不透明黑
    samplerInfo.unnormalizedCoordinates = VK_FALSE;                     // 使用归一化坐标
    samplerInfo.compareEnable = VK_FALSE;                               // 不开启比较模式
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;                       // 比较操作：总是通过
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;             // mipmap 过滤：线性

    //4-创建采样器
    DebugLog(vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler), "Failed to create texture sampler!");  // 创建采样器
}

void TestRender::创建图形管线_11()
{
    //1-着色器阶段
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};  // 顶点着色器阶段
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};  // 片元着色器阶段

    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;// 结构体类型标识
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;// 结构体类型标识
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;                         // 阶段：顶点着色器
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;                       // 阶段：片元着色器
    vertShaderStageInfo.module = vertShaderModule;                                  // 使用的着色器模块
    fragShaderStageInfo.module = fragShaderModule;                                  // 使用的着色器模块
    vertShaderStageInfo.pName = "main";                                             // 入口函数名
    fragShaderStageInfo.pName = "main";                                             // 入口函数名

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    //2-顶点绑定描述
    VkVertexInputBindingDescription bindingDescription = {};    // 顶点绑定描述
    bindingDescription.binding = 0;                             // 绑定点 0
    bindingDescription.stride = sizeof(Other1::Vertex);         // 单个顶点的字节数
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // 每读取一个顶点前进一次

    std::array<VkVertexInputAttributeDescription, 8> attributeDescriptions = {};
    attributeDescriptions[0].binding = 0;  // 绑定点 0
    attributeDescriptions[1].binding = 0;  // 绑定点 0
    attributeDescriptions[2].binding = 0;  // 绑定点 0
    attributeDescriptions[3].binding = 0;  // 绑定点 0
    attributeDescriptions[4].binding = 0;  // 绑定点 0
    attributeDescriptions[5].binding = 0;  // 绑定点 0
    attributeDescriptions[6].binding = 0;  // 绑定点 0
    attributeDescriptions[7].binding = 0;  // 绑定点 0


    attributeDescriptions[0].location = 0;  // 着色器位置 0
    attributeDescriptions[1].location = 1;  // 着色器位置 1
    attributeDescriptions[2].location = 2;  // 着色器位置 2
    attributeDescriptions[3].location = 3;  // 着色器位置 3
    attributeDescriptions[4].location = 4;  // 着色器位置 4
    attributeDescriptions[5].location = 5; // 着色器位置 5
    attributeDescriptions[6].location = 6;  // 着色器位置 6
    attributeDescriptions[7].location = 7;  // 着色器位置 7


    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;   // 格式：3 个 float
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;   // 格式：3 个 float
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;      // 格式：2 个 float
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;   // 格式：3 个 float
    attributeDescriptions[4].format = VK_FORMAT_R32_UINT;           // 格式：32 位无符号整数
    attributeDescriptions[5].format = VK_FORMAT_R32G32_SFLOAT;      // 格式：2 个 float
    attributeDescriptions[6].format = VK_FORMAT_R32G32_SFLOAT;      // 格式：2 个 float
    attributeDescriptions[7].format = VK_FORMAT_R32G32_SFLOAT;      // 格式：2 个 float


    attributeDescriptions[0].offset = offsetof(Other1::Vertex, position);        // 偏移：顶点位置
    attributeDescriptions[1].offset = offsetof(Other1::Vertex, color);           // 偏移：顶点色
    attributeDescriptions[2].offset = offsetof(Other1::Vertex, uv0);             // 偏移：UV0
    attributeDescriptions[3].offset = offsetof(Other1::Vertex, normal);         // 偏移：法线
    attributeDescriptions[4].offset = offsetof(Other1::Vertex, tangents);        // 偏移：mesh 序号
    attributeDescriptions[5].offset = offsetof(Other1::Vertex, uv1);   // 偏移：UV1
    attributeDescriptions[6].offset = offsetof(Other1::Vertex, uv2);   // 偏移：UV2
    attributeDescriptions[7].offset = offsetof(Other1::Vertex, uv3);   // 偏移：UV3



    //3-顶点输入状态
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;  // 结构体类型标识
    vertexInputInfo.vertexBindingDescriptionCount = 1;                                  // 绑定描述数量
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;                   // 绑定描述指针   22222222222
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());  // 属性描述数量 2222222
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();        // 属性描述指针

    //4-图元装配状态
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};                          // 图元装配状态
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;  // 结构体类型标识
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;                       // 图元类型：三角形列表
    inputAssembly.primitiveRestartEnable = VK_FALSE;                                    // 不启用图元重启

    //5-视口
    VkViewport viewport = {};
    viewport.x = 0.0f;                                      // 左上角 X
    viewport.y = 0.0f;                                      // 左上角 Y
    viewport.width = static_cast<float>(swapchain.extent.width);    // 视口宽度
    viewport.height = static_cast<float>(swapchain.extent.height);  // 视口高度
    viewport.minDepth = 0.0f;                               // 深度最小值
    viewport.maxDepth = 1.0f;                               // 深度最大值

    //6-裁剪矩形
    VkRect2D scissor = {};                 // 裁剪矩形
    scissor.offset = { 0, 0 };    // 左上角偏移
    scissor.extent = swapchain.extent;     // 裁剪范围

    //7-视口状态
    VkPipelineViewportStateCreateInfo viewportState = {};  // 视口状态
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;  // 结构体类型标识
    viewportState.viewportCount = 1;        // 视口数量
    viewportState.pViewports = &viewport;   // 视口数组     5555555
    viewportState.scissorCount = 1;         // 裁剪矩形数量
    viewportState.pScissors = &scissor;     // 裁剪矩形数组   666666

    //8-光栅化状态
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;  // 结构体类型标识
    rasterizer.depthClampEnable = VK_FALSE;                         // 不启用深度钳制
    rasterizer.rasterizerDiscardEnable = VK_FALSE;                  // 不丢弃所有图元
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;                  // 多边形模式：填充
    rasterizer.lineWidth = 1.0f;                                    // 线宽
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;                    // 剔除模式
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;         // 正面绕序
    rasterizer.depthBiasEnable = VK_FALSE;                          // 不启用深度偏移

    //9-多重采样状态
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO; // 结构体类型标识
    multisampling.sampleShadingEnable = VK_FALSE;                                   // 不启用采样着色
    multisampling.rasterizationSamples = sampleCount;                               // 采样数

    // 10-深度模板状态
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;    // 结构体类型标识
    depthStencil.depthTestEnable = VK_TRUE;                                             // 是否开启深度测试
    depthStencil.depthWriteEnable = VK_TRUE;                                            // 是否写入深度
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;                                   // 深度比较操作
    depthStencil.depthBoundsTestEnable = VK_FALSE;                                      // 不启用深度范围测试

    // 11-颜色混合附件状态
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; // 写入 RGBA 四个通道
    colorBlendAttachment.blendEnable = VK_TRUE;                                                 // 是否开启混合
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;                       // 源颜色混合因子
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;             // 目标颜色混合因子
    colorBlendAttachment.colorBlendOp =VK_BLEND_OP_ADD;                                         // 颜色混合操作
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;                       // 源 alpha 混合因子
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;             // 目标 alpha 混合因子
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;                                        // alpha 混合操作

    // 12-颜色混合状态
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO; // 结构体类型标识
    colorBlending.logicOpEnable = VK_FALSE;                                         // 不启用逻辑运算
    colorBlending.attachmentCount = 1;                                              // 附件数量
    colorBlending.pAttachments = &colorBlendAttachment;                             // 附件数组   11 11 11 11 11

    // 13-动态状态
    // 视口 / 裁剪 / 模板参考值都是动态状态：模板参考值做成动态的，各模型换参考值就不必重建管线
    std::array<VkDynamicState, 3> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_STENCIL_REFERENCE };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;      // 结构体类型标识
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());   // 动态状态数量
    dynamicState.pDynamicStates = dynamicStates.data();                             // 动态状态数组

    // 14-推送常量范围
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;  // 可用于顶点 / 片元着色器
    pushConstantRange.offset = 0;                                                   // 起始偏移
    pushConstantRange.size = sizeof(Vertex);                                 // 数据大小

    // 15-管线布局创建信息
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;   // 结构体类型标识
    pipelineLayoutInfo.setLayoutCount = 1;                                      // 描述符集布局数量
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;                      // 描述符集布局数组
    pipelineLayoutInfo.pushConstantRangeCount = 1;                              // 推送常量数量
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;                // 推送常量数组  14 14 14 14

    //16-创建
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);   //15 15 15 15

    //17-图形管线创建信息
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;   // 结构体类型标识
    pipelineInfo.stageCount = 2;                                            // 着色器阶段数量
    pipelineInfo.pStages = shaderStages;                                    // 着色器阶段数组 1
    pipelineInfo.pVertexInputState = &vertexInputInfo;                      // 顶点输入状态   3
    pipelineInfo.pInputAssemblyState = &inputAssembly;                      // 图元装配状态   4
    pipelineInfo.pViewportState = &viewportState;                           // 视口状态       7
    pipelineInfo.pRasterizationState = &rasterizer;                         // 光栅化状态     8
    pipelineInfo.pMultisampleState = &multisampling;                        // 多重采样状态   9
    pipelineInfo.pDepthStencilState = &depthStencil;                        // 深度模板状态   10
    pipelineInfo.pColorBlendState = &colorBlending;                         // 颜色混合状态   12
    pipelineInfo.pDynamicState = &dynamicState;                             // 动态状态       13
    pipelineInfo.layout = pipelineLayout;                                   // 管线布局
    pipelineInfo.renderPass = renderPass;                                   // 使用的渲染通道
    pipelineInfo.subpass = 0;                                               // 子通道下标
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;                       // 不派生自其它管线

    //18-创建渲染管线
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
}

void TestRender::创建MSAA颜色附件_12()
{
    //1-创建Image
    创建VkImage_12(physicalDevice,device,swapchain.extent.width, swapchain.extent.height, swapchain.format, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,sampleCount,
                msaaImage.image, msaaImage.memory);
    //2-创建ImageView
    创建图像视图_12(device,msaaImage.image, swapchain.format, VK_IMAGE_ASPECT_COLOR_BIT,msaaImage.view);
}

void TestRender::创建深度附件_13()
{
    //1-深度格式
    VkFormat depthFormat = 选定深度格式7_1(physicalDevice);

    //2-创建DepthImage
    创建VkImage_12(physicalDevice,device,swapchain.extent.width, swapchain.extent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sampleCount,
                depthImage.image, depthImage.memory);

    // 3-找到
    VkImageAspectFlags depthAspectFlags = 选定深度模板分量_7_2(depthFormat) ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT): VK_IMAGE_ASPECT_DEPTH_BIT;

    //4-创建ImageView
    创建图像视图_12(device,depthImage.image, depthFormat,depthAspectFlags,depthImage.view);
}

void TestRender::创建帧缓冲_14()
{
    //1-按图像数量分配帧缓冲
    swapchain.framebuffers.resize(swapchain.imageViews.size());             // 按图像数量分配帧缓冲

    for (size_t i = 0; i < swapchain.imageViews.size(); i++)
    {
        //2-本帧缓冲的附件视图列表
        //  顺序必须和渲染通道的附件下标一致：0=MSAA 颜色，1=深度，2=解析目标（交换链图像）
        std::vector<VkImageView> attachments = { msaaImage.view, depthImage.view, swapchain.imageViews[i] };

        //3-帧缓冲创建信息
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;          // 结构体类型标识
        framebufferInfo.renderPass = renderPass;                                    // 关联的渲染通道
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());// 附件数量
        framebufferInfo.pAttachments = attachments.data();                          // 附件视图数组
        framebufferInfo.width = swapchain.extent.width;                             // 宽度
        framebufferInfo.height = swapchain.extent.height;                           // 高度
        framebufferInfo.layers = 1;                                                 // 层数

        //4-创建该图像的帧缓冲
        DebugLog(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchain.framebuffers[i]),"Failed to create framebuffer!");  // 创建该图像的帧缓冲
    }
}

void TestRender::上传顶点缓冲_15()
{
    //1-设置Buffer大小
    VkDeviceSize bufferSize = sizeof(Other1::Vertex) ;//* mesh.;  // 顶点数据总字节数

    //2-CPU 可见的暂存缓冲
    GpuBuffer stagingBuffer;
    创建Buffer(device,bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer.buffer,
                 stagingBuffer.memory);  // 创建暂存缓冲

    void* data;

    //3-映射暂存缓冲内存
    DebugLog(vkMapMemory(device, stagingBuffer.memory, 0, bufferSize, 0, &data), "Failed to map staging buffer!");
    //memcpy(data, model.vertices.data(), static_cast<size_t>(bufferSize));  // 拷贝顶点数据
    vkUnmapMemory(device, stagingBuffer.memory);  // 解除映射

    //4-创建Buffer
    //创建Buffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    //             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, model.vertexBuffer.buffer, model.vertexBuffer.memory);  // 创建设备本地顶点缓冲

    //5-复制Buffer
    //复制Buffer(stagingBuffer.buffer, model.vertexBuffer.buffer, bufferSize);  // 暂存缓冲拷到顶点缓冲

    //6-销毁临时缓冲
    vkDestroyBuffer(device, stagingBuffer.buffer, nullptr);                 // 销毁缓冲句柄
    vkFreeMemory(device, stagingBuffer.memory, nullptr);                    // 释放显存
    stagingBuffer = {};
}

void TestRender::上传索引缓冲_16()
{
    //1-设置Buffer大小
    VkDeviceSize bufferSize = sizeof(uint32_t); // 索引数据总字节数
    //model.indexCount = static_cast<uint32_t>(model.indices.size()); // 记录索引数量

    //2-CPU 可见的暂存缓冲
    GpuBuffer stagingBuffer;
    创建Buffer(device,bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer.buffer, stagingBuffer.memory); // 创建暂存缓冲

    void* data;

    //3-映射暂存缓冲内存
    DebugLog(vkMapMemory(device, stagingBuffer.memory, 0, bufferSize, 0, &data),"Failed to map staging buffer!"); // 映射暂存缓冲内存
    //memcpy(data, model.indices.data(), static_cast<size_t>(bufferSize)); // 拷贝索引数据
    //vkUnmapMemory(app.device, stagingBuffer.memory); // 解除映射

    //4-创建设备本地索引缓冲
    //创建Buffer(device,bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    //             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, model.indexBuffer.buffer, model.indexBuffer.memory);

    //5-复制Buffer
    //复制Buffer(stagingBuffer.buffer, model.indexBuffer.buffer, bufferSize); // 暂存缓冲拷到索引缓冲

    //6-销毁临时缓冲
    vkDestroyBuffer(device, stagingBuffer.buffer, nullptr);                 // 销毁缓冲句柄
    vkFreeMemory(device, stagingBuffer.memory, nullptr);                    // 释放显存
    stagingBuffer = {};

}

void TestRender::计算各模型变换矩阵_17()
{
}

void TestRender::创建UniformBuffer_18()
{
    //Buffer大小
    VkDeviceSize bufferSize = sizeof(UniformBuffer);

    // 按在途帧数分配
    frames.resize(MAX_FRAME_DRAWS);

    //
    for (FrameResources& frame : frames)
    {
        // 创建主机可见的 UBO
        创建Buffer(device,bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     frame.uniformBuffer.buffer, frame.uniformBuffer.memory);

        // 一直保持映射，方便每帧写入
        DebugLog(vkMapMemory(device, frame.uniformBuffer.memory, 0, bufferSize, 0, &frame.uniformBufferMapped),"Failed to map uniform buffer memory!");
    }
}

void TestRender::创建描述符池_19()
{
    const uint32_t materialCount = 0; // 材质总数
    std::array<VkDescriptorPoolSize, 2> poolSizes = {}; // 两种描述符类型
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // 类型 0：统一缓冲（相机 + 材质）
    poolSizes[0].descriptorCount = MAX_FRAME_DRAWS + materialCount + MATERIAL_DESCRIPTOR_HEADROOM; // 每帧相机 UBO + 每个材质一个
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // 类型 1：贴图采样器
    poolSizes[1].descriptorCount = materialCount + MATERIAL_DESCRIPTOR_HEADROOM; // 每个材质一张

    VkDescriptorPoolCreateInfo poolInfo = {}; // 描述符池创建信息
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; // 结构体类型标识
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // 允许单独释放（删模型时把材质描述符集还回来）
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size()); // 池大小数组长度
    poolInfo.pPoolSizes = poolSizes.data(); // 池大小数组
    poolInfo.maxSets = MAX_FRAME_DRAWS + materialCount + MATERIAL_DESCRIPTOR_HEADROOM; // 最大描述符集数量

    DebugLog(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool),
            "Failed to create descriptor pool!"); // 创建描述符池
}

void TestRender::分配相机描述符集_20()
{
    //1-每个在途帧一个描述符集（所有 SubMesh 共用同一份 view/projection）
    cameraDescriptorSets.resize(frames.size());
    std::vector<VkDescriptorSetLayout> layouts(frames.size(), descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;           // 结构体类型标识
    allocInfo.descriptorPool = descriptorPool;                                 // 从哪个池分配
    allocInfo.descriptorSetCount = static_cast<uint32_t>(frames.size());       // 分配几个
    allocInfo.pSetLayouts = layouts.data();                                    // 用哪个布局
    DebugLog(vkAllocateDescriptorSets(device, &allocInfo, cameraDescriptorSets.data()), "Failed to allocate camera descriptor sets!");

    //2-把每帧的相机 UBO 写到 binding 0（binding 1 的贴图当前用不到，先不写）
    for (size_t i = 0; i < frames.size(); i++)
    {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = frames[i].uniformBuffer.buffer;                    // 这一帧的相机统一缓冲
        bufferInfo.offset = 0;                                                 // 从 0 开始
        bufferInfo.range = sizeof(UniformBuffer);                        // 整个 UBO

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;                  // 结构体类型标识
        write.dstSet = cameraDescriptorSets[i];                                // 写到哪个描述符集
        write.dstBinding = 0;                                                  // 绑定点 0
        write.dstArrayElement = 0;                                             // 数组第 0 个
        write.descriptorCount = 1;                                             // 写一个
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;              // 类型：uniform 缓冲
        write.pBufferInfo = &bufferInfo;                                       // 缓冲信息

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);                 // 提交更新
    }
}

void TestRender::分配绘制命令缓冲_21()
{
    //1-命令缓冲分配信息
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;   // 结构体类型标识
    allocInfo.commandPool = commandPool;                                // 从哪个命令池分配
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;                  // 主命令缓冲
    allocInfo.commandBufferCount = static_cast<uint32_t>(frames.size());// 分配数量

    //2-存放分配结果
    std::vector<VkCommandBuffer> commandBuffers(frames.size());
    DebugLog(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()),"Failed to allocate command buffers!");

    //3-保存到该帧资源
    for (size_t i = 0; i < frames.size(); i++)
    {
        frames[i].commandBuffer = commandBuffers[i];
    }
}

void TestRender::创建同步对象_22()
{
    //1-每张交换链图像一个
    renderFinishedSemaphores.resize(swapchain.images.size());

    //2-信号量创建信息
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    //3-栅栏创建信息
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // 初始即有信号，首帧不阻塞

    for (FrameResources& frame : frames)
    {
        //4-创建图像可用信号量
        DebugLog(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.imageAvailable),"Failed to create semaphore!");
        //5-创建在途帧栅栏
        DebugLog(vkCreateFence(device, &fenceInfo, nullptr, &frame.inFlight), "Failed to create fence!");
    }

    for (VkSemaphore& semaphore : renderFinishedSemaphores)
    {
        //6-创建渲染完成信号量
        DebugLog(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore),"Failed to create semaphore!");
    }
}

void TestRender::初始化ImGui_23()
{
    // 必须写进成员 GUI，不能建局部临时对象：
    // GUI.draw() 用它自己的 window 弹文件对话框，临时对象一析构窗口句柄就丢了
    GUI = Test_ImguiVulaknBackend(window,instance,physicalDevice,device,
        indices.graphicsFamily,static_cast<uint32_t>(swapchain.images.size()),graphics,renderPass,sampleCount);
}

//=======================================================================================
TestRender::QueueFamilyIndices TestRender::查找物理设备_家族队列_3_1(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices;
    // 先查询家族队列数量
    uint32_t queueFamilyCount = 0; // 队列家族数量
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr); // 先查数量
    // 再分配属性数组
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    //再匹配家族队列数量
    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        // 1-没有队列的家族跳过
        if (queueFamilies[i].queueCount == 0) continue;
        // 2-记录图形队列家族下标
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }
        // 3-是否支持呈现到该表面
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport); // 查询呈现支持
        // 4-记录呈现队列家族下标
        if (presentSupport)
        {
            indices.presentFamily = i;
        }
        // 5-两者都找到就提前退出 退出
        if (indices.isComplete()) break;
    }
    // 没找到返回false
    return indices;
}

bool TestRender::查找物理设备_扩展_3_2(VkPhysicalDevice device)
{
    // 1-先查扩展数量
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    // 2-分配扩展属性数组
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    // 3-需要的扩展集合
    std::vector<const char*> DEVICE_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    std::set<std::string> requiredExtensions(DEVICE_EXTENSIONS.begin(), DEVICE_EXTENSIONS.end());
    // 4-已支持的从需求中移除
    for (const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }
    // 5-需求清空即全部支持
    return requiredExtensions.empty();
}

bool TestRender::查找物理设备_表面和呈现_3_3(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    // 1-查询表面格式数量
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    // 2-查询呈现模式数量
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    // 3-如果都不为0就合格
    if (formatCount == 0 || presentModeCount == 0) return false;
    else return true;
}
//=======================================================================================
VkSurfaceFormatKHR TestRender::选定表面格式6_1(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    // 1-查询表面
    uint32_t formatCount = 0; // 表面格式数量
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr); // 先查格式数量
    // 2-分配格式数组
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());
    // 3-选择表面格式
    for (const auto& availableFormat : formats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace ==
            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }
    // 4-否则退回第一个格式
    return formats[0];
}

VkPresentModeKHR TestRender::选定呈现模式6_2(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    // 1-先查模式数量
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    // 2-再取模式列表
    std::vector<VkPresentModeKHR> presentModes(presentModeCount); // 分配模式数组
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, presentModes.data());
    // 3-选择支持的
    for (const auto& availablePresentMode : presentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    // 4-否则退回 FIFO（垂直同步）
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D TestRender::选定交换链分辨率6_3(GLFWwindow* window,VkSurfaceCapabilitiesKHR capabilities)
{
    // 1-驱动给了固定尺寸就用它（Windows 上正常窗口给的就是真实尺寸）；
    //   给 UINT32_MAX 才表示"由你自己决定"，那就问 GLFW 要帧缓冲尺寸
    VkExtent2D actualExtent = capabilities.currentExtent;
    if (actualExtent.width == UINT32_MAX)
    {
        int width = 0;  // 帧缓冲宽度
        int height = 0;  // 帧缓冲高度
        glfwGetFramebufferSize(window, &width, &height);  // 取窗口帧缓冲尺寸
        actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    }

    // 2-【两个分支都要夹】这里不能提前 return。
    //   窗口最小化时 Windows 会把 currentExtent 报成 (0,0)，它是"固定值"走的就是上面那个分支；
    //   以前这里直接 return，(0,0) 就被端去建交换链了——Vulkan 要求 imageExtent 两个分量都非 0
    //   （VUID-VkSwapchainCreateInfoKHR-imageExtent-01689），vkCreateSwapchainKHR 必然失败，
    //   DebugLog 抛异常又没人接住 -> terminate -> abort，退出码就是 0xC0000409。
    //   minImageExtent 按规范保证 >= 1，所以夹完至少是 1×1，不会再出现 0
    actualExtent.width  = std::clamp(actualExtent.width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actualExtent;  // 返回最终尺寸
}
//=======================================================================================
VkFormat TestRender::选定深度格式7_1(VkPhysicalDevice device)
{
    //1-候选格式
    const std::vector<VkFormat> candidates = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT };

    //2-选择格式
    for (VkFormat format : candidates) {
        //1-问驱动这个格式的能力
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(device, format, &props);
        //2-试出
        VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        switch (VK_IMAGE_TILING_OPTIMAL) {
        case VK_IMAGE_TILING_LINEAR:
            if ((props.linearTilingFeatures & features) == features) return format;   // 线性排布下能力齐全
            break;  // 线性排布不满足，换下一个格式
        case VK_IMAGE_TILING_OPTIMAL:
            if ((props.optimalTilingFeatures & features) == features) return format;  // 最优排布下能力齐全
            break;  // 最优排布不满足，换下一个格式
        default:
            break;  // 其它排布方式不支持
        }
    }
    // 3-一个都不满足
    throw std::runtime_error("Failed to find supported format!");
}

bool TestRender::选定深度模板分量_7_2(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return true;                                                        // 这两种带 S8 模板分量
    default:
        return false;                                                       // 其它（纯深度）没有模板
    }
}

VkShaderModule TestRender::读取着色器文件(VkDevice device,const std::string& filename)
{
    //1-读取二进制文件
    std::ifstream file(filename, std::ios::ate | std::ios::binary);       // 二进制打开，读指针停在文件末尾
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);   // 打不开（路径错 / 文件不存在）
    }
    // 2-设置结束位
    size_t fileSize = static_cast<size_t>(file.tellg());                        // 末尾位置就是文件字节数
    std::vector<char> buffer(fileSize);                                         // 按大小开好缓冲区
    file.seekg(0);                                                           // 读指针回到开头
    file.read(buffer.data(), fileSize);                                     // 一次性读入全部内容

    // 3-着色器模块创建信息
    VkShaderModuleCreateInfo createInfo = {};                                   // 着色器模块创建信息
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;             // 结构体类型标识
    createInfo.codeSize = buffer.size();  // 字节码长度
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());        // 字节码指针

    // 4-着色器模块句柄
    VkShaderModule shaderModule;                                                // 着色器模块句柄
    vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule); // 创建着色器模块
    // 5-返回模块
    return shaderModule;
}

void TestRender::创建VkImage_12(VkPhysicalDevice physicalDevice,VkDevice device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
    VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkSampleCountFlagBits samples, VkImage& image,
    VkDeviceMemory& imageMemory)
{
    //1-图像创建信息
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;                // 结构体类型标识
    imageInfo.imageType = VK_IMAGE_TYPE_2D;                               // 2D 图像
    imageInfo.extent.width = width;                                       // 宽度
    imageInfo.extent.height = height;                                     // 高度
    imageInfo.extent.depth = 1;                                           // 深度固定 1（2D）
    imageInfo.mipLevels = 1;                                              // 只有 1 级 mip
    imageInfo.arrayLayers = 1;                                            // 只有 1 层
    imageInfo.format = format;                                            // 像素格式
    imageInfo.tiling = tiling;                                            // 排布方式（OPTIMAL / LINEAR）
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;                  // 初始布局：内容未定义
    imageInfo.usage = usage;                                              // 用途（采样 / 附件 / 传输目标…）
    imageInfo.samples = samples;                                          // 采样数（MSAA）
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;                    // 只给一个队列家族用

    //2-创建图像句柄
    DebugLog(vkCreateImage(device, &imageInfo, nullptr, &image), "Failed to create image!");

    //3-问驱动要多少显存
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    //4-显存分配信息
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;            // 结构体类型标识
    allocInfo.allocationSize = memRequirements.size;                     // 申请驱动要求的大小
    allocInfo.memoryTypeIndex = 选显存类型_12(physicalDevice,memRequirements.memoryTypeBits, properties);  // 选显存类型

    //5-分配和绑定图像显存
    DebugLog(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory), "Failed to allocate image memory!");
    DebugLog(vkBindImageMemory(device, image, imageMemory, 0), "Failed to bind image memory!");
}

uint32_t TestRender::选显存类型_12(VkPhysicalDevice physicalDevice,uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    //1-查显存类型表
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    //2-遍历输出
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1u << i)) &&
            (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
            }
    }
    //3-一个都不满足：报错
    throw std::runtime_error("Failed to find suitable memory type!");
}

void TestRender::创建Buffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    //1-缓冲创建信息
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;                // 结构体类型标识（Vulkan 必需）
    bufferInfo.size = size;                                                 // 缓冲字节数
    bufferInfo.usage = usage;                                               // 用途（顶点 / 索引 / 传输 / UBO…）
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;                     // 只给一个队列家族用

    //2-创建缓冲句柄
    DebugLog(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer), "Failed to create buffer!");

    //3-问驱动要多少显存、哪种类型
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    //4-显存分配信息
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;                // 结构体类型标识
    allocInfo.allocationSize = memRequirements.size;                         // 申请驱动要求的大小
    allocInfo.memoryTypeIndex = 选显存类型_12(physicalDevice,memRequirements.memoryTypeBits, properties);  // 选显存类型

    //5-分配缓冲显存  把显存绑到缓冲
    DebugLog(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory), "Failed to allocate buffer memory!");
    DebugLog(vkBindBufferMemory(device, buffer, bufferMemory, 0), "Failed to bind buffer memory!");

}

void TestRender::复制Buffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    //1-开一个临时命令缓冲
    VkCommandBuffer commandBuffer = {};
    创建临时命令缓冲(device,commandPool,commandBuffer);

    //2-拷贝区域描述
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0;                                               // 源缓冲从 0 开始
    copyRegion.dstOffset = 0;                                               // 目标缓冲从 0 开始
    copyRegion.size = size;                                                 // 拷贝全部字节
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);   // 记录拷贝命令

    //3-提交并等待完成
    结束临时命令缓冲(device,commandPool,commandBuffer,graphics);              // 提交并等待完成
}

void TestRender::创建临时命令缓冲(VkDevice device,VkCommandPool commandPool,VkCommandBuffer& commandBuffer)
{
    //1-命令缓冲分配信息
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;       // 结构体类型标识
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;                      // 一级命令缓冲（能直接提交）
    allocInfo.commandPool = commandPool;                                    // 从主命令池分配
    allocInfo.commandBufferCount = 1;                                       // 只要一个

    //2-分配出来的命令缓冲
    DebugLog(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer), "Failed to allocate one-time command buffer!");  // 从命令池分配一个命令缓冲

    //3-开始录制的参数
    VkCommandBufferBeginInfo beginInfo = {};  // 开始录制的参数
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;          // 结构体类型标识
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;          // 声明只提交一次

    //返回已开始录制的命令缓冲
    DebugLog(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin one-time command buffer!");  // 开始录制命令
}

void TestRender::结束临时命令缓冲(VkDevice device, VkCommandPool commandPool, VkCommandBuffer commandBuffer,VkQueue graphics)
{
    //1-结束录制
    DebugLog(vkEndCommandBuffer(commandBuffer), "Failed to end one-time command buffer!");

    //2-提交信息
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;  // 结构体类型标识
    submitInfo.commandBufferCount = 1;                 // 提交一个命令缓冲
    submitInfo.pCommandBuffers = &commandBuffer;       // 就是刚才录的这个

    //3-提交到图形队列等它执行完
    DebugLog(vkQueueSubmit(graphics, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit one-time command buffer!");
    DebugLog(vkQueueWaitIdle(graphics), "Failed to wait for one-time command buffer!");

    //4-用完即弃
    vkFreeCommandBuffers(device,commandPool, 1, &commandBuffer);
}

//=======================================================================================
//模型渲染
//=======================================================================================
VkSampleCountFlagBits TestRender::选定采样数_7_3()
{
    //1-问物理设备：颜色附件和深度附件都支持哪些采样数（取交集）
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    const VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts &
                                      props.limits.framebufferDepthSampleCounts;

    //2-设备级上限只说明硬件支持，具体"交换链格式 / 深度格式"能不能用这个采样数还得单独问。
    //  不查的话 vkCreateImage 会返回 VK_ERROR_FORMAT_NOT_SUPPORTED，MSAA 附件和渲染通道都建不起来。
    const VkFormat depthFormat = 选定深度格式7_1(physicalDevice);
    auto 格式支持 = [&](VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits samples)
    {
        VkImageFormatProperties 属性 = {};
        const VkResult 结果 = vkGetPhysicalDeviceImageFormatProperties(
            physicalDevice, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &属性);
        return 结果 == VK_SUCCESS && (属性.sampleCounts & samples) != 0;
    };

    //3-从高到低挑：优先 4x，退化到 2x，最后 1x（不采样）；颜色和深度格式都必须支持
    const VkSampleCountFlagBits 候选[] = { VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_2_BIT };
    for (VkSampleCountFlagBits 采样 : 候选)
    {
        if ((counts & 采样) == 0) continue;
        if (!格式支持(swapchain.format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 采样)) continue;
        if (!格式支持(depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 采样)) continue;
        return 采样;
    }
    return VK_SAMPLE_COUNT_1_BIT;
}

void TestRender::更新相关Uniform()
{
    const Camera& camera = GUI.hierarchy.camera;         // Hierarchy 面板改的就是这个相机
    const MainLight& mainLight = GUI.hierarchy.mainLight;   // Hierarchy 面板改的就是这个相机

    //1-视图矩阵：eye = 相机位置；朝向由旋转角度决定（先绕 Y 偏航，再绕 X 俯仰）；向上固定 +Y
    const glm::vec3 camerapos = camera.transform.position;
    const glm::vec3 forward = camera.transform.forward();   // 和 Transform::LookAt() 用的是同一份公式，别再在这里重算一遍
    const glm::mat4 view = glm::lookAt(camerapos, camerapos + forward, glm::vec3(0.0f, 1.0f, 0.0f));

    //2-投影矩阵：Vulkan 的裁剪空间 Y 轴朝下，要把 glm 的 Y 翻过来
    const float aspect = static_cast<float>(swapchain.extent.width) / static_cast<float>(swapchain.extent.height);
    glm::mat4 projection = glm::perspective(glm::radians(camera.fov), aspect, camera.nearPlane, camera.farPlane);
    projection[1][1] *= -1.0f;

    //时间
    const double 秒 = glfwGetTime();
    //灯光
    const glm::vec3 mainLightDir = mainLight.transform.up();


    //3-写进这一帧映射着的 UBO
    UniformBuffer ubo = {};

    ubo._WorldSpaceCameraPos = glm::vec4(camera.transform.position, 1.0f);
    ubo._Time = glm::vec4(static_cast<float>(秒 / 20.0), static_cast<float>(秒),static_cast<float>(秒 * 2.0), static_cast<float>(秒 * 3.0));
    ubo._MATRIX_V = view;
    ubo._MATRIX_P = projection;
    ubo._MainLightColor = glm::vec4(mainLight.color.x,mainLight.color.y,mainLight.color.z,mainLight.color.w);
    ubo._MainLightDirection = glm::vec4(mainLightDir,0.0f);

    std::memcpy(frames[currentFrame].uniformBufferMapped, &ubo, sizeof(ubo));

    //4-顺手存回相机，面板 / 别处可以查看当前矩阵
    GUI.hierarchy.camera.view = view;
    GUI.hierarchy.camera.projection = projection;
}

void TestRender::上传网格缓冲(Other1::Mesh& mesh)
{
    //0-空网格不处理（没顶点 / 没索引就没法画）
    if (mesh.vertices.empty() || mesh.indices.empty())
    {
        return;
    }

    //1-顶点缓冲：先建主机可见的暂存缓冲，填完再拷到设备本地
    const VkDeviceSize vertexSize = sizeof(Other1::Vertex) * mesh.vertices.size();
    VkBuffer vertexStaging = VK_NULL_HANDLE;
    VkDeviceMemory vertexStagingMemory = VK_NULL_HANDLE;
    创建Buffer(device, vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               vertexStaging, vertexStagingMemory);

    void* vertexData = nullptr;
    vkMapMemory(device, vertexStagingMemory, 0, vertexSize, 0, &vertexData);
    std::memcpy(vertexData, mesh.vertices.data(), static_cast<size_t>(vertexSize));  // 顶点数据搬进暂存
    vkUnmapMemory(device, vertexStagingMemory);

    创建Buffer(device, vertexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mesh.vertexBuffer, mesh.vertexBufferMemory);
    复制Buffer(vertexStaging, mesh.vertexBuffer, vertexSize);   // 暂存 → 设备本地

    vkDestroyBuffer(device, vertexStaging, nullptr);            // 暂存用完即弃
    vkFreeMemory(device, vertexStagingMemory, nullptr);

    //2-索引缓冲：同样走一次暂存 + 拷贝
    const VkDeviceSize indexSize = sizeof(uint32_t) * mesh.indices.size();
    VkBuffer indexStaging = VK_NULL_HANDLE;
    VkDeviceMemory indexStagingMemory = VK_NULL_HANDLE;
    创建Buffer(device, indexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               indexStaging, indexStagingMemory);

    void* indexData = nullptr;
    vkMapMemory(device, indexStagingMemory, 0, indexSize, 0, &indexData);
    std::memcpy(indexData, mesh.indices.data(), static_cast<size_t>(indexSize));    // 索引数据搬进暂存
    vkUnmapMemory(device, indexStagingMemory);

    创建Buffer(device, indexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mesh.indexBuffer, mesh.indexBufferMemory);
    复制Buffer(indexStaging, mesh.indexBuffer, indexSize);

    vkDestroyBuffer(device, indexStaging, nullptr);
    vkFreeMemory(device, indexStagingMemory, nullptr);

    //3-记下索引数量，绘制时用
    mesh.indexCount = static_cast<uint32_t>(mesh.indices.size());
}

VkShaderModule TestRender::创建着色器模块(const std::vector<char>& spv)
{
    //1-着色器模块创建信息
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;          // 结构体类型标识
    createInfo.codeSize = spv.size();                                        // 字节码长度
    createInfo.pCode = reinterpret_cast<const uint32_t*>(spv.data());        // 字节码指针

    //2-创建模块
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    DebugLog(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule), "Failed to create shader module!");
    return shaderModule;
}

void TestRender::创建材质管线(Other1::Mesh& mesh)
{
    Other1::Shader& shader = mesh.material.shader;
    const Other1::RenderState& state = mesh.material.state;
    //0-shader 为空就跳过（调用方一般已判过，这里再兜一层）
    if (shader.spvVert.empty() || shader.spvFrag.empty())
    {
        return;
    }

    //1-共享的管线布局：只在第一次建
    //  描述符集 0 = 相机 UBO；推送常量 = 模型矩阵（顶点着色器用）
    if (pipelineLayout == VK_NULL_HANDLE)
    {
        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;   // 顶点着色器里用
        pushConstantRange.offset = 0;                                // 起始偏移
        pushConstantRange.size = sizeof(glm::mat4);                  // 一个模型矩阵

        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;   // 结构体类型标识
        layoutInfo.setLayoutCount = 1;                                      // 描述符集布局数量
        layoutInfo.pSetLayouts = &descriptorSetLayout;                      // 相机 UBO 的布局
        layoutInfo.pushConstantRangeCount = 1;                              // 推送常量数量
        layoutInfo.pPushConstantRanges = &pushConstantRange;                // 推送常量范围
        DebugLog(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout!");
    }

    //2-着色器阶段：DXC 编译时用 -fspv-entrypoint-name=main，入口统一叫 main
    VkShaderModule vertModule = 创建着色器模块(shader.spvVert);
    VkShaderModule fragModule = 创建着色器模块(shader.spvFrag);

    VkPipelineShaderStageCreateInfo vertStage = {};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;  // 结构体类型标识
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;                           // 顶点阶段
    vertStage.module = vertModule;                                          // 顶点模块
    vertStage.pName = "main";                                               // 入口名

    VkPipelineShaderStageCreateInfo fragStage = {};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;  // 结构体类型标识
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;                         // 片元阶段
    fragStage.module = fragModule;                                          // 片元模块
    fragStage.pName = "main";                                               // 入口名

    VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

    //3-顶点输入：直接由 shader.inputDescriptions（location / format / offset）生成
    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;                                      // 绑定点 0
    binding.stride = sizeof(Other1::Vertex);                  // 单个顶点的字节数
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;          // 每顶点前进一次

    std::vector<VkVertexInputAttributeDescription> attributes;
    attributes.reserve(shader.inputDescriptions.size());
    for (const Other1::VerInputDes& des : shader.inputDescriptions)
    {
        VkVertexInputAttributeDescription attr = {};
        attr.location = des.location;   // 着色器里的 location
        attr.binding = 0;               // 都来自绑定点 0
        attr.format = des.format;       // 顶点格式
        attr.offset = des.offset;       // 在 Vertex 结构体里的字节偏移
        attributes.push_back(attr);
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;  // 结构体类型标识
    vertexInputInfo.vertexBindingDescriptionCount = 1;                                  // 一个绑定
    vertexInputInfo.pVertexBindingDescriptions = &binding;                              // 绑定描述
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributes.empty() ? nullptr : attributes.data();

    //4-图元装配
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;  // 结构体类型标识
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;                       // 三角形列表
    inputAssembly.primitiveRestartEnable = VK_FALSE;                                    // 不启用图元重启

    //5-视口 / 裁剪矩形：改成"动态状态"，具体数值由渲染时每帧 vkCmdSetViewport / vkCmdSetScissor 给。
    //  否则尺寸会写死在管线里，一改窗口分辨率就得把所有材质管线重建一遍
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;  // 结构体类型标识
    viewportState.viewportCount = 1;               // 一个视口（范围是动态的，所以不填 pViewports）
    viewportState.scissorCount = 1;                // 一个裁剪矩形（范围是动态的，所以不填 pScissors）

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;  // 结构体类型标识
    dynamicState.dynamicStateCount = 2;                                          // 两个动态状态
    dynamicState.pDynamicStates = dynamicStates;                                 // 视口 + 裁剪矩形

    //6-光栅化状态：剔除 / 绕序读材质自己的渲染状态
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;  // 结构体类型标识
    rasterizer.depthClampEnable = VK_FALSE;                              // 不启用深度钳制
    rasterizer.rasterizerDiscardEnable = VK_FALSE;                       // 不丢弃图元
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;                       // 填充模式
    rasterizer.lineWidth = 1.0f;                                         // 线宽
    rasterizer.cullMode = static_cast<VkCullModeFlags>(state.cullMode);  // 剔除哪一面
    rasterizer.frontFace = static_cast<VkFrontFace>(state.frontFace);    // 正面绕序
    rasterizer.depthBiasEnable = VK_FALSE;                               // 不启用深度偏移

    //7-多重采样：颜色附件是 MSAA，采样数必须和渲染通道一致
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;  // 结构体类型标识
    multisampling.sampleShadingEnable = VK_FALSE;                                    // 不启用采样着色
    multisampling.rasterizationSamples = sampleCount;                                // 采样数

    //8-深度 / 模板状态：读材质状态
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;  // 结构体类型标识
    depthStencil.depthTestEnable = state.depthTestEnable ? VK_TRUE : VK_FALSE;        // 深度测试开关
    depthStencil.depthWriteEnable = state.depthWriteEnable ? VK_TRUE : VK_FALSE;      // 深度写入开关
    depthStencil.depthCompareOp = static_cast<VkCompareOp>(state.depthCompareOp);     // 深度比较操作
    depthStencil.depthBoundsTestEnable = VK_FALSE;                                    // 不做深度范围测试
    depthStencil.stencilTestEnable = state.stencilTestEnable ? VK_TRUE : VK_FALSE;    // 模板测试开关

    //9-颜色混合附件状态：读材质状态
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;  // 写 RGBA
    colorBlendAttachment.blendEnable = state.blendEnable ? VK_TRUE : VK_FALSE;               // 混合开关
    colorBlendAttachment.srcColorBlendFactor = static_cast<VkBlendFactor>(state.blendSrcFactor);  // 源颜色因子
    colorBlendAttachment.dstColorBlendFactor = static_cast<VkBlendFactor>(state.blendDstFactor);  // 目标颜色因子
    colorBlendAttachment.colorBlendOp = static_cast<VkBlendOp>(state.blendOp);               // 颜色混合运算
    colorBlendAttachment.srcAlphaBlendFactor = static_cast<VkBlendFactor>(state.blendSrcFactor);  // 源 alpha 因子
    colorBlendAttachment.dstAlphaBlendFactor = static_cast<VkBlendFactor>(state.blendDstFactor);  // 目标 alpha 因子
    colorBlendAttachment.alphaBlendOp = static_cast<VkBlendOp>(state.blendOp);               // alpha 混合运算

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;  // 结构体类型标识
    colorBlending.logicOpEnable = VK_FALSE;                                          // 不启用逻辑运算
    colorBlending.attachmentCount = 1;                                               // 一个颜色附件
    colorBlending.pAttachments = &colorBlendAttachment;                              // 附件数组

    //10-管线创建信息
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;   // 结构体类型标识
    pipelineInfo.stageCount = 2;                                            // 顶点 + 片元
    pipelineInfo.pStages = stages;                                          // 着色器阶段数组
    pipelineInfo.pVertexInputState = &vertexInputInfo;                      // 顶点输入状态
    pipelineInfo.pInputAssemblyState = &inputAssembly;                      // 图元装配状态
    pipelineInfo.pViewportState = &viewportState;                           // 视口状态
    pipelineInfo.pRasterizationState = &rasterizer;                         // 光栅化状态
    pipelineInfo.pMultisampleState = &multisampling;                        // 多重采样状态
    pipelineInfo.pDepthStencilState = &depthStencil;                        // 深度模板状态
    pipelineInfo.pColorBlendState = &colorBlending;                         // 颜色混合状态
    pipelineInfo.pDynamicState = &dynamicState;                             // 视口 / 裁剪矩形是动态状态
    pipelineInfo.layout = pipelineLayout;                                   // 管线布局
    pipelineInfo.renderPass = renderPass;                                   // 使用的渲染通道
    pipelineInfo.subpass = 0;                                               // 子通道下标
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;                       // 不派生自其它管线

    //11-创建管线
    DebugLog(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mesh.pipeline),
             "Failed to create material pipeline!");

    //12-模块用完即销毁（SPIR-V 已经被管线拷走了）
    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);
}

void TestRender::创建图像视图_12(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,VkImageView& imageView)
{
    //1-图像视图创建信息
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;// 结构体类型标识
    viewInfo.image = image;                                   // 基于哪张图像
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;                // 2D 视图
    viewInfo.format = format;                                 // 格式（要和图像一致）
    viewInfo.subresourceRange.aspectMask = aspectFlags;       // 看哪部分（颜色 / 深度 / 模板）
    viewInfo.subresourceRange.baseMipLevel = 0;               // 从第 0 级 mip 开始
    viewInfo.subresourceRange.levelCount = 1;                 // 只含 1 级
    viewInfo.subresourceRange.baseArrayLayer = 0;             // 从第 0 层开始
    viewInfo.subresourceRange.layerCount = 1;                 // 只含 1 层

    //2-创建图像视图
    DebugLog(vkCreateImageView(device, &viewInfo, nullptr, &imageView), "Failed to create image view!");  // 创建图像视图
}
//=======================================================================================
