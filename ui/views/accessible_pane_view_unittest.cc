// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessible_pane_view.h"

#include "ui/base/accelerators/accelerator.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

// TODO(alicet): bring pane rotation into views and add tests.
//               See browser_view.cc for details.

typedef ViewsTestBase AccessiblePaneViewTest;

class TestBarView : public AccessiblePaneView,
                    public ButtonListener {
 public:
  TestBarView();
  virtual ~TestBarView();

  virtual void ButtonPressed(Button* sender,
                             const ui::Event& event) OVERRIDE;
  TextButton* child_button() const { return child_button_.get(); }
  TextButton* second_child_button() const { return second_child_button_.get(); }
  TextButton* third_child_button() const { return third_child_button_.get(); }
  TextButton* not_child_button() const { return not_child_button_.get(); }

  virtual View* GetDefaultFocusableChild() OVERRIDE;

 private:
  void Init();

  scoped_ptr<TextButton> child_button_;
  scoped_ptr<TextButton> second_child_button_;
  scoped_ptr<TextButton> third_child_button_;
  scoped_ptr<TextButton> not_child_button_;

  DISALLOW_COPY_AND_ASSIGN(TestBarView);
};

TestBarView::TestBarView() {
  Init();
}

TestBarView::~TestBarView() {}

void TestBarView::ButtonPressed(views::Button* sender, const ui::Event& event) {
}

void TestBarView::Init() {
  SetLayoutManager(new views::FillLayout());
  string16 label;
  child_button_.reset(new TextButton(this, label));
  AddChildView(child_button_.get());
  second_child_button_.reset(new TextButton(this, label));
  AddChildView(second_child_button_.get());
  third_child_button_.reset(new TextButton(this, label));
  AddChildView(third_child_button_.get());
  not_child_button_.reset(new TextButton(this, label));
}

View* TestBarView::GetDefaultFocusableChild() {
  return child_button_.get();
}

TEST_F(AccessiblePaneViewTest, SimpleSetPaneFocus) {
  TestBarView* test_view = new TestBarView();
  scoped_ptr<Widget> widget(new Widget());
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);
  View* root = widget->GetRootView();
  root->AddChildView(test_view);
  widget->Show();
  widget->Activate();

  // Set pane focus succeeds, focus on child.
  EXPECT_TRUE(test_view->SetPaneFocusAndFocusDefault());
  EXPECT_EQ(test_view, test_view->GetPaneFocusTraversable());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());

  // Set focus on non child view, focus failed, stays on pane.
  EXPECT_TRUE(test_view->SetPaneFocus(test_view->not_child_button()));
  EXPECT_FALSE(test_view->not_child_button() ==
               test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  widget->CloseNow();
  widget.reset();
}

TEST_F(AccessiblePaneViewTest, TwoSetPaneFocus) {
  TestBarView* test_view = new TestBarView();
  TestBarView* test_view_2 = new TestBarView();
  scoped_ptr<Widget> widget(new Widget());
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);
  View* root = widget->GetRootView();
  root->AddChildView(test_view);
  root->AddChildView(test_view_2);
  widget->Show();
  widget->Activate();

  // Set pane focus succeeds, focus on child.
  EXPECT_TRUE(test_view->SetPaneFocusAndFocusDefault());
  EXPECT_EQ(test_view, test_view->GetPaneFocusTraversable());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());

  // Set focus on another test_view, focus move to that pane.
  EXPECT_TRUE(test_view_2->SetPaneFocus(test_view_2->second_child_button()));
  EXPECT_FALSE(test_view->child_button() ==
               test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(test_view_2->second_child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  widget->CloseNow();
  widget.reset();
}

TEST_F(AccessiblePaneViewTest, PaneFocusTraversal) {
  TestBarView* test_view = new TestBarView();
  TestBarView* original_test_view = new TestBarView();
  scoped_ptr<Widget> widget(new Widget());
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);
  View* root = widget->GetRootView();
  root->AddChildView(original_test_view);
  root->AddChildView(test_view);
  widget->Show();
  widget->Activate();

  // Set pane focus on first view.
  EXPECT_TRUE(original_test_view->SetPaneFocus(
      original_test_view->third_child_button()));

  // Test travesal in second view.
  // Set pane focus on second child.
  EXPECT_TRUE(test_view->SetPaneFocus(test_view->second_child_button()));
  // home
  test_view->AcceleratorPressed(test_view->home_key());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  // end
  test_view->AcceleratorPressed(test_view->end_key());
  EXPECT_EQ(test_view->third_child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  // left
  test_view->AcceleratorPressed(test_view->left_key());
  EXPECT_EQ(test_view->second_child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  // right, right
  test_view->AcceleratorPressed(test_view->right_key());
  test_view->AcceleratorPressed(test_view->right_key());
  EXPECT_EQ(test_view->child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());

  // ESC
  test_view->AcceleratorPressed(test_view->escape_key());
  EXPECT_EQ(original_test_view->third_child_button(),
            test_view->GetWidget()->GetFocusManager()->GetFocusedView());
  widget->CloseNow();
  widget.reset();
}
}  // namespace views
