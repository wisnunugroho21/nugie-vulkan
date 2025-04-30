#pragma once

#include "VulkanApp.h"

#include <assimp/GltfMaterial.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>

#include "UtilsAnim.h"
#include <lvk/LVK.h>

// --------- Skeleton ---------

inline glm::mat4 aiMatrix4x4ToMat4(const aiMatrix4x4 &from)
{
	glm::mat4 to;

	to[0][0] = (float)from.a1;
	to[0][1] = (float)from.b1;
	to[0][2] = (float)from.c1;
	to[0][3] = (float)from.d1;
	to[1][0] = (float)from.a2;
	to[1][1] = (float)from.b2;
	to[1][2] = (float)from.c2;
	to[1][3] = (float)from.d2;
	to[2][0] = (float)from.a3;
	to[2][1] = (float)from.b3;
	to[2][2] = (float)from.c3;
	to[2][3] = (float)from.d3;
	to[3][0] = (float)from.a4;
	to[3][1] = (float)from.b4;
	to[3][2] = (float)from.c4;
	to[3][3] = (float)from.d4;

	return to;
}

inline glm::vec3 aiVector3DToVec3(const aiVector3D &from)
{
	return glm::vec3(from.x, from.y, from.z);
}

inline glm::quat aiQuaternionToQuat(const aiQuaternion &from)
{
	return glm::quat(from.w, from.x, from.y, from.z);
}

struct Vertex
{
	vec3 position;
	vec3 normal;
	vec4 color;
	vec2 uv0;
	vec2 uv1;
	float padding[2];
};

struct MorphTarget
{
	uint32_t meshId = ~0;
	std::vector<uint32_t> offset;
};

static_assert(sizeof(Vertex) == sizeof(uint32_t) * 16);

using NodeRef = uint32_t;
using MeshRef = uint32_t;

struct Mesh
{
	lvk::Topology primitive;
	uint32_t vertexOffset;
	uint32_t vertexCount;
	uint32_t indexOffset;
	uint32_t indexCount;
};

struct Node
{
	std::string name;
	uint32_t modelMtxId;
	glm::mat4 transform = glm::mat4(1);
	std::vector<NodeRef> children;
	std::vector<MeshRef> meshes;
};

struct FrameData
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	vec4 cameraPos;
};

struct NodeTransformRef
{
	uint32_t modelMtxId;
	NodeRef nodeRef; // for CPU only
	MeshRef meshRef; // for CPU only
};

// Skeleton, animation, morphing
#define MAX_BONES_PER_VERTEX 8

struct VertexBoneData
{
	vec4 position;
	vec4 normal;
	uint32_t boneId[MAX_BONES_PER_VERTEX] = {~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u};
	float weight[MAX_BONES_PER_VERTEX] = {};
	uint32_t meshId = ~0u;
};

static_assert(sizeof(VertexBoneData) == sizeof(uint32_t) * 25);

struct Bone
{
	uint32_t boneId = ~0u;
	glm::mat4 transform = glm::mat4(1);
};

struct SkeletalMesh
{
	explicit SkeletalMesh(lvk::IContext* ctx, lvk::Format depthFormat)
		: ctx_(ctx), depthFormat_(depthFormat)
	{
	}

	FrameData frameData;
	std::vector<NodeTransformRef> nodeTransformRefs;
	std::vector<mat4> nodeTransformMatrices;	

	std::vector<Node> nodes;
	std::vector<Mesh> meshes;
	std::unordered_map<std::string, Bone> bonesByName;

	std::vector<MorphTarget> morphTargets;
	std::unordered_map<std::string, uint32_t> meshIdNameMap;

	std::vector<MorphState> morphStates;
	std::vector<Animation> animations;

	lvk::Holder<lvk::BufferHandle> perFrameBuffer;
	lvk::Holder<lvk::BufferHandle> nodeTransformRefBuffer;
	lvk::Holder<lvk::BufferHandle> nodeTransformMatricesBuffer;
	lvk::Holder<lvk::BufferHandle> morphStatesBuffer;
	
	lvk::Holder<lvk::BufferHandle> vertexBuffer;
	lvk::Holder<lvk::BufferHandle> vertexSkinningBuffer;
	lvk::Holder<lvk::BufferHandle> vertexMorphingBuffer;
	lvk::Holder<lvk::BufferHandle> indexBuffer;

	lvk::Holder<lvk::RenderPipelineHandle> pipelineSolid;
	lvk::Holder<lvk::ComputePipelineHandle> pipelineComputeAnimations;

	lvk::Holder<lvk::ShaderModuleHandle> vert;
	lvk::Holder<lvk::ShaderModuleHandle> frag;
	lvk::Holder<lvk::ShaderModuleHandle> animation;

	uint32_t maxVertices = 0;	
	NodeRef root;

	lvk::IContext* ctx_;
	lvk::Format depthFormat_;	

	bool hasBones = false;
	bool animated = false;
	bool skinning = false;
	bool morphing = false;
	bool doublesided = false;
	bool enableMorphing = true;
};

void load(SkeletalMesh &context, const char *gltfName, const char *glTFDataPath);
void render(SkeletalMesh &context, lvk::TextureHandle depthTexture, const glm::mat4 &model, const glm::mat4 &view, const glm::mat4 &proj, bool rebuildRenderList = false);
void animate(SkeletalMesh &gltf, AnimationState &anim, float dt);
void animateBlending(SkeletalMesh &gltf, AnimationState &anim1, AnimationState &anim2, float weight, float dt);

void printPrefix(int ofs);
void printMat4(const aiMatrix4x4 &m);
