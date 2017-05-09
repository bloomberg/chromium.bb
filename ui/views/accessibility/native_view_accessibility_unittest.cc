// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_widget_obj_wrapper.h"
#include "ui/views/accessibility/native_view_accessibility_base.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace test {

class NativeViewAccessibilityTest;

namespace {

class TestButton : public Button {
 public:
  TestButton() : Button(NULL) {}
};

}  // namespace

class NativeViewAccessibilityTest : public ViewsTestBase {
 public:
  NativeViewAccessibilityTest() {}
  ~NativeViewAccessibilityTest() override {}

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = new views::Widget;
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(params);

    button_ = new TestButton();
    button_->SetSize(gfx::Size(20, 20));
    button_accessibility_ = NativeViewAccessibility::Create(button_);

    label_ = new Label();
    button_->AddChildView(label_);
    label_accessibility_ = NativeViewAccessibility::Create(label_);

    widget_->GetContentsView()->AddChildView(button_);
    widget_->Show();
  }

  void TearDown() override {
    if (!widget_->IsClosed())
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  NativeViewAccessibilityBase* button_accessibility() {
    return static_cast<NativeViewAccessibilityBase*>(
        button_accessibility_.get());
  }

  NativeViewAccessibilityBase* label_accessibility() {
    return static_cast<NativeViewAccessibilityBase*>(
        label_accessibility_.get());
  }

  bool SetFocused(NativeViewAccessibilityBase* view_accessibility,
                  bool focused) {
    ui::AXActionData data;
    data.action = focused ? ui::AX_ACTION_FOCUS : ui::AX_ACTION_BLUR;
    return view_accessibility->AccessibilityPerformAction(data);
  }

 protected:
  views::Widget* widget_;
  TestButton* button_;
  std::unique_ptr<NativeViewAccessibility> button_accessibility_;
  Label* label_;
  std::unique_ptr<NativeViewAccessibility> label_accessibility_;
};

TEST_F(NativeViewAccessibilityTest, RoleShouldMatch) {
  EXPECT_EQ(ui::AX_ROLE_BUTTON, button_accessibility()->GetData().role);
  EXPECT_EQ(ui::AX_ROLE_STATIC_TEXT, label_accessibility()->GetData().role);
}

TEST_F(NativeViewAccessibilityTest, BoundsShouldMatch) {
  gfx::Rect bounds =
      gfx::ToEnclosingRect(button_accessibility()->GetData().location);
  gfx::Rect screen_bounds = button_accessibility()->GetScreenBoundsRect();

  EXPECT_EQ(button_->GetBoundsInScreen(), bounds);
  EXPECT_EQ(screen_bounds, bounds);
}

TEST_F(NativeViewAccessibilityTest, LabelIsChildOfButton) {
  EXPECT_EQ(1, button_accessibility()->GetChildCount());
  EXPECT_EQ(label_->GetNativeViewAccessible(),
            button_accessibility()->ChildAtIndex(0));
  EXPECT_EQ(button_->GetNativeViewAccessible(),
            label_accessibility()->GetParent());
}

// Verify Views with invisible ancestors have AX_STATE_INVISIBLE.
TEST_F(NativeViewAccessibilityTest, InvisibleViews) {
  EXPECT_TRUE(widget_->IsVisible());
  EXPECT_FALSE(
      button_accessibility()->GetData().HasState(ui::AX_STATE_INVISIBLE));
  EXPECT_FALSE(
      label_accessibility()->GetData().HasState(ui::AX_STATE_INVISIBLE));
  button_->SetVisible(false);
  EXPECT_TRUE(
      button_accessibility()->GetData().HasState(ui::AX_STATE_INVISIBLE));
  EXPECT_TRUE(
      label_accessibility()->GetData().HasState(ui::AX_STATE_INVISIBLE));
}

