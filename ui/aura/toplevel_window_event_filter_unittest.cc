// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/hit_test.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_desktop_delegate.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/gfx/screen.h"

#if defined(OS_WIN)
// Windows headers define macros for these function names which screw with us.
#if defined(CreateWindow)
#undef CreateWindow
#endif
#endif


namespace aura {
namespace test {

namespace {

// A simple window delegate that returns the specified hit-test code when
// requested.
class HitTestWindowDelegate : public TestWindowDelegate {
 public:
  explicit HitTestWindowDelegate(int hittest_code)
      : hittest_code_(hittest_code) {
  }
  virtual ~HitTestWindowDelegate() {}

 private:
  // Overridden from TestWindowDelegate:
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return hittest_code_;
  }
  virtual void OnWindowDestroyed() OVERRIDE {
    delete this;
  }

  int hittest_code_;

  DISALLOW_COPY_AND_ASSIGN(HitTestWindowDelegate);
};

class ToplevelWindowEventFilterTest : public AuraTestBase {
 public:
  ToplevelWindowEventFilterTest() {}
  virtual ~ToplevelWindowEventFilterTest() {}

 protected:
  Window* CreateWindow(int hittest_code) {
    HitTestWindowDelegate* d1 = new HitTestWindowDelegate(hittest_code);
    Window* w1 = new Window(d1);
    w1->set_id(1);
    w1->Init();
    w1->SetParent(NULL);
    w1->SetBounds(gfx::Rect(0, 0, 100, 100));
    w1->Show();
    return w1;
  }

  void DragFromCenterBy(Window* window, int dx, int dy) {
    EventGenerator generator(window);
    generator.DragMouseBy(dx, dy);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ToplevelWindowEventFilterTest);
};

}

TEST_F(ToplevelWindowEventFilterTest, Caption) {
  scoped_ptr<Window> w1(CreateWindow(HTCAPTION));
  gfx::Size size = w1->bounds().size();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should have been offset by 100,100.
  EXPECT_EQ(gfx::Point(100, 100), w1->bounds().origin());
  // Size should not have.
  EXPECT_EQ(size, w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, BottomRight) {
  scoped_ptr<Window> w1(CreateWindow(HTBOTTOMRIGHT));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have increased by 100,100.
  EXPECT_EQ(gfx::Size(200, 200), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, GrowBox) {
  scoped_ptr<Window> w1(CreateWindow(HTGROWBOX));
  gfx::Point position = w1->bounds().origin();
  w1->set_minimum_size(gfx::Size(50, 50));
  EventGenerator generator;
  generator.MoveMouseToCenterOf(w1.get());
  generator.DragMouseBy(100, 100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have increased by 100,100.
  EXPECT_EQ(gfx::Size(200, 200), w1->bounds().size());

  // Shrink the wnidow by (-100, -100).
  generator.DragMouseBy(-100, -100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have decreased by 100,100.
  EXPECT_EQ(gfx::Size(100, 100), w1->bounds().size());

  // Enforce minimum size.
  generator.DragMouseBy(-60, -60);
  EXPECT_EQ(position, w1->bounds().origin());
  EXPECT_EQ(gfx::Size(50, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, Right) {
  scoped_ptr<Window> w1(CreateWindow(HTRIGHT));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have increased by 100,0.
  EXPECT_EQ(gfx::Size(200, 100), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, Bottom) {
  scoped_ptr<Window> w1(CreateWindow(HTBOTTOM));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have increased by 0,100.
  EXPECT_EQ(gfx::Size(100, 200), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, TopRight) {
  scoped_ptr<Window> w1(CreateWindow(HTTOPRIGHT));
  DragFromCenterBy(w1.get(), -50, 50);
  // Position should have been offset by 0,50.
  EXPECT_EQ(gfx::Point(0, 50), w1->bounds().origin());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, Top) {
  scoped_ptr<Window> w1(CreateWindow(HTTOP));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 0,50.
  EXPECT_EQ(gfx::Point(0, 50), w1->bounds().origin());
  // Size should have decreased by 0,50.
  EXPECT_EQ(gfx::Size(100, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, Left) {
  scoped_ptr<Window> w1(CreateWindow(HTLEFT));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 50,0.
  EXPECT_EQ(gfx::Point(50, 0), w1->bounds().origin());
  // Size should have decreased by 50,0.
  EXPECT_EQ(gfx::Size(50, 100), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, BottomLeft) {
  scoped_ptr<Window> w1(CreateWindow(HTBOTTOMLEFT));
  DragFromCenterBy(w1.get(), 50, -50);
  // Position should have been offset by 50,0.
  EXPECT_EQ(gfx::Point(50, 0), w1->bounds().origin());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, TopLeft) {
  scoped_ptr<Window> w1(CreateWindow(HTTOPLEFT));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 50,50.
  EXPECT_EQ(gfx::Point(50, 50), w1->bounds().origin());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, Client) {
  scoped_ptr<Window> w1(CreateWindow(HTCLIENT));
  gfx::Rect bounds = w1->bounds();
  DragFromCenterBy(w1.get(), 100, 100);
  // Neither position nor size should have changed.
  EXPECT_EQ(bounds, w1->bounds());
}

TEST_F(ToplevelWindowEventFilterTest, Maximized) {
  scoped_ptr<Window> w1(CreateWindow(HTCLIENT));
  gfx::Rect workarea = gfx::Screen::GetMonitorWorkAreaNearestWindow(w1.get());
  // Maximized window cannot be dragged.
  gfx::Rect original_bounds = w1->bounds();
  w1->Maximize();
  EXPECT_EQ(workarea, w1->bounds());
  DragFromCenterBy(w1.get(), 100, 100);
  EXPECT_EQ(workarea, w1->bounds());
  w1->Restore();
  EXPECT_EQ(original_bounds, w1->bounds());
}

TEST_F(ToplevelWindowEventFilterTest, Fullscreen) {
  scoped_ptr<Window> w1(CreateWindow(HTCLIENT));
  gfx::Rect monitor = gfx::Screen::GetMonitorAreaNearestWindow(w1.get());
  // Fullscreen window cannot be dragged.
  gfx::Rect original_bounds = w1->bounds();
  w1->Fullscreen();
  EXPECT_EQ(monitor, w1->bounds());
  DragFromCenterBy(w1.get(), 100, 100);
  EXPECT_EQ(monitor, w1->bounds());
  w1->Restore();
  EXPECT_EQ(original_bounds, w1->bounds());
}

}  // namespace test
}  // namespace aura
