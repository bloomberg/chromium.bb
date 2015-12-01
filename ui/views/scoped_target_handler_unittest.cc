// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/scoped_target_handler.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/views/view.h"

namespace views {

namespace {

// A View that holds ownership of its target and delegate EventHandlers.
class TestView : public View {
 public:
  TestView() {}
  ~TestView() override {}

  void SetHandler(scoped_ptr<ui::EventHandler> target_handler,
                  scoped_ptr<ui::EventHandler> delegate) {
    target_handler_ = target_handler.Pass();
    delegate_ = delegate.Pass();
  }

  void DispatchEvent(ui::Event* event) { target_handler()->OnEvent(event); }

 private:
  scoped_ptr<ui::EventHandler> target_handler_;
  scoped_ptr<ui::EventHandler> delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestView);
};

// An EventHandler that sets itself as a target handler for a View and can
// recursively dispatch an Event.
class NestedEventHandler : public ui::EventHandler {
 public:
  NestedEventHandler(TestView* view, int nesting)
      : view_(view), nesting_(nesting) {
    original_handler_ = view_->SetTargetHandler(this);
  }
  ~NestedEventHandler() override {
    ui::EventHandler* handler = view_->SetTargetHandler(original_handler_);
    DCHECK_EQ(this, handler);
  }

 protected:
  void OnEvent(ui::Event* event) override {
    if (--nesting_ == 0)
      return;
    view_->DispatchEvent(event);
  }

 private:
  TestView* view_;
  int nesting_;
  ui::EventHandler* original_handler_;

  DISALLOW_COPY_AND_ASSIGN(NestedEventHandler);
};

// An EventHandler that sets itself as a target handler for a View and destroys
// that View when handling an Event, possibly after recursively handling the
// Event.
class ViewDestroyingEventHandler : public ui::EventHandler {
 public:
  ViewDestroyingEventHandler(TestView* view, int nesting)
      : view_(view), nesting_(nesting) {
    original_handler_ = view_->SetTargetHandler(this);
  }
  ~ViewDestroyingEventHandler() override {
    ui::EventHandler* handler = view_->SetTargetHandler(original_handler_);
    DCHECK_EQ(this, handler);
  }

 protected:
  void OnEvent(ui::Event* event) override {
    if (--nesting_ == 0) {
      delete view_;
      return;
    }
    view_->DispatchEvent(event);
  }

 private:
  TestView* view_;
  int nesting_;
  ui::EventHandler* original_handler_;

  DISALLOW_COPY_AND_ASSIGN(ViewDestroyingEventHandler);
};

// An EventHandler that can be set to receive events in addition to the target
// handler and counts the Events that it receives.
class EventCountingEventHandler : public ui::EventHandler {
 public:
  EventCountingEventHandler(View* view, int* count)
      : scoped_target_handler_(new views::ScopedTargetHandler(view, this)),
        count_(count) {}
  ~EventCountingEventHandler() override {}

 protected:
  void OnEvent(ui::Event* event) override { (*count_)++; }

 private:
  scoped_ptr<ScopedTargetHandler> scoped_target_handler_;
  int* count_;

  DISALLOW_COPY_AND_ASSIGN(EventCountingEventHandler);
};

}  // namespace

// Tests that a ScopedTargetHandler invokes both the target and a delegate.
TEST(ScopedTargetHandlerTest, HandlerInvoked) {
  int count = 0;
  TestView* view = new TestView;
  scoped_ptr<NestedEventHandler> target_handler(
      new NestedEventHandler(view, 1));
  scoped_ptr<EventCountingEventHandler> delegate(
      new EventCountingEventHandler(view, &count));
  view->SetHandler(target_handler.Pass(), delegate.Pass());
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  view->DispatchEvent(&event);
  EXPECT_EQ(1, count);
  delete view;
}

// Tests that a ScopedTargetHandler invokes both the target and a delegate when
// an Event is dispatched recursively such as with synthetic events.
TEST(ScopedTargetHandlerTest, HandlerInvokedNested) {
  int count = 0;
  TestView* view = new TestView;
  scoped_ptr<NestedEventHandler> target_handler(
      new NestedEventHandler(view, 2));
  scoped_ptr<EventCountingEventHandler> delegate(
      new EventCountingEventHandler(view, &count));
  view->SetHandler(target_handler.Pass(), delegate.Pass());
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  view->DispatchEvent(&event);
  EXPECT_EQ(2, count);
  delete view;
}

// Tests that a it is safe to delete a ScopedTargetHandler while handling an
// event.
TEST(ScopedTargetHandlerTest, SafeToDestroy) {
  int count = 0;
  TestView* view = new TestView;
  scoped_ptr<ViewDestroyingEventHandler> target_handler(
      new ViewDestroyingEventHandler(view, 1));
  scoped_ptr<EventCountingEventHandler> delegate(
      new EventCountingEventHandler(view, &count));
  view->SetHandler(target_handler.Pass(), delegate.Pass());
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  view->DispatchEvent(&event);
  EXPECT_EQ(0, count);
}

// Tests that a it is safe to delete a ScopedTargetHandler while handling an
// event recursively.
TEST(ScopedTargetHandlerTest, SafeToDestroyNested) {
  int count = 0;
  TestView* view = new TestView;
  scoped_ptr<ViewDestroyingEventHandler> target_handler(
      new ViewDestroyingEventHandler(view, 2));
  scoped_ptr<EventCountingEventHandler> delegate(
      new EventCountingEventHandler(view, &count));
  view->SetHandler(target_handler.Pass(), delegate.Pass());
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  view->DispatchEvent(&event);
  EXPECT_EQ(0, count);
}

}  // namespace views
