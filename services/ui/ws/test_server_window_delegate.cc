// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/server_window.h"
#include "services/ui/ws/test_server_window_delegate.h"

namespace ui {

namespace ws {

TestServerWindowDelegate::TestServerWindowDelegate()
    : root_window_(nullptr) {}

TestServerWindowDelegate::~TestServerWindowDelegate() {}

cc::mojom::DisplayCompositor* TestServerWindowDelegate::GetDisplayCompositor() {
  return nullptr;
}

ServerWindow* TestServerWindowDelegate::GetRootWindow(
    const ServerWindow* window) {
  return root_window_;
}

}  // namespace ws

}  // namespace ui
