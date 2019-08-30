// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/platform_window_delegate_linux.h"

#include "base/logging.h"

namespace ui {

PlatformWindowDelegateLinux::PlatformWindowDelegateLinux() = default;

PlatformWindowDelegateLinux::~PlatformWindowDelegateLinux() = default;

void PlatformWindowDelegateLinux::OnXWindowMapped() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PlatformWindowDelegateLinux::OnXWindowUnmapped() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PlatformWindowDelegateLinux::OnLostMouseGrab() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PlatformWindowDelegateLinux::OnWorkspaceChanged() {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ui
