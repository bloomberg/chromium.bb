// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/windows_mixed_reality/wrappers/wmr_holographic_space.h"

#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <windows.graphics.holographic.h>
#include <wrl.h>

#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "device/vr/windows_mixed_reality/mixed_reality_statics.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_holographic_frame.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_holographic_space.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_holographic_frame.h"

using ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice;
using ABI::Windows::Graphics::Holographic::HolographicAdapterId;
using ABI::Windows::Graphics::Holographic::IHolographicFrame;
using ABI::Windows::Graphics::Holographic::IHolographicSpace;
using Microsoft::WRL::ComPtr;

namespace device {

WMRHolographicSpaceImpl::WMRHolographicSpaceImpl(
    ComPtr<IHolographicSpace> space)
    : space_(space) {
  DCHECK(space_);
}

WMRHolographicSpaceImpl::~WMRHolographicSpaceImpl() = default;

HolographicAdapterId WMRHolographicSpaceImpl::PrimaryAdapterId() {
  HolographicAdapterId id;
  HRESULT hr = space_->get_PrimaryAdapterId(&id);
  DCHECK(SUCCEEDED(hr));
  return id;
}

std::unique_ptr<WMRHolographicFrame>
WMRHolographicSpaceImpl::TryCreateNextFrame() {
  ComPtr<IHolographicFrame> frame;
  HRESULT hr = space_->CreateNextFrame(&frame);
  if (FAILED(hr))
    return nullptr;
  return std::make_unique<WMRHolographicFrameImpl>(frame);
}

bool WMRHolographicSpaceImpl::TrySetDirect3D11Device(
    const ComPtr<IDirect3DDevice>& device) {
  HRESULT hr = space_->SetDirect3D11Device(device.Get());
  return SUCCEEDED(hr);
}
}  // namespace device
