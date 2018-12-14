// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_PROXY_WINDOW_TEST_HELPER_H_
#define SERVICES_WS_PROXY_WINDOW_TEST_HELPER_H_

#include "base/macros.h"
#include "ui/events/event.h"

namespace ws {

class ProxyWindow;

// Used for accessing private members of ProxyWindow in tests.
class ProxyWindowTestHelper {
 public:
  explicit ProxyWindowTestHelper(ProxyWindow* proxy_window);
  ~ProxyWindowTestHelper();

  bool IsHandlingPointerPress(ui::PointerId pointer_id);

 private:
  ProxyWindow* proxy_window_;

  DISALLOW_COPY_AND_ASSIGN(ProxyWindowTestHelper);
};

}  // namespace ws

#endif  // SERVICES_WS_PROXY_WINDOW_TEST_HELPER_H_
