#include "SkeletalMesh.h"

uint32_t saveTransformMatrix(SkeletalMesh &gltf, const char *name, uint32_t &nextEmptyId, const glm::mat4 &mtx) {
	const auto it = gltf.bonesByName.find(name);

	const uint32_t mtxId = (it == gltf.bonesByName.end()) ? nextEmptyId++ : it->second.boneId;

	if (gltf.nodeTransformMatrices.size() <= mtxId) {
		gltf.nodeTransformMatrices.resize(mtxId + 1);
	}

	gltf.nodeTransformMatrices[mtxId] = mtx;
	return mtxId;
}

static uint32_t getNumVertices(const aiScene &scene)
{
	uint32_t num = 0;
	for (uint32_t i = 0; i != scene.mNumMeshes; i++) {
		num += scene.mMeshes[i]->mNumVertices;
	}

	return num;
}

static uint32_t getNumIndices(const aiScene &scene)
{
	uint32_t num = 0;
	for (uint32_t i = 0; i != scene.mNumMeshes; i++) {
		for (uint32_t j = 0; j != scene.mMeshes[i]->mNumFaces; j++) {
			LVK_ASSERT(scene.mMeshes[i]->mFaces[j].mNumIndices == 3);
			num += scene.mMeshes[i]->mFaces[j].mNumIndices;
		}
	}

	return num;
}

static uint32_t getNumMorphVertices(const aiScene &scene)
{
	uint32_t num = 0;
	for (uint32_t i = 0; i != scene.mNumMeshes; i++) {
		num += scene.mMeshes[i]->mNumVertices * scene.mMeshes[i]->mNumAnimMeshes;
	}

	return num;
}

static uint32_t getNodeId(SkeletalMesh &gltf, const char *name) {
	for (uint32_t i = 0; i != gltf.nodes.size(); i++) {
		if (gltf.nodes[i].name == name)
			return i;
	}

	return ~0;
}

