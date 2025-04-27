#include "UtilsGLTF.h"

uint32_t getNextMtxId(Context &gltf, const char *name, uint32_t &nextEmptyId, const mat4 &mtx)
{
	const auto it = gltf.bonesByName.find(name);

	const uint32_t mtxId = (it == gltf.bonesByName.end()) ? nextEmptyId++ : it->second.boneId;

	if (gltf.matrices.size() <= mtxId)
	{
		gltf.matrices.resize(mtxId + 1);
	}

	gltf.matrices[mtxId] = mtx;

	return mtxId;
}

static uint32_t getNumVertices(const aiScene &scene)
{
	uint32_t num = 0;
	for (uint32_t i = 0; i != scene.mNumMeshes; i++)
	{
		num += scene.mMeshes[i]->mNumVertices;
	}
	return num;
}

static uint32_t getNumIndices(const aiScene &scene)
{
	uint32_t num = 0;
	for (uint32_t i = 0; i != scene.mNumMeshes; i++)
	{
		for (uint32_t j = 0; j != scene.mMeshes[i]->mNumFaces; j++)
		{
			LVK_ASSERT(scene.mMeshes[i]->mFaces[j].mNumIndices == 3);
			num += scene.mMeshes[i]->mFaces[j].mNumIndices;
		}
	}
	return num;
}

static uint32_t getNumMorphVertices(const aiScene &scene)
{
	uint32_t num = 0;
	for (uint32_t i = 0; i != scene.mNumMeshes; i++)
	{
		num += scene.mMeshes[i]->mNumVertices * scene.mMeshes[i]->mNumAnimMeshes;
	}
	return num;
}

static uint32_t getNodeId(Context &gltf, const char *name)
{
	for (uint32_t i = 0; i != gltf.nodesStorage.size(); i++)
	{
		if (gltf.nodesStorage[i].name == name)
			return i;
	}
	return ~0;
}

