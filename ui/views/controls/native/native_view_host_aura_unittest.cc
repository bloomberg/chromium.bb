// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_aura.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window.h"
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
  virtual ~NativeViewHostTesting() {
    destroyed_count_++;
  }

  static void ResetDestroyedCount() {
    destroyed_count_ = 0;
  }

  static int destroyed_count() {
    return destroyed_count_;
  }

 private:
  static int destroyed_count_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostTesting);
};
int NativeViewHostTesting::destroyed_count_ = 0;

// Observer watching for how many times an aura::Window has been destroyed. This
// is used to confirm that an object has been cleaned up when expected.
class AuraWindowDestroyedObserver : public aura::WindowObserver {
 public:
  AuraWindowDestroyedObserver(aura::Window *window) :
      destroyed_count_(0) {
    window->AddObserver(this);
  }

  virtual ~AuraWindowDestroyedObserver() {}

  int destroyed_count() { return destroyed_count_; }

  // aura::WindowObserver overrides
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {
    destroyed_count_++;
  }

 private:
  int destroyed_count_;

  DISALLOW_COPY_AND_ASSIGN(AuraWindowDestroyedObserver);
};

class NativeViewHostAuraTest : public ViewsTestBase {
 public:
  NativeViewHostAuraTest() {
  }

  NativeViewHostAura* native_host() {
    return static_cast<NativeViewHostAura*>(host_->native_wrapper_.get());
  }

  NativeViewHost* host() {
    return host_.get();
  }

  Widget* child() {
    return child_.get();
  }

  aura::Window* clipping_window() {
    return native_host()->clipping_window_;
  }

  Widget* toplevel() {
    return toplevel_.get();
  }

  void CreateHost() {
    // Create the top level widget.
    toplevel_.reset(new Widget);
    Widget::InitParams toplevel_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    toplevel_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    toplevel_->Init(toplevel_params);

    // And the child widget.
    View* test_view = new View;
    child_.reset(new Widget);
    Widget::InitParams child_params(Widget::InitParams::TYPE_CONTROL);
    child_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    child_params.parent = toplevel_->GetNativeView();
    child_->Init(child_params);
    child_->SetContentsView(test_view);

    // Owned by |toplevel|.
    host_.reset(new NativeViewHostTesting);
    toplevel_->GetRootView()->AddChildView(host_.get());
    host_->Attach(child_->GetNativeView());
  }

  void DestroyHost() {
    host_.reset();
  }

  NativeViewHostTesting* ReleaseHost() {
    return host_.release();
  }

  void DestroyTopLevel() {
    toplevel_.reset();
  }

  gfx::Point CalculateNativeViewOrigin(gfx::Rect input_rect,
                                       gfx::Rect native_rect) {
    return native_host()->CalculateNativeViewOrigin(input_rect, native_rect);
  }

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
  EXPECT_EQ(host(), clipping_window()->GetProperty(views::kHostViewKey));

  host()->Detach();
  EXPECT_FALSE(child_win->GetProperty(views::kHostViewKey));

  host()->Attach(child_win);
  EXPECT_EQ(host(), child_win->GetProperty(views::kHostViewKey));
  EXPECT_EQ(host(), clipping_window()->GetProperty(views::kHostViewKey));

  DestroyHost();
  EXPECT_FALSE(child_win->GetProperty(views::kHostViewKey));
}

// Tests that the values being calculated by CalculateNativeViewOrigin are
// correct.
TEST_F(NativeViewHostAuraTest, CalculateNewNativeViewOrigin) {
  CreateHost();

  gfx::Rect clip_rect(0, 0, 50, 50);
  gfx::Rect contents_rect(50, 50, 100, 100);

  host()->set_fast_resize_gravity(NativeViewHost::GRAVITY_NORTHWEST);
  EXPECT_EQ(gfx::Point(0, 0).ToString(),
            CalculateNativeViewOrigin(clip_rect, contents_rect).ToString());

  host()->set_fast_resize_gravity(NativeViewHost::GRAVITY_NORTH);
  EXPECT_EQ(gfx::Point(-25, 0).ToString(),
            CalculateNativeViewOrigin(clip_rect, contents_rect).ToString());

  host()->set_fast_resize_gravity(NativeViewHost::GRAVITY_NORTHEAST);
  EXPECT_EQ(gfx::Point(-50, 0).ToString(),
            CalculateNativeViewOrigin(clip_rect, contents_rect).ToString());

  host()->set_fast_resize_gravity(NativeViewHost::GRAVITY_EAST);
  EXPECT_EQ(gfx::Point(-50, -25).ToString(),
            CalculateNativeViewOrigin(clip_rect, contents_rect).ToString());

  host()->set_fast_resize_gravity(NativeViewHost::GRAVITY_SOUTHEAST);
  EXPECT_EQ(gfx::Point(-50, -50).ToString(),
            CalculateNativeViewOrigin(clip_rect, contents_rect).ToString());

  host()->set_fast_resize_gravity(NativeViewHost::GRAVITY_SOUTH);
  EXPECT_EQ(gfx::Point(-25, -50).ToString(),
            CalculateNativeViewOrigin(clip_rect, contents_rect).ToString());

  host()->set_fast_resize_gravity(NativeViewHost::GRAVITY_SOUTHWEST);
  EXPECT_EQ(gfx::Point(0, -50).ToString(),
            CalculateNativeViewOrigin(clip_rect, contents_rect).ToString());

  host()->set_fast_resize_gravity(NativeViewHost::GRAVITY_WEST);
  EXPECT_EQ(gfx::Point(0, -25).ToString(),
            CalculateNativeViewOrigin(clip_rect, contents_rect).ToString());

  host()->set_fast_resize_gravity(NativeViewHost::GRAVITY_CENTER);
  EXPECT_EQ(gfx::Point(-25, -25).ToString(),
            CalculateNativeViewOrigin(clip_rect, contents_rect).ToString());

  DestroyHost();
}

