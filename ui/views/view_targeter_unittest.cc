// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_targeter.h"

#include "ui/events/event_targeter.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/path.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/root_view.h"

namespace views {

// A derived class of View used for testing purposes.
class TestingView : public View, public ViewTargeterDelegate {
 public:
  TestingView() : can_process_events_within_subtree_(true) {}
  virtual ~TestingView() {}

  // Reset all test state.
  void Reset() { can_process_events_within_subtree_ = true; }

  void set_can_process_events_within_subtree(bool can_process) {
    can_process_events_within_subtree_ = can_process;
  }

  // A call-through function to ViewTargeterDelegate::DoesIntersectRect().
  bool TestDoesIntersectRect(const View* target, const gfx::Rect& rect) const {
    return DoesIntersectRect(target, rect);
  }

  // View:
  virtual bool CanProcessEventsWithinSubtree() const OVERRIDE {
    return can_process_events_within_subtree_;
  }

 private:
  // Value to return from CanProcessEventsWithinSubtree().
  bool can_process_events_within_subtree_;

  DISALLOW_COPY_AND_ASSIGN(TestingView);
};

// A derived class of View having a triangular-shaped hit test mask.
class TestMaskedView : public View, public MaskedTargeterDelegate {
 public:
  TestMaskedView() {}
  virtual ~TestMaskedView() {}

  // A call-through function to MaskedTargeterDelegate::DoesIntersectRect().
  bool TestDoesIntersectRect(const View* target, const gfx::Rect& rect) const {
    return DoesIntersectRect(target, rect);
  }

 private:
  // MaskedTargeterDelegate:
  virtual bool GetHitTestMask(gfx::Path* mask) const OVERRIDE {
    DCHECK(mask);
    SkScalar w = SkIntToScalar(width());
    SkScalar h = SkIntToScalar(height());

    // Create a triangular mask within the bounds of this View.
    mask->moveTo(w / 2, 0);
    mask->lineTo(w, h);
    mask->lineTo(0, h);
    mask->close();
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(TestMaskedView);
};

namespace test {

typedef ViewsTestBase ViewTargeterTest;

namespace {

gfx::Point ConvertPointToView(View* view, const gfx::Point& p) {
  gfx::Point tmp(p);
  View::ConvertPointToTarget(view->GetWidget()->GetRootView(), view, &tmp);
  return tmp;
}

gfx::Rect ConvertRectToView(View* view, const gfx::Rect& r) {
  gfx::Rect tmp(r);
  tmp.set_origin(ConvertPointToView(view, r.origin()));
  return tmp;
}

}  // namespace

// Verifies that the the functions ViewTargeter::FindTargetForEvent()
// and ViewTargeter::FindNextBestTarget() are implemented correctly
// for key events.
TEST_F(ViewTargeterTest, ViewTargeterForKeyEvents) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);

  View* content = new View;
  View* child = new View;
  View* grandchild = new View;

  widget.SetContentsView(content);
  content->AddChildView(child);
  child->AddChildView(grandchild);

  grandchild->SetFocusable(true);
  grandchild->RequestFocus();

  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  ViewTargeter* view_targeter = new ViewTargeter(root_view);
  ui::EventTargeter* targeter = view_targeter;
  root_view->SetEventTargeter(make_scoped_ptr(view_targeter));

  ui::KeyEvent key_event('a', ui::VKEY_A, ui::EF_NONE);

  // The focused view should be the initial target of the event.
  ui::EventTarget* current_target = targeter->FindTargetForEvent(root_view,
                                                                 &key_event);
  EXPECT_EQ(grandchild, static_cast<View*>(current_target));

