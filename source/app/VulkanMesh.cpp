#include "VulkanMesh.h"
#include "VulkanUtils.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>

/*
 */
VulkanMesh::~VulkanMesh()
{
	clearGPUData();
	clearCPUData();
}

/*
 */
VkVertexInputBindingDescription VulkanMesh::getVertexInputBindingDescription()
{
	VkVertexInputBindingDescription bindingDescription;
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> VulkanMesh::getAttributeDescriptions()
{
	std::array<VkVertexInputAttributeDescription, 3> attributes = {};

	attributes[0].binding = 0;
	attributes[0].location = 0;
	attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributes[0].offset = offsetof(Vertex, position);

	attributes[1].binding = 0;
	attributes[1].location = 1;
	attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributes[1].offset = offsetof(Vertex, color);

	attributes[2].binding = 0;
	attributes[2].location = 2;
	attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributes[2].offset = offsetof(Vertex, uv);

	return attributes;
}

/*
 */
bool VulkanMesh::loadFromFile(const std::string &path)
{
	Assimp::Importer importer;

	unsigned int flags = aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType;
	const aiScene *scene = importer.ReadFile(path, flags);

	if (!scene)
	{
		std::cerr << "VulkanMesh::loadFromFile(): " << importer.GetErrorString() << std::endl;
		return false;
	}

	if (!scene->HasMeshes())
	{
		std::cerr << "VulkanMesh::loadFromFile(): model has no meshes" << std::endl;
		return false;
	}

	aiMesh *mesh = scene->mMeshes[0];
	assert(mesh != nullptr);

	// Fill CPU data
	vertices.resize(mesh->mNumVertices);
	indices.resize(mesh->mNumFaces * 3);

	aiVector3D *meshVertices = mesh->mVertices;
	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		vertices[i].position = glm::vec3(meshVertices[i].x, meshVertices[i].y, meshVertices[i].z);

	aiVector3D *meshUVs = mesh->mTextureCoords[0];
	if (meshUVs)
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
			vertices[i].uv = glm::vec2(meshUVs[i].x, 1.0f - meshUVs[i].y);
	else
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
			vertices[i].uv = glm::vec2(0.0f, 0.0f);

	aiColor4D *meshColors = mesh->mColors[0];
	if (meshColors)
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
			vertices[i].color = glm::vec3(meshColors[i].r, meshColors[i].g, meshColors[i].b);
	else
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
			vertices[i].color = glm::vec3(1.0f, 1.0f, 1.0f);

	aiFace *meshFaces = mesh->mFaces;
	unsigned int index = 0;
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		for (unsigned int faceIndex = 0; faceIndex < meshFaces[i].mNumIndices; faceIndex++)
			indices[index++] = meshFaces[i].mIndices[faceIndex];

	// Upload CPU data to GPU
	clearGPUData();
	uploadToGPU();

	// TODO: should we clear CPU data after uploading it to the GPU?

	return true;
}

/*
 */
void VulkanMesh::createVertexBuffer()
{
	VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;

	VulkanUtils::createBuffer(
		context,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBuffer,
		vertexBufferMemory
	);

	// Create staging buffer
	VulkanUtils::createBuffer(
		context,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	// Fill staging buffer
	void *data = nullptr;
	vkMapMemory(context.device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(context.device, stagingBufferMemory);

	// Transfer to GPU local memory
	VulkanUtils::copyBuffer(context, stagingBuffer, vertexBuffer, bufferSize);

	// Destroy staging buffer
	vkDestroyBuffer(context.device, stagingBuffer, nullptr);
	vkFreeMemory(context.device, stagingBufferMemory, nullptr);
}

void VulkanMesh::createIndexBuffer()
{
	VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();

	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;

	VulkanUtils::createBuffer(
		context,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBuffer,
		indexBufferMemory
	);

	// Create staging buffer
	VulkanUtils::createBuffer(
		context,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	// Fill staging buffer
	void *data = nullptr;
	vkMapMemory(context.device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(context.device, stagingBufferMemory);

	// Transfer to GPU local memory
	VulkanUtils::copyBuffer(context, stagingBuffer, indexBuffer, bufferSize);

	// Destroy staging buffer
	vkDestroyBuffer(context.device, stagingBuffer, nullptr);
	vkFreeMemory(context.device, stagingBufferMemory, nullptr);
}

/*
 */
void VulkanMesh::uploadToGPU()
{
	createVertexBuffer();
	createIndexBuffer();
}

void VulkanMesh::clearGPUData()
{
	vkDestroyBuffer(context.device, vertexBuffer, nullptr);
	vertexBuffer = VK_NULL_HANDLE;

	vkFreeMemory(context.device, vertexBufferMemory, nullptr);
	vertexBufferMemory = VK_NULL_HANDLE;

	vkDestroyBuffer(context.device, indexBuffer, nullptr);
	indexBuffer = VK_NULL_HANDLE;

	vkFreeMemory(context.device, indexBufferMemory, nullptr);
	indexBufferMemory = VK_NULL_HANDLE;
}

void VulkanMesh::clearCPUData()
{
	vertices.clear();
	indices.clear();
}