void load(Context &gltf, const char *glTFName, const char *glTFDataPath)
{
	const aiScene *scene = aiImportFile(glTFName, aiProcess_Triangulate);
	if (!scene || !scene->HasMeshes())
	{
		printf("Unable to load %s\n", glTFName);
		exit(255);
	}
	SCOPE_EXIT
	{
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

	for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
	{
		const aiMesh *mesh = scene->mMeshes[m];
		gltf.meshesRemap[mesh->mName.C_Str()] = m;

		for (uint32_t i = 0; i < mesh->mNumVertices; i++)
		{
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

			if (mesh->mNumBones == 0)
			{
				auto &vertex = skinningData[vertices.size() - 1];
				vertex.meshId = m;
				vertex.position = vec4(v.x, v.y, v.z, 0.0f);
				vertex.normal = vec4(n.x, n.y, n.z, 0.0f);
			}
		}

		startVertex.push_back((uint32_t)vertices.size());
		for (uint32_t i = 0; i < mesh->mNumFaces; i++)
		{
			for (int j = 0; j != 3; j++)
			{
				indices.push_back(mesh->mFaces[i].mIndices[j]);
			}
		}
		startIndex.push_back((uint32_t)indices.size());

		gltf.hasBones = mesh->mNumBones > 0;
		// load bones
		for (uint32_t id = 0; id < mesh->mNumBones; id++)
		{
			const aiBone &bone = *mesh->mBones[id];
			const char *boneName = bone.mName.C_Str();

			const bool hasBone = gltf.bonesByName.contains(boneName);

			const uint32_t boneId = hasBone ? gltf.bonesByName[boneName].boneId : numBones++;

			if (!hasBone)
			{
				gltf.bonesByName[boneName] = {
					.boneId = boneId,
					.transform = aiMatrix4x4ToMat4(bone.mOffsetMatrix),
				};
			}

			for (uint32_t w = 0; w < bone.mNumWeights; w++)
			{
				const uint32_t vertexId = bone.mWeights[w].mVertexId;
				assert(vertexId <= vertices.size());

				VertexBoneData &vtx = skinningData[vertexId + vertOffset];
				assert(vtx.meshId == ~0u || vtx.meshId == m);

				vtx.position = vec4(vertices[vertexId + vertOffset].position, 1.0f);
				vtx.normal = vec4(vertices[vertexId + vertOffset].normal, 0.0f);
				vtx.meshId = m;
				for (uint32_t i = 0; i < MAX_BONES_PER_VERTEX; i++)
				{
					if (vtx.boneId[i] == ~0u)
					{
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
	for (uint32_t meshId = 0; meshId != scene->mNumMeshes; meshId++)
	{
		const aiMesh *m = scene->mMeshes[meshId];

		if (!m->mNumAnimMeshes)
			continue;

		MorphTarget &morphTarget = gltf.morphTargets[meshId];
		morphTarget.meshId = meshId;

		for (uint32_t a = 0; a < m->mNumAnimMeshes; a++)
		{
			const aiAnimMesh *mesh = m->mAnimMeshes[a];

			for (uint32_t i = 0; i < mesh->mNumVertices; i++)
			{
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

	if (!scene->mRootNode)
	{
		printf("Scene has no root node\n");
		exit(255);
	}

	auto &ctx = gltf.ctx_;

	uint32_t nonBoneMtxId = numBones;

	const char *rootName = scene->mRootNode->mName.C_Str() ? scene->mRootNode->mName.C_Str() : "root";

	gltf.nodesStorage.push_back({
		.name = rootName,
		.modelMtxId = getNextMtxId(gltf, rootName, nonBoneMtxId, aiMatrix4x4ToMat4(scene->mRootNode->mTransformation)),
		.transform = aiMatrix4x4ToMat4(scene->mRootNode->mTransformation),
	});

	gltf.root = gltf.nodesStorage.size() - 1;

	std::function<void(const aiNode *rootNode, NodeRef gltfNode)> traverseTree = [&](const aiNode *rootNode, NodeRef gltfNode)
	{
		for (unsigned int m = 0; m < rootNode->mNumMeshes; ++m)
		{
			const uint32_t meshIdx = rootNode->mMeshes[m];
			const aiMesh *mesh = scene->mMeshes[meshIdx];

			gltf.meshesStorage.push_back({
				.primitive = lvk::Topology_Triangle,
				.vertexOffset = startVertex[meshIdx],
				.vertexCount = mesh->mNumVertices,
				.indexOffset = startIndex[meshIdx],
				.indexCount = mesh->mNumFaces * 3,
				.matIdx = mesh->mMaterialIndex
			});

			gltf.nodesStorage[gltfNode].meshes.push_back(gltf.meshesStorage.size() - 1);
		}

		for (NodeRef i = 0; i < rootNode->mNumChildren; i++)
		{
			const aiNode *node = rootNode->mChildren[i];
			const char *childName = node->mName.C_Str() ? node->mName.C_Str() : "node";
			const Node childNode{
				.name = childName,
				.modelMtxId = getNextMtxId(
					gltf, childName, nonBoneMtxId,
					gltf.matrices[gltf.nodesStorage[gltfNode].modelMtxId] * aiMatrix4x4ToMat4(node->mTransformation)),
				.transform = aiMatrix4x4ToMat4(node->mTransformation),
			};

			gltf.nodesStorage.push_back(childNode);
			const size_t nodeIdx = gltf.nodesStorage.size() - 1;
			gltf.nodesStorage[gltfNode].children.push_back(nodeIdx);
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
			{.location = 0, .format = lvk::VertexFormat::Float3, .offset = 0},
			{.location = 1, .format = lvk::VertexFormat::Float3, .offset = 12},
			{.location = 2, .format = lvk::VertexFormat::Float4, .offset = 24},
			{.location = 3, .format = lvk::VertexFormat::Float2, .offset = 40},
			{.location = 4, .format = lvk::VertexFormat::Float2, .offset = 48},
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

void buildTransformsList(Context &gltf)
{
	gltf.transforms.clear();
	gltf.opaqueNodes.clear();

	std::function<void(NodeRef gltfNode)> traverseTree = [&](NodeRef nodeRef)
	{
		Node &node = gltf.nodesStorage[nodeRef];
		for (NodeRef meshId : node.meshes)
		{
			const Mesh &mesh = gltf.meshesStorage[meshId];
			gltf.transforms.push_back({
				.modelMtxId = node.modelMtxId,
				.matId = mesh.matIdx,
				.nodeRef = nodeRef,
				.meshRef = meshId
			});
			
			gltf.opaqueNodes.push_back(gltf.transforms.size() - 1);
		}
		
		for (NodeRef child : node.children)
		{
			traverseTree(child);
		}
	};

	traverseTree(gltf.root);

	gltf.transformBuffer = gltf.ctx_->createBuffer({
		.usage = lvk::BufferUsageBits_Storage,
		.storage = lvk::StorageType_HostVisible,
		.size = gltf.transforms.size() * sizeof(Transforms),
		.data = gltf.transforms.data(),
		.debugName = "Per Frame data",
	});

	gltf.matricesBuffer = gltf.ctx_->createBuffer({
		.usage = lvk::BufferUsageBits_Storage,
		.storage = lvk::StorageType_HostVisible,
		.size = gltf.matrices.size() * sizeof(mat4),
		.data = gltf.matrices.data(),
		.debugName = "Node matrices",
	});
}

void render(Context &gltf, lvk::TextureHandle depthTexture, const mat4 &model, const mat4 &view, const mat4 &proj, bool rebuildRenderList)
{
	auto &ctx = gltf.ctx_;

	const vec4 camPos = glm::inverse(view)[3];

	if (rebuildRenderList || gltf.transforms.empty())
	{
		buildTransformsList(gltf);
	}

	gltf.frameData = {
		.model = model,
		.view = view,
		.proj = proj,
		.cameraPos = camPos,
	};

	struct PushConstants
	{
		uint64_t draw;
		uint64_t transforms;
		uint64_t matrices;
	} pushConstants = {
		.draw = ctx->gpuAddress(gltf.perFrameBuffer),
		.transforms = ctx->gpuAddress(gltf.transformBuffer),
		.matrices = ctx->gpuAddress(gltf.matricesBuffer)
	};

	lvk::ICommandBuffer &buf = ctx->acquireCommandBuffer();

	buf.cmdUpdateBuffer(gltf.perFrameBuffer, gltf.frameData);

	if (gltf.animated)
	{
		buf.cmdUpdateBuffer(gltf.matricesBuffer, 0, gltf.matrices.size() * sizeof(mat4), gltf.matrices.data());
		if (gltf.morphing)
		{
			buf.cmdUpdateBuffer(gltf.morphStatesBuffer, 0, gltf.morphStates.size() * sizeof(MorphState), gltf.morphStates.data());
		}
		
		if ((gltf.skinning && gltf.hasBones) || gltf.morphing)
		{
			// Run compute shader to do skinning and morphing

			struct ComputeSetup
			{
				uint64_t matrices;
				uint64_t morphStates;
				uint64_t morphVertexBuffer;
				uint64_t inBuffer;
				uint64_t outBuffer;
				uint32_t numMorphStates;
			} pc = {
				.matrices = ctx->gpuAddress(gltf.matricesBuffer),
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
          { .buffers = { lvk::BufferHandle(gltf.vertexBuffer),
                         lvk::BufferHandle(gltf.morphStatesBuffer),
                         lvk::BufferHandle(gltf.matricesBuffer),
                         lvk::BufferHandle(gltf.vertexSkinningBuffer) } });
			// clang-format on
		}
	}

	{
		// 1st pass
		const lvk::RenderPass renderPass = {
			.color = {{.loadOp = lvk::LoadOp_Clear, .clearColor = {1.0f, 1.0f, 1.0f, 1.0f}}},
			.depth = {.loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f},
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
			for (uint32_t transformId : gltf.opaqueNodes)
			{
				const Transforms transform = gltf.transforms[transformId];

				buf.cmdPushDebugGroupLabel(gltf.nodesStorage[transform.nodeRef].name.c_str(), 0xff0000ff);
				const Mesh submesh = gltf.meshesStorage[transform.meshRef];
				buf.cmdDrawIndexed(submesh.indexCount, 1, submesh.indexOffset, submesh.vertexOffset, transformId);
				buf.cmdPopDebugGroupLabel();
			}
		}

		buf.cmdEndRendering();
	}

	ctx->wait(ctx->submit(buf, ctx->getCurrentSwapchainTexture()));
}

void printPrefix(int ofs)
{
	for (int i = 0; i < ofs; i++)
		printf("\t");
}

void printMat4(const aiMatrix4x4 &m)
{
	if (!m.IsIdentity())
	{
		for (int i = 0; i < 4; i++)
			for (int j = 0; j < 4; j++)
				printf("%f ;", m[i][j]);
	}
	else
	{
		printf(" Identity");
	}
}

void animate(Context &gltf, AnimationState &anim, float dt)
{
	if (gltf.transforms.empty())
		return;

	if (gltf.pipelineComputeAnimations.empty())
	{
		gltf.pipelineComputeAnimations = gltf.ctx_->createComputePipeline({
			.smComp = gltf.animation,
		});
	}

	// we support only one single animation at this time
	anim.active = anim.animId != ~0;
	gltf.animated = anim.active;
	if (anim.active)
	{
		updateAnimation(gltf, anim, dt);
	}
}

void animateBlending(Context &gltf, AnimationState &anim1, AnimationState &anim2, float weight, float dt)
{
	if (gltf.transforms.empty())
		return;

	if (gltf.pipelineComputeAnimations.empty())
	{
		gltf.pipelineComputeAnimations = gltf.ctx_->createComputePipeline({
			.smComp = gltf.animation,
		});
	}

	anim1.active = anim1.animId != ~0;
	anim2.active = anim2.animId != ~0;
	gltf.animated = anim1.active || anim2.active;
	if (anim1.active && anim2.active)
	{
		updateAnimationBlending(gltf, anim1, anim2, weight, dt);
	}
	else if (anim1.active)
	{
		updateAnimation(gltf, anim1, dt);
	}
	else if (anim2.active)
	{
		updateAnimation(gltf, anim2, dt);
	}
}

AnimationChannel initChannel(const aiNodeAnim *anim)
{
	AnimationChannel channel;
	channel.pos.resize(anim->mNumPositionKeys);

	for (uint32_t i = 0; i < anim->mNumPositionKeys; ++i)
	{
		channel.pos[i] = {.pos = aiVector3DToVec3(anim->mPositionKeys[i].mValue), .time = (float)anim->mPositionKeys[i].mTime};
	}

	channel.rot.resize(anim->mNumRotationKeys);
	for (uint32_t i = 0; i < anim->mNumRotationKeys; ++i)
	{
		channel.rot[i] = {.rot = aiQuaternionToQuat(anim->mRotationKeys[i].mValue), .time = (float)anim->mRotationKeys[i].mTime};
	}

	channel.scale.resize(anim->mNumScalingKeys);
	for (uint32_t i = 0; i < anim->mNumScalingKeys; ++i)
	{
		channel.scale[i] = {.scale = aiVector3DToVec3(anim->mScalingKeys[i].mValue), .time = (float)anim->mScalingKeys[i].mTime};
	}

	return channel;
}

template <typename T>
uint32_t getTimeIndex(const std::vector<T> &t, float time)
{
	return std::max(
		0,
		(int)std::distance(t.begin(), std::lower_bound(t.begin(), t.end(), time, [&](const T &lhs, float rhs)
													   { return lhs.time < rhs; })) -
			1);
}

float interpolationVal(float lastTimeStamp, float nextTimeStamp, float animationTime)
{
	return (animationTime - lastTimeStamp) / (nextTimeStamp - lastTimeStamp);
}

vec3 interpolatePosition(const AnimationChannel &channel, float time)
{
	if (channel.pos.size() == 1)
		return channel.pos[0].pos;

	uint32_t start = getTimeIndex<>(channel.pos, time);
	uint32_t end = start + 1;
	float mix = interpolationVal(channel.pos[start].time, channel.pos[end].time, time);
	return glm::mix(channel.pos[start].pos, channel.pos[end].pos, mix);
}

glm::quat interpolateRotation(const AnimationChannel &channel, float time)
{
	if (channel.rot.size() == 1)
		return channel.rot[0].rot;

	uint32_t start = getTimeIndex<>(channel.rot, time);
	uint32_t end = start + 1;
	float mix = interpolationVal(channel.rot[start].time, channel.rot[end].time, time);
	return glm::slerp(channel.rot[start].rot, channel.rot[end].rot, mix);
}

vec3 interpolateScaling(const AnimationChannel &channel, float time)
{
	if (channel.scale.size() == 1)
		return channel.scale[0].scale;

	uint32_t start = getTimeIndex<>(channel.scale, time);
	uint32_t end = start + 1;
	float coef = interpolationVal(channel.scale[start].time, channel.scale[end].time, time);
	return glm::mix(channel.scale[start].scale, channel.scale[end].scale, coef);
}

mat4 animationTransform(const AnimationChannel &channel, float time)
{
	mat4 translation = glm::translate(mat4(1.0f), interpolatePosition(channel, time));
	mat4 rotation = glm::toMat4(glm::normalize(interpolateRotation(channel, time)));
	mat4 scale = glm::scale(mat4(1.0f), interpolateScaling(channel, time));
	return translation * rotation * scale;
}

mat4 animationTransformBlending(const AnimationChannel &channel1, float time1, const AnimationChannel &channel2, float time2, float weight)
{
	mat4 trans1 = glm::translate(mat4(1.0f), interpolatePosition(channel1, time1));
	mat4 trans2 = glm::translate(mat4(1.0f), interpolatePosition(channel2, time2));
	mat4 translation = glm::mix(trans1, trans2, weight);

	glm::quat rot1 = interpolateRotation(channel1, time1);
	glm::quat rot2 = interpolateRotation(channel2, time2);

	mat4 rotation = glm::toMat4(glm::normalize(glm::slerp(rot1, rot2, weight)));

	vec3 scl1 = interpolateScaling(channel1, time1);
	vec3 scl2 = interpolateScaling(channel2, time2);
	mat4 scale = glm::scale(mat4(1.0f), glm::mix(scl1, scl2, weight));

	return translation * rotation * scale;
}

MorphState morphTransform(const MorphTarget &target, const MorphingChannel &channel, float time)
{
	MorphState ms;
	ms.meshId = target.meshId;

	float mix = 0.0f;
	int start = 0;
	int end = 0;

	if (channel.key.size() > 0)
	{
		start = getTimeIndex(channel.key, time);
		end = start + 1;
		mix = interpolationVal(channel.key[start].time, channel.key[end].time, time);
	}

	for (uint32_t i = 0; i < std::min((uint32_t)target.offset.size(), (uint32_t)MAX_MORPH_WEIGHTS); ++i)
	{
		ms.morphTarget[i] = target.offset[channel.key[start].mesh[i]];
		ms.weights[i] = glm::mix(channel.key[start].weight[i], channel.key[end].weight[i], mix);
	}

	return ms;
}

void initAnimations(Context &glTF, const aiScene *scene)
{
	glTF.animations.resize(scene->mNumAnimations);

	for (uint32_t i = 0; i < scene->mNumAnimations; ++i)
	{
		Animation &anim = glTF.animations[i];
		anim.name = scene->mAnimations[i]->mName.C_Str();
		anim.duration = scene->mAnimations[i]->mDuration;
		anim.ticksPerSecond = scene->mAnimations[i]->mTicksPerSecond;
		for (uint32_t c = 0; c < scene->mAnimations[i]->mNumChannels; c++)
		{
			const aiNodeAnim *channel = scene->mAnimations[i]->mChannels[c];
			const char *boneName = channel->mNodeName.data;
			uint32_t boneId = glTF.bonesByName[boneName].boneId;
			if (boneId == ~0u)
			{
				for (const Node &node : glTF.nodesStorage)
				{
					if (node.name != boneName)
						continue;
					boneId = node.modelMtxId;
					glTF.bonesByName[boneName] = {
						.boneId = boneId,
						.transform = glTF.hasBones ? glm::inverse(node.transform) : mat4(1),
					};
					break;
				}
			}
			assert(boneId != ~0u);
			anim.channels[boneId] = initChannel(channel);
		}

		const uint32_t numMorphTargetChannels = scene->mAnimations[i]->mNumMorphMeshChannels;
		anim.morphChannels.resize(numMorphTargetChannels);

		for (uint32_t c = 0; c < numMorphTargetChannels; c++)
		{
			const aiMeshMorphAnim *channel = scene->mAnimations[i]->mMorphMeshChannels[c];

			MorphingChannel &morphChannel = anim.morphChannels[c];

			morphChannel.name = channel->mName.C_Str();
			morphChannel.key.resize(channel->mNumKeys);

			for (uint32_t k = 0; k < channel->mNumKeys; ++k)
			{
				MorphingChannelKey &key = morphChannel.key[k];
				key.time = channel->mKeys[k].mTime;
				for (uint32_t v = 0; v < std::min((uint32_t)MAX_MORPH_WEIGHTS, channel->mKeys[k].mNumValuesAndWeights); ++v)
				{
					key.mesh[v] = channel->mKeys[k].mValues[v];
					key.weight[v] = channel->mKeys[k].mWeights[v];
				}
			}
		}
	}
}

void updateAnimation(Context &glTF, AnimationState &anim, float dt)
{
	if (!anim.active || (anim.animId == ~0u))
	{
		glTF.morphing = false;
		glTF.skinning = false;
		return;
	}

	const Animation &activeAnim = glTF.animations[anim.animId];
	anim.currentTime += activeAnim.ticksPerSecond * dt;

	if (anim.playOnce && anim.currentTime > activeAnim.duration)
	{
		anim.currentTime = activeAnim.duration;
		anim.active = false;
	}
	else
		anim.currentTime = fmodf(anim.currentTime, activeAnim.duration);

	// Apply animations
	std::function<void(NodeRef gltfNode, const mat4 &parentTransform)> traverseTree = [&](NodeRef gltfNode,
																							  const mat4 &parentTransform)
	{
		const Bone &bone = glTF.bonesByName[glTF.nodesStorage[gltfNode].name];
		const uint32_t boneId = bone.boneId;

		if (boneId != ~0u)
		{
			assert(boneId == glTF.nodesStorage[gltfNode].modelMtxId);
			auto channel = activeAnim.channels.find(boneId);
			const bool hasActiveChannel = channel != activeAnim.channels.end();

			glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] =
				parentTransform *
				(hasActiveChannel ? animationTransform(channel->second, anim.currentTime) : glTF.nodesStorage[gltfNode].transform);

			glTF.skinning = true;
		}
		else
		{
			glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] = parentTransform * glTF.nodesStorage[gltfNode].transform;
		}

		for (uint32_t i = 0; i < glTF.nodesStorage[gltfNode].children.size(); i++)
		{
			const NodeRef child = glTF.nodesStorage[gltfNode].children[i];

			traverseTree(child, glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId]);
		}
	};

	traverseTree(glTF.root, mat4(1.0f));

	for (const std::pair<std::string, Bone> &b : glTF.bonesByName)
	{
		if (b.second.boneId != ~0u)
		{
			glTF.matrices[b.second.boneId] = glTF.matrices[b.second.boneId] * b.second.transform;
		}
	}

	glTF.morphStates.resize(glTF.meshesStorage.size());
	// update morphing
	if (glTF.enableMorphing)
	{
		if (!activeAnim.morphChannels.empty())
		{
			for (size_t i = 0; i < activeAnim.morphChannels.size(); ++i)
			{
				const MorphingChannel &channel = activeAnim.morphChannels[i];
				const uint32_t meshId = glTF.meshesRemap[channel.name];
				const MorphTarget &morphTarget = glTF.morphTargets[meshId];

				if (morphTarget.meshId != ~0u)
				{
					glTF.morphStates[morphTarget.meshId] = morphTransform(morphTarget, channel, anim.currentTime);
				}
			}

			glTF.morphing = true;
		}
	}
}

void updateAnimationBlending(Context &glTF, AnimationState &anim1, AnimationState &anim2, float weight, float dt)
{
	if (anim1.active && anim2.active)
	{
		const Animation &activeAnim1 = glTF.animations[anim1.animId];
		anim1.currentTime += activeAnim1.ticksPerSecond * dt;

		if (anim1.playOnce && anim1.currentTime > activeAnim1.duration)
		{
			anim1.currentTime = activeAnim1.duration;
			anim1.active = false;
		}
		else
		{
			anim1.currentTime = fmodf(anim1.currentTime, activeAnim1.duration);
		}

		const Animation &activeAnim2 = glTF.animations[anim2.animId];
		anim2.currentTime += activeAnim2.ticksPerSecond * dt;

		if (anim2.playOnce && anim2.currentTime > activeAnim2.duration)
		{
			anim2.currentTime = activeAnim2.duration;
			anim2.active = false;
		}
		else
		{
			anim2.currentTime = fmodf(anim2.currentTime, activeAnim2.duration);
		}

		// Update skinning
		std::function<void(NodeRef gltfNode, const mat4 &parentTransform)> traverseTree = [&](NodeRef gltfNode,
																								  const mat4 &parentTransform)
		{
			const Bone &bone = glTF.bonesByName[glTF.nodesStorage[gltfNode].name];
			const uint32_t boneId = bone.boneId;
			if (boneId != ~0u)
			{
				auto channel1 = activeAnim1.channels.find(boneId);
				auto channel2 = activeAnim2.channels.find(boneId);

				if (channel1 != activeAnim1.channels.end() && channel2 != activeAnim2.channels.end())
				{
					glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] =
						parentTransform *
						animationTransformBlending(channel1->second, anim1.currentTime, channel2->second, anim2.currentTime, weight);
				}
				else if (channel1 != activeAnim1.channels.end())
				{
					glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] = parentTransform * animationTransform(channel1->second, anim1.currentTime);
				}
				else if (channel2 != activeAnim2.channels.end())
				{
					glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] = parentTransform * animationTransform(channel2->second, anim2.currentTime);
				}
				else
				{
					glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] = parentTransform * glTF.nodesStorage[gltfNode].transform;
				}
				glTF.skinning = true;
			}

			for (uint32_t i = 0; i < glTF.nodesStorage[gltfNode].children.size(); i++)
			{
				const uint32_t child = glTF.nodesStorage[gltfNode].children[i];

				traverseTree(child, glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId]);
			}
		};

		traverseTree(glTF.root, mat4(1.0f));

		for (const std::pair<std::string, Bone> &b : glTF.bonesByName)
		{
			if (b.second.boneId != ~0u)
			{
				glTF.matrices[b.second.boneId] = glTF.matrices[b.second.boneId] * b.second.transform;
			}
		}
	}
	else
	{
		glTF.morphing = false;
		glTF.skinning = false;
	}
}