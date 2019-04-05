// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_desktop_screen_x11.h"

#include <memory>

#include "base/memory/singleton.h"

namespace views {
namespace test {

TestDesktopScreenX11* TestDesktopScreenX11::GetInstance() {
  return base::Singleton<TestDesktopScreenX11>::get();
}

TestDesktopScreenX11::TestDesktopScreenX11() = default;

TestDesktopScreenX11::~TestDesktopScreenX11() = default;

gfx::Point TestDesktopScreenX11::GetCursorScreenPoint() {
  return cursor_screen_point_;
}

TestDesktopScreenX11* GetTestDesktopScreenX11() {
  static std::unique_ptr<TestDesktopScreenX11> test_screen_instance;
  if (!test_screen_instance.get())
    test_screen_instance = std::make_unique<TestDesktopScreenX11>();
  return test_screen_instance.get();
}

}  // namespace test
}  // namespace views
