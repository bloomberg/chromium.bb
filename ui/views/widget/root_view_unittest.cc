// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/root_view_test_helper.h"

namespace views {
namespace test {

typedef ViewsTestBase RootViewTest;

class DeleteOnKeyEventView : public View {
 public:
  explicit DeleteOnKeyEventView(bool* set_on_key) : set_on_key_(set_on_key) {}
  virtual ~DeleteOnKeyEventView() {}

  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE {
    *set_on_key_ = true;
    delete this;
    return true;
  }

 private:
  // Set to true in OnKeyPressed().
  bool* set_on_key_;

  DISALLOW_COPY_AND_ASSIGN(DeleteOnKeyEventView);
};

// Verifies deleting a View in OnKeyPressed() doesn't crash.
TEST_F(RootViewTest, DeleteViewDuringKeyEventDispatch) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);

  bool got_key_event = false;

  View* content = new View;
  widget.SetContentsView(content);

  View* child = new DeleteOnKeyEventView(&got_key_event);
  content->AddChildView(child);

  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, 0, false);
  RootViewTestHelper test_helper(
      static_cast<internal::RootView*>(widget.GetRootView()));
  test_helper.DispatchKeyEventStartAt(child, &key_event);
  EXPECT_TRUE(got_key_event);
}

}  // namespace test
}  // namespace views
