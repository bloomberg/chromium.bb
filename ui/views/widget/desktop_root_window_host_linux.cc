// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_root_window_host_linux.h"

#include "ui/aura/root_window.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, public:

DesktopRootWindowHostLinux::DesktopRootWindowHostLinux() {
}

DesktopRootWindowHostLinux::~DesktopRootWindowHostLinux() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHost, public:

// static
DesktopRootWindowHost* DesktopRootWindowHost::Create() {
  return new DesktopRootWindowHostLinux;
}

}  // namespace views
