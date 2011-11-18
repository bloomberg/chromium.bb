// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_stacking_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura_shell/toplevel_window_event_filter.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/screen.h"

#if defined(OS_WIN)
// Windows headers define macros for these function names which screw with us.
#if defined(CreateWindow)
#undef CreateWindow
#endif
#endif


namespace aura_shell {
namespace test {

namespace {

// A simple window delegate that returns the specified hit-test code when
// requested and applies a minimum size constraint if there is one.
class TestWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  explicit TestWindowDelegate(int hittest_code)
      : hittest_code_(hittest_code) {
  }
  virtual ~TestWindowDelegate() {}

  void set_min_size(const gfx::Size& size) {
    min_size_ = size;
  }

 private:
  // Overridden from aura::Test::TestWindowDelegate:
  virtual void OnBoundsChanging(gfx::Rect* new_bounds) OVERRIDE {
    if (!min_size_.IsEmpty()) {
      new_bounds->set_width(std::max(min_size_.width(),
                                     new_bounds->width()));
      new_bounds->set_height(std::max(min_size_.height(),
                                      new_bounds->height()));
    }
  }
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return hittest_code_;
  }
  virtual void OnWindowDestroyed() OVERRIDE {
    delete this;
  }

  int hittest_code_;
  gfx::Size min_size_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegate);
};

class ToplevelWindowEventFilterTest : public aura::test::AuraTestBase {
 public:
  ToplevelWindowEventFilterTest() {}
  virtual ~ToplevelWindowEventFilterTest() {}

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    aura::Window* default_container =
        static_cast<aura::test::TestStackingClient*>(
            aura::Desktop::GetInstance()->stacking_client())->
                default_container();
    default_container->SetEventFilter(
        new ToplevelWindowEventFilter(default_container));
  }

 protected:
  aura::Window* CreateWindow(int hittest_code) {
    TestWindowDelegate* d1 = new TestWindowDelegate(hittest_code);
    aura::Window* w1 = new aura::Window(d1);
    w1->set_id(1);
    w1->Init(ui::Layer::LAYER_HAS_TEXTURE);
    w1->SetParent(NULL);
    w1->SetBounds(gfx::Rect(0, 0, 100, 100));
    w1->Show();
    return w1;
  }

  void DragFromCenterBy(aura::Window* window, int dx, int dy) {
    aura::test::EventGenerator generator(window);
    generator.DragMouseBy(dx, dy);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ToplevelWindowEventFilterTest);
};

}

TEST_F(ToplevelWindowEventFilterTest, Caption) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTCAPTION));
  gfx::Size size = w1->bounds().size();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should have been offset by 100,100.
  EXPECT_EQ(gfx::Point(100, 100), w1->bounds().origin());
  // Size should not have.
  EXPECT_EQ(size, w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, BottomRight) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTBOTTOMRIGHT));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have increased by 100,100.
  EXPECT_EQ(gfx::Size(200, 200), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, GrowBox) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTGROWBOX));
  TestWindowDelegate* window_delegate =
    static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_min_size(gfx::Size(50, 50));

  gfx::Point position = w1->bounds().origin();
  aura::test::EventGenerator generator;
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
  scoped_ptr<aura::Window> w1(CreateWindow(HTRIGHT));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have increased by 100,0.
  EXPECT_EQ(gfx::Size(200, 100), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, Bottom) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTBOTTOM));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have increased by 0,100.
  EXPECT_EQ(gfx::Size(100, 200), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, TopRight) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTTOPRIGHT));
  DragFromCenterBy(w1.get(), -50, 50);
  // Position should have been offset by 0,50.
  EXPECT_EQ(gfx::Point(0, 50), w1->bounds().origin());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, Top) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTTOP));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 0,50.
  EXPECT_EQ(gfx::Point(0, 50), w1->bounds().origin());
  // Size should have decreased by 0,50.
  EXPECT_EQ(gfx::Size(100, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, Left) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTLEFT));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 50,0.
  EXPECT_EQ(gfx::Point(50, 0), w1->bounds().origin());
  // Size should have decreased by 50,0.
  EXPECT_EQ(gfx::Size(50, 100), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, BottomLeft) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTBOTTOMLEFT));
  DragFromCenterBy(w1.get(), 50, -50);
  // Position should have been offset by 50,0.
  EXPECT_EQ(gfx::Point(50, 0), w1->bounds().origin());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, TopLeft) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTTOPLEFT));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 50,50.
  EXPECT_EQ(gfx::Point(50, 50), w1->bounds().origin());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, Client) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTCLIENT));
  gfx::Rect bounds = w1->bounds();
  DragFromCenterBy(w1.get(), 100, 100);
  // Neither position nor size should have changed.
  EXPECT_EQ(bounds, w1->bounds());
}

}  // namespace test
}  // namespace aura