TEST_F(NativeViewAccessibilityTest, WritableFocus) {
  // Make |button_| focusable, and focus/unfocus it via NativeViewAccessibility.
  button_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  EXPECT_EQ(nullptr, button_->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(nullptr, button_accessibility()->GetFocus());
  EXPECT_TRUE(SetFocused(button_accessibility(), true));
  EXPECT_EQ(button_, button_->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(button_->GetNativeViewAccessible(),
            button_accessibility()->GetFocus());
  EXPECT_TRUE(SetFocused(button_accessibility(), false));
  EXPECT_EQ(nullptr, button_->GetFocusManager()->GetFocusedView());
  EXPECT_EQ(nullptr, button_accessibility()->GetFocus());

  // If not focusable at all, SetFocused() should return false.
  button_->SetEnabled(false);
  EXPECT_FALSE(SetFocused(button_accessibility(), true));
}

// Subclass of NativeViewAccessibility that destroys itself when its
// parent widget is destroyed, for the purposes of making sure this
// doesn't lead to a crash.
class TestNativeViewAccessibility : public NativeViewAccessibilityBase {
 public:
  explicit TestNativeViewAccessibility(View* view)
      : NativeViewAccessibilityBase(view) {}

  void OnWidgetDestroying(Widget* widget) override {
    bool is_destroying_parent_widget = (parent_widget_ == widget);
    NativeViewAccessibilityBase::OnWidgetDestroying(widget);
    if (is_destroying_parent_widget)
      delete this;
  }
};

TEST_F(NativeViewAccessibilityTest, CrashOnWidgetDestroyed) {
  std::unique_ptr<Widget> parent_widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  parent_widget->Init(params);

  std::unique_ptr<Widget> child_widget(new Widget);
  child_widget->Init(params);

  // Make sure that deleting the parent widget can't cause a crash
  // due to NativeViewAccessibility not unregistering itself as a
  // WidgetObserver. Note that TestNativeViewAccessibility is a subclass
  // defined above that destroys itself when its parent widget is destroyed.
  TestNativeViewAccessibility* child_accessible =
      new TestNativeViewAccessibility(child_widget->GetRootView());
  child_accessible->SetParentWidget(parent_widget.get());
  parent_widget.reset();
}

#if defined(USE_AURA)
class DerivedTestView : public View {
 public:
  DerivedTestView() : View() {}
  ~DerivedTestView() override {}

  void OnBlur() override { SetVisible(false); }
};

class AxTestViewsDelegate : public TestViewsDelegate {
 public:
  AxTestViewsDelegate() {}
  ~AxTestViewsDelegate() override {}

  void NotifyAccessibilityEvent(views::View* view,
                                ui::AXEvent event_type) override {
    AXAuraObjCache* ax = AXAuraObjCache::GetInstance();
    std::vector<AXAuraObjWrapper*> out_children;
    AXAuraObjWrapper* ax_obj = ax->GetOrCreate(view->GetWidget());
    ax_obj->GetChildren(&out_children);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AxTestViewsDelegate);
};

class AXViewTest : public ViewsTestBase {
 public:
  AXViewTest() {}
  ~AXViewTest() override {}
  void SetUp() override {
    std::unique_ptr<TestViewsDelegate> views_delegate(
        new AxTestViewsDelegate());
    set_views_delegate(std::move(views_delegate));
    views::ViewsTestBase::SetUp();
  }
};

// Check if the destruction of the widget ends successfully if |view|'s
// visibility changed during destruction.
TEST_F(AXViewTest, LayoutCalledInvalidateRootView) {
  std::unique_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);
  widget->Show();

  View* root = widget->GetRootView();
  DerivedTestView* parent = new DerivedTestView();
  DerivedTestView* child = new DerivedTestView();
  root->AddChildView(parent);
  parent->AddChildView(child);
  child->SetFocusBehavior(DerivedTestView::FocusBehavior::ALWAYS);
  parent->SetFocusBehavior(DerivedTestView::FocusBehavior::ALWAYS);
  root->SetFocusBehavior(DerivedTestView::FocusBehavior::ALWAYS);
  parent->RequestFocus();
  // During the destruction of parent, OnBlur will be called and change the
  // visibility to false.
  parent->SetVisible(true);
  AXAuraObjCache* ax = AXAuraObjCache::GetInstance();
  ax->GetOrCreate(widget.get());
}
#endif

}  // namespace test
}  // namespace views
