#include "InputPreprocessor.h"
#include "utils/Logger.h"
#include <fstream>
#include <vector>

namespace macroman {

bool InputPreprocessor::initialize(ID3D11Device* device, const std::string& shaderPath) {
    // Load compiled shader bytecode (.cso)
    std::ifstream file(shaderPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open shader file: {}", shaderPath);
        return false;
    }

    size_t size = file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    file.close();

    HRESULT hr = device->CreateComputeShader(buffer.data(), size, nullptr, computeShader_.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create compute shader");
        return false;
    }

    // Create Constant Buffer
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(Constants);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device->CreateBuffer(&desc, nullptr, constantBuffer_.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create constant buffer");
        return false;
    }

    return true;
}

void InputPreprocessor::dispatch(
    ID3D11DeviceContext* context,
    ID3D11ShaderResourceView* inputSRV,
    ID3D11UnorderedAccessView* outputUAV,
    uint32_t inputW, uint32_t inputH,
    uint32_t outputW, uint32_t outputH
) {
    // Update constants
    D3D11_MAPPED_SUBRESOURCE mapped;
    context->Map(constantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    Constants* cb = (Constants*)mapped.pData;
    cb->inputWidth = inputW;
    cb->inputHeight = inputH;
    cb->outputWidth = outputW;
    cb->outputHeight = outputH;
    context->Unmap(constantBuffer_.Get(), 0);

    // Set state
    context->CSSetShader(computeShader_.Get(), nullptr, 0);
    context->CSSetConstantBuffers(0, 1, constantBuffer_.GetAddressOf());

    ID3D11ShaderResourceView* srvs[] = { inputSRV };
    context->CSSetShaderResources(0, 1, srvs);

    ID3D11UnorderedAccessView* uavs[] = { outputUAV };
    context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

    // Dispatch (8x8 threads per group)
    uint32_t groupsX = (outputW + 7) / 8;
    uint32_t groupsY = (outputH + 7) / 8;
    context->Dispatch(groupsX, groupsY, 1);

    // Unbind to avoid hazards
    ID3D11ShaderResourceView* nullSRV[] = { nullptr };
    context->CSSetShaderResources(0, 1, nullSRV);

    ID3D11UnorderedAccessView* nullUAV[] = { nullptr };
    context->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);
}

} // namespace macroman
