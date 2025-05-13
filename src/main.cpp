#include "shared/VulkanApp.h"

#include <assimp/GltfMaterial.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

struct Vertex {
	vec3 position;
	vec3 normal;
	vec4 color;
	vec2 uv0;
	vec2 uv1;
	float padding[2];
};

struct Mesh {
	uint32_t vertexOffset;
	uint32_t vertexCount;
	uint32_t indexOffset;
	uint32_t indexCount;
};

struct TotalMeshData {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

void loadGLTFModel(const char *filename, TotalMeshData* const totalMeshData, std::vector<Mesh>* const meshes) {
	const aiScene *scene = aiImportFile(filename, aiProcess_Triangulate);

	if (!scene || !scene->HasMeshes()) {
		printf("Unable to load %s\n", filename);
		exit(255);
	}
	SCOPE_EXIT
	{
		aiReleaseImport(scene);
	};

	std::vector<uint32_t> startVertex{0};
	std::vector<uint32_t> startIndices{0};

	uint32_t vertexNums = 0; 
	for (uint32_t i = 0; i < scene->mNumMeshes; i++) {
		vertexNums += scene->mMeshes[i]->mNumVertices;
	}

	uint32_t indexNums = 0;
	for (uint32_t i = 0; i < scene->mNumMeshes; i++) {
		for (uint32_t j = 0; j < scene->mMeshes[i]->mNumFaces; j++) {
			indexNums += scene->mMeshes[i]->mFaces[j].mNumIndices;
		}
	}

	startVertex.reserve(vertexNums);
	startIndices.reserve(indexNums);
	meshes->reserve(scene->mNumMeshes);

	for (uint32_t m = 0; m < scene->mNumMeshes; m++) {
		const aiMesh *mesh = scene->mMeshes[m];

		for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
			const aiVector3D v = mesh->mVertices[i];
			const aiVector3D n = mesh->mNormals ? mesh->mNormals[i] : aiVector3D(0.0f, 1.0f, 0.0);
			const aiColor4D c = mesh->mColors[0] ? mesh->mColors[0][i] : aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
			const aiVector3D uv0 = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i] : aiVector3D(0.0f, 0.0f, 0.0f);
			const aiVector3D uv1 = mesh->mTextureCoords[1] ? mesh->mTextureCoords[1][i] : aiVector3D(0.0f, 0.0f, 0.0f);

			totalMeshData->vertices.push_back({
				.position = vec3(v.x, v.y, v.z),
				.normal = vec3(n.x, n.y, n.z),
				.color = vec4(c.r, c.g, c.b, c.a),
				.uv0 = vec2(uv0.x, 1.0f - uv0.y),
				.uv1 = vec2(uv1.x, 1.0f - uv1.y)
			});
		}

		for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
			for (uint32_t j = 0; j < 3; j++) {
				totalMeshData->indices.push_back(mesh->mFaces[i].mIndices[j]);
			}
		}

		startVertex.push_back(static_cast<uint32_t>(totalMeshData->vertices.size()));
		startIndices.push_back(static_cast<uint32_t>(totalMeshData->indices.size()));
	}


	for (uint32_t m = 0; m < scene->mNumMeshes; m++) {
		meshes->push_back(Mesh{
			.vertexOffset = startVertex[m],
			.vertexCount = scene->mMeshes[m]->mNumVertices,
			.indexOffset = startIndices[m],
			.indexCount = scene->mMeshes[m]->mNumFaces * 3
		});
	}
}