void load(SkeletalMesh &gltf, const char *glTFName, const char *glTFDataPath)
{
	const aiScene *scene = aiImportFile(glTFName, aiProcess_Triangulate);
	if (!scene || !scene->HasMeshes()) {
		printf("Unable to load %s\n", glTFName);
		exit(255);
	}

	SCOPE_EXIT {
		aiReleaseImport(scene);
	};

	std::vector<Vertex> vertices;
	std::vector<VertexBoneData> skinningData;
	std::vector<uint32_t> indices;
	std::vector<uint32_t> startVertex;
	std::vector<uint32_t> startIndex;

	startVertex.push_back(0);
	startIndex.push_back(0);

	vertices.reserve(getNumVertices(*scene));
	indices.reserve(getNumIndices(*scene));
	skinningData.resize(getNumVertices(*scene));

	std::vector<Vertex> morphData;
	gltf.morphTargets.resize(scene->mNumMeshes);
	morphData.reserve(getNumMorphVertices(*scene));

	uint32_t numBones = 0;
	uint32_t morphTargetsOffset = 0;
	uint32_t vertOffset = 0;

	for (uint32_t m = 0; m < scene->mNumMeshes; ++m) {
		const aiMesh *mesh = scene->mMeshes[m];
		gltf.meshIdNameMap[mesh->mName.C_Str()] = m;

		for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
			const aiVector3D v = mesh->mVertices[i];
			const aiVector3D n = mesh->mNormals ? mesh->mNormals[i] : aiVector3D(0.0f, 1.0f, 0.0f);
			const aiColor4D c = mesh->mColors[0] ? mesh->mColors[0][i] : aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
			const aiVector3D uv0 = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i] : aiVector3D(0.0f, 0.0f, 0.0f);
			const aiVector3D uv1 = mesh->mTextureCoords[1] ? mesh->mTextureCoords[1][i] : aiVector3D(0.0f, 0.0f, 0.0f);
			vertices.push_back({
				.position = vec3(v.x, v.y, v.z),
				.normal = vec3(n.x, n.y, n.z),
				.color = vec4(c.r, c.g, c.b, c.a),
				.uv0 = vec2(uv0.x, 1.0f - uv0.y),
				.uv1 = vec2(uv1.x, 1.0f - uv1.y),
			});

			if (mesh->mNumBones == 0) {
				auto &vertex = skinningData[vertices.size() - 1];
				vertex.meshId = m;
				vertex.position = vec4(v.x, v.y, v.z, 0.0f);
				vertex.normal = vec4(n.x, n.y, n.z, 0.0f);
			}
		}

		startVertex.push_back((uint32_t)vertices.size());
		for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
			for (int j = 0; j != 3; j++) {
				indices.push_back(mesh->mFaces[i].mIndices[j]);
			}
		}

		startIndex.push_back((uint32_t)indices.size());

		gltf.hasBones = mesh->mNumBones > 0;
		// load bones
		for (uint32_t id = 0; id < mesh->mNumBones; id++) {
			const aiBone &bone = *mesh->mBones[id];
			const char *boneName = bone.mName.C_Str();

			const bool isExistingBone = gltf.bonesByName.contains(boneName);

			const uint32_t boneId = isExistingBone ? gltf.bonesByName[boneName].boneId : numBones++;

			if (!isExistingBone) {
				gltf.bonesByName[boneName] = {
					.boneId = boneId,
					.transform = aiMatrix4x4ToMat4(bone.mOffsetMatrix),
				};
			}

			for (uint32_t w = 0; w < bone.mNumWeights; w++) {
				const uint32_t vertexId = bone.mWeights[w].mVertexId;
				assert(vertexId <= vertices.size());

				VertexBoneData &vtx = skinningData[vertexId + vertOffset];
				assert(vtx.meshId == ~0u || vtx.meshId == m);

				vtx.position = vec4(vertices[vertexId + vertOffset].position, 1.0f);
				vtx.normal = vec4(vertices[vertexId + vertOffset].normal, 0.0f);
				vtx.meshId = m;

				for (uint32_t i = 0; i < MAX_BONES_PER_VERTEX; i++) {
					if (vtx.boneId[i] == ~0u) {
						vtx.weight[i] = bone.mWeights[w].mWeight;
						vtx.boneId[i] = boneId;
						break;
					}
				}
			}
		}

		vertOffset += mesh->mNumVertices;
	}

	// load morph targets
	for (uint32_t meshId = 0; meshId != scene->mNumMeshes; meshId++) {
		const aiMesh *m = scene->mMeshes[meshId];

		if (!m->mNumAnimMeshes)
			continue;

		MorphTarget &morphTarget = gltf.morphTargets[meshId];
		morphTarget.meshId = meshId;

		for (uint32_t a = 0; a < m->mNumAnimMeshes; a++) {
			const aiAnimMesh *mesh = m->mAnimMeshes[a];

			for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
				const aiVector3D v = mesh->mVertices[i];
				const aiVector3D n = mesh->mNormals ? mesh->mNormals[i] : aiVector3D(0.0f, 1.0f, 0.0f);
				const aiVector3D srcNorm = m->mNormals ? m->mNormals[i] : aiVector3D(0.0f, 1.0f, 0.0f);
				const aiColor4D c = mesh->mColors[0] ? mesh->mColors[0][i] : aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
				const aiVector3D uv0 = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i] : aiVector3D(0.0f, 0.0f, 0.0f);
				const aiVector3D uv1 = mesh->mTextureCoords[1] ? mesh->mTextureCoords[1][i] : aiVector3D(0.0f, 0.0f, 0.0f);

				morphData.push_back({
					.position = vec3(v.x - m->mVertices[i].x, v.y - m->mVertices[i].y, v.z - m->mVertices[i].z),
					.normal = vec3(n.x - srcNorm.x, n.y - srcNorm.y, n.z - srcNorm.z),
					.color = vec4(c.r, c.g, c.b, c.a),
					.uv0 = vec2(uv0.x, 1.0f - uv0.y),
					.uv1 = vec2(uv1.x, 1.0f - uv1.y),
				});
			}

			morphTarget.offset.push_back(morphTargetsOffset);
			morphTargetsOffset += mesh->mNumVertices;
		}
	}

	if (!scene->mRootNode) {
		printf("Scene has no root node\n");
		exit(255);
	}

	auto &ctx = gltf.ctx_;

	uint32_t nonBoneMtxId = numBones;

	const char *rootName = scene->mRootNode->mName.C_Str() ? scene->mRootNode->mName.C_Str() : "root";

	gltf.nodes.push_back({
		.name = rootName,
		.modelMtxId = saveTransformMatrix(gltf, rootName, nonBoneMtxId, aiMatrix4x4ToMat4(scene->mRootNode->mTransformation)),
		.transform = aiMatrix4x4ToMat4(scene->mRootNode->mTransformation),
	});

	gltf.root = gltf.nodes.size() - 1;

	std::function<void(const aiNode *rootNode, NodeRef gltfNode)> traverseTree = [&](const aiNode *rootNode, NodeRef gltfNode) {
		for (unsigned int m = 0; m < rootNode->mNumMeshes; ++m) {
			const uint32_t meshIdx = rootNode->mMeshes[m];
			const aiMesh *mesh = scene->mMeshes[meshIdx];

			gltf.meshes.push_back({
				.primitive = lvk::Topology_Triangle,
				.vertexOffset = startVertex[meshIdx],
				.vertexCount = mesh->mNumVertices,
				.indexOffset = startIndex[meshIdx],
				.indexCount = mesh->mNumFaces * 3
			});

			gltf.nodes[gltfNode].meshes.push_back(gltf.meshes.size() - 1);
		}

		for (NodeRef i = 0; i < rootNode->mNumChildren; i++) {
			const aiNode *node = rootNode->mChildren[i];
			const char *childName = node->mName.C_Str() ? node->mName.C_Str() : "node";
			const Node childNode{
				.name = childName,
				.modelMtxId = saveTransformMatrix(
					gltf, childName, nonBoneMtxId,
					gltf.nodeTransformMatrices[gltf.nodes[gltfNode].modelMtxId] * aiMatrix4x4ToMat4(node->mTransformation)
				),
				.transform = aiMatrix4x4ToMat4(node->mTransformation),
			};

			gltf.nodes.push_back(childNode);
			const size_t nodeIdx = gltf.nodes.size() - 1;
			gltf.nodes[gltfNode].children.push_back(nodeIdx);
			traverseTree(node, nodeIdx);
		}
	};

	traverseTree(scene->mRootNode, gltf.root);

	initAnimations(gltf, scene);

	// add dummy vertices to align buffer to 16 to run compute shader
	gltf.maxVertices = (1 + (vertices.size() / 16)) * 16;
	vertices.resize(gltf.maxVertices);

	gltf.vertexBuffer = ctx->createBuffer({
		.usage = lvk::BufferUsageBits_Vertex | lvk::BufferUsageBits_Storage,
		.storage = lvk::StorageType_Device,
		.size = sizeof(Vertex) * vertices.size(),
		.data = vertices.data(),
		.debugName = "Buffer: vertex",
	});

	size_t vssize = (1 + (skinningData.size() / 16)) * 16;
	skinningData.resize(vssize);

	assert(vssize == gltf.maxVertices);

	gltf.vertexSkinningBuffer = ctx->createBuffer({
		.usage = lvk::BufferUsageBits_Vertex | lvk::BufferUsageBits_Storage,
		.storage = lvk::StorageType_Device,
		.size = sizeof(VertexBoneData) * skinningData.size(),
		.data = skinningData.data(),
		.debugName = "Buffer: skinning vertex data",
	});

	const bool hasMorphData = !morphData.empty();

	gltf.vertexMorphingBuffer = ctx->createBuffer({
		.usage = lvk::BufferUsageBits_Vertex | lvk::BufferUsageBits_Storage,
		.storage = lvk::StorageType_Device,
		.size = hasMorphData ? sizeof(Vertex) * morphData.size() : sizeof(Vertex), // always have dummy buffer
		.data = hasMorphData ? morphData.data() : nullptr,
		.debugName = "Buffer: morphing vertex data",
	});

	gltf.morphStatesBuffer = ctx->createBuffer({
		.usage = lvk::BufferUsageBits_Vertex | lvk::BufferUsageBits_Storage,
		.storage = lvk::StorageType_HostVisible,
		.size = MAX_MORPHS * sizeof(MorphState),
		.debugName = "Morphs matrices",
	});

	gltf.indexBuffer = ctx->createBuffer({
		.usage = lvk::BufferUsageBits_Index,
		.storage = lvk::StorageType_Device,
		.size = sizeof(uint32_t) * indices.size(),
		.data = indices.data(),
		.debugName = "Buffer: index",
	});

	const lvk::VertexInput vdesc = {
		.attributes = {
			{ .location = 0, .format = lvk::VertexFormat::Float3, .offset = 0  },
			{ .location = 1, .format = lvk::VertexFormat::Float3, .offset = 12 },
			{ .location = 2, .format = lvk::VertexFormat::Float4, .offset = 24 },
			{ .location = 3, .format = lvk::VertexFormat::Float2, .offset = 40 },
			{ .location = 4, .format = lvk::VertexFormat::Float2, .offset = 48 },
		},
		.inputBindings = {{.stride = sizeof(Vertex)}},
	};

	gltf.vert = loadShaderModule(ctx, "../../src/shaders/gltf/main.vert");
	gltf.frag = loadShaderModule(ctx, "../../src/shaders/gltf/main.frag");
	gltf.animation = loadShaderModule(ctx, "../../src/shaders/gltf/animation.comp");

	gltf.pipelineSolid = ctx->createRenderPipeline({
		.vertexInput = vdesc,
		.smVert = gltf.vert,
		.smFrag = gltf.frag,
		.color = {{.format = ctx->getSwapchainFormat()}},
		.depthFormat = gltf.depthFormat_,
		.cullMode = gltf.doublesided ? lvk::CullMode_None : lvk::CullMode_Back,
	});

	gltf.perFrameBuffer = ctx->createBuffer({
		.usage = lvk::BufferUsageBits_Uniform,
		.storage = lvk::StorageType_Device,
		.size = sizeof(FrameData),
		.data = &gltf.frameData,
		.debugName = "Context::perFrameBuffer",
	});
}

