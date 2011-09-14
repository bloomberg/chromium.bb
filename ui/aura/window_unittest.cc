// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/base/keycodes/keyboard_codes.h"

#if !defined(OS_WIN)
#include "ui/aura/hit_test.h"
#endif

namespace aura {
namespace internal {

namespace {

// WindowDelegate implementation with all methods stubbed out.
class WindowDelegateImpl : public WindowDelegate {
 public:
  WindowDelegateImpl() {}
  virtual ~WindowDelegateImpl() {}

  // Overriden from WindowDelegate:
  virtual void OnFocus() OVERRIDE {}
  virtual void OnBlur() OVERRIDE {}
  virtual bool OnKeyEvent(KeyEvent* event) OVERRIDE {
    return false;
  }
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return HTCLIENT;
  }
  virtual bool OnMouseEvent(MouseEvent* event) OVERRIDE { return false; }
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {}
  virtual void OnWindowDestroying() OVERRIDE {}
  virtual void OnWindowDestroyed() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowDelegateImpl);
};

// Used for verifying destruction methods are invoked.
class DestroyTrackingDelegateImpl : public WindowDelegateImpl {
 public:
  DestroyTrackingDelegateImpl()
      : destroying_count_(0),
        destroyed_count_(0),
        in_destroying_(false) {}

  void clear_destroying_count() { destroying_count_ = 0; }
  int destroying_count() const { return destroying_count_; }

  void clear_destroyed_count() { destroyed_count_ = 0; }
  int destroyed_count() const { return destroyed_count_; }

  bool in_destroying() const { return in_destroying_; }

  virtual void OnWindowDestroying() OVERRIDE {
    EXPECT_FALSE(in_destroying_);
    in_destroying_ = true;
    destroying_count_++;
  }

  virtual void OnWindowDestroyed() OVERRIDE {
    EXPECT_TRUE(in_destroying_);
    in_destroying_ = false;
    destroyed_count_++;
  }

 private:
  bool in_destroying_;
  int destroying_count_;
  int destroyed_count_;

  DISALLOW_COPY_AND_ASSIGN(DestroyTrackingDelegateImpl);
};

// Used to verify that when OnWindowDestroying is invoked the parent is also
// is in the process of being destroyed.
class ChildWindowDelegateImpl : public DestroyTrackingDelegateImpl {
 public:
  explicit ChildWindowDelegateImpl(
      DestroyTrackingDelegateImpl* parent_delegate)
      : parent_delegate_(parent_delegate) {
  }

  virtual void OnWindowDestroying() OVERRIDE {
    EXPECT_TRUE(parent_delegate_->in_destroying());
    DestroyTrackingDelegateImpl::OnWindowDestroying();
  }

 private:
  DestroyTrackingDelegateImpl* parent_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChildWindowDelegateImpl);
};

// A simple WindowDelegate implementation for these tests. It owns itself
// (deletes itself when the Window it is attached to is destroyed).
class TestWindowDelegate : public WindowDelegateImpl {
 public:
  TestWindowDelegate(SkColor color)
      : color_(color),
        last_key_code_(ui::VKEY_UNKNOWN) {
  }
  virtual ~TestWindowDelegate() {}

  ui::KeyboardCode last_key_code() const { return last_key_code_; }

  // Overridden from WindowDelegateImpl:
  virtual bool OnKeyEvent(KeyEvent* event) OVERRIDE {
    last_key_code_ = event->key_code();
    return true;
  }
  virtual void OnWindowDestroyed() OVERRIDE {
    delete this;
  }
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->AsCanvasSkia()->drawColor(color_, SkXfermode::kSrc_Mode);
  }

 private:
  SkColor color_;
  ui::KeyboardCode last_key_code_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegate);
};

class WindowTest : public testing::Test {
 public:
  WindowTest() : main_message_loop(MessageLoop::TYPE_UI) {
    aura::Desktop::GetInstance()->Show();
    aura::Desktop::GetInstance()->SetSize(gfx::Size(500, 500));
  }
  virtual ~WindowTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
  }

  virtual void TearDown() OVERRIDE {
  }

  Window* CreateTestWindow(SkColor color,
                           int id,
                           const gfx::Rect& bounds,
                           Window* parent) {
    return CreateTestWindowWithDelegate(new TestWindowDelegate(color),
                                        id, bounds, parent);
  }

  Window* CreateTestWindowWithDelegate(WindowDelegate* delegate,
                                       int id,
                                       const gfx::Rect& bounds,
                                       Window* parent) {
    Window* window = new Window(delegate);
    window->set_id(id);
    window->Init();
    window->SetBounds(bounds, 0);
    window->SetVisibility(Window::VISIBILITY_SHOWN);
    window->SetParent(parent);
    return window;
  }

  void RunPendingMessages() {
    MessageLoop message_loop(MessageLoop::TYPE_UI);
    MessageLoopForUI::current()->Run(NULL);
  }

 private:
  MessageLoop main_message_loop;

  DISALLOW_COPY_AND_ASSIGN(WindowTest);
};

}  // namespace

TEST_F(WindowTest, HitTest) {
  Window w1(new TestWindowDelegate(SK_ColorWHITE));
  w1.set_id(1);
  w1.Init();
  w1.SetBounds(gfx::Rect(10, 10, 50, 50), 0);
  w1.SetVisibility(Window::VISIBILITY_SHOWN);
  w1.SetParent(NULL);

  // Points are in the Window's coordinates.
  EXPECT_TRUE(w1.HitTest(gfx::Point(1, 1)));
  EXPECT_FALSE(w1.HitTest(gfx::Point(-1, -1)));

  // TODO(beng): clip Window to parent.
}

