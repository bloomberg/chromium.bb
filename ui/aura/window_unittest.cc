// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/stacking_client.h"
#include "ui/aura/desktop.h"
#include "ui/aura/desktop_observer.h"
#include "ui/aura/event.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/screen.h"

namespace aura {
namespace test {

typedef AuraTestBase WindowTest;

namespace {

// Used for verifying destruction methods are invoked.
class DestroyTrackingDelegateImpl : public TestWindowDelegate {
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
class CaptureWindowDelegateImpl : public TestWindowDelegate {
 public:
  explicit CaptureWindowDelegateImpl()
      : capture_lost_count_(0),
        mouse_event_count_(0),
        touch_event_count_(0) {
  }

  int capture_lost_count() const { return capture_lost_count_; }
  void set_capture_lost_count(int value) { capture_lost_count_ = value; }
  int mouse_event_count() const { return mouse_event_count_; }
  void set_mouse_event_count(int value) { mouse_event_count_ = value; }
  int touch_event_count() const { return touch_event_count_; }
  void set_touch_event_count(int value) { touch_event_count_ = value; }

  virtual bool OnMouseEvent(MouseEvent* event) OVERRIDE {
    mouse_event_count_++;
    return false;
  }
  virtual ui::TouchStatus OnTouchEvent(TouchEvent* event) OVERRIDE {
    touch_event_count_++;
    return ui::TOUCH_STATUS_UNKNOWN;
  }
  virtual void OnCaptureLost() OVERRIDE {
    capture_lost_count_++;
  }

 private:
  int capture_lost_count_;
  int mouse_event_count_;
  int touch_event_count_;

