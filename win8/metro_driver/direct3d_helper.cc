// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "win8/metro_driver/direct3d_helper.h"
#include "win8/metro_driver/winrt_utils.h"

#include "base/logging.h"

#include <windows.graphics.display.h>

namespace {

void CheckIfFailed(HRESULT hr) {
  DCHECK(!FAILED(hr));
  if (FAILED(hr))
    DVLOG(0) << "Direct3D call failed, hr = " << hr;
}

float GetLogicalDpi() {
  mswr::ComPtr<wingfx::Display::IDisplayPropertiesStatics> display_properties;
  CheckIfFailed(winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_Graphics_Display_DisplayProperties,
      display_properties.GetAddressOf()));
  float dpi = 0.0;
  CheckIfFailed(display_properties->get_LogicalDpi(&dpi));
  return dpi;
}

float ConvertDipsToPixels(float dips) {
  static const float dips_per_inch = 96.f;
  float logical_dpi = GetLogicalDpi();
  return floor(dips * logical_dpi / dips_per_inch + 0.5f);
}

}

namespace metro_driver {

Direct3DHelper::Direct3DHelper() {
}

Direct3DHelper::~Direct3DHelper() {
}

void Direct3DHelper::Initialize(winui::Core::ICoreWindow* window) {
  window_ = window;
  CreateDeviceResources();
  CreateWindowSizeDependentResources();
}

// TODO(scottmg): Need to handle resize messages and recreation.

void Direct3DHelper::CreateDeviceResources() {
  unsigned int creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
  D3D_FEATURE_LEVEL feature_levels[] = {
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3,
    D3D_FEATURE_LEVEL_9_2,
    D3D_FEATURE_LEVEL_9_1,
  };

  mswr::ComPtr<ID3D11Device> device;
  mswr::ComPtr<ID3D11DeviceContext> context;
  CheckIfFailed(
      D3D11CreateDevice(
          nullptr,
          D3D_DRIVER_TYPE_HARDWARE,
          nullptr,
          creation_flags,
          feature_levels,
          ARRAYSIZE(feature_levels),
          D3D11_SDK_VERSION,
          &device,
          &feature_level_,
          &context));
  CheckIfFailed(device.As(&d3d_device_));
  CheckIfFailed(context.As(&d3d_context_));
}

void Direct3DHelper::CreateWindowSizeDependentResources() {
  CheckIfFailed(window_->get_Bounds(&window_bounds_));
  float window_width = ConvertDipsToPixels(window_bounds_.Width);
  float window_height = ConvertDipsToPixels(window_bounds_.Height);

  // TODO(scottmg): Orientation.

  if (swap_chain_ != nullptr) {
    // TODO(scottmg): Resize if it already exists.
    NOTIMPLEMENTED();
  } else {
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = { 0 };
    swap_chain_desc.Width = window_width;
    swap_chain_desc.Height = window_height;
    swap_chain_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_chain_desc.Stereo = false;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 2; // TODO(scottmg): Probably 1 is fine.
    swap_chain_desc.Scaling = DXGI_SCALING_NONE;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swap_chain_desc.Flags = 0;

    mswr::ComPtr<IDXGIDevice1> dxgi_device;
    CheckIfFailed(d3d_device_.As(&dxgi_device));

    mswr::ComPtr<IDXGIAdapter> dxgi_adapter;
    CheckIfFailed(dxgi_device->GetAdapter(&dxgi_adapter));

    mswr::ComPtr<IDXGIFactory2> dxgi_factory;
    CheckIfFailed(dxgi_adapter->GetParent(
        __uuidof(IDXGIFactory2), &dxgi_factory));

    CheckIfFailed(dxgi_factory->CreateSwapChainForCoreWindow(
        d3d_device_.Get(),
        reinterpret_cast<IUnknown*>(window_),
        &swap_chain_desc,
        nullptr,
        &swap_chain_));
  }
}

}  // namespace metro_driver
