#pragma once

#include "Graphics.hpp"

#include <nodec/unicode.hpp>

#include <d3dcompiler.h>


class VertexShader {
public:
    VertexShader(Graphics* pGraphics, const std::string& path) {

        ThrowIfFailedGfx(
            D3DReadFileToBlob(nodec::unicode::utf8to16<std::wstring>(path).c_str(), &mpBytecodeBlob),
            pGraphics, __FILE__, __LINE__
        );

        ThrowIfFailedGfx(
            pGraphics->device().CreateVertexShader(
                mpBytecodeBlob->GetBufferPointer(), mpBytecodeBlob->GetBufferSize(),
                nullptr, &mpVertexShader
            ),
            pGraphics, __FILE__, __LINE__
        );
    }

    void Bind(Graphics* pGraphics) {
        pGraphics->context().VSSetShader(mpVertexShader.Get(), nullptr, 0u);

        // NOTE: The following code is too heavy to run for each model.
        // const auto logs = pGraphics->info_logger().Dump();
        // if (!logs.empty()) {
        //     nodec::logging::WarnStream(__FILE__, __LINE__)
        //         << "[VertexShader::Bind] >>> DXGI Logs:"
        //         << logs;
        // }
    }

    ID3DBlob& GetBytecode() {
        return *mpBytecodeBlob.Get();
    }

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> mpVertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob> mpBytecodeBlob;

};