  DISALLOW_COPY_AND_ASSIGN(CaptureWindowDelegateImpl);
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

// Make sure that Window::Contains correctly handles children, grandchildren,
// and not containing NULL or parents.
TEST_F(WindowTest, Contains) {
  Window parent(NULL);
  parent.Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
  Window child1(NULL);
  child1.Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
  Window child2(NULL);
  child2.Init(ui::Layer::LAYER_HAS_NO_TEXTURE);

  child1.SetParent(&parent);
  child2.SetParent(&child1);

  EXPECT_TRUE(parent.Contains(&parent));
  EXPECT_TRUE(parent.Contains(&child1));
  EXPECT_TRUE(parent.Contains(&child2));

  EXPECT_FALSE(parent.Contains(NULL));
  EXPECT_FALSE(child1.Contains(&parent));
  EXPECT_FALSE(child2.Contains(&child1));
}

TEST_F(WindowTest, ConvertPointToWindow) {
  // Window::ConvertPointToWindow is mostly identical to
  // Layer::ConvertPointToLayer, except NULL values for |source| are permitted,
  // in which case the function just returns.
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, NULL));
  gfx::Point reference_point(100, 100);
  gfx::Point test_point = reference_point;
  Window::ConvertPointToWindow(NULL, w1.get(), &test_point);
  EXPECT_EQ(reference_point, test_point);
}

TEST_F(WindowTest, HitTest) {
  Window w1(new ColorTestWindowDelegate(SK_ColorWHITE));
  w1.set_id(1);
  w1.Init(ui::Layer::LAYER_HAS_TEXTURE);
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

  Window* root = Desktop::GetInstance();
  w1->parent()->SetBounds(gfx::Rect(500, 500));
  EXPECT_EQ(NULL, root->GetEventHandlerForPoint(gfx::Point(5, 5)));
  EXPECT_EQ(w1.get(), root->GetEventHandlerForPoint(gfx::Point(11, 11)));
  EXPECT_EQ(w11.get(), root->GetEventHandlerForPoint(gfx::Point(16, 16)));
  EXPECT_EQ(w111.get(), root->GetEventHandlerForPoint(gfx::Point(21, 21)));
  EXPECT_EQ(w1111.get(), root->GetEventHandlerForPoint(gfx::Point(26, 26)));
  EXPECT_EQ(w12.get(), root->GetEventHandlerForPoint(gfx::Point(21, 431)));
  EXPECT_EQ(w121.get(), root->GetEventHandlerForPoint(gfx::Point(26, 436)));
  EXPECT_EQ(w13.get(), root->GetEventHandlerForPoint(gfx::Point(26, 481)));
}

TEST_F(WindowTest, GetTopWindowContainingPoint) {
  Window* root = Desktop::GetInstance();
  root->SetBounds(gfx::Rect(0, 0, 300, 300));

  scoped_ptr<Window> w1(
      CreateTestWindow(SK_ColorWHITE, 1, gfx::Rect(10, 10, 100, 100), NULL));
  scoped_ptr<Window> w11(
      CreateTestWindow(SK_ColorGREEN, 11, gfx::Rect(0, 0, 120, 120), w1.get()));

  scoped_ptr<Window> w2(
      CreateTestWindow(SK_ColorRED, 2, gfx::Rect(5, 5, 55, 55), NULL));

  scoped_ptr<Window> w3(
      CreateTestWindowWithDelegate(
          NULL, 3, gfx::Rect(200, 200, 100, 100), NULL));
  scoped_ptr<Window> w31(
      CreateTestWindow(SK_ColorCYAN, 31, gfx::Rect(0, 0, 50, 50), w3.get()));
  scoped_ptr<Window> w311(
      CreateTestWindow(SK_ColorBLUE, 311, gfx::Rect(0, 0, 10, 10), w31.get()));

  // The stop-event-propagation flag shouldn't have any effect on the behavior
  // of this method.
  w3->set_stops_event_propagation(true);

  EXPECT_EQ(NULL, root->GetTopWindowContainingPoint(gfx::Point(0, 0)));
  EXPECT_EQ(w2.get(), root->GetTopWindowContainingPoint(gfx::Point(5, 5)));
  EXPECT_EQ(w2.get(), root->GetTopWindowContainingPoint(gfx::Point(10, 10)));
  EXPECT_EQ(w2.get(), root->GetTopWindowContainingPoint(gfx::Point(59, 59)));
  EXPECT_EQ(w1.get(), root->GetTopWindowContainingPoint(gfx::Point(60, 60)));
  EXPECT_EQ(w1.get(), root->GetTopWindowContainingPoint(gfx::Point(109, 109)));
  EXPECT_EQ(NULL, root->GetTopWindowContainingPoint(gfx::Point(110, 110)));
  EXPECT_EQ(w31.get(), root->GetTopWindowContainingPoint(gfx::Point(200, 200)));
  EXPECT_EQ(w31.get(), root->GetTopWindowContainingPoint(gfx::Point(220, 220)));
  EXPECT_EQ(NULL, root->GetTopWindowContainingPoint(gfx::Point(260, 260)));
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

// Make sure StackChildAtTop moves both the window and layer to the front.
TEST_F(WindowTest, StackChildAtTop) {
  Window parent(NULL);
  parent.Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
  Window child1(NULL);
  child1.Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
  Window child2(NULL);
  child2.Init(ui::Layer::LAYER_HAS_NO_TEXTURE);

  child1.SetParent(&parent);
  child2.SetParent(&parent);
  ASSERT_EQ(2u, parent.children().size());
  EXPECT_EQ(&child1, parent.children()[0]);
  EXPECT_EQ(&child2, parent.children()[1]);
  ASSERT_EQ(2u, parent.layer()->children().size());
  EXPECT_EQ(child1.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child2.layer(), parent.layer()->children()[1]);

  parent.StackChildAtTop(&child1);
  ASSERT_EQ(2u, parent.children().size());
  EXPECT_EQ(&child1, parent.children()[1]);
  EXPECT_EQ(&child2, parent.children()[0]);
  ASSERT_EQ(2u, parent.layer()->children().size());
  EXPECT_EQ(child1.layer(), parent.layer()->children()[1]);
  EXPECT_EQ(child2.layer(), parent.layer()->children()[0]);
}

// Various assertions for StackChildAbove.
TEST_F(WindowTest, StackChildAbove) {
  Window parent(NULL);
  parent.Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
  Window child1(NULL);
  child1.Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
  Window child2(NULL);
  child2.Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
  Window child3(NULL);
  child3.Init(ui::Layer::LAYER_HAS_NO_TEXTURE);

  child1.SetParent(&parent);
  child2.SetParent(&parent);

  // Move 1 in front of 2.
  parent.StackChildAbove(&child1, &child2);
  ASSERT_EQ(2u, parent.children().size());
  EXPECT_EQ(&child2, parent.children()[0]);
  EXPECT_EQ(&child1, parent.children()[1]);
  ASSERT_EQ(2u, parent.layer()->children().size());
  EXPECT_EQ(child2.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child1.layer(), parent.layer()->children()[1]);

  // Add 3, resulting in order [2, 1, 3], then move 2 in front of 1, resulting
  // in [1, 2, 3].
  child3.SetParent(&parent);
  parent.StackChildAbove(&child2, &child1);
  ASSERT_EQ(3u, parent.children().size());
  EXPECT_EQ(&child1, parent.children()[0]);
  EXPECT_EQ(&child2, parent.children()[1]);
  EXPECT_EQ(&child3, parent.children()[2]);
  ASSERT_EQ(3u, parent.layer()->children().size());
  EXPECT_EQ(child1.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child2.layer(), parent.layer()->children()[1]);
  EXPECT_EQ(child3.layer(), parent.layer()->children()[2]);

  // Move 1 in front of 3, resulting in [2, 3, 1].
  parent.StackChildAbove(&child1, &child3);
  ASSERT_EQ(3u, parent.children().size());
  EXPECT_EQ(&child2, parent.children()[0]);
  EXPECT_EQ(&child3, parent.children()[1]);
  EXPECT_EQ(&child1, parent.children()[2]);
  ASSERT_EQ(3u, parent.layer()->children().size());
  EXPECT_EQ(child2.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child3.layer(), parent.layer()->children()[1]);
  EXPECT_EQ(child1.layer(), parent.layer()->children()[2]);

  // Moving 1 in front of 2 should lower it, resulting in [2, 1, 3].
  parent.StackChildAbove(&child1, &child2);
  ASSERT_EQ(3u, parent.children().size());
  EXPECT_EQ(&child2, parent.children()[0]);
  EXPECT_EQ(&child1, parent.children()[1]);
  EXPECT_EQ(&child3, parent.children()[2]);
  ASSERT_EQ(3u, parent.layer()->children().size());
  EXPECT_EQ(child2.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child1.layer(), parent.layer()->children()[1]);
  EXPECT_EQ(child3.layer(), parent.layer()->children()[2]);
}

// Various destruction assertions.
TEST_F(WindowTest, CaptureTests) {
  aura::Desktop* desktop = aura::Desktop::GetInstance();
  CaptureWindowDelegateImpl delegate;
  scoped_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 20, 20), NULL));
  EXPECT_FALSE(window->HasCapture());

