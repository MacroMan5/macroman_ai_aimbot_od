// InputPreprocessing.hlsl
// Compute Shader for DML Preprocessing
// 1. Resizes input texture (Point/Linear sampler)
// 2. Converts BGRA to RGB Planar
// 3. Normalizes 0-255 to 0.0-1.0

Texture2D<float4> InputTexture : register(t0);
SamplerState LinearSampler : register(s0);

// Output buffer: float array [3 * Height * Width]
// Layout: RRR...GGG...BBB...
RWStructuredBuffer<float> OutputBuffer : register(u0);

cbuffer Constants : register(b0)
{
    uint InputWidth;
    uint InputHeight;
    uint OutputWidth;
    uint OutputHeight;
    // ROI in normalized coordinates [0.0 - 1.0]
    float RoiLeft;
    float RoiTop;
    float RoiWidth;
    float RoiHeight;
}

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= OutputWidth || DTid.y >= OutputHeight)
        return;

    // Calculate UV in Output Space [0, 1]
    float u_out = (float(DTid.x) + 0.5f) / float(OutputWidth);
    float v_out = (float(DTid.y) + 0.5f) / float(OutputHeight);

    // Map to Input ROI [RoiLeft, RoiLeft + RoiWidth]
    float u_in = RoiLeft + (u_out * RoiWidth);
    float v_in = RoiTop + (v_out * RoiHeight);

    // Sample the input texture
    // Input is typically BGRA (DXGI standard)
    // .x=B, .y=G, .z=R, .w=A
    float4 pixel = InputTexture.SampleLevel(LinearSampler, float2(u_in, v_in), 0);

    // Calculate linear indices for Planar Output (NCHW)
    uint planeSize = OutputWidth * OutputHeight;
    uint pixelIndex = DTid.y * OutputWidth + DTid.x;

    // Write to output buffer (R, G, B planes)
    // Swap BGR to RGB
    OutputBuffer[pixelIndex]                 = pixel.z; // R
    OutputBuffer[pixelIndex + planeSize]     = pixel.y; // G
    OutputBuffer[pixelIndex + planeSize * 2] = pixel.x; // B
}
