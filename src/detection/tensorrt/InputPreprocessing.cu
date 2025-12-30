#include <cuda_runtime.h>
#include <device_launch_parameters.h>

// CUDA Kernel for resizing and normalization
// Input: BGRA (or RGBA) texture data [height, width, 4]
// Output: RGB planar [3, netHeight, netWidth] float (normalized 0-1)

__global__ void preprocessKernel(
    cudaTextureObject_t inputTexture,
    float* outputBuffer,
    int inputWidth, int inputHeight,
    int outputWidth, int outputHeight,
    int roiX, int roiY, int roiW, int roiH
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= outputWidth || y >= outputHeight) return;

    // Calculate normalized texture coordinates (0.0 to 1.0) relative to ROI
    float u_local = (x + 0.5f) / (float)outputWidth;
    float v_local = (y + 0.5f) / (float)outputHeight;

    // Map to global texture coordinates
    // u = (roiX + u_local * roiW) / inputWidth
    // v = (roiY + v_local * roiH) / inputHeight
    
    // Note: tex2D coords are normalized 0..1 if addressMode is normalized (default for textures)
    // BUT we used normalizedCoords = 0 (unnormalized) in TensorRTDetector.cpp:
    // texDesc.normalizedCoords = 0; // Use non-normalized coords (0..width)
    
    // So we calculate pixel coordinates directly:
    float u_global = (float)roiX + u_local * (float)roiW;
    float v_global = (float)roiY + v_local * (float)roiH;

    // Fetch pixel from texture
    float4 pixel = tex2D<float4>(inputTexture, u_global, v_global);

    // Swap BGR/RGB if needed. 
    // Capture is usually BGRA (DXGI standard).
    // TensorRT expects RGB planar.
    // tex2D returns normalized float (0.0-1.0) if ReadMode is NormalizedFloat, 
    // or 0-255 if ElementType. 
    // We will configure texture to return 0-1 float.

    // Write to planar output: [R plane, G plane, B plane]
    int planeSize = outputWidth * outputHeight;
    int idx = y * outputWidth + x;

    // Output: RGB Planar
    // Pixel is BGRA from DXGI
    outputBuffer[idx]                 = pixel.z; // R (from BGR's z which is R? No, BGRA: x=B, y=G, z=R, w=A)
    outputBuffer[idx + planeSize]     = pixel.y; // G
    outputBuffer[idx + planeSize * 2] = pixel.x; // B
}

extern "C" void launchPreprocessKernel(
    cudaTextureObject_t texObj,
    float* outputDeviceBuffer,
    int inputWidth, int inputHeight,
    int outputWidth, int outputHeight,
    int roiX, int roiY, int roiW, int roiH,
    cudaStream_t stream
) {
    dim3 blockSize(16, 16);
    dim3 gridSize((outputWidth + blockSize.x - 1) / blockSize.x, 
                  (outputHeight + blockSize.y - 1) / blockSize.y);

    preprocessKernel<<<gridSize, blockSize, 0, stream>>>(
        texObj, outputDeviceBuffer, inputWidth, inputHeight, outputWidth, outputHeight, roiX, roiY, roiW, roiH
    );
}
