#pragma once

#include <Windows.h>
#include <d3d11.h>

namespace votv::render::imgui_layer {

bool initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
void begin_frame();
void render();
void shutdown();
bool initialized();

} // namespace votv::render::imgui_layer
