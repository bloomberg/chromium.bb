// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_SCOPED_PREFERRED_SCROLLER_STYLE_LEGACY_MAC_H_
#define UI_BASE_TEST_SCOPED_PREFERRED_SCROLLER_STYLE_LEGACY_MAC_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace base {
namespace mac {
class ScopedObjCClassSwizzler;
}
}

namespace ui {
namespace test {

// Overrides system setting for scrollbar style with NSScrollerStyleLegacy,
// which means "always show scrollbars".
class ScopedPreferredScrollerStyleLegacy {
 public:
  ScopedPreferredScrollerStyleLegacy();
  ~ScopedPreferredScrollerStyleLegacy();

 private:
  scoped_ptr<base::mac::ScopedObjCClassSwizzler> swizzler_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPreferredScrollerStyleLegacy);
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_TEST_SCOPED_PREFERRED_SCROLLER_STYLE_LEGACY_MAC_H_
