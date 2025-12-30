#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>

namespace macroman {

using Microsoft::WRL::ComPtr;

class InputPreprocessor {
private:
    ComPtr<ID3D11ComputeShader> computeShader_;
    ComPtr<ID3D11Buffer> constantBuffer_;

    struct Constants {
        uint32_t inputWidth;
        uint32_t inputHeight;
        uint32_t outputWidth;
        uint32_t outputHeight;
    };

public:
    bool initialize(ID3D11Device* device, const std::string& shaderBytecodePath);

    // Dispatch compute shader
    // inputSRV: Shader Resource View of Input Texture (BGRA)
    // outputUAV: Unordered Access View of Output Buffer (Tensor)
    void dispatch(
        ID3D11DeviceContext* context,
        ID3D11ShaderResourceView* inputSRV,
        ID3D11UnorderedAccessView* outputUAV,
        uint32_t inputW, uint32_t inputH,
        uint32_t outputW, uint32_t outputH
    );
};

} // namespace macroman
