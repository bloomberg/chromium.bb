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
#include "ui/gfx/compositor/layer.h"
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

  // Overridden from WindowDelegate:
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {}
  virtual void OnFocus() OVERRIDE {}
  virtual void OnBlur() OVERRIDE {}
  virtual bool OnKeyEvent(KeyEvent* event) OVERRIDE {
    return false;
  }
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE {
    return NULL;
  }
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return HTCLIENT;
  }
  virtual bool OnMouseEvent(MouseEvent* event) OVERRIDE { return false; }
  virtual bool ShouldActivate(MouseEvent* event) OVERRIDE { return true; }
  virtual void OnActivated() OVERRIDE {}
  virtual void OnLostActive() OVERRIDE {}
  virtual void OnCaptureLost() OVERRIDE {}
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
  int destroying_count_;
  int destroyed_count_;
  bool in_destroying_;

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

// Used in verifying mouse capture.
class CaptureWindowDelegateImpl : public WindowDelegateImpl {
 public:
  explicit CaptureWindowDelegateImpl()
      : capture_lost_count_(0),
        mouse_event_count_(0) {
  }

  int capture_lost_count() const { return capture_lost_count_; }
  void set_capture_lost_count(int value) { capture_lost_count_ = value; }
  int mouse_event_count() const { return mouse_event_count_; }
  void set_mouse_event_count(int value) { mouse_event_count_ = value; }

  virtual bool OnMouseEvent(MouseEvent* event) OVERRIDE {
    mouse_event_count_++;
    return false;
  }
  virtual void OnCaptureLost() OVERRIDE {
    capture_lost_count_++;
  }

 private:
  int capture_lost_count_;
  int mouse_event_count_;

  DISALLOW_COPY_AND_ASSIGN(CaptureWindowDelegateImpl);
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
    Desktop::GetInstance()->Show();
    Desktop::GetInstance()->SetSize(gfx::Size(500, 500));
    if (!Desktop::GetInstance()->default_parent())
      Desktop::GetInstance()->CreateDefaultParentForTesting();
  }
  virtual ~WindowTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
  }

  virtual void TearDown() OVERRIDE {
  }

  Window* CreateTestWindowWithId(int id, Window* parent) {
    return CreateTestWindowWithDelegate(NULL, id, gfx::Rect(), parent);
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
    window->SetBounds(bounds);
    window->Show();
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

TEST_F(WindowTest, GetChildById) {
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, NULL));
  scoped_ptr<Window> w11(CreateTestWindowWithId(11, w1.get()));
  scoped_ptr<Window> w111(CreateTestWindowWithId(111, w11.get()));
  scoped_ptr<Window> w12(CreateTestWindowWithId(12, w1.get()));

  EXPECT_EQ(NULL, w1->GetChildById(57));
  EXPECT_EQ(w12.get(), w1->GetChildById(12));
  EXPECT_EQ(w111.get(), w1->GetChildById(111));
}

TEST_F(WindowTest, HitTest) {
  Window w1(new TestWindowDelegate(SK_ColorWHITE));
  w1.set_id(1);
  w1.Init();
  w1.SetBounds(gfx::Rect(10, 10, 50, 50));
  w1.Show();
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
  Desktop::GetInstance()->default_parent()->SetBounds(gfx::Rect(500, 500));
  EXPECT_EQ(NULL, desktop->GetEventHandlerForPoint(gfx::Point(5, 5)));
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
    CreateTestWindowWithDelegate(&child_delegate, 0, gfx::Rect(), parent.get());
  }
  // Both the parent and child should have been destroyed.
  EXPECT_EQ(1, parent_delegate.destroying_count());
  EXPECT_EQ(1, parent_delegate.destroyed_count());
  EXPECT_EQ(1, child_delegate.destroying_count());
  EXPECT_EQ(1, child_delegate.destroyed_count());
}

// Make sure MoveChildToFront moves both the window and layer to the front.
TEST_F(WindowTest, MoveChildToFront) {
  Window parent(NULL);
  parent.Init();
  Window child1(NULL);
  child1.Init();
  Window child2(NULL);
  child2.Init();

  child1.SetParent(&parent);
  child2.SetParent(&parent);
  ASSERT_EQ(2u, parent.children().size());
  EXPECT_EQ(&child1, parent.children()[0]);
  EXPECT_EQ(&child2, parent.children()[1]);
  ASSERT_EQ(2u, parent.layer()->children().size());
  EXPECT_EQ(child1.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child2.layer(), parent.layer()->children()[1]);

  parent.MoveChildToFront(&child1);
  ASSERT_EQ(2u, parent.children().size());
  EXPECT_EQ(&child1, parent.children()[1]);
  EXPECT_EQ(&child2, parent.children()[0]);
  ASSERT_EQ(2u, parent.layer()->children().size());
  EXPECT_EQ(child1.layer(), parent.layer()->children()[1]);
  EXPECT_EQ(child2.layer(), parent.layer()->children()[0]);
}

