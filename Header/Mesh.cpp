#include "Mesh.h"

Mesh::Mesh()
{
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice,
	VkQueue transferQueue, VkCommandPool transferCommandPool,
	std::vector<Vertex_u>* vertices, std::vector<uint32_t> * indices)
{
	vertexCount = vertices->size();
	indexCount = indices->size();
	physicalDevice = newPhysicalDevice;
	device = newDevice;
	createVertexBuffer(transferQueue, transferCommandPool, vertices);
	createIndexBuffer(transferQueue, transferCommandPool, indices);
}

int Mesh::getVertexCount()
{
	return vertexCount;
}

VkBuffer Mesh::getVertexBuffer()
{
	return vertexBuffer;
}

int Mesh::getIndexCount()
{
	return indexCount;
}

VkBuffer Mesh::getIndexBuffer()
{
	return indexBuffer;
}

void Mesh::destroyBuffers()
{
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);
}


Mesh::~Mesh()
{
}
void Mesh::createVertexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex_u>* vertices)
{
	// 计算顶点数据所需字节数
	VkDeviceSize bufferSize = sizeof(Vertex_u) * vertices->size();

	// 临时缓冲区：用于在传输到 GPU 前"暂存"顶点数据
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	// 创建 Staging Buffer 并为其分配内存
	createBuffer_u(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer, &stagingBufferMemory);
	// ↑ 属性说明：
	//   HOST_VISIBLE  = CPU 可以访问这块内存
	//   HOST_COHERENT = CPU 写入后无需手动 flush，GPU 立即可见

	// 将内存映射到顶点缓冲区
	void * data;                                                              // 1. 创建一个指向普通内存的指针
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);        // 2. 将 staging 内存"映射"到该指针
	memcpy(data, vertices->data(), (size_t)bufferSize);                       // 3. 把顶点 vector 的数据拷贝过去
	vkUnmapMemory(device, stagingBufferMemory);                               // 4. 解除映射

	// 创建 GPU 上的实际顶点缓冲区
	// 用途标记：TRANSFER_DST（作为传输目标）+ VERTEX_BUFFER（作为顶点缓冲区）
	// 内存属性：DEVICE_LOCAL（位于 GPU 显存，CPU 不可直接访问，速度最快）
	createBuffer_u(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertexBuffer, &vertexBufferMemory);

	// 把 staging buffer 的数据拷贝到 GPU 上的顶点缓冲区
	copyBuffer_u(device, transferQueue, transferCommandPool, stagingBuffer, vertexBuffer, bufferSize);

	// 清理 staging buffer 相关资源
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Mesh::createIndexBuffer(VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<uint32_t>* indices)
{
	// 计算索引数据所需字节数
	VkDeviceSize bufferSize = sizeof(uint32_t) * indices->size();

	// 创建临时 staging buffer 并分配内存
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer_u(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

	// 将内存映射到索引缓冲区
	void * data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices->data(), (size_t)bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	// 创建 GPU 上的索引缓冲区（仅 GPU 访问）
	// 用途标记：TRANSFER_DST + INDEX_BUFFER
	createBuffer_u(physicalDevice, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &indexBuffer, &indexBufferMemory);

	// 从 staging buffer 拷贝到 GPU 索引缓冲区
	copyBuffer_u(device, transferQueue, transferCommandPool, stagingBuffer, indexBuffer, bufferSize);

	// 销毁并释放 staging buffer 资源
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}