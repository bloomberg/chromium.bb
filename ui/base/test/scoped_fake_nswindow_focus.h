// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_SCOPED_FAKE_NSWINDOW_FOCUS_H_
#define UI_BASE_TEST_SCOPED_FAKE_NSWINDOW_FOCUS_H_

#include "base/memory/scoped_ptr.h"

namespace base {
namespace mac {
class ScopedObjCClassSwizzler;
}
}

namespace ui {
namespace test {

// Simulates key and main status by listening for -makeKeyWindow and
// -makeMainWindow. This allows test coverage of code relying on window focus
// changes without resorting to an interactive_ui_test.
class ScopedFakeNSWindowFocus {
 public:
  ScopedFakeNSWindowFocus();
  ~ScopedFakeNSWindowFocus();

 private:
  scoped_ptr<base::mac::ScopedObjCClassSwizzler> is_main_swizzler_;
  scoped_ptr<base::mac::ScopedObjCClassSwizzler> make_main_swizzler_;
  scoped_ptr<base::mac::ScopedObjCClassSwizzler> is_key_swizzler_;
  scoped_ptr<base::mac::ScopedObjCClassSwizzler> make_key_swizzler_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFakeNSWindowFocus);
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_TEST_SCOPED_FAKE_NSWINDOW_FOCUS_H_
