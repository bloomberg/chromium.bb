// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_TEST_EVENT_INJECTOR_DELEGATE_H_
#define SERVICES_UI_WS2_TEST_EVENT_INJECTOR_DELEGATE_H_

#include <stdint.h>

namespace aura {
class WindowTreeHost;
}

namespace ui {
namespace ws2 {

class TestEventInjectorDelegate {
 public:
  // Returns the WindowTreeHost for the specified display id, null if not a
  // valid display.
  virtual aura::WindowTreeHost* GetWindowTreeHostForDisplayId(
      int64_t display_id) = 0;

 protected:
  virtual ~TestEventInjectorDelegate() {}
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_TEST_EVENT_INJECTOR_DELEGATE_H_
