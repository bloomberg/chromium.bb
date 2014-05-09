// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_targeter.h"

#include "ui/events/event_targeter.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/root_view.h"

namespace views {
namespace test {

typedef ViewsTestBase ViewTargeterTest;

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

  ui::EventTargeter* targeter = new ViewTargeter();
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  root_view->SetEventTargeter(make_scoped_ptr(targeter));

  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, 0, true);

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

  ui::EventTargeter* targeter = new ViewTargeter();
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  root_view->SetEventTargeter(make_scoped_ptr(targeter));

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

// Tests the basic functionality of the method
// ViewTargeter::SubtreeShouldBeExploredForEvent().
TEST_F(ViewTargeterTest, SubtreeShouldBeExploredForEvent) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 650, 650);
  widget.Init(params);

  ui::EventTargeter* targeter = new ViewTargeter();
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  root_view->SetEventTargeter(make_scoped_ptr(targeter));

  // The coordinates used for SetBounds() are in the parent coordinate space.
  View v1, v2, v3;
  v1.SetBounds(0, 0, 300, 300);
  v2.SetBounds(100, 100, 100, 100);
  v3.SetBounds(0, 0, 10, 10);
  v3.SetVisible(false);
  root_view->AddChildView(&v1);
  v1.AddChildView(&v2);
  v2.AddChildView(&v3);

  // Note that the coordinates used below are in |v1|'s coordinate space,
  // and that SubtreeShouldBeExploredForEvent() expects the event location
  // to be in the coordinate space of the target's parent. |v1| and
  // its parent share a common coordinate space.

  // Event located within |v1| only.
  gfx::Point point(10, 10);
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, point, point,
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_TRUE(targeter->SubtreeShouldBeExploredForEvent(&v1, event));
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(&v2, event));
  v1.ConvertEventToTarget(&v2, &event);
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(&v3, event));

  // Event located within |v1| and |v2| only.
  event.set_location(gfx::Point(150, 150));
  EXPECT_TRUE(targeter->SubtreeShouldBeExploredForEvent(&v1, event));
  EXPECT_TRUE(targeter->SubtreeShouldBeExploredForEvent(&v2, event));
  v1.ConvertEventToTarget(&v2, &event);
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(&v3, event));

  // Event located within |v1|, |v2|, and |v3|. Note that |v3| is not
  // visible, so it cannot handle the event.
  event.set_location(gfx::Point(105, 105));
  EXPECT_TRUE(targeter->SubtreeShouldBeExploredForEvent(&v1, event));
  EXPECT_TRUE(targeter->SubtreeShouldBeExploredForEvent(&v2, event));
  v1.ConvertEventToTarget(&v2, &event);
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(&v3, event));

  // Event located outside the bounds of all views.
  event.set_location(gfx::Point(400, 400));
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(&v1, event));
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(&v2, event));
  v1.ConvertEventToTarget(&v2, &event);
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(&v3, event));

  // TODO(tdanderson): Move the hit-testing unit tests out of view_unittest
  // and into here. See crbug.com/355425.
}

}  // namespace test
}  // namespace views
