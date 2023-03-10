#pragma once
#include "Graphics.hpp"

class RasterizerState {
public:
    RasterizerState(Graphics *pGfx, D3D11_CULL_MODE cullMode) {
        D3D11_RASTERIZER_DESC desc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT{});
        desc.CullMode = cullMode;
        //desc.FillMode = D3D11_FILL_WIREFRAME;

        ThrowIfFailedGfx(pGfx->device().CreateRasterizerState(&desc, &mpRasterizerState),
                         pGfx, __FILE__, __LINE__);
    }

    void Bind(Graphics *pGfx) {
        pGfx->context().RSSetState(mpRasterizerState.Get());

        // NOTE: The following code is too heavy to run for each model.
        // const auto logs = pGfx->info_logger().Dump();
        // if (!logs.empty()) {
        //     nodec::logging::WarnStream(__FILE__, __LINE__)
        //         << "[RasterizerState::Bind] >>> DXGI Logs:" << logs;
        // }
    }

private:
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> mpRasterizerState;
};
