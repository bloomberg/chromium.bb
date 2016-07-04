// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_TESTS_TEST_WINDOW_H_
#define SERVICES_UI_PUBLIC_CPP_TESTS_TEST_WINDOW_H_

#include "base/macros.h"
#include "services/ui/public/cpp/lib/window_private.h"
#include "services/ui/public/cpp/window.h"

namespace ui {

// Subclass with public ctor/dtor.
class TestWindow : public Window {
 public:
  TestWindow() { WindowPrivate(this).set_server_id(1); }
  explicit TestWindow(Id window_id) {
    WindowPrivate(this).set_server_id(window_id);
  }
  ~TestWindow() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWindow);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_TESTS_TEST_WINDOW_H_