int main() {
	VulkanApp app({
		.initialCameraPos = vec3(0, 0, -3.0f),
		.initialCameraTarget = vec3(0, 0, 0)
	});

	TotalMeshData totalMeshData{};
	std::vector<Mesh> meshes;

	loadGLTFModel("../../data/box/Box.gltf", &totalMeshData, &meshes);

	lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(app.ctx_.get(), "../../src/shaders/main.vert");
	lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(app.ctx_.get(), "../../src/shaders/main.frag");

	lvk::Holder<lvk::BufferHandle> vertexBuffer = app.ctx_->createBuffer({
		.usage = lvk::BufferUsageBits_Vertex,
		.storage = lvk::StorageType_Device,
		.size = sizeof(Vertex) * totalMeshData.vertices.size(),
		.data = totalMeshData.vertices.data(),
		.debugName = "Vertex Buffer"
	});

	lvk::Holder<lvk::BufferHandle> indexBuffer = app.ctx_->createBuffer({
		.usage = lvk::BufferUsageBits_Index,
		.storage = lvk::StorageType_Device,
		.size = sizeof(uint32_t) * totalMeshData.indices.size(),
		.data = totalMeshData.indices.data(),
		.debugName = "Index Buffer"
	});

	lvk::Holder<lvk::TextureHandle> depthTexture = app.ctx_->createTexture({
		.type = lvk::TextureType_2D,
		.format = lvk::Format_Z_F32,
		.dimensions = {1280, 720},
		.usage = lvk::TextureUsageBits_Attachment,
		.debugName = "Depth Buffer"
	});

	const lvk::VertexInput vertexDesc {
		.attributes = {
			{
				.location = 0,
				.format = lvk::VertexFormat::Float3,
				.offset = 0
			},
			{
				.location = 1,
				.format = lvk::VertexFormat::Float3,
				.offset = 12
			},
			{
				.location = 2,
				.format = lvk::VertexFormat::Float4,
				.offset = 24
			},
			{
				.location = 3,
				.format = lvk::VertexFormat::Float2,
				.offset = 40
			},
			{
				.location = 4,
				.format = lvk::VertexFormat::Float2,
				.offset = 48
			}
		},
		.inputBindings = {
			{
				.stride = sizeof(Vertex)
			}
		}
	};

	lvk::Holder<lvk::RenderPipelineHandle> rpTriangle = app.ctx_->createRenderPipeline({
		.vertexInput = vertexDesc,
		.smVert = vert,
		.smFrag = frag,
		.color = {
			{
				.format = app.ctx_->getSwapchainFormat()
			}
		},
		.depthFormat = app.ctx_->getFormat(depthTexture),
		.cullMode = lvk::CullMode_Back
	});

	const mat4 m = glm::rotate(mat4(1.0f), glm::radians(-45.0f), vec3(1, 0, 0));
    /* const mat4 v = glm::rotate(glm::translate(mat4(1.0f), vec3(0.0f, -0.5f, -1.5f)), 0.0f, vec3(0.0f, 1.0f, 0.0f));
    const mat4 p = glm::perspective(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);

	const mat4 mvp = p * v * m; */

	app.run([&](uint32_t width, uint32_t height, float deltaSeconds) {
		lvk::ICommandBuffer& commandBuf = app.ctx_->acquireCommandBuffer();

		const mat4 mvp = app.projection_.getProjectionMatrix() * app.positioner_.getViewMatrix() * m;

		commandBuf.cmdBeginRendering({
			.color = {
				{
					.loadOp = lvk::LoadOp_Clear,
					.clearColor = { 0.0f, 0.0f, 0.0f, 1.0f }
				}
			},
			.depth = {
				.loadOp = lvk::LoadOp_Clear,
				.clearDepth = 1.0f
			}
		}, {
			.color = {
				{
					.texture = app.ctx_->getCurrentSwapchainTexture()
				}
			},
			.depthStencil = {
				.texture = depthTexture
			}
		}, {
			.buffers = {
				{
					lvk::BufferHandle(vertexBuffer)
				}
			}
		});

		commandBuf.cmdPushDebugGroupLabel("Render Mesh", 0xff0000ff);

		commandBuf.cmdBindVertexBuffer(0, vertexBuffer, 0);
		commandBuf.cmdBindIndexBuffer(indexBuffer, lvk::IndexFormat_UI32);
		commandBuf.cmdBindRenderPipeline(rpTriangle);
		commandBuf.cmdPushConstants(mvp);

		for (size_t i = 0; i < meshes.size(); i++) {
			const Mesh mesh = meshes[i];
			commandBuf.cmdDrawIndexed(mesh.indexCount, 1, mesh.indexOffset, mesh.vertexOffset, 0);
		}

		commandBuf.cmdPopDebugGroupLabel();
		commandBuf.cmdEndRendering();

		app.ctx_->submit(commandBuf, app.ctx_->getCurrentSwapchainTexture());
	});
	
	return 0;
}