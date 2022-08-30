// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/linux/linux_ui_delegate_stub.h"

#include "base/callback.h"

namespace ui {

LinuxUiDelegateStub::LinuxUiDelegateStub() = default;

LinuxUiDelegateStub::~LinuxUiDelegateStub() = default;

LinuxUiBackend LinuxUiDelegateStub::GetBackend() const {
  return LinuxUiBackend::kStub;
}

bool LinuxUiDelegateStub::ExportWindowHandle(
    gfx::AcceleratedWidget window_id,
    base::OnceCallback<void(std::string)> callback) {
  return false;
}

}  // namespace ui