// Various destruction assertions.
TEST_F(WindowTest, CaptureTests) {
  Desktop* desktop = Desktop::GetInstance();
  CaptureWindowDelegateImpl delegate;
  scoped_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 20, 20), NULL));
  EXPECT_FALSE(window->HasCapture());

  // Do a capture.
  window->SetCapture();
  EXPECT_TRUE(window->HasCapture());
  EXPECT_EQ(0, delegate.capture_lost_count());

  desktop->OnMouseEvent(MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(50, 50),
                                   ui::EF_LEFT_BUTTON_DOWN));
  EXPECT_EQ(1, delegate.mouse_event_count());
  desktop->OnMouseEvent(MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(50, 50),
                                   ui::EF_LEFT_BUTTON_DOWN));
  EXPECT_EQ(2, delegate.mouse_event_count());
  delegate.set_mouse_event_count(0);

  window->ReleaseCapture();
  EXPECT_FALSE(window->HasCapture());
  EXPECT_EQ(1, delegate.capture_lost_count());

  desktop->OnMouseEvent(MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(50, 50),
                                   ui::EF_LEFT_BUTTON_DOWN));
  EXPECT_EQ(0, delegate.mouse_event_count());
}

// Verifies capture is reset when a window is destroyed.
TEST_F(WindowTest, ReleaseCaptureOnDestroy) {
  Desktop* desktop = Desktop::GetInstance();
  RootWindow* root = static_cast<RootWindow*>(desktop->window());
  CaptureWindowDelegateImpl delegate;
  scoped_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 20, 20), NULL));
  EXPECT_FALSE(window->HasCapture());

  // Do a capture.
  window->SetCapture();
  EXPECT_TRUE(window->HasCapture());

  // Destroy the window.
  window.reset();

  // Make sure the root doesn't reference the window anymore.
  EXPECT_EQ(NULL, root->mouse_pressed_handler());
  EXPECT_EQ(NULL, root->capture_window());
}

class MouseEnterExitWindowDelegate : public WindowDelegateImpl {
 public:
  MouseEnterExitWindowDelegate() : entered_(false), exited_(false) {}

  virtual bool OnMouseEvent(MouseEvent* event) OVERRIDE {
    switch (event->type()) {
      case ui::ET_MOUSE_ENTERED:
        entered_ = true;
        break;
      case ui::ET_MOUSE_EXITED:
        exited_ = true;
        break;
      default:
        break;
    }
    return false;
  }

  bool entered() const { return entered_; }
  bool exited() const { return exited_; }

 private:
  bool entered_;
  bool exited_;

  DISALLOW_COPY_AND_ASSIGN(MouseEnterExitWindowDelegate);
};


// Verifies that the WindowDelegate receives MouseExit and MouseEnter events for
// mouse transitions from window to window.
TEST_F(WindowTest, MouseEnterExit) {
  Desktop* desktop = Desktop::GetInstance();

  MouseEnterExitWindowDelegate d1;
  scoped_ptr<Window> w1(
      CreateTestWindowWithDelegate(&d1, 1, gfx::Rect(10, 10, 50, 50), NULL));
  MouseEnterExitWindowDelegate d2;
  scoped_ptr<Window> w2(
      CreateTestWindowWithDelegate(&d2, 2, gfx::Rect(70, 70, 50, 50), NULL));

  gfx::Point move_point = w1->bounds().CenterPoint();
  Window::ConvertPointToWindow(w1->parent(), desktop->window(), &move_point);
  desktop->OnMouseEvent(MouseEvent(ui::ET_MOUSE_MOVED, move_point, 0));

  EXPECT_TRUE(d1.entered());
  EXPECT_FALSE(d1.exited());
  EXPECT_FALSE(d2.entered());
  EXPECT_FALSE(d2.exited());

  move_point = w2->bounds().CenterPoint();
  Window::ConvertPointToWindow(w2->parent(), desktop->window(), &move_point);
  desktop->OnMouseEvent(MouseEvent(ui::ET_MOUSE_MOVED, move_point, 0));

  EXPECT_TRUE(d1.entered());
  EXPECT_TRUE(d1.exited());
  EXPECT_TRUE(d2.entered());
  EXPECT_FALSE(d2.exited());
}

class ActivateWindowDelegate : public WindowDelegateImpl {
 public:
  ActivateWindowDelegate()
      : activate_(true),
        activated_count_(0),
        lost_active_count_(0),
        should_activate_count_(0) {
  }

  void set_activate(bool v) { activate_ = v; }
  int activated_count() const { return activated_count_; }
  int lost_active_count() const { return lost_active_count_; }
  int should_activate_count() const { return should_activate_count_; }
  void Clear() {
    activated_count_ = lost_active_count_ = should_activate_count_ = 0;
  }

