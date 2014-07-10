// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEWS_TEST_HELPER_MAC_H_
#define UI_VIEWS_TEST_VIEWS_TEST_HELPER_MAC_H_

#include "base/memory/scoped_ptr.h"
#include "ui/views/test/views_test_helper.h"

namespace ui {
class ScopedAnimationDurationScaleMode;
}

namespace views {

class ViewsTestHelperMac : public ViewsTestHelper {
 public:
  ViewsTestHelperMac();
  virtual ~ViewsTestHelperMac();

 private:
  // Disable animations during tests.
  scoped_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;

  DISALLOW_COPY_AND_ASSIGN(ViewsTestHelperMac);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_VIEWS_TEST_HELPER_MAC_H_