  // Do a capture.
  window->SetCapture();
  EXPECT_TRUE(window->HasCapture());
  EXPECT_EQ(0, delegate.capture_lost_count());
  EventGenerator generator(gfx::Point(50, 50));
  generator.PressLeftButton();
  EXPECT_EQ(1, delegate.mouse_event_count());
  generator.ReleaseLeftButton();

  EXPECT_EQ(2, delegate.mouse_event_count());
  delegate.set_mouse_event_count(0);

  TouchEvent touchev(ui::ET_TOUCH_PRESSED, gfx::Point(50, 50), 0);
  desktop->DispatchTouchEvent(&touchev);
  EXPECT_EQ(1, delegate.touch_event_count());
  delegate.set_touch_event_count(0);

  window->ReleaseCapture();
  EXPECT_FALSE(window->HasCapture());
  EXPECT_EQ(1, delegate.capture_lost_count());

  generator.PressLeftButton();
  EXPECT_EQ(0, delegate.mouse_event_count());

  desktop->DispatchTouchEvent(&touchev);
  EXPECT_EQ(0, delegate.touch_event_count());

  // Removing the capture window from parent should reset the capture window
  // in the desktop.
  window->SetCapture();
  EXPECT_EQ(window.get(), desktop->capture_window());
  window->parent()->RemoveChild(window.get());
  EXPECT_FALSE(window->HasCapture());
  EXPECT_EQ(NULL, desktop->capture_window());
}

// Verifies capture is reset when a window is destroyed.
TEST_F(WindowTest, ReleaseCaptureOnDestroy) {
  Desktop* desktop = Desktop::GetInstance();
  CaptureWindowDelegateImpl delegate;
  scoped_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 20, 20), NULL));
  EXPECT_FALSE(window->HasCapture());

  // Do a capture.
  window->SetCapture();
  EXPECT_TRUE(window->HasCapture());

  // Destroy the window.
  window.reset();

  // Make sure the desktop doesn't reference the window anymore.
  EXPECT_EQ(NULL, desktop->mouse_pressed_handler());
  EXPECT_EQ(NULL, desktop->capture_window());
}

TEST_F(WindowTest, GetScreenBounds) {
  scoped_ptr<Window> viewport(CreateTestWindowWithBounds(
      gfx::Rect(0, 0, 300, 300), NULL));
  scoped_ptr<Window> child(CreateTestWindowWithBounds(
      gfx::Rect(0, 0, 100, 100), viewport.get()));
  // Sanity check.
  EXPECT_EQ("0,0 100x100", child->GetScreenBounds().ToString());

  // The |child| window's screen bounds should move along with the |viewport|.
  viewport->SetBounds(gfx::Rect(-100, -100, 300, 300));
  EXPECT_EQ("-100,-100 100x100", child->GetScreenBounds().ToString());

  // The |child| window is moved to the 0,0 in screen coordinates.
  // |GetScreenBounds()| should return 0,0.
  child->SetBounds(gfx::Rect(100, 100, 100, 100));
  EXPECT_EQ("0,0 100x100", child->GetScreenBounds().ToString());
}

class MouseEnterExitWindowDelegate : public TestWindowDelegate {
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
  Window::ConvertPointToWindow(w1->parent(), desktop, &move_point);
  MouseEvent mouseev1(ui::ET_MOUSE_MOVED, move_point, 0);
  desktop->DispatchMouseEvent(&mouseev1);

  EXPECT_TRUE(d1.entered());
  EXPECT_FALSE(d1.exited());
  EXPECT_FALSE(d2.entered());
  EXPECT_FALSE(d2.exited());

  move_point = w2->bounds().CenterPoint();
  Window::ConvertPointToWindow(w2->parent(), desktop, &move_point);
  MouseEvent mouseev2(ui::ET_MOUSE_MOVED, move_point, 0);
  desktop->DispatchMouseEvent(&mouseev2);