void buildTransformsList(SkeletalMesh &gltf) {
	gltf.nodeTransformRefs.clear();

	std::function<void(NodeRef gltfNode)> traverseTree = [&](NodeRef nodeRef) {
		Node &node = gltf.nodes[nodeRef];
		for (NodeRef meshId : node.meshes) {
			const Mesh &mesh = gltf.meshes[meshId];
			gltf.nodeTransformRefs.push_back({
				.modelMtxId = node.modelMtxId,
				.nodeRef = nodeRef,
				.meshRef = meshId
			});
		}
		
		for (NodeRef child : node.children) {
			traverseTree(child);
		}
	};

	traverseTree(gltf.root);

	gltf.nodeTransformRefBuffer = gltf.ctx_->createBuffer({
		.usage = lvk::BufferUsageBits_Storage,
		.storage = lvk::StorageType_HostVisible,
		.size = gltf.nodeTransformRefs.size() * sizeof(NodeTransformRef),
		.data = gltf.nodeTransformRefs.data(),
		.debugName = "Per Frame data",
	});

	gltf.nodeTransformMatricesBuffer = gltf.ctx_->createBuffer({
		.usage = lvk::BufferUsageBits_Storage,
		.storage = lvk::StorageType_HostVisible,
		.size = gltf.nodeTransformMatrices.size() * sizeof(mat4),
		.data = gltf.nodeTransformMatrices.data(),
		.debugName = "Node matrices",
	});
}

