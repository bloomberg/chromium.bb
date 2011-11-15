// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event_filter.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_event_filter.h"
#include "ui/aura/test/test_window_delegate.h"

#if defined(OS_WIN)
// Windows headers define macros for these function names which screw with us.
#if defined(CreateWindow)
#undef CreateWindow
#endif
#endif

namespace aura {
namespace test {

typedef AuraTestBase EventFilterTest;

class TestEventFilterWindowDelegate : public TestWindowDelegate {
 public:
  TestEventFilterWindowDelegate()
      : key_event_count_(0),
        mouse_event_count_(0),
        touch_event_count_(0) {}
  virtual ~TestEventFilterWindowDelegate() {}

  void ResetCounts() {
    key_event_count_ = 0;
    mouse_event_count_ = 0;
    touch_event_count_ = 0;
  }

  int key_event_count() const { return key_event_count_; }
  int mouse_event_count() const { return mouse_event_count_; }
  int touch_event_count() const { return touch_event_count_; }

  // Overridden from TestWindowDelegate:
  virtual bool OnKeyEvent(KeyEvent* event) OVERRIDE {
    ++key_event_count_;
    return true;
  }
  virtual bool OnMouseEvent(MouseEvent* event) OVERRIDE {
    ++mouse_event_count_;
    return true;
  }
  virtual ui::TouchStatus OnTouchEvent(TouchEvent* event) OVERRIDE {
    ++touch_event_count_;
    return ui::TOUCH_STATUS_UNKNOWN;
  }

 private:
  int key_event_count_;
  int mouse_event_count_;
  int touch_event_count_;

  DISALLOW_COPY_AND_ASSIGN(TestEventFilterWindowDelegate);
};

Window* CreateWindow(int id, Window* parent, WindowDelegate* delegate) {
  Window* window = new Window(delegate ? delegate : new TestWindowDelegate);
  window->set_id(id);
  window->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window->SetParent(parent);
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  return window;
}

// Creates this hierarchy:
//
// Desktop Window (EF)
//  +- w1 (EF)
//    +- w11
//      +- w111 (EF)
//        +- w1111 <-- target window
TEST_F(EventFilterTest, Basic) {
  scoped_ptr<Window> w1(CreateWindow(1, Desktop::GetInstance(), NULL));
  scoped_ptr<Window> w11(CreateWindow(11, w1.get(), NULL));
  scoped_ptr<Window> w111(CreateWindow(111, w11.get(), NULL));
  TestEventFilterWindowDelegate* d1111 = new TestEventFilterWindowDelegate;
  scoped_ptr<Window> w1111(CreateWindow(1111, w111.get(), d1111));

  TestEventFilter* desktop_filter = new TestEventFilter(Desktop::GetInstance());
  TestEventFilter* w1_filter = new TestEventFilter(w1.get());
  TestEventFilter* w111_filter = new TestEventFilter(w111.get());
  Desktop::GetInstance()->SetEventFilter(desktop_filter);
  w1->SetEventFilter(w1_filter);
  w111->SetEventFilter(w111_filter);

  w1111->GetFocusManager()->SetFocusedWindow(w1111.get());

  // To start with, no one is going to consume any events. All three filters
  // and the w1111's delegate should receive the event.
  EventGenerator generator(w1111.get());
  generator.PressLeftButton();
  KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, 0);
  Desktop::GetInstance()->DispatchKeyEvent(&key_event);

  // TODO(sadrul): TouchEvent!
  EXPECT_EQ(1, desktop_filter->key_event_count());
  EXPECT_EQ(1, w1_filter->key_event_count());
  EXPECT_EQ(1, w111_filter->key_event_count());
  EXPECT_EQ(1, d1111->key_event_count());
  EXPECT_EQ(1, desktop_filter->mouse_event_count());
  EXPECT_EQ(1, w1_filter->mouse_event_count());
  EXPECT_EQ(1, w111_filter->mouse_event_count());
  EXPECT_EQ(1, d1111->mouse_event_count());
  EXPECT_EQ(0, desktop_filter->touch_event_count());
  EXPECT_EQ(0, w1_filter->touch_event_count());
  EXPECT_EQ(0, w111_filter->touch_event_count());
  EXPECT_EQ(0, d1111->touch_event_count());

  d1111->ResetCounts();
  desktop_filter->ResetCounts();
  w1_filter->ResetCounts();
  w111_filter->ResetCounts();

  // Now make w1's EF consume the event.
  w1_filter->set_consumes_key_events(true);
  w1_filter->set_consumes_mouse_events(true);

  generator.ReleaseLeftButton();
  Desktop::GetInstance()->DispatchKeyEvent(&key_event);

  // TODO(sadrul): TouchEvent!
  EXPECT_EQ(1, desktop_filter->key_event_count());
  EXPECT_EQ(1, w1_filter->key_event_count());
  EXPECT_EQ(0, w111_filter->key_event_count());
  EXPECT_EQ(0, d1111->key_event_count());
  EXPECT_EQ(1, desktop_filter->mouse_event_count());
  EXPECT_EQ(1, w1_filter->mouse_event_count());
  EXPECT_EQ(0, w111_filter->mouse_event_count());
  EXPECT_EQ(0, d1111->mouse_event_count());
  EXPECT_EQ(0, desktop_filter->touch_event_count());
  EXPECT_EQ(0, w1_filter->touch_event_count());
  EXPECT_EQ(0, w111_filter->touch_event_count());
  EXPECT_EQ(0, d1111->touch_event_count());
}

}  // namespace test
}  // namespace aura