  EXPECT_TRUE(d1.entered());
  EXPECT_TRUE(d1.exited());
  EXPECT_TRUE(d2.entered());
  EXPECT_FALSE(d2.exited());
}

namespace {

class ActiveWindowDelegate : public TestWindowDelegate {
 public:
  ActiveWindowDelegate() : window_(NULL), was_active_(false), hit_count_(0) {
  }

  void set_window(Window* window) { window_ = window; }

  // Number of times OnLostActive has been invoked.
  int hit_count() const { return hit_count_; }

  // Was the window active from the first call to OnLostActive?
  bool was_active() const { return was_active_; }

  virtual void OnLostActive() OVERRIDE {
    if (hit_count_++ == 0)
      was_active_ = window_->IsActive();
  }

 private:
  Window* window_;

  // See description above getters for details on these.
  bool was_active_;
  int hit_count_;

  DISALLOW_COPY_AND_ASSIGN(ActiveWindowDelegate);
};

}  // namespace

// Verifies that when WindowDelegate::OnLostActive is invoked the window is not
// active.
TEST_F(WindowTest, NotActiveInLostActive) {
  Desktop* desktop = Desktop::GetInstance();

  ActiveWindowDelegate d1;
  scoped_ptr<Window> w1(
      CreateTestWindowWithDelegate(&d1, 1, gfx::Rect(10, 10, 50, 50), NULL));
  d1.set_window(w1.get());
  scoped_ptr<Window> w2(
      CreateTestWindowWithDelegate(NULL, 1, gfx::Rect(10, 10, 50, 50), NULL));

  // Activate w1.
  desktop->SetActiveWindow(w1.get(), NULL);
  EXPECT_EQ(w1.get(), desktop->active_window());

  // Should not have gotten a OnLostActive yet.
  EXPECT_EQ(0, d1.hit_count());

  // SetActiveWindow(NULL) should not change the active window.
  desktop->SetActiveWindow(NULL, NULL);
  EXPECT_TRUE(desktop->active_window() == w1.get());

  // Now activate another window.
  desktop->SetActiveWindow(w2.get(), NULL);

  // Should have gotten OnLostActive and w1 should not have been active at that
  // time.
  EXPECT_EQ(1, d1.hit_count());
  EXPECT_FALSE(d1.was_active());
}

// Creates a window with a delegate (w111) that can handle events at a lower
// z-index than a window without a delegate (w12). w12 is sized to fill the
// entire bounds of the container. This test verifies that
// GetEventHandlerForPoint() skips w12 even though its bounds contain the event,
// because it has no children that can handle the event and it has no delegate
// allowing it to handle the event itself.
TEST_F(WindowTest, GetEventHandlerForPoint_NoDelegate) {
  TestWindowDelegate d111;
  scoped_ptr<Window> w1(CreateTestWindowWithDelegate(NULL, 1,
      gfx::Rect(0, 0, 500, 500), NULL));
  scoped_ptr<Window> w11(CreateTestWindowWithDelegate(NULL, 11,
      gfx::Rect(0, 0, 500, 500), w1.get()));
  scoped_ptr<Window> w111(CreateTestWindowWithDelegate(&d111, 111,
      gfx::Rect(50, 50, 450, 450), w11.get()));
  scoped_ptr<Window> w12(CreateTestWindowWithDelegate(NULL, 12,
      gfx::Rect(0, 0, 500, 500), w1.get()));

  gfx::Point target_point = w111->bounds().CenterPoint();
  EXPECT_EQ(w111.get(), w1->GetEventHandlerForPoint(target_point));
}

class VisibilityWindowDelegate : public TestWindowDelegate {
 public:
  VisibilityWindowDelegate()
      : shown_(0),
        hidden_(0) {
  }

  int shown() const { return shown_; }
  int hidden() const { return hidden_; }
  void Clear() {
    shown_ = 0;
    hidden_ = 0;
  }

  virtual void OnWindowVisibilityChanged(bool visible) OVERRIDE {
    if (visible)
      shown_++;
    else
      hidden_++;
  }

 private:
  int shown_;
  int hidden_;

  DISALLOW_COPY_AND_ASSIGN(VisibilityWindowDelegate);
};

// Verifies show/hide propagate correctly to children and the layer.
TEST_F(WindowTest, Visibility) {
  VisibilityWindowDelegate d;
  scoped_ptr<Window> w1(CreateTestWindowWithDelegate(&d, 1, gfx::Rect(), NULL));
  scoped_ptr<Window> w2(CreateTestWindowWithId(2, w1.get()));
  scoped_ptr<Window> w3(CreateTestWindowWithId(3, w2.get()));

  // Create shows all the windows.
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(w3->IsVisible());
  EXPECT_EQ(1, d.shown());

  d.Clear();
  w1->Hide();
  EXPECT_FALSE(w1->IsVisible());
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_FALSE(w3->IsVisible());
  EXPECT_EQ(1, d.hidden());
  EXPECT_EQ(0, d.shown());

  w2->Show();
  EXPECT_FALSE(w1->IsVisible());
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_FALSE(w3->IsVisible());

  w3->Hide();
  EXPECT_FALSE(w1->IsVisible());
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_FALSE(w3->IsVisible());

  d.Clear();
  w1->Show();
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_FALSE(w3->IsVisible());
  EXPECT_EQ(0, d.hidden());
  EXPECT_EQ(1, d.shown());

  w3->Show();
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(w3->IsVisible());
}