  virtual bool ShouldActivate(MouseEvent* event) OVERRIDE {
    should_activate_count_++;
    return activate_;
  }
  virtual void OnActivated() OVERRIDE {
    activated_count_++;
  }
  virtual void OnLostActive() OVERRIDE {
    lost_active_count_++;
  }

 private:
  bool activate_;
  int activated_count_;
  int lost_active_count_;
  int should_activate_count_;

  DISALLOW_COPY_AND_ASSIGN(ActivateWindowDelegate);
};

// Various assertion testing for activating windows.
TEST_F(WindowTest, ActivateOnMouse) {
  Desktop* desktop = Desktop::GetInstance();

  ActivateWindowDelegate d1;
  scoped_ptr<Window> w1(
      CreateTestWindowWithDelegate(&d1, 1, gfx::Rect(10, 10, 50, 50), NULL));
  ActivateWindowDelegate d2;
  scoped_ptr<Window> w2(
      CreateTestWindowWithDelegate(&d2, 2, gfx::Rect(70, 70, 50, 50), NULL));
  internal::FocusManager* focus_manager = w1->GetFocusManager();

  d1.Clear();
  d2.Clear();

  // Activate window1.
  desktop->SetActiveWindow(w1.get(), NULL);
  EXPECT_EQ(w1.get(), desktop->active_window());
  EXPECT_EQ(w1.get(), focus_manager->focused_window());
  EXPECT_EQ(1, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
  d1.Clear();

  // Click on window2.
  gfx::Point press_point = w2->bounds().CenterPoint();
  Window::ConvertPointToWindow(w2->parent(), desktop->window(), &press_point);
  desktop->OnMouseEvent(MouseEvent(ui::ET_MOUSE_PRESSED, press_point, 0));
  desktop->OnMouseEvent(MouseEvent(ui::ET_MOUSE_RELEASED, press_point, 0));

  // Window2 should have become active.
  EXPECT_EQ(w2.get(), desktop->active_window());
  EXPECT_EQ(w2.get(), focus_manager->focused_window());
  EXPECT_EQ(0, d1.activated_count());
  EXPECT_EQ(1, d1.lost_active_count());
  EXPECT_EQ(1, d2.activated_count());
  EXPECT_EQ(0, d2.lost_active_count());
  d1.Clear();
  d2.Clear();

  // Click back on window1, but set it up so w1 doesn't activate on click.
  press_point = w1->bounds().CenterPoint();
  Window::ConvertPointToWindow(w1->parent(), desktop->window(), &press_point);
  d1.set_activate(false);
  desktop->OnMouseEvent(MouseEvent(ui::ET_MOUSE_PRESSED, press_point, 0));
  desktop->OnMouseEvent(MouseEvent(ui::ET_MOUSE_RELEASED, press_point, 0));

  // Window2 should still be active and focused.
  EXPECT_EQ(w2.get(), desktop->active_window());
  EXPECT_EQ(w2.get(), focus_manager->focused_window());
  EXPECT_EQ(0, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
  EXPECT_EQ(0, d2.activated_count());
  EXPECT_EQ(0, d2.lost_active_count());
  d1.Clear();
  d2.Clear();

  // Destroy window2, this should make window1 active.
  d1.set_activate(true);
  w2.reset();
  EXPECT_EQ(0, d2.activated_count());
  EXPECT_EQ(0, d2.lost_active_count());
  EXPECT_EQ(w1.get(), desktop->active_window());
  EXPECT_EQ(w1.get(), focus_manager->focused_window());
  EXPECT_EQ(1, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
}

// Creates a window with a delegate (w111) that can handle events at a lower
// z-index than a window without a delegate (w12). w12 is sized to fill the
// entire bounds of the container. This test verifies that
// GetEventHandlerForPoint() skips w12 even though its bounds contain the event,
// because it has no children that can handle the event and it has no delegate
// allowing it to handle the event itself.
TEST_F(WindowTest, GetEventHandlerForPoint_NoDelegate) {
  WindowDelegateImpl d111;
  scoped_ptr<Window> w1(CreateTestWindowWithDelegate(NULL, 1,
      gfx::Rect(0, 0, 500, 500), NULL));
  scoped_ptr<Window> w11(CreateTestWindowWithDelegate(NULL, 11,
      gfx::Rect(0, 0, 500, 500), w1.get()));
  scoped_ptr<Window> w111(CreateTestWindowWithDelegate(&d111, 1,
      gfx::Rect(50, 50, 450, 450), w11.get()));
  scoped_ptr<Window> w12(CreateTestWindowWithDelegate(NULL, 12,
      gfx::Rect(0, 0, 500, 500), w1.get()));

  gfx::Point target_point = w111->bounds().CenterPoint();
  EXPECT_EQ(w111.get(), w1->GetEventHandlerForPoint(target_point));
}

}  // namespace internal
}  // namespace aura
