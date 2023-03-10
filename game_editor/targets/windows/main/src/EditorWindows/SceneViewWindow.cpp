#include "SceneViewWindow.hpp"

#include <nodec/math/gfx.hpp>

#include <DirectXMath.h>

SceneViewWindow::SceneViewWindow(Graphics &gfx, nodec_scene::Scene &scene, SceneRenderer &renderer,
                                 nodec_scene::SceneEntity init_selected_entity,
                                 nodec::signals::SignalInterface<void(nodec_scene::SceneEntity)> selected_change_signal)
    : BaseWindow("Scene View##EditorWindows", nodec::Vector2f(VIEW_WIDTH, VIEW_HEIGHT)),
      scene_(&scene), renderer_(&renderer), selected_entity_(init_selected_entity) {
    // Generate the render target textures.
    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.Width = VIEW_WIDTH;
    texture_desc.Height = VIEW_HEIGHT;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ThrowIfFailedGfx(
        gfx.device().CreateTexture2D(&texture_desc, nullptr, &texture_),
        &gfx, __FILE__, __LINE__);

    // Generate the render target views.
    {
        D3D11_RENDER_TARGET_VIEW_DESC desc{};
        desc.Format = texture_desc.Format;
        desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

        ThrowIfFailedGfx(
            gfx.device().CreateRenderTargetView(texture_.Get(), &desc, &render_target_view_),
            &gfx, __FILE__, __LINE__);
    }

    // Generate the shader resource views.
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = texture_desc.Format;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;

        ThrowIfFailedGfx(
            gfx.device().CreateShaderResourceView(texture_.Get(), &desc, &shader_resource_view_),
            &gfx, __FILE__, __LINE__);
    }

    rendering_context_.reset(new SceneRenderingContext(VIEW_WIDTH, VIEW_HEIGHT, &gfx));
    scene_ = &scene;
    renderer_ = &renderer;

    selected_entity_changed_conn_ = selected_change_signal.connect([&](auto entity) { selected_entity_ = entity; });
}

void SceneViewWindow::on_gui() {
    using namespace nodec;
    using namespace DirectX;
    using namespace nodec_scene::components;

    if (ImGui::RadioButton("Translate", gizmo_operation_ == ImGuizmo::TRANSLATE)) {
        gizmo_operation_ = ImGuizmo::TRANSLATE;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", gizmo_operation_ == ImGuizmo::ROTATE)) {
        gizmo_operation_ = ImGuizmo::ROTATE;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", gizmo_operation_ == ImGuizmo::SCALE)) {
        gizmo_operation_ = ImGuizmo::SCALE;
    }

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    if (ImGui::RadioButton("Local", gizmo_mode_ == ImGuizmo::LOCAL)) {
        gizmo_mode_ = ImGuizmo::LOCAL;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("World", gizmo_mode_ == ImGuizmo::WORLD)) {
        gizmo_mode_ = ImGuizmo::WORLD;
    }

    // ImGui::BeginChildFrame(100, ImVec2(VIEW_WIDTH, VIEW_HEIGHT));
    ImGui::BeginChild("SceneRender", ImVec2(VIEW_WIDTH, VIEW_HEIGHT), false, ImGuiWindowFlags_NoMove);

    // const float window_width = (float)ImGui::GetWindowWidth();
    // const float window_height = (float)ImGui::GetWindowHeight();

    // const float view_manipulate_right = ImGui::GetWindowPos().x + window_width;
    // const float view_manipulate_top = ImGui::GetWindowPos().y;

    const auto view_aspect = static_cast<float>(VIEW_WIDTH) / VIEW_HEIGHT;

    {
        XMFLOAT4X4 matrix;
        XMStoreFloat4x4(&matrix, XMMatrixPerspectiveFovLH(
                                     XMConvertToRadians(45),
                                     view_aspect,
                                     0.01f, 10000.0f));

        projection_.set(matrix.m[0], matrix.m[1], matrix.m[2], matrix.m[3]);
    }

    if (ImGui::IsWindowHovered()) {
        auto &io = ImGui::GetIO();
        Vector3f position;
        Vector3f scale;
        Quaternionf rotation;

        auto view_inverse_ = math::inv(view_);

        math::gfx::decompose_trs(view_inverse_, position, rotation, scale);

        const auto forward = math::gfx::rotate(Vector3f(0.f, 0.f, 1.f), rotation);
        const auto right = math::gfx::rotate(Vector3f(1.f, 0.f, 0.f), rotation);
        const auto up = math::gfx::rotate(Vector3f(0.f, 1.f, 0.f), rotation);

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            constexpr float SCALE_FACTOR = 0.01f;
            const auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);

            // The mouse moving right, the camera moving left.
            position -= right * delta.x * SCALE_FACTOR;
            position += up * delta.y * SCALE_FACTOR;

            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            constexpr float SCALE_FACTOR = 0.1f;
            const auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);

            // Apply rotation around the local right vector after current rotation.
            rotation = math::gfx::quaternion_from_angle_axis(delta.y * SCALE_FACTOR, right) * rotation;

            // And apply rotation arround the world up vector.
            rotation = math::gfx::quaternion_from_angle_axis(delta.x * SCALE_FACTOR, Vector3f(0.f, 1.f, 0.f)) * rotation;

            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
        }

        position += forward * io.MouseWheel;

        view_inverse_ = math::gfx::trs(position, rotation, scale);
        view_ = math::inv(view_inverse_);
    }

    // logging::InfoStream(__FILE__, __LINE__) << io.MouseWheel;

    renderer_->Render(*scene_, view_, projection_, render_target_view_.Get(), *rendering_context_);

    ImGui::Image((void *)shader_resource_view_.Get(), ImVec2(VIEW_WIDTH, VIEW_HEIGHT));

    ImGuizmo::SetDrawlist();

    ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, VIEW_WIDTH, VIEW_HEIGHT);

    // ImGuizmo::DrawGrid(view_.m, projection_.m, Matrix4x4f::identity.m, 100.f);
    // ImGuizmo::ViewManipulate(view_.m, 0.8f, ImVec2(view_manipulate_right - 128, view_manipulate_top), ImVec2(128, 128), 0x10101010);

    // Edit transformation.
    [&]() {
        if (!scene_->registry().is_valid(selected_entity_)) return;

        auto *trfm = scene_->registry().try_get_component<Transform>(selected_entity_);
        if (!trfm) return;

        auto model_matrix = trfm->local2world;
        Matrix4x4f delta_matrix;

        if (ImGuizmo::Manipulate(view_.m, projection_.m, gizmo_operation_, gizmo_mode_, model_matrix.m, delta_matrix.m)) {
            Vector3f delta_translation;
            Vector3f delta_scale;
            Quaternionf delta_rotation;
            math::gfx::decompose_trs(delta_matrix, delta_translation, delta_rotation, delta_scale);

            // I dont know why translation is not zero even if in rotate mode.
            if (gizmo_operation_ == ImGuizmo::ROTATE) delta_translation =  Vector3f::zero;

            trfm->local_position += delta_translation;
            trfm->local_rotation = delta_rotation * trfm->local_rotation;
            trfm->local_scale *= delta_scale;

            trfm->dirty = true;
        }
    }();

    ImGui::EndChild();
}