// When set_consume_events() is called with |true| for a Window, that Window
// should make sure that none behind it in the z-order see events if it has
// children. If it does not have children, event targeting works as usual.
TEST_F(WindowTest, StopsEventPropagation) {
  TestWindowDelegate d11;
  TestWindowDelegate d111;
  TestWindowDelegate d121;
  scoped_ptr<Window> w1(CreateTestWindowWithDelegate(NULL, 1,
      gfx::Rect(0, 0, 500, 500), NULL));
  scoped_ptr<Window> w11(CreateTestWindowWithDelegate(&d11, 11,
      gfx::Rect(0, 0, 500, 500), w1.get()));
  scoped_ptr<Window> w111(CreateTestWindowWithDelegate(&d111, 111,
      gfx::Rect(50, 50, 450, 450), w11.get()));
  scoped_ptr<Window> w12(CreateTestWindowWithDelegate(NULL, 12,
      gfx::Rect(0, 0, 500, 500), w1.get()));
  scoped_ptr<Window> w121(CreateTestWindowWithDelegate(&d121, 121,
      gfx::Rect(150, 150, 50, 50), NULL));

  w12->set_stops_event_propagation(true);
  EXPECT_EQ(w11.get(), w1->GetEventHandlerForPoint(gfx::Point(10, 10)));

  EXPECT_TRUE(w111->CanFocus());
  w111->Focus();
  EXPECT_EQ(w111.get(), w1->GetFocusManager()->GetFocusedWindow());

  w12->AddChild(w121.get());

  EXPECT_EQ(NULL, w1->GetEventHandlerForPoint(gfx::Point(10, 10)));
  EXPECT_EQ(w121.get(), w1->GetEventHandlerForPoint(gfx::Point(175, 175)));

  // It should be possible to focus w121 since it is at or above the
  // consumes_events_ window.
  EXPECT_TRUE(w121->CanFocus());
  w121->Focus();
  EXPECT_EQ(w121.get(), w1->GetFocusManager()->GetFocusedWindow());

  // An attempt to focus 111 should be ignored and w121 should retain focus,
  // since a consumes_events_ window with a child is in the z-index above w111.
  EXPECT_FALSE(w111->CanFocus());
  w111->Focus();
  EXPECT_EQ(w121.get(), w1->GetFocusManager()->GetFocusedWindow());
}

