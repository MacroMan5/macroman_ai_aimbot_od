// Input Preprocessing Shader
// Converts BGRA (0-255) Texture -> RGB Planar (0-1) Tensor (NCHW)
// Includes simple resizing/sampling (Linear or Nearest)

Texture2D<float4> InputTexture : register(t0);
RWBuffer<float> OutputTensor : register(u0); // Linear buffer [1 * 3 * 640 * 640]

cbuffer Constants : register(b0)
{
    uint InputWidth;   // e.g., 640
    uint InputHeight;  // e.g., 640
    uint OutputWidth;  // e.g., 640
    uint OutputHeight; // e.g., 640
};

// Dispatch(OutputWidth / 8, OutputHeight / 8, 1)
[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint x = dispatchThreadId.x;
    uint y = dispatchThreadId.y;

    if (x >= OutputWidth || y >= OutputHeight)
        return;

    // Nearest neighbor sampling (simplest/fastest)
    // For production, use bilinear if quality is critical
    uint srcX = (x * InputWidth) / OutputWidth;
    uint srcY = (y * InputHeight) / OutputHeight;

    // Read BGRA [0..1] from texture
    // Load() expects integer coords. Returns float4 normalized [0..1].
    // Format is DXGI_FORMAT_B8G8R8A8_UNORM, so:
    // .x = B, .y = G, .z = R, .w = A
    float4 pixel = InputTexture.Load(int3(srcX, srcY, 0));

    // NCHW Layout: [Batch, Channel, Height, Width]
    // R offset: 0 * H * W
    // G offset: 1 * H * W
    // B offset: 2 * H * W
    uint stride = OutputWidth * OutputHeight;
    uint idx = y * OutputWidth + x;

    // Write RGB Planar
    // pixel.z is Red, pixel.y is Green, pixel.x is Blue
    OutputTensor[0 * stride + idx] = pixel.z; // R
    OutputTensor[1 * stride + idx] = pixel.y; // G
    OutputTensor[2 * stride + idx] = pixel.x; // B
}
