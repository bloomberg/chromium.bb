// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_SCOPED_PREFERRED_SCROLLER_STYLE_LEGACY_MAC_H_
#define UI_BASE_TEST_SCOPED_PREFERRED_SCROLLER_STYLE_LEGACY_MAC_H_

#include <memory>

#include "base/macros.h"

namespace base {
namespace mac {
class ScopedObjCClassSwizzler;
}
}

namespace ui {
namespace test {

// Overrides system setting for scrollbar style with NSScrollerOverlay if we
// want the scrollbar to overlay. Otherwise, override with
// NSScrollerStyleLegacy which means "always show scrollbars".
class ScopedPreferredScrollerStyle {
 public:
  explicit ScopedPreferredScrollerStyle(bool overlay);
  ~ScopedPreferredScrollerStyle();

 private:
  std::unique_ptr<base::mac::ScopedObjCClassSwizzler> swizzler_;

  // True if the scrollbar style should overlay.
  bool overlay_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPreferredScrollerStyle);
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_TEST_SCOPED_PREFERRED_SCROLLER_STYLE_LEGACY_MAC_H_
