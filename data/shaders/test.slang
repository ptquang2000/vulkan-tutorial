struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};
struct Global {
ConstantBuffer<UniformBuffer> ubo;
RWStructuredBuffer<float4> outputData;
Texture2D mainTexture[2];
SamplerState mainSampler;
Sampler2D combinedTexture[4];
RWTexture2D<float4> storageImage;
Buffer<float4> texelBuffer;
RWBuffer<float4> rwTexelBuffer;
// SubpassInput inputAttachment;
// RaytracingAccelerationStructure sceneAS[6];
};
ParameterBlock<Global> global;
struct VertData {
ConstantBuffer<UniformBuffer> ubo;
};
ParameterBlock<VertData> vertData;
struct FragData {
Sampler2D texture;
};
ParameterBlock<FragData> fragData;

struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = mul(vertData.ubo.proj, mul(vertData.ubo.view, mul(vertData.ubo.model, float4(input.inPosition, 1.0))));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
   return fragData.texture.Sample(vertIn.fragTexCoord);
}