  // Verify that FindNextBestTarget() will return the parent view of the
  // argument (and NULL if the argument has no parent view).
  current_target = targeter->FindNextBestTarget(grandchild, &key_event);
  EXPECT_EQ(child, static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(child, &key_event);
  EXPECT_EQ(content, static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(content, &key_event);
  EXPECT_EQ(widget.GetRootView(), static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(widget.GetRootView(),
                                                &key_event);
  EXPECT_EQ(NULL, static_cast<View*>(current_target));
}

// Verifies that the the functions ViewTargeter::FindTargetForEvent()
// and ViewTargeter::FindNextBestTarget() are implemented correctly
// for scroll events.
TEST_F(ViewTargeterTest, ViewTargeterForScrollEvents) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  widget.Init(init_params);

  // The coordinates used for SetBounds() are in the parent coordinate space.
  View* content = new View;
  content->SetBounds(0, 0, 100, 100);
  View* child = new View;
  child->SetBounds(50, 50, 20, 20);
  View* grandchild = new View;
  grandchild->SetBounds(0, 0, 5, 5);

  widget.SetContentsView(content);
  content->AddChildView(child);
  child->AddChildView(grandchild);

  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  ViewTargeter* view_targeter = new ViewTargeter(root_view);
  ui::EventTargeter* targeter = view_targeter;
  root_view->SetEventTargeter(make_scoped_ptr(view_targeter));

  // The event falls within the bounds of |child| and |content| but not
  // |grandchild|, so |child| should be the initial target for the event.
  ui::ScrollEvent scroll(ui::ET_SCROLL,
                         gfx::Point(60, 60),
                         ui::EventTimeForNow(),
                         0,
                         0, 3,
                         0, 3,
                         2);
  ui::EventTarget* current_target = targeter->FindTargetForEvent(root_view,
                                                                 &scroll);
  EXPECT_EQ(child, static_cast<View*>(current_target));

  // Verify that FindNextBestTarget() will return the parent view of the
  // argument (and NULL if the argument has no parent view).
  current_target = targeter->FindNextBestTarget(child, &scroll);
  EXPECT_EQ(content, static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(content, &scroll);
  EXPECT_EQ(widget.GetRootView(), static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(widget.GetRootView(),
                                                &scroll);
  EXPECT_EQ(NULL, static_cast<View*>(current_target));

  // The event falls outside of the original specified bounds of |content|,
  // |child|, and |grandchild|. But since |content| is the contents view,
  // and contents views are resized to fill the entire area of the root
  // view, the event's initial target should still be |content|.
  scroll = ui::ScrollEvent(ui::ET_SCROLL,
                           gfx::Point(150, 150),
                           ui::EventTimeForNow(),
                           0,
                           0, 3,
                           0, 3,
                           2);
  current_target = targeter->FindTargetForEvent(root_view, &scroll);
  EXPECT_EQ(content, static_cast<View*>(current_target));
}

// Tests that the functions ViewTargeterDelegate::DoesIntersectRect()
// and MaskedTargeterDelegate::DoesIntersectRect() work as intended when
// called on views which are derived from ViewTargeterDelegate.
// Also verifies that ViewTargeterDelegate::DoesIntersectRect() can
// be called from the ViewTargeter installed on RootView.
TEST_F(ViewTargeterTest, DoesIntersectRect) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 650, 650);
  widget.Init(params);

  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  ViewTargeter* view_targeter = new ViewTargeter(root_view);
  root_view->SetEventTargeter(make_scoped_ptr(view_targeter));

  // The coordinates used for SetBounds() are in the parent coordinate space.
  TestingView v2;
  TestMaskedView v1, v3;
  v1.SetBounds(0, 0, 200, 200);
  v2.SetBounds(300, 0, 300, 300);
  v3.SetBounds(0, 0, 100, 100);
  root_view->AddChildView(&v1);
  root_view->AddChildView(&v2);
  v2.AddChildView(&v3);

  // The coordinates used below are in the local coordinate space of the
  // view that is passed in as an argument.

  // Hit tests against |v1|, which has a hit test mask.
  EXPECT_TRUE(v1.TestDoesIntersectRect(&v1, gfx::Rect(0, 0, 200, 200)));
  EXPECT_TRUE(v1.TestDoesIntersectRect(&v1, gfx::Rect(-10, -10, 110, 12)));
  EXPECT_TRUE(v1.TestDoesIntersectRect(&v1, gfx::Rect(112, 142, 1, 1)));
  EXPECT_FALSE(v1.TestDoesIntersectRect(&v1, gfx::Rect(0, 0, 20, 20)));
  EXPECT_FALSE(v1.TestDoesIntersectRect(&v1, gfx::Rect(-10, -10, 90, 12)));
  EXPECT_FALSE(v1.TestDoesIntersectRect(&v1, gfx::Rect(150, 49, 1, 1)));

  // Hit tests against |v2|, which does not have a hit test mask.
  EXPECT_TRUE(v2.TestDoesIntersectRect(&v2, gfx::Rect(0, 0, 200, 200)));
  EXPECT_TRUE(v2.TestDoesIntersectRect(&v2, gfx::Rect(-10, 250, 60, 60)));
  EXPECT_TRUE(v2.TestDoesIntersectRect(&v2, gfx::Rect(250, 250, 1, 1)));
  EXPECT_FALSE(v2.TestDoesIntersectRect(&v2, gfx::Rect(-10, 250, 7, 7)));
  EXPECT_FALSE(v2.TestDoesIntersectRect(&v2, gfx::Rect(-1, -1, 1, 1)));

  // Hit tests against |v3|, which has a hit test mask and is a child of |v2|.
  EXPECT_TRUE(v3.TestDoesIntersectRect(&v3, gfx::Rect(0, 0, 50, 50)));
  EXPECT_TRUE(v3.TestDoesIntersectRect(&v3, gfx::Rect(90, 90, 1, 1)));
  EXPECT_FALSE(v3.TestDoesIntersectRect(&v3, gfx::Rect(10, 125, 50, 50)));
  EXPECT_FALSE(v3.TestDoesIntersectRect(&v3, gfx::Rect(110, 110, 1, 1)));

  // Verify that hit-testing is performed correctly when using the
  // call-through function ViewTargeter::DoesIntersectRect().
  EXPECT_TRUE(view_targeter->DoesIntersectRect(root_view,
                                               gfx::Rect(0, 0, 50, 50)));
  EXPECT_FALSE(view_targeter->DoesIntersectRect(root_view,
                                                gfx::Rect(-20, -20, 10, 10)));
}

// Tests that calls made directly on the hit-testing methods in View
// (HitTestPoint(), HitTestRect(), etc.) return the correct values.
TEST_F(ViewTargeterTest, HitTestCallsOnView) {
  // The coordinates in this test are in the coordinate space of the root view.
  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  widget->Init(params);
  View* root_view = widget->GetRootView();
  root_view->SetBoundsRect(gfx::Rect(0, 0, 500, 500));

  // |v1| has no hit test mask. No ViewTargeter is installed on |v1|, which
  // means that View::HitTestRect() will call into the targeter installed on
  // the root view instead when we hit test against |v1|.
  gfx::Rect v1_bounds = gfx::Rect(0, 0, 100, 100);
  TestingView* v1 = new TestingView();
  v1->SetBoundsRect(v1_bounds);
  root_view->AddChildView(v1);

  // |v2| has a triangular hit test mask. Install a ViewTargeter on |v2| which
  // will be called into by View::HitTestRect().
  gfx::Rect v2_bounds = gfx::Rect(105, 0, 100, 100);
  TestMaskedView* v2 = new TestMaskedView();
  v2->SetBoundsRect(v2_bounds);
  root_view->AddChildView(v2);
  ViewTargeter* view_targeter = new ViewTargeter(v2);
  v2->SetEventTargeter(make_scoped_ptr(view_targeter));

  gfx::Point v1_centerpoint = v1_bounds.CenterPoint();
  gfx::Point v2_centerpoint = v2_bounds.CenterPoint();
  gfx::Point v1_origin = v1_bounds.origin();
  gfx::Point v2_origin = v2_bounds.origin();
  gfx::Rect r1(10, 10, 110, 15);
  gfx::Rect r2(106, 1, 98, 98);
  gfx::Rect r3(0, 0, 300, 300);
  gfx::Rect r4(115, 342, 200, 10);

  // Test calls into View::HitTestPoint().
  EXPECT_TRUE(v1->HitTestPoint(ConvertPointToView(v1, v1_centerpoint)));
  EXPECT_TRUE(v2->HitTestPoint(ConvertPointToView(v2, v2_centerpoint)));

  EXPECT_TRUE(v1->HitTestPoint(ConvertPointToView(v1, v1_origin)));
  EXPECT_FALSE(v2->HitTestPoint(ConvertPointToView(v2, v2_origin)));

  // Test calls into View::HitTestRect().
  EXPECT_TRUE(v1->HitTestRect(ConvertRectToView(v1, r1)));
  EXPECT_FALSE(v2->HitTestRect(ConvertRectToView(v2, r1)));

  EXPECT_FALSE(v1->HitTestRect(ConvertRectToView(v1, r2)));
  EXPECT_TRUE(v2->HitTestRect(ConvertRectToView(v2, r2)));

  EXPECT_TRUE(v1->HitTestRect(ConvertRectToView(v1, r3)));
  EXPECT_TRUE(v2->HitTestRect(ConvertRectToView(v2, r3)));

  EXPECT_FALSE(v1->HitTestRect(ConvertRectToView(v1, r4)));
  EXPECT_FALSE(v2->HitTestRect(ConvertRectToView(v2, r4)));

  // Test calls into View::GetEventHandlerForPoint().
  EXPECT_EQ(v1, root_view->GetEventHandlerForPoint(v1_centerpoint));
  EXPECT_EQ(v2, root_view->GetEventHandlerForPoint(v2_centerpoint));

  EXPECT_EQ(v1, root_view->GetEventHandlerForPoint(v1_origin));
  EXPECT_EQ(root_view, root_view->GetEventHandlerForPoint(v2_origin));

  // Test calls into View::GetTooltipHandlerForPoint().
  EXPECT_EQ(v1, root_view->GetTooltipHandlerForPoint(v1_centerpoint));
  EXPECT_EQ(v2, root_view->GetTooltipHandlerForPoint(v2_centerpoint));

  EXPECT_EQ(v1, root_view->GetTooltipHandlerForPoint(v1_origin));
  EXPECT_EQ(root_view, root_view->GetTooltipHandlerForPoint(v2_origin));

  EXPECT_FALSE(v1->GetTooltipHandlerForPoint(v2_origin));

  widget->CloseNow();
}

}  // namespace test
}  // namespace views
