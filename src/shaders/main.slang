struct VertexInput
{
    float3 position;
    float3 normal;
    float4 color;
    float2 uv0;
    float2 uv1;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color;
};

struct FragmentInput
{
    float4 color;
};

struct FragmentOutput
{
    float4 color;
};

struct PerFrameData
{
    float4x4 MVP;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uniform PerFrameData ubo)
{
    VertexOutput output;

    output.position = mul(ubo.MVP, float4(input.position, 1.0));
    output.color = input.color;

    return output;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input)
{
    FragmentOutput output;
    output.color = input.color;

    return output;
}

