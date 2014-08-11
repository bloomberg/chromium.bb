// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_aura.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

// Testing wrapper of the NativeViewHost
class NativeViewHostTesting : public NativeViewHost {
 public:
  NativeViewHostTesting() {}
  virtual ~NativeViewHostTesting() { destroyed_count_++; }

  static void ResetDestroyedCount() { destroyed_count_ = 0; }

  static int destroyed_count() { return destroyed_count_; }

 private:
  static int destroyed_count_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostTesting);
};
int NativeViewHostTesting::destroyed_count_ = 0;

// Observer watching for window visibility and bounds change events. This is
// used to verify that the child and clipping window operations are done in the
// right order.
class NativeViewHostWindowObserver : public aura::WindowObserver {
 public:
  enum EventType {
    EVENT_NONE,
    EVENT_SHOWN,
    EVENT_HIDDEN,
    EVENT_BOUNDS_CHANGED,
  };

  struct EventDetails {
    EventType type;
    aura::Window* window;
    gfx::Rect bounds;
    bool operator!=(const EventDetails& rhs) {
      return type != rhs.type || window != rhs.window || bounds != rhs.bounds;
    }
  };

  NativeViewHostWindowObserver() {}
  virtual ~NativeViewHostWindowObserver() {}

  const std::vector<EventDetails>& events() const { return events_; }

  // aura::WindowObserver overrides
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE {
    EventDetails event;
    event.type = visible ? EVENT_SHOWN : EVENT_HIDDEN;
    event.window = window;
    event.bounds = window->GetBoundsInRootWindow();

    // Dedupe events as a single Hide() call can result in several
    // notifications.
    if (events_.size() == 0u || events_.back() != event)
      events_.push_back(event);
  }

  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE {
    EventDetails event;
    event.type = EVENT_BOUNDS_CHANGED;
    event.window = window;
    event.bounds = window->GetBoundsInRootWindow();
    events_.push_back(event);
  }

 private:
  std::vector<EventDetails> events_;
  gfx::Rect bounds_at_visibility_changed_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostWindowObserver);
};

class NativeViewHostAuraTest : public ViewsTestBase {
 public:
  NativeViewHostAuraTest() {
  }

  NativeViewHostAura* native_host() {
    return static_cast<NativeViewHostAura*>(host_->native_wrapper_.get());
  }

  Widget* toplevel() {
    return toplevel_.get();
  }

  NativeViewHost* host() {
    return host_.get();
  }

  Widget* child() {
    return child_.get();
  }

  aura::Window* clipping_window() { return &(native_host()->clipping_window_); }

  void CreateHost() {
    // Create the top level widget.
    toplevel_.reset(new Widget);
    Widget::InitParams toplevel_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    toplevel_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    toplevel_->Init(toplevel_params);

    // And the child widget.
    child_.reset(new Widget);
    Widget::InitParams child_params(Widget::InitParams::TYPE_CONTROL);
    child_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    child_params.parent = toplevel_->GetNativeView();
    child_->Init(child_params);
    child_->SetContentsView(new View);

    // Owned by |toplevel|.
    host_.reset(new NativeViewHostTesting);
    toplevel_->GetRootView()->AddChildView(host_.get());
    host_->Attach(child_->GetNativeView());
  }

  void DestroyHost() {
    host_.reset();
  }

  NativeViewHostTesting* ReleaseHost() { return host_.release(); }

  void DestroyTopLevel() { toplevel_.reset(); }

 private:
  scoped_ptr<Widget> toplevel_;
  scoped_ptr<NativeViewHostTesting> host_;
  scoped_ptr<Widget> child_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostAuraTest);
};

// Verifies NativeViewHostAura stops observing native view on destruction.
TEST_F(NativeViewHostAuraTest, StopObservingNativeViewOnDestruct) {
  CreateHost();
  aura::Window* child_win = child()->GetNativeView();
  NativeViewHostAura* aura_host = native_host();

  EXPECT_TRUE(child_win->HasObserver(aura_host));
  DestroyHost();
  EXPECT_FALSE(child_win->HasObserver(aura_host));
}