TEST_F(WindowTest, IgnoreEventsTest) {
  TestWindowDelegate d11;
  TestWindowDelegate d12;
  TestWindowDelegate d111;
  TestWindowDelegate d121;
  scoped_ptr<Window> w1(CreateTestWindowWithDelegate(NULL, 1,
      gfx::Rect(0, 0, 500, 500), NULL));
  scoped_ptr<Window> w11(CreateTestWindowWithDelegate(&d11, 11,
      gfx::Rect(0, 0, 500, 500), w1.get()));
  scoped_ptr<Window> w111(CreateTestWindowWithDelegate(&d111, 111,
      gfx::Rect(50, 50, 450, 450), w11.get()));
  scoped_ptr<Window> w12(CreateTestWindowWithDelegate(&d12, 12,
      gfx::Rect(0, 0, 500, 500), w1.get()));
  scoped_ptr<Window> w121(CreateTestWindowWithDelegate(&d121, 121,
      gfx::Rect(150, 150, 50, 50), w12.get()));

  EXPECT_EQ(w12.get(), w1->GetEventHandlerForPoint(gfx::Point(10, 10)));
  w12->set_ignore_events(true);
  EXPECT_EQ(w11.get(), w1->GetEventHandlerForPoint(gfx::Point(10, 10)));
  w12->set_ignore_events(false);

  EXPECT_EQ(w121.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
  w121->set_ignore_events(true);
  EXPECT_EQ(w12.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
  w12->set_ignore_events(true);
  EXPECT_EQ(w111.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
  w111->set_ignore_events(true);
  EXPECT_EQ(w11.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
}

// Various assertions for activating/deactivating.
TEST_F(WindowTest, Deactivate) {
  TestWindowDelegate d1;
  TestWindowDelegate d2;
  scoped_ptr<Window> w1(
      CreateTestWindowWithDelegate(&d1, 1, gfx::Rect(), NULL));
  scoped_ptr<Window> w2(
      CreateTestWindowWithDelegate(&d2, 2, gfx::Rect(), NULL));
  Window* parent = w1->parent();
  parent->Show();
  ASSERT_TRUE(parent);
  ASSERT_EQ(2u, parent->children().size());
  // Activate w2 and make sure it's active and frontmost.
  w2->Activate();
  EXPECT_TRUE(w2->IsActive());
  EXPECT_FALSE(w1->IsActive());
  EXPECT_EQ(w2.get(), parent->children()[1]);

  // Activate w1 and make sure it's active and frontmost.
  w1->Activate();
  EXPECT_TRUE(w1->IsActive());
  EXPECT_FALSE(w2->IsActive());
  EXPECT_EQ(w1.get(), parent->children()[1]);

  // Deactivate w1 and make sure w2 becomes active and frontmost.
  w1->Deactivate();
  EXPECT_FALSE(w1->IsActive());
  EXPECT_TRUE(w2->IsActive());
  EXPECT_EQ(w2.get(), parent->children()[1]);
}

// Tests transformation on the desktop.
TEST_F(WindowTest, Transform) {
  Desktop* desktop = Desktop::GetInstance();
  gfx::Size size = desktop->GetHostSize();
  EXPECT_EQ(gfx::Rect(size),
            gfx::Screen::GetMonitorAreaNearestPoint(gfx::Point()));

  // Rotate it clock-wise 90 degrees.
  ui::Transform transform;
  transform.SetRotate(90.0f);
  transform.ConcatTranslate(size.width(), 0);
  desktop->SetTransform(transform);

  // The size should be the transformed size.
  gfx::Size transformed_size(size.height(), size.width());
  EXPECT_EQ(transformed_size.ToString(), desktop->GetHostSize().ToString());
  EXPECT_EQ(gfx::Rect(transformed_size).ToString(),
            desktop->bounds().ToString());
  EXPECT_EQ(gfx::Rect(transformed_size).ToString(),
            gfx::Screen::GetMonitorAreaNearestPoint(gfx::Point()).ToString());
}

// Various assertions for transient children.
TEST_F(WindowTest, TransientChildren) {
  scoped_ptr<Window> parent(CreateTestWindowWithId(0, NULL));
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, parent.get()));
  scoped_ptr<Window> w3(CreateTestWindowWithId(3, parent.get()));
  Window* w2 = CreateTestWindowWithId(2, parent.get());
  w1->AddTransientChild(w2);  // w2 is now owned by w1.
  // Stack w1 at the top (end), this should force w2 to be last (on top of w1).
  parent->StackChildAtTop(w1.get());
  ASSERT_EQ(3u, parent->children().size());
  EXPECT_EQ(w2, parent->children().back());

  // Destroy w1, which should also destroy w3 (since it's a transient child).
  w1.reset();
  w2 = NULL;
  ASSERT_EQ(1u, parent->children().size());
  EXPECT_EQ(w3.get(), parent->children()[0]);

  w1.reset(CreateTestWindowWithId(4, parent.get()));
  w2 = CreateTestWindowWithId(5, w3.get());
  w1->AddTransientChild(w2);
  parent->StackChildAtTop(w3.get());
  // Stack w1 at the top (end), this shouldn't affect w2 since it has a
  // different parent.
  parent->StackChildAtTop(w1.get());
  ASSERT_EQ(2u, parent->children().size());
  EXPECT_EQ(w3.get(), parent->children()[0]);
  EXPECT_EQ(w1.get(), parent->children()[1]);
}

TEST_F(WindowTest, Property) {
  scoped_ptr<Window> w(CreateTestWindowWithId(0, NULL));
  const char* key = "test";
  EXPECT_EQ(NULL, w->GetProperty(key));
  EXPECT_EQ(0, w->GetIntProperty(key));

  w->SetIntProperty(key, 1);
  EXPECT_EQ(1, w->GetIntProperty(key));
  EXPECT_EQ(reinterpret_cast<void*>(static_cast<intptr_t>(1)),
            w->GetProperty(key));

  // Overwrite the property with different value type.
  w->SetProperty(key, static_cast<void*>(const_cast<char*>("string")));
  std::string expected("string");
  EXPECT_EQ(expected, static_cast<const char*>(w->GetProperty(key)));

  // Non-existent property.
  EXPECT_EQ(NULL, w->GetProperty("foo"));
  EXPECT_EQ(0, w->GetIntProperty("foo"));

  // Set NULL and make sure the property is gone.
  w->SetProperty(key, NULL);
  EXPECT_EQ(NULL, w->GetProperty(key));
  EXPECT_EQ(0, w->GetIntProperty(key));
}

TEST_F(WindowTest, SetBoundsInternalShouldCheckTargetBounds) {
  scoped_ptr<Window> w1(
      CreateTestWindowWithBounds(gfx::Rect(0, 0, 100, 100), NULL));

  EXPECT_FALSE(!w1->layer());
  w1->layer()->GetAnimator()->set_disable_timer_for_test(true);
  ui::AnimationContainerElement* element = w1->layer()->GetAnimator();

  EXPECT_EQ("0,0 100x100", w1->bounds().ToString());
  EXPECT_EQ("0,0 100x100", w1->layer()->GetTargetBounds().ToString());

  // Animate to a different position.
  {
    ui::LayerAnimator::ScopedSettings settings(w1->layer()->GetAnimator());
    w1->SetBounds(gfx::Rect(100, 100, 100, 100));
  }

  EXPECT_EQ("0,0 100x100", w1->bounds().ToString());
  EXPECT_EQ("100,100 100x100", w1->layer()->GetTargetBounds().ToString());

  // Animate back to the first position. The animation hasn't started yet, so
  // the current bounds are still (0, 0, 100, 100), but the target bounds are
  // (100, 100, 100, 100). If we step the animator ahead, we should find that
  // we're at (0, 0, 100, 100). That is, the second animation should be applied.
  {
    ui::LayerAnimator::ScopedSettings settings(w1->layer()->GetAnimator());
    w1->SetBounds(gfx::Rect(0, 0, 100, 100));
  }

  EXPECT_EQ("0,0 100x100", w1->bounds().ToString());
  EXPECT_EQ("0,0 100x100", w1->layer()->GetTargetBounds().ToString());

  // Confirm that the target bounds are reached.
  base::TimeTicks start_time =
      w1->layer()->GetAnimator()->get_last_step_time_for_test();

  element->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_EQ("0,0 100x100", w1->bounds().ToString());
}


class WindowObserverTest : public WindowTest,
                           public WindowObserver {
 public:
  struct VisibilityInfo {
    bool window_visible;
    bool visible_param;
  };

  WindowObserverTest()
      : added_count_(0),
        removed_count_(0),
        destroyed_count_(0),
        old_property_value_(0),
        new_property_value_(0) {
  }

  virtual ~WindowObserverTest() {}

  const VisibilityInfo* GetVisibilityInfo() const {
    return visibility_info_.get();
  }

  void ResetVisibilityInfo() {
    visibility_info_.reset();
  }

  // Returns a description of the WindowObserver methods that have been invoked.
  std::string WindowObserverCountStateAndClear() {
    std::string result(
        base::StringPrintf("added=%d removed=%d",
        added_count_, removed_count_));
    added_count_ = removed_count_ = 0;
    return result;
  }

  int DestroyedCountAndClear() {
    int result = destroyed_count_;
    destroyed_count_ = 0;
    return result;
  }

  // Return a string representation of the arguments passed in
  // OnWindowPropertyChanged callback.
  std::string PropertyChangeInfoAndClear() {
    std::string result(
        base::StringPrintf("name=%s old=%ld new=%ld",
                           property_name_.c_str(),
                           static_cast<long>(old_property_value_),
                           static_cast<long>(new_property_value_)));
    property_name_.clear();
    old_property_value_ = 0;
    new_property_value_ = 0;
    return result;
  }

 private:
  virtual void OnWindowAdded(Window* new_window) OVERRIDE {
    added_count_++;
  }

  virtual void OnWillRemoveWindow(Window* window) OVERRIDE {
    removed_count_++;
  }

  virtual void OnWindowVisibilityChanged(Window* window,
                                         bool visible) OVERRIDE {
    visibility_info_.reset(new VisibilityInfo);
    visibility_info_->window_visible = window->IsVisible();
    visibility_info_->visible_param = visible;
  }

  virtual void OnWindowDestroyed(Window* window) OVERRIDE {
    destroyed_count_++;
  }

  virtual void OnWindowPropertyChanged(Window* window,
                                       const char* name,
                                       void* old) OVERRIDE {
    property_name_ = std::string(name);
    old_property_value_ = reinterpret_cast<intptr_t>(old);
    new_property_value_ = reinterpret_cast<intptr_t>(window->GetProperty(name));
  }

  int added_count_;
  int removed_count_;
  int destroyed_count_;
  scoped_ptr<VisibilityInfo> visibility_info_;
  std::string property_name_;
  intptr_t old_property_value_;
  intptr_t new_property_value_;

  DISALLOW_COPY_AND_ASSIGN(WindowObserverTest);
};

// Various assertions for WindowObserver.
TEST_F(WindowObserverTest, WindowObserver) {
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, NULL));
  w1->AddObserver(this);

  // Create a new window as a child of w1, our observer should be notified.
  scoped_ptr<Window> w2(CreateTestWindowWithId(2, w1.get()));
  EXPECT_EQ("added=1 removed=0", WindowObserverCountStateAndClear());

  // Delete w2, which should result in the remove notification.
  w2.reset();
  EXPECT_EQ("added=0 removed=1", WindowObserverCountStateAndClear());

  // Create a window that isn't parented to w1, we shouldn't get any
  // notification.
  scoped_ptr<Window> w3(CreateTestWindowWithId(3, NULL));
  EXPECT_EQ("added=0 removed=0", WindowObserverCountStateAndClear());

  // Similarly destroying w3 shouldn't notify us either.
  w3.reset();
  EXPECT_EQ("added=0 removed=0", WindowObserverCountStateAndClear());
  w1->RemoveObserver(this);
}

