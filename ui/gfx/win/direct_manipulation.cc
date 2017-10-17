// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/direct_manipulation.h"

#include <objbase.h>

#include "base/win/windows_version.h"

namespace gfx {
namespace win {

// static
std::unique_ptr<DirectManipulationHelper>
DirectManipulationHelper::CreateInstance() {
  // TODO(dtapuska): Do not create a DirectManipulationHelper on any windows
  // versions as it only causes issues. High Precision Touchpad events seem to
  // always be sent to apps with recent Windows 10 versions. This class should
  // eventually be removed. See crbug.com/647038.
  return nullptr;
}

DirectManipulationHelper::DirectManipulationHelper() {}

DirectManipulationHelper::~DirectManipulationHelper() {
  if (view_port_outer_)
    view_port_outer_->Abandon();
}

void DirectManipulationHelper::Initialize(HWND window) {
  DCHECK(::IsWindow(window));

  // TODO(ananta)
  // Remove the CHECK statements here and below and replace them with logs
  // when this code stabilizes.
  HRESULT hr =
      ::CoCreateInstance(CLSID_DirectManipulationManager, nullptr,
                         CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&manager_));
  CHECK(SUCCEEDED(hr));

  hr = ::CoCreateInstance(CLSID_DCompManipulationCompositor, nullptr,
                          CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&compositor_));
  CHECK(SUCCEEDED(hr));

  hr = manager_->GetUpdateManager(IID_PPV_ARGS(update_manager_.GetAddressOf()));
  CHECK(SUCCEEDED(hr));

  hr = compositor_->SetUpdateManager(update_manager_.Get());
  CHECK(SUCCEEDED(hr));

  hr = compositor_.CopyTo(frame_info_.GetAddressOf());
  CHECK(SUCCEEDED(hr));

  hr = manager_->CreateViewport(frame_info_.Get(), window,
                                IID_PPV_ARGS(view_port_outer_.GetAddressOf()));
  CHECK(SUCCEEDED(hr));

  //
  // Enable the desired configuration for each viewport.
  //
  DIRECTMANIPULATION_CONFIGURATION configuration =
      DIRECTMANIPULATION_CONFIGURATION_INTERACTION
      | DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X
      | DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y
      | DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_INERTIA
      | DIRECTMANIPULATION_CONFIGURATION_RAILS_X
      | DIRECTMANIPULATION_CONFIGURATION_RAILS_Y
      | DIRECTMANIPULATION_CONFIGURATION_SCALING
      | DIRECTMANIPULATION_CONFIGURATION_SCALING_INERTIA;

  hr = view_port_outer_->ActivateConfiguration(configuration);
  CHECK(SUCCEEDED(hr));
}

void DirectManipulationHelper::SetBounds(const gfx::Rect& bounds) {
  Microsoft::WRL::ComPtr<IDirectManipulationPrimaryContent>
      primary_content_outer;
  HRESULT hr = view_port_outer_->GetPrimaryContent(
      IID_PPV_ARGS(primary_content_outer.GetAddressOf()));
  CHECK(SUCCEEDED(hr));

  Microsoft::WRL::ComPtr<IDirectManipulationContent> content_outer;
  hr = primary_content_outer.CopyTo(content_outer.GetAddressOf());
  CHECK(SUCCEEDED(hr));

  RECT rect = bounds.ToRECT();

  hr = view_port_outer_->SetViewportRect(&rect);
  CHECK(SUCCEEDED(hr));

  hr = content_outer->SetContentRect(&rect);
  CHECK(SUCCEEDED(hr));
}

void DirectManipulationHelper::Activate(HWND window) {
  DCHECK(::IsWindow(window));
  manager_->Activate(window);
}

void DirectManipulationHelper::Deactivate(HWND window) {
  DCHECK(::IsWindow(window));
  manager_->Deactivate(window);
}

void DirectManipulationHelper:: HandleMouseWheel(HWND window, UINT message,
    WPARAM w_param, LPARAM l_param) {
  MSG msg = { window, message, w_param, l_param};

  HRESULT hr = view_port_outer_->SetContact(DIRECTMANIPULATION_MOUSEFOCUS);
  if (SUCCEEDED(hr)) {
    BOOL handled = FALSE;
    manager_->ProcessInput(&msg, &handled);
    view_port_outer_->ReleaseContact(DIRECTMANIPULATION_MOUSEFOCUS);
  }
}

}  // namespace win.
}  // namespace gfx.
