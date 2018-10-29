// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_WINDOW_SERVICE_TEST_HELPER_H_
#define SERVICES_WS_WINDOW_SERVICE_TEST_HELPER_H_

#include "base/macros.h"
#include "services/ws/window_service.h"

namespace ws {

// Used for accessing private members of WindowService in tests.
class WindowServiceTestHelper {
 public:
  explicit WindowServiceTestHelper(WindowService* window_service);
  ~WindowServiceTestHelper();

  EventInjector* event_injector() {
    return window_service_->event_injector_.get();
  }

 private:
  WindowService* window_service_;

  DISALLOW_COPY_AND_ASSIGN(WindowServiceTestHelper);
};

}  // namespace ws

#endif  // SERVICES_WS_WINDOW_SERVICE_TEST_HELPER_H_