void render(SkeletalMesh &gltf, lvk::TextureHandle depthTexture, const glm::mat4 &model, const glm::mat4 &view, const glm::mat4 &proj, bool rebuildRenderList) {
	auto &ctx = gltf.ctx_;

	const vec4 camPos = glm::inverse(view)[3];

	if (rebuildRenderList || gltf.nodeTransformRefs.empty()) {
		buildTransformsList(gltf);
	}

	gltf.frameData = {
		.model = model,
		.view = view,
		.proj = proj,
		.cameraPos = camPos,
	};

	struct PushConstants {
		uint64_t draw;
		uint64_t nodeTransformRefs;
		uint64_t nodeTransformMatrices;
	} pushConstants = {
		.draw = ctx->gpuAddress(gltf.perFrameBuffer),
		.nodeTransformRefs = ctx->gpuAddress(gltf.nodeTransformRefBuffer),
		.nodeTransformMatrices = ctx->gpuAddress(gltf.nodeTransformMatricesBuffer)
	};

	lvk::ICommandBuffer &buf = ctx->acquireCommandBuffer();

	buf.cmdUpdateBuffer(gltf.perFrameBuffer, gltf.frameData);

	if (gltf.animated) {
		buf.cmdUpdateBuffer(gltf.nodeTransformMatricesBuffer, 0, gltf.nodeTransformMatrices.size() * sizeof(mat4), gltf.nodeTransformMatrices.data());

		if (gltf.morphing) {
			buf.cmdUpdateBuffer(gltf.morphStatesBuffer, 0, gltf.morphStates.size() * sizeof(MorphState), gltf.morphStates.data());
		}
		
		if ((gltf.skinning && gltf.hasBones) || gltf.morphing) {
			// Run compute shader to do skinning and morphing

			struct ComputeSetup
			{
				uint64_t nodeTransformMatrices;
				uint64_t morphStates;
				uint64_t morphVertexBuffer;
				uint64_t inBuffer;
				uint64_t outBuffer;
				uint32_t numMorphStates;
			} pc = {
				.nodeTransformMatrices = ctx->gpuAddress(gltf.nodeTransformMatricesBuffer),
				.morphStates = ctx->gpuAddress(gltf.morphStatesBuffer),
				.morphVertexBuffer = ctx->gpuAddress(gltf.vertexMorphingBuffer),
				.inBuffer = ctx->gpuAddress(gltf.vertexSkinningBuffer),
				.outBuffer = ctx->gpuAddress(gltf.vertexBuffer),
				.numMorphStates = static_cast<uint32_t>(gltf.morphStates.size()),
			};

			buf.cmdBindComputePipeline(gltf.pipelineComputeAnimations);
			buf.cmdPushConstants(pc);

			// clang-format off
			buf.cmdDispatchThreadGroups(
			{ .width = gltf.maxVertices / 16 },
			{ 
				.buffers = { 
					lvk::BufferHandle(gltf.vertexBuffer),
					lvk::BufferHandle(gltf.morphStatesBuffer),
					lvk::BufferHandle(gltf.nodeTransformMatricesBuffer),
					lvk::BufferHandle(gltf.vertexSkinningBuffer) 
				} 
			});
			// clang-format on
		}
	}

	{
		// 1st pass
		const lvk::RenderPass renderPass = {
			.color = {{ .loadOp = lvk::LoadOp_Clear, .clearColor = {1.0f, 1.0f, 1.0f, 1.0f} }},
			.depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f},
		};

		const lvk::Framebuffer framebuffer = {
			.color = {{ .texture = ctx->getCurrentSwapchainTexture() }},
			.depthStencil = {.texture = depthTexture},
		};

		buf.cmdBeginRendering(renderPass, framebuffer, {.buffers = {{lvk::BufferHandle(gltf.vertexBuffer)}}});

		{
			buf.cmdBindVertexBuffer(0, gltf.vertexBuffer, 0);
			buf.cmdBindIndexBuffer(gltf.indexBuffer, lvk::IndexFormat_UI32);

			buf.cmdBindDepthState({.compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true});

			buf.cmdBindRenderPipeline(gltf.pipelineSolid);
			buf.cmdPushConstants(pushConstants);
			for (uint32_t transformId = 0; transformId < static_cast<uint32_t>(gltf.nodeTransformRefs.size()); transformId++)
			{
				const NodeTransformRef transform = gltf.nodeTransformRefs[transformId];
				const Mesh submesh = gltf.meshes[transform.meshRef];

				buf.cmdPushDebugGroupLabel(gltf.nodes[transform.nodeRef].name.c_str(), 0xff0000ff);				
				buf.cmdDrawIndexed(submesh.indexCount, 1, submesh.indexOffset, submesh.vertexOffset, transformId);
				buf.cmdPopDebugGroupLabel();
			}
		}

		buf.cmdEndRendering();
	}

	ctx->wait(ctx->submit(buf, ctx->getCurrentSwapchainTexture()));
}

