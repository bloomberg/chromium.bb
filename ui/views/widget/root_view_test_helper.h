// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_ROOT_VIEW_TEST_HELPER_H_
#define UI_VIEWS_WIDGET_ROOT_VIEW_TEST_HELPER_H_

#include "ui/views/widget/root_view.h"

namespace views {
namespace test {

class RootViewTestHelper {
 public:
  explicit RootViewTestHelper(internal::RootView* root_view)
      : root_view_(root_view) {
  }
  ~RootViewTestHelper() {}

  void DispatchKeyEventStartAt(View* view, ui::KeyEvent* event) {
    root_view_->DispatchKeyEventStartAt(view, event);
  }

 private:
  internal::RootView* root_view_;

  DISALLOW_COPY_AND_ASSIGN(RootViewTestHelper);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_WIDGET_ROOT_VIEW_TEST_HELPER_H_
