// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_x11.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"

namespace views {

const int64 kFirstDisplay = 5321829;
const int64 kSecondDisplay = 928310;

// Class which waits till the X11 window associated with the widget passed into
// the constructor is activated. We cannot listen for the widget's activation
// because the _NET_ACTIVE_WINDOW property is changed after the widget is
// activated.
class ActivationWaiter : public ui::PlatformEventDispatcher {
 public:
  explicit ActivationWaiter(views::Widget* widget)
      : x_root_window_(DefaultRootWindow(gfx::GetXDisplay())),
        widget_xid_(0),
        active_(false) {
    const char* kAtomToCache[] = {
        "_NET_ACTIVE_WINDOW",
        NULL
    };
    atom_cache_.reset(new ui::X11AtomCache(gfx::GetXDisplay(), kAtomToCache));
    widget_xid_ = widget->GetNativeWindow()->GetHost()->
         GetAcceleratedWidget();
    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  }

  virtual ~ActivationWaiter() {
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  }

  // Blocks till |widget_xid_| becomes active.
  void Wait() {
    if (active_)
      return;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // ui::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
    return event->type == PropertyNotify &&
           event->xproperty.window == x_root_window_;
  }

  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) OVERRIDE {
    ::Window xid;
    CHECK_EQ(PropertyNotify, event->type);
    CHECK_EQ(x_root_window_, event->xproperty.window);

    if (event->xproperty.atom == atom_cache_->GetAtom("_NET_ACTIVE_WINDOW") &&
        ui::GetXIDProperty(x_root_window_, "_NET_ACTIVE_WINDOW", &xid) &&
        xid == widget_xid_) {
      active_ = true;
      if (!quit_closure_.is_null())
        quit_closure_.Run();
    }
    return ui::POST_DISPATCH_NONE;
  }

 private:
  scoped_ptr<ui::X11AtomCache> atom_cache_;
  ::Window x_root_window_;
  ::Window widget_xid_;

  bool active_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ActivationWaiter);
};

class DesktopScreenX11Test : public views::ViewsTestBase,
                             public gfx::DisplayObserver {
 public:
  DesktopScreenX11Test() {}
  virtual ~DesktopScreenX11Test() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    ViewsTestBase::SetUp();
    // Initialize the world to the single monitor case.
    std::vector<gfx::Display> displays;
    displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
    screen_.reset(new DesktopScreenX11(displays));
    screen_->AddObserver(this);
  }

  virtual void TearDown() OVERRIDE {
    screen_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  std::vector<gfx::Display> changed_display_;
  std::vector<gfx::Display> added_display_;
  std::vector<gfx::Display> removed_display_;

  DesktopScreenX11* screen() { return screen_.get(); }

  void ResetDisplayChanges() {
    changed_display_.clear();
    added_display_.clear();
    removed_display_.clear();
  }

  Widget* BuildTopLevelDesktopWidget(const gfx::Rect& bounds) {
    Widget* toplevel = new Widget;
    Widget::InitParams toplevel_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    toplevel_params.native_widget =
        new views::DesktopNativeWidgetAura(toplevel);
    toplevel_params.bounds = bounds;
    toplevel_params.remove_standard_frame = true;
    toplevel->Init(toplevel_params);
    return toplevel;
  }

 private:
  // Overridden from gfx::DisplayObserver:
  virtual void OnDisplayBoundsChanged(const gfx::Display& display) OVERRIDE {
    changed_display_.push_back(display);
  }

  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE {
    added_display_.push_back(new_display);
  }

  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE {
    removed_display_.push_back(old_display);
  }

  scoped_ptr<DesktopScreenX11> screen_;

  DISALLOW_COPY_AND_ASSIGN(DesktopScreenX11Test);
};

