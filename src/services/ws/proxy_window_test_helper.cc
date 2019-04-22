// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/proxy_window_test_helper.h"

#include "services/ws/proxy_window.h"

namespace ws {

ProxyWindowTestHelper::ProxyWindowTestHelper(ProxyWindow* proxy_window)
    : proxy_window_(proxy_window) {}

ProxyWindowTestHelper::~ProxyWindowTestHelper() = default;

bool ProxyWindowTestHelper::IsHandlingPointerPress(ui::PointerId pointer_id) {
  return proxy_window_->IsHandlingPointerPressForTesting(pointer_id);
}

}  // namespace ws
