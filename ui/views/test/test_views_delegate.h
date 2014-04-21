// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_TEST_VIEWS_DELEGATE_H_
#define UI_VIEWS_TEST_TEST_VIEWS_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "ui/views/views_delegate.h"

namespace wm {
class WMState;
}

namespace views {

class TestViewsDelegate : public ViewsDelegate {
 public:
  TestViewsDelegate();
  virtual ~TestViewsDelegate();

  void SetUseTransparentWindows(bool transparent);

  // ViewsDelegate:
  virtual void OnBeforeWidgetInit(
      Widget::InitParams* params,
      internal::NativeWidgetDelegate* delegate) OVERRIDE;

 private:
  bool use_transparent_windows_;
  scoped_ptr<wm::WMState> wm_state_;

  DISALLOW_COPY_AND_ASSIGN(TestViewsDelegate);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_TEST_VIEWS_DELEGATE_H_