void animate(SkeletalMesh &gltf, AnimationState &anim, float dt) {
	if (gltf.nodeTransformRefs.empty())
		return;

	if (gltf.pipelineComputeAnimations.empty()) {
		gltf.pipelineComputeAnimations = gltf.ctx_->createComputePipeline({
			.smComp = gltf.animation,
		});
	}

	// we support only one single animation at this time
	anim.active = anim.animId != ~0;
	gltf.animated = anim.active;
	if (anim.active) {
		updateAnimation(gltf, anim, dt);
	}
}

void animateBlending(SkeletalMesh &gltf, AnimationState &anim1, AnimationState &anim2, float weight, float dt) {
	if (gltf.nodeTransformRefs.empty())
		return;

	if (gltf.pipelineComputeAnimations.empty()) {
		gltf.pipelineComputeAnimations = gltf.ctx_->createComputePipeline({
			.smComp = gltf.animation,
		});
	}

	anim1.active = anim1.animId != ~0;
	anim2.active = anim2.animId != ~0;
	gltf.animated = anim1.active || anim2.active;
	
	if (anim1.active && anim2.active) {
		updateAnimationBlending(gltf, anim1, anim2, weight, dt);
	} else if (anim1.active) {
		updateAnimation(gltf, anim1, dt);
	} else if (anim2.active) {
		updateAnimation(gltf, anim2, dt);
	}
}