TEST_F(DesktopScreenX11Test, BoundsChangeSingleMonitor) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(1u, changed_display_.size());
  EXPECT_EQ(0u, added_display_.size());
  EXPECT_EQ(0u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, AddMonitorToTheRight) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  displays.push_back(gfx::Display(kSecondDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(0u, changed_display_.size());
  EXPECT_EQ(1u, added_display_.size());
  EXPECT_EQ(0u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, AddMonitorToTheLeft) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kSecondDisplay, gfx::Rect(0, 0, 1024, 768)));
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(1024, 0, 640, 480)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(1u, changed_display_.size());
  EXPECT_EQ(1u, added_display_.size());
  EXPECT_EQ(0u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, RemoveMonitorOnRight) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  displays.push_back(gfx::Display(kSecondDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  ResetDisplayChanges();

  displays.clear();
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(0u, changed_display_.size());
  EXPECT_EQ(0u, added_display_.size());
  EXPECT_EQ(1u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, RemoveMonitorOnLeft) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  displays.push_back(gfx::Display(kSecondDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  ResetDisplayChanges();

  displays.clear();
  displays.push_back(gfx::Display(kSecondDisplay, gfx::Rect(0, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(1u, changed_display_.size());
  EXPECT_EQ(0u, added_display_.size());
  EXPECT_EQ(1u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, GetDisplayNearestPoint) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  displays.push_back(gfx::Display(kSecondDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(kSecondDisplay,
            screen()->GetDisplayNearestPoint(gfx::Point(650, 10)).id());
  EXPECT_EQ(kFirstDisplay,
            screen()->GetDisplayNearestPoint(gfx::Point(10, 10)).id());
  EXPECT_EQ(kFirstDisplay,
            screen()->GetDisplayNearestPoint(gfx::Point(10000, 10000)).id());
}

TEST_F(DesktopScreenX11Test, GetDisplayMatchingBasic) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  displays.push_back(gfx::Display(kSecondDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(kSecondDisplay,
            screen()->GetDisplayMatching(gfx::Rect(700, 20, 100, 100)).id());
}

TEST_F(DesktopScreenX11Test, GetDisplayMatchingOverlap) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  displays.push_back(gfx::Display(kSecondDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(kSecondDisplay,
            screen()->GetDisplayMatching(gfx::Rect(630, 20, 100, 100)).id());
}

TEST_F(DesktopScreenX11Test, GetPrimaryDisplay) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  displays.push_back(gfx::Display(kSecondDisplay, gfx::Rect(0, 0, 640, 480)));
  screen()->ProcessDisplayChange(displays);

  // The first display in the list is always the primary, even if other
  // displays are to the left in screen layout.
  EXPECT_EQ(kFirstDisplay, screen()->GetPrimaryDisplay().id());
}

TEST_F(DesktopScreenX11Test, GetWindowAtScreenPoint) {
  Widget* window_one = BuildTopLevelDesktopWidget(gfx::Rect(110, 110, 10, 10));
  Widget* window_two = BuildTopLevelDesktopWidget(gfx::Rect(150, 150, 10, 10));
  Widget* window_three =
      BuildTopLevelDesktopWidget(gfx::Rect(115, 115, 20, 20));

  window_three->Show();
  window_two->Show();
  window_one->Show();

  // Make sure the internal state of DesktopWindowTreeHostX11 is set up
  // correctly.
  ASSERT_EQ(3u, DesktopWindowTreeHostX11::GetAllOpenWindows().size());

  EXPECT_EQ(window_one->GetNativeWindow(),
            screen()->GetWindowAtScreenPoint(gfx::Point(115, 115)));
  EXPECT_EQ(window_two->GetNativeWindow(),
            screen()->GetWindowAtScreenPoint(gfx::Point(155, 155)));
  EXPECT_EQ(NULL,
            screen()->GetWindowAtScreenPoint(gfx::Point(200, 200)));

  // Bring the third window in front. It overlaps with the first window.
  // Hit-testing on the intersecting region should give the third window.
  ActivationWaiter activation_waiter(window_three);
  window_three->Activate();
  activation_waiter.Wait();

  EXPECT_EQ(window_three->GetNativeWindow(),
            screen()->GetWindowAtScreenPoint(gfx::Point(115, 115)));

  window_one->CloseNow();
  window_two->CloseNow();
  window_three->CloseNow();
}

TEST_F(DesktopScreenX11Test, GetDisplayNearestWindow) {
  // Set up a two monitor situation.
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  displays.push_back(gfx::Display(kSecondDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  Widget* window_one = BuildTopLevelDesktopWidget(gfx::Rect(10, 10, 10, 10));
  Widget* window_two = BuildTopLevelDesktopWidget(gfx::Rect(650, 50, 10, 10));

  EXPECT_EQ(
      kFirstDisplay,
      screen()->GetDisplayNearestWindow(window_one->GetNativeWindow()).id());
  EXPECT_EQ(
      kSecondDisplay,
      screen()->GetDisplayNearestWindow(window_two->GetNativeWindow()).id());

  window_one->CloseNow();
  window_two->CloseNow();
}

}  // namespace views
