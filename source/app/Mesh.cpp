#include "Mesh.h"

#include <render/backend/Driver.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>

/*
 */
Mesh::~Mesh()
{
	clearGPUData();
	clearCPUData();
}

/*
 */
bool Mesh::import(const char *path)
{
	Assimp::Importer importer;

	const aiScene *scene = importer.ReadFile(path, aiProcessPreset_TargetRealtime_MaxQuality);

	if (!scene)
	{
		std::cerr << "Mesh::import(): " << importer.GetErrorString() << std::endl;
		return false;
	}

	if (!scene->HasMeshes())
	{
		std::cerr << "Mesh::import(): model has no meshes" << std::endl;
		return false;
	}

	return import(scene->mMeshes[0]);
}

bool Mesh::import(const aiMesh *mesh)
{
	assert(mesh != nullptr);

	// Fill CPU data
	vertices.resize(mesh->mNumVertices);
	indices.resize(mesh->mNumFaces * 3);

	aiVector3D *meshVertices = mesh->mVertices;

	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		vertices[i].position = glm::vec3(meshVertices[i].x, meshVertices[i].y, meshVertices[i].z);

	aiVector3D *meshTangents = mesh->mTangents;
	if (meshTangents)
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
			vertices[i].tangent = glm::vec3(meshTangents[i].x, meshTangents[i].y, meshTangents[i].z);
	else
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
			vertices[i].tangent = glm::vec3(0.0f, 0.0f, 0.0f);

	aiVector3D *meshBinormals = mesh->mBitangents;
	if (meshBinormals)
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
			vertices[i].binormal = glm::vec3(meshBinormals[i].x, meshBinormals[i].y, meshBinormals[i].z);
	else
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
			vertices[i].binormal = glm::vec3(0.0f, 0.0f, 0.0f);

	aiVector3D *meshNormals = mesh->mNormals;
	if (meshNormals)
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
			vertices[i].normal = glm::vec3(meshNormals[i].x, meshNormals[i].y, meshNormals[i].z);
	else
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
			vertices[i].normal = glm::vec3(0.0f, 0.0f, 0.0f);

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

void Mesh::createSkybox(float size)
{
	clearCPUData();
	clearGPUData();

	vertices.resize(8);
	indices.resize(36);

	float halfSize = size * 0.5f;

	vertices[0].position = glm::vec3(-halfSize, -halfSize, -halfSize);
	vertices[1].position = glm::vec3( halfSize, -halfSize, -halfSize);
	vertices[2].position = glm::vec3( halfSize,  halfSize, -halfSize);
	vertices[3].position = glm::vec3(-halfSize,  halfSize, -halfSize);
	vertices[4].position = glm::vec3(-halfSize, -halfSize,  halfSize);
	vertices[5].position = glm::vec3( halfSize, -halfSize,  halfSize);
	vertices[6].position = glm::vec3( halfSize,  halfSize,  halfSize);
	vertices[7].position = glm::vec3(-halfSize,  halfSize,  halfSize);

	indices = {
		0, 1, 2, 2, 3, 0,
		1, 5, 6, 6, 2, 1,
		3, 2, 6, 6, 7, 3,
		5, 4, 6, 4, 7, 6,
		1, 0, 4, 4, 5, 1,
		4, 0, 3, 3, 7, 4,
	};

	uploadToGPU();
}

void Mesh::createQuad(float size)
{
	clearCPUData();
	clearGPUData();

	vertices.resize(4);
	indices.resize(6);

	float halfSize = size * 0.5f;

	vertices[0].position = glm::vec3(-halfSize, -halfSize, 0.0f);
	vertices[1].position = glm::vec3( halfSize, -halfSize, 0.0f);
	vertices[2].position = glm::vec3( halfSize,  halfSize, 0.0f);
	vertices[3].position = glm::vec3(-halfSize,  halfSize, 0.0f);

	vertices[0].uv = glm::vec2(0.0f, 0.0f);
	vertices[1].uv = glm::vec2(1.0f, 0.0f);
	vertices[2].uv = glm::vec2(1.0f, 1.0f);
	vertices[3].uv = glm::vec2(0.0f, 1.0f);

	indices = {
		1, 0, 2, 3, 2, 0,
	};

	uploadToGPU();
}

/*
 */
void Mesh::createVertexBuffer()
{
	static render::backend::VertexAttribute attributes[6] =
	{
		{ render::backend::Format::R32G32B32_SFLOAT, offsetof(Vertex, position) },
		{ render::backend::Format::R32G32_SFLOAT, offsetof(Vertex, uv) },
		{ render::backend::Format::R32G32B32_SFLOAT, offsetof(Vertex, tangent) },
		{ render::backend::Format::R32G32B32_SFLOAT, offsetof(Vertex, binormal) },
		{ render::backend::Format::R32G32B32_SFLOAT, offsetof(Vertex, normal) },
		{ render::backend::Format::R32G32B32_SFLOAT, offsetof(Vertex, color) },
	};

	vertex_buffer = driver->createVertexBuffer(
		render::backend::BufferType::STATIC,
		sizeof(Vertex), static_cast<uint32_t>(vertices.size()),
		6, attributes,
		vertices.data()
	);
}

void Mesh::createIndexBuffer()
{
	num_indices = static_cast<uint32_t>(indices.size());

	index_buffer = driver->createIndexBuffer(
		render::backend::BufferType::STATIC,
		render::backend::IndexFormat::UINT32,
		num_indices,
		indices.data()
	);
}

/*
 */
void Mesh::uploadToGPU()
{
	clearGPUData();

	createVertexBuffer();
	createIndexBuffer();
}

void Mesh::clearGPUData()
{
	driver->destroyVertexBuffer(vertex_buffer);
	driver->destroyIndexBuffer(index_buffer);
}

void Mesh::clearCPUData()
{
	vertices.clear();
	indices.clear();
}