// Test if OnWindowVisibilityChagned is invoked with expected
// parameters.
TEST_F(WindowObserverTest, WindowVisibility) {
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, NULL));
  scoped_ptr<Window> w2(CreateTestWindowWithId(1, w1.get()));
  w2->AddObserver(this);

  // Hide should make the window invisible and the passed visible
  // parameter is false.
  w2->Hide();
  EXPECT_FALSE(!GetVisibilityInfo());
  EXPECT_FALSE(!GetVisibilityInfo());
  if (!GetVisibilityInfo())
    return;
  EXPECT_FALSE(GetVisibilityInfo()->window_visible);
  EXPECT_FALSE(GetVisibilityInfo()->visible_param);

  // If parent isn't visible, showing window won't make the window visible, but
  // passed visible value must be true.
  w1->Hide();
  ResetVisibilityInfo();
  EXPECT_TRUE(!GetVisibilityInfo());
  w2->Show();
  EXPECT_FALSE(!GetVisibilityInfo());
  if (!GetVisibilityInfo())
    return;
  EXPECT_FALSE(GetVisibilityInfo()->window_visible);
  EXPECT_TRUE(GetVisibilityInfo()->visible_param);

  // If parent is visible, showing window will make the window
  // visible and the passed visible value is true.
  w1->Show();
  w2->Hide();
  ResetVisibilityInfo();
  w2->Show();
  EXPECT_FALSE(!GetVisibilityInfo());
  if (!GetVisibilityInfo())
    return;
  EXPECT_TRUE(GetVisibilityInfo()->window_visible);
  EXPECT_TRUE(GetVisibilityInfo()->visible_param);
}