// Test that the fast resize path places the clipping and content windows were
// they are supposed to be.
TEST_F(NativeViewHostAuraTest, FastResizePath) {
  CreateHost();
  host()->set_fast_resize(false);
  toplevel()->SetBounds(gfx::Rect(0, 0, 100, 100));
  native_host()->ShowWidget(0, 0, 100, 100);
  host()->set_fast_resize(true);

  host()->set_fast_resize_gravity(NativeViewHost::GRAVITY_CENTER);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            host()->native_view()->layer()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            clipping_window()->layer()->bounds().ToString());

  host()->set_fast_resize_gravity(NativeViewHost::GRAVITY_NORTHWEST);
  native_host()->ShowWidget(0, 0, 50, 50);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            host()->native_view()->layer()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(),
            clipping_window()->layer()->bounds().ToString());

  host()->set_fast_resize_gravity(NativeViewHost::GRAVITY_SOUTHEAST);
  native_host()->ShowWidget(0, 0, 50, 50);
  EXPECT_EQ(gfx::Rect(-50, -50, 100, 100).ToString(),
            host()->native_view()->layer()->bounds().ToString());
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(),
            clipping_window()->layer()->bounds().ToString());

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

// Test that the clipping window and the native view are destroyed when the
// containing widget for the native view is closed.
TEST_F(NativeViewHostAuraTest, ClosingWidgetDoesntLeak) {
  CreateHost();
  AuraWindowDestroyedObserver clipping_observer(clipping_window());
  AuraWindowDestroyedObserver native_view_observer(child()->GetNativeView());
  EXPECT_EQ(0, clipping_observer.destroyed_count());
  EXPECT_EQ(0, native_view_observer.destroyed_count());
  child()->CloseNow();
  EXPECT_EQ(1, clipping_observer.destroyed_count());
  EXPECT_EQ(1, native_view_observer.destroyed_count());
  DestroyHost();
}

// Test that the none fast resize path is clipped and positioned correctly.
TEST_F(NativeViewHostAuraTest, NonFastResizePath) {
  const gfx::Rect base_rect = gfx::Rect(0, 0, 100, 100);
  CreateHost();
  toplevel()->SetBounds(base_rect);
  native_host()->ShowWidget(base_rect.x(), base_rect.y(),
                            base_rect.width(), base_rect.height());
  EXPECT_EQ(base_rect.ToString(),
            host()->native_view()->bounds().ToString());
  EXPECT_EQ(base_rect.ToString(),
            clipping_window()->bounds().ToString());

  const gfx::Rect kTestCases[] = {
    gfx::Rect(-10, -20, 100, 100),
    gfx::Rect(0, -20, 100, 100),
    gfx::Rect(10, -20, 100, 100),
    gfx::Rect(-10, 0, 100, 100),
    gfx::Rect(0, 0, 100, 100),
    gfx::Rect(10, 0, 100, 100),
    gfx::Rect(-10, 20, 100, 100),
    gfx::Rect(0, 20, 100, 100),
    gfx::Rect(10, 20, 100, 100),
    gfx::Rect(0, 0, 200, 300),
    gfx::Rect(-50, -100, 200, 300),
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    const gfx::Rect& bounds = kTestCases[i];

    host()->SetBoundsRect(bounds);

    gfx::Rect clip_rect = gfx::IntersectRects(bounds, base_rect);
    EXPECT_EQ(clip_rect.ToString(), clipping_window()->bounds().ToString());

    gfx::Rect native_view_bounds = bounds;
    native_view_bounds.Offset(-clip_rect.x(), -clip_rect.y());
    EXPECT_EQ(native_view_bounds.ToString(),
              host()->native_view()->bounds().ToString());
  }

  DestroyHost();
}

}  // namespace views