TEST_F(WindowTest, GetEventHandlerForPoint) {
  scoped_ptr<Window> w1(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(10, 10, 500, 500), NULL));
  scoped_ptr<Window> w11(
      CreateTestWindow(SK_ColorGREEN, 11, gfx::Rect(5, 5, 100, 100), w1.get()));
  scoped_ptr<Window> w111(
      CreateTestWindow(SK_ColorCYAN, 111, gfx::Rect(5, 5, 75, 75), w11.get()));
  scoped_ptr<Window> w1111(
      CreateTestWindow(SK_ColorRED, 1111, gfx::Rect(5, 5, 50, 50), w111.get()));
  scoped_ptr<Window> w12(
      CreateTestWindow(SK_ColorMAGENTA, 12, gfx::Rect(10, 420, 25, 25),
                       w1.get()));
  scoped_ptr<Window> w121(
      CreateTestWindow(SK_ColorYELLOW, 121, gfx::Rect(5, 5, 5, 5), w12.get()));
  scoped_ptr<Window> w13(
      CreateTestWindow(SK_ColorGRAY, 13, gfx::Rect(5, 470, 50, 50), w1.get()));

  Window* desktop = Desktop::GetInstance()->window();
  EXPECT_EQ(desktop, desktop->GetEventHandlerForPoint(gfx::Point(5, 5)));
  EXPECT_EQ(w1.get(), desktop->GetEventHandlerForPoint(gfx::Point(11, 11)));
  EXPECT_EQ(w11.get(), desktop->GetEventHandlerForPoint(gfx::Point(16, 16)));
  EXPECT_EQ(w111.get(), desktop->GetEventHandlerForPoint(gfx::Point(21, 21)));
  EXPECT_EQ(w1111.get(), desktop->GetEventHandlerForPoint(gfx::Point(26, 26)));
  EXPECT_EQ(w12.get(), desktop->GetEventHandlerForPoint(gfx::Point(21, 431)));
  EXPECT_EQ(w121.get(), desktop->GetEventHandlerForPoint(gfx::Point(26, 436)));
  EXPECT_EQ(w13.get(), desktop->GetEventHandlerForPoint(gfx::Point(26, 481)));
}

TEST_F(WindowTest, Focus) {
  scoped_ptr<Window> w1(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(10, 10, 500, 500), NULL));
  scoped_ptr<Window> w11(
      CreateTestWindow(SK_ColorGREEN, 11, gfx::Rect(5, 5, 100, 100), w1.get()));
  scoped_ptr<Window> w111(
      CreateTestWindow(SK_ColorCYAN, 111, gfx::Rect(5, 5, 75, 75), w11.get()));
  scoped_ptr<Window> w1111(
      CreateTestWindow(SK_ColorRED, 1111, gfx::Rect(5, 5, 50, 50), w111.get()));
  scoped_ptr<Window> w12(
      CreateTestWindow(SK_ColorMAGENTA, 12, gfx::Rect(10, 420, 25, 25),
                       w1.get()));
  TestWindowDelegate* w121delegate = new TestWindowDelegate(SK_ColorYELLOW);
  scoped_ptr<Window> w121(
      CreateTestWindowWithDelegate(w121delegate, 121, gfx::Rect(5, 5, 5, 5),
                                   w12.get()));
  scoped_ptr<Window> w13(
      CreateTestWindow(SK_ColorGRAY, 13, gfx::Rect(5, 470, 50, 50), w1.get()));

  // Click on a sub-window (w121) to focus it.
  Desktop* desktop = Desktop::GetInstance();
  gfx::Point click_point = w121->bounds().CenterPoint();
  Window::ConvertPointToWindow(w121->parent(), desktop->window(), &click_point);
  desktop->OnMouseEvent(
      MouseEvent(ui::ET_MOUSE_PRESSED, click_point, ui::EF_LEFT_BUTTON_DOWN));
  internal::FocusManager* focus_manager = w121->GetFocusManager();
  EXPECT_EQ(w121.get(), focus_manager->focused_window());

  // The key press should be sent to the focused sub-window.
  desktop->OnKeyEvent(KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_E, 0));
  EXPECT_EQ(ui::VKEY_E, w121delegate->last_key_code());
}

// Various destruction assertions.
TEST_F(WindowTest, DestroyTest) {
  DestroyTrackingDelegateImpl parent_delegate;
  ChildWindowDelegateImpl child_delegate(&parent_delegate);
  {
    scoped_ptr<Window> parent(
        CreateTestWindowWithDelegate(&parent_delegate, 0, gfx::Rect(), NULL));
    Window* child = CreateTestWindowWithDelegate(&child_delegate, 0,
                                                 gfx::Rect(), parent.get());
  }
  // Both the parent and child should have been destroyed.
  EXPECT_EQ(1, parent_delegate.destroying_count());
  EXPECT_EQ(1, parent_delegate.destroyed_count());
  EXPECT_EQ(1, child_delegate.destroying_count());
  EXPECT_EQ(1, child_delegate.destroyed_count());
}

}  // namespace internal
}  // namespace aura