// Test if OnWindowDestroyed is invoked as expected.
TEST_F(WindowObserverTest, WindowDestroyed) {
  // Delete a window should fire a destroyed notification.
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, NULL));
  w1->AddObserver(this);
  w1.reset();
  EXPECT_EQ(1, DestroyedCountAndClear());

  // Observe on child and delete parent window should fire a notification.
  scoped_ptr<Window> parent(CreateTestWindowWithId(1, NULL));
  Window* child = CreateTestWindowWithId(1, parent.get());  // owned by parent
  child->AddObserver(this);
  parent.reset();
  EXPECT_EQ(1, DestroyedCountAndClear());
}

TEST_F(WindowObserverTest, PropertyChanged) {
  // Setting property should fire a property change notification.
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, NULL));
  const char* key = "test";

  w1->AddObserver(this);
  w1->SetIntProperty(key, 1);
  EXPECT_EQ("name=test old=0 new=1", PropertyChangeInfoAndClear());
  w1->SetIntProperty(key, 2);
  EXPECT_EQ(2, w1->GetIntProperty(key));
  EXPECT_EQ(reinterpret_cast<void*>(2), w1->GetProperty(key));
  EXPECT_EQ("name=test old=1 new=2", PropertyChangeInfoAndClear());
  w1->SetProperty(key, NULL);
  EXPECT_EQ("name=test old=2 new=0", PropertyChangeInfoAndClear());

  // Sanity check to see if |PropertyChangeInfoAndClear| really clears.
  EXPECT_EQ("name= old=0 new=0", PropertyChangeInfoAndClear());
}

class DesktopObserverTest : public WindowTest,
                            public DesktopObserver {
 public:
  DesktopObserverTest() : active_(NULL) {
  }

  virtual ~DesktopObserverTest() {}

  Window* active() const { return active_; }

  void Reset() {
    active_ = NULL;
  }

 private:
  virtual void SetUp() OVERRIDE {
    WindowTest::SetUp();
    Desktop::GetInstance()->AddObserver(this);
  }

  virtual void TearDown() OVERRIDE {
    Desktop::GetInstance()->RemoveObserver(this);
    WindowTest::TearDown();
  }

  virtual void OnActiveWindowChanged(Window* active) OVERRIDE {
    active_ = active;
  }

  Window* active_;

  DISALLOW_COPY_AND_ASSIGN(DesktopObserverTest);
};

TEST_F(DesktopObserverTest, WindowActivationObserve) {
  scoped_ptr<Window> w1(CreateTestWindowWithId(1, NULL));
  scoped_ptr<Window> w2(CreateTestWindowWithId(2, NULL));
  scoped_ptr<Window> w3(CreateTestWindowWithId(3, w1.get()));

  EXPECT_EQ(NULL, active());

  w2->Activate();
  EXPECT_EQ(w2.get(), active());

  w3->Activate();
  EXPECT_EQ(w2.get(), active());

  w1->Activate();
  EXPECT_EQ(w1.get(), active());
}

}  // namespace test
}  // namespace aura
