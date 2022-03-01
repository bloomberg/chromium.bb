// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BASE_CONTROL_TEST_WIDGET_H_
#define UI_VIEWS_CONTROLS_BASE_CONTROL_TEST_WIDGET_H_

#include "build/build_config.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

#if defined(OS_MAC)
#include <memory>

namespace display {
namespace test {
class TestScreen;
}  // namespace test
}  // namespace display
#endif

namespace views {

namespace test {

class BaseControlTestWidget : public ViewsTestBase {
 public:
  BaseControlTestWidget();
  BaseControlTestWidget(const BaseControlTestWidget&) = delete;
  BaseControlTestWidget& operator=(const BaseControlTestWidget&) = delete;
  ~BaseControlTestWidget() override;

  // ViewsTestBase:
  void SetUp() override;
  void TearDown() override;

 protected:
  virtual void CreateWidgetContent(View* container);

  Widget* widget() { return widget_.get(); }

 private:
  UniqueWidgetPtr widget_;

#if defined(OS_MAC)
  // Need a test screen to work with the event generator to correctly track
  // cursor locations. See https://crbug.com/1071633. Consider moving this
  // into ViewsTestHelperMac in the future.
  std::unique_ptr<display::test::TestScreen> test_screen_;
#endif
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BASE_CONTROL_TEST_WIDGET_H_