// Tests that the kHostViewKey is correctly set and cleared.
TEST_F(NativeViewHostAuraTest, HostViewPropertyKey) {
  // Create the NativeViewHost and attach a NativeView.
  CreateHost();
  aura::Window* child_win = child()->GetNativeView();
  EXPECT_EQ(host(), child_win->GetProperty(views::kHostViewKey));
  EXPECT_EQ(host()->GetWidget()->GetNativeView(),
            child_win->GetProperty(aura::client::kHostWindowKey));
  EXPECT_EQ(host(), clipping_window()->GetProperty(views::kHostViewKey));

  host()->Detach();
  EXPECT_FALSE(child_win->GetProperty(views::kHostViewKey));
  EXPECT_FALSE(child_win->GetProperty(aura::client::kHostWindowKey));
  EXPECT_TRUE(clipping_window()->GetProperty(views::kHostViewKey));

  host()->Attach(child_win);
  EXPECT_EQ(host(), child_win->GetProperty(views::kHostViewKey));
  EXPECT_EQ(host()->GetWidget()->GetNativeView(),
            child_win->GetProperty(aura::client::kHostWindowKey));
  EXPECT_EQ(host(), clipping_window()->GetProperty(views::kHostViewKey));

  DestroyHost();
  EXPECT_FALSE(child_win->GetProperty(views::kHostViewKey));
  EXPECT_FALSE(child_win->GetProperty(aura::client::kHostWindowKey));
}

// Tests that the NativeViewHost reports the cursor set on its native view.
TEST_F(NativeViewHostAuraTest, CursorForNativeView) {
  CreateHost();

  toplevel()->SetCursor(ui::kCursorHand);
  child()->SetCursor(ui::kCursorWait);
  ui::MouseEvent move_event(ui::ET_MOUSE_MOVED, gfx::Point(0, 0),
                            gfx::Point(0, 0), 0, 0);

  EXPECT_EQ(ui::kCursorWait, host()->GetCursor(move_event).native_type());

  DestroyHost();
}

// Test that destroying the top level widget before destroying the attached
// NativeViewHost works correctly. Specifically the associated NVH should be
// destroyed and there shouldn't be any errors.
TEST_F(NativeViewHostAuraTest, DestroyWidget) {
  NativeViewHostTesting::ResetDestroyedCount();
  CreateHost();
  ReleaseHost();
  EXPECT_EQ(0, NativeViewHostTesting::destroyed_count());
  DestroyTopLevel();
  EXPECT_EQ(1, NativeViewHostTesting::destroyed_count());
}

// Test that the fast resize path places the clipping and content windows were
// they are supposed to be.
TEST_F(NativeViewHostAuraTest, FastResizePath) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));

  // Without fast resize, the clipping window should size to the native view
  // with the native view positioned at the origin of the clipping window and
  // the clipping window positioned where the native view was requested.
  host()->set_fast_resize(false);
  native_host()->ShowWidget(5, 10, 100, 100);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(5, 10, 100, 100).ToString(),
            clipping_window()->bounds().ToString());

  // With fast resize, the native view should remain the same size but be
  // clipped the requested size.
  host()->set_fast_resize(true);
  native_host()->ShowWidget(10, 25, 50, 50);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 25, 50, 50).ToString(),
            clipping_window()->bounds().ToString());

  // Turning off fast resize should make the native view start resizing again.
  host()->set_fast_resize(false);
  native_host()->ShowWidget(10, 25, 50, 50);
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 25, 50, 50).ToString(),
            clipping_window()->bounds().ToString());

  DestroyHost();
}

// Test installing and uninstalling a clip.
TEST_F(NativeViewHostAuraTest, InstallClip) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));

  // Without a clip, the clipping window should always be positioned at the
  // requested coordinates with the native view positioned at the origin of the
  // clipping window.
  native_host()->ShowWidget(10, 20, 100, 100);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 20, 100, 100).ToString(),
            clipping_window()->bounds().ToString());

  // Clip to the bottom right quarter of the native view.
  native_host()->InstallClip(60, 70, 50, 50);
  native_host()->ShowWidget(10, 20, 100, 100);
  EXPECT_EQ(gfx::Rect(-50, -50, 100, 100).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(60, 70, 50, 50).ToString(),
            clipping_window()->bounds().ToString());

  // Clip to the center of the native view.
  native_host()->InstallClip(35, 45, 50, 50);
  native_host()->ShowWidget(10, 20, 100, 100);
  EXPECT_EQ(gfx::Rect(-25, -25, 100, 100).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(35, 45, 50, 50).ToString(),
            clipping_window()->bounds().ToString());

  // Uninstalling the clip should make the clipping window match the native view
  // again.
  native_host()->UninstallClip();
  native_host()->ShowWidget(10, 20, 100, 100);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(10, 20, 100, 100).ToString(),
            clipping_window()->bounds().ToString());

  DestroyHost();
}

