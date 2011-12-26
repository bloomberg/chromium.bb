// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/base/x/x11_util.h"
#endif

namespace aura {
namespace test {

TEST(EventTest, NoNativeEvent) {
  KeyEvent keyev(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, 0);
  EXPECT_FALSE(keyev.HasNativeEvent());
}

TEST(EventTest, NativeEvent) {
#if defined(OS_WIN)
  MSG native_event = { NULL, WM_KEYUP, ui::VKEY_A, 0 };
  KeyEvent keyev(native_event, false);
  EXPECT_TRUE(keyev.HasNativeEvent());
#elif defined(USE_X11)
  scoped_ptr<XEvent> native_event(new XEvent);
  ui::InitXKeyEventForTesting(ui::ET_KEY_RELEASED,
                              ui::VKEY_A,
                              0,
                              native_event.get());
  KeyEvent keyev(native_event.get(), false);
  EXPECT_TRUE(keyev.HasNativeEvent());
#endif
}

}  // namespace test
}  // namespace aura