// Ensure native view is parented to the root window after detaching. This is
// a regression test for http://crbug.com/389261.
TEST_F(NativeViewHostAuraTest, ParentAfterDetach) {
  CreateHost();
  aura::Window* child_win = child()->GetNativeView();
  aura::Window* root_window = child_win->GetRootWindow();
  aura::WindowTreeHost* child_win_tree_host = child_win->GetHost();

  host()->Detach();
  EXPECT_EQ(root_window, child_win->GetRootWindow());
  EXPECT_EQ(child_win_tree_host, child_win->GetHost());

  DestroyHost();
}

// Ensure the clipping window is hidden before setting the native view's bounds.
// This is a regression test for http://crbug.com/388699.
TEST_F(NativeViewHostAuraTest, RemoveClippingWindowOrder) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(20, 20, 100, 100));
  native_host()->ShowWidget(10, 20, 100, 100);

  NativeViewHostWindowObserver test_observer;
  clipping_window()->AddObserver(&test_observer);
  child()->GetNativeView()->AddObserver(&test_observer);

  host()->Detach();

  ASSERT_EQ(3u, test_observer.events().size());
  EXPECT_EQ(NativeViewHostWindowObserver::EVENT_HIDDEN,
            test_observer.events()[0].type);
  EXPECT_EQ(clipping_window(), test_observer.events()[0].window);
  EXPECT_EQ(NativeViewHostWindowObserver::EVENT_BOUNDS_CHANGED,
            test_observer.events()[1].type);
  EXPECT_EQ(child()->GetNativeView(), test_observer.events()[1].window);
  EXPECT_EQ(NativeViewHostWindowObserver::EVENT_HIDDEN,
            test_observer.events()[2].type);
  EXPECT_EQ(child()->GetNativeView(), test_observer.events()[2].window);

  clipping_window()->RemoveObserver(&test_observer);
  child()->GetNativeView()->RemoveObserver(&test_observer);

  DestroyHost();
}

// Ensure the native view receives the correct bounds notification when it is
// attached. This is a regression test for https://crbug.com/399420.
TEST_F(NativeViewHostAuraTest, Attach) {
  CreateHost();
  host()->Detach();

  child()->GetNativeView()->SetBounds(gfx::Rect(0, 0, 0, 0));
  toplevel()->SetBounds(gfx::Rect(0, 0, 100, 100));
  host()->SetBounds(10, 10, 80, 80);

  NativeViewHostWindowObserver test_observer;
  child()->GetNativeView()->AddObserver(&test_observer);

  host()->Attach(child()->GetNativeView());

  ASSERT_EQ(3u, test_observer.events().size());
  EXPECT_EQ(NativeViewHostWindowObserver::EVENT_BOUNDS_CHANGED,
            test_observer.events()[0].type);
  EXPECT_EQ(child()->GetNativeView(), test_observer.events()[0].window);
  EXPECT_EQ(gfx::Rect(10, 10, 80, 80).ToString(),
            test_observer.events()[0].bounds.ToString());
  EXPECT_EQ(NativeViewHostWindowObserver::EVENT_SHOWN,
            test_observer.events()[1].type);
  EXPECT_EQ(child()->GetNativeView(), test_observer.events()[1].window);
  EXPECT_EQ(gfx::Rect(10, 10, 80, 80).ToString(),
            test_observer.events()[1].bounds.ToString());
  EXPECT_EQ(NativeViewHostWindowObserver::EVENT_SHOWN,
            test_observer.events()[2].type);
  EXPECT_EQ(clipping_window(), test_observer.events()[2].window);
  EXPECT_EQ(gfx::Rect(10, 10, 80, 80).ToString(),
            test_observer.events()[2].bounds.ToString());

  child()->GetNativeView()->RemoveObserver(&test_observer);
  DestroyHost();
}

}  // namespace views
