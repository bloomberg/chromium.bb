// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events/event_dispatcher.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

class TestEventDispatcher : public EventDispatcher {
 public:
  TestEventDispatcher() {}
  virtual ~TestEventDispatcher() {}

 private:
  // Overridden from EventDispatcher:
  virtual bool CanDispatchToTarget(EventTarget* target) OVERRIDE {
    return true;
  }

  virtual void ProcessPreTargetList(EventHandlerList* list) OVERRIDE {
  }

  virtual void ProcessPostTargetList(EventHandlerList* list) OVERRIDE {
  }

  DISALLOW_COPY_AND_ASSIGN(TestEventDispatcher);
};

class TestTarget : public EventTarget {
 public:
  TestTarget() : parent_(NULL) {}
  virtual ~TestTarget() {}

  void set_parent(TestTarget* parent) { parent_ = parent; }

  void AddHandlerId(int id) {
    handler_list_.push_back(id);
  }

  const std::vector<int>& handler_list() const { return handler_list_; }

  void Reset() {
    handler_list_.clear();
  }

 private:
  // Overridden from EventTarget:
  virtual bool CanAcceptEvents() OVERRIDE {
    return true;
  }

  virtual EventTarget* GetParentTarget() OVERRIDE {
    return parent_;
  }

  TestTarget* parent_;
  std::vector<int> handler_list_;

  DISALLOW_COPY_AND_ASSIGN(TestTarget);
};

class TestEventHandler : public EventHandler {
 public:
  TestEventHandler(int id)
      : id_(id),
        event_result_(ER_UNHANDLED) {
  }

  virtual ~TestEventHandler() {}

  void ReceivedEventForTarget(EventTarget* target) {
    static_cast<TestTarget*>(target)->AddHandlerId(id_);
  }

  void set_event_result(EventResult result) { event_result_ = result; }

 private:
  // Overridden from EventHandler:
  virtual EventResult OnKeyEvent(KeyEvent* event) OVERRIDE {
    ReceivedEventForTarget(event->target());
    return event_result_;
  }

  virtual EventResult OnMouseEvent(MouseEvent* event) OVERRIDE {
    ReceivedEventForTarget(event->target());
    return event_result_;
  }

  virtual EventResult OnScrollEvent(ScrollEvent* event) OVERRIDE {
    ReceivedEventForTarget(event->target());
    return event_result_;
  }

  virtual TouchStatus OnTouchEvent(TouchEvent* event) OVERRIDE {
    ReceivedEventForTarget(event->target());
    return ui::TOUCH_STATUS_UNKNOWN;
  }

  virtual EventResult OnGestureEvent(GestureEvent* event) OVERRIDE {
    ReceivedEventForTarget(event->target());
    return event_result_;
  }

  int id_;
  EventResult event_result_;

  DISALLOW_COPY_AND_ASSIGN(TestEventHandler);
};

}  // namespace

TEST(EventDispatcherTest, EventDispatchOrder) {
  TestEventDispatcher dispatcher;
  TestTarget parent, child;
  TestEventHandler h1(1), h2(2), h3(3), h4(4);
  TestEventHandler h5(5), h6(6), h7(7), h8(8);

  child.set_parent(&parent);

  parent.AddPreTargetHandler(&h1);
  parent.AddPreTargetHandler(&h2);

  child.AddPreTargetHandler(&h3);
  child.AddPreTargetHandler(&h4);

  child.AddPostTargetHandler(&h5);
  child.AddPostTargetHandler(&h6);

  parent.AddPostTargetHandler(&h7);
  parent.AddPostTargetHandler(&h8);

  MouseEvent mouse(ui::ET_MOUSE_MOVED, gfx::Point(3, 4),
      gfx::Point(3, 4), 0);
  int result = dispatcher.ProcessEvent(&child, &mouse);
  EXPECT_FALSE(result & ER_CONSUMED);
  EXPECT_FALSE(result & ER_HANDLED);

  // Note that the pre-handlers for the target itself (i.e. |child|) will not
  // receive these events. This is for compatibility reasons for how
  // event-filters behave in aura. The desired behaviour is that the
  // pre-handlers for the target will receive the events before the target.
  // http://crbug.com/147523
  int expected[] = { 1, 2, 5, 6, 7, 8 };
  EXPECT_EQ(
      std::vector<int>(expected, expected + sizeof(expected) / sizeof(int)),
      child.handler_list());

  child.Reset();

  h1.set_event_result(ER_HANDLED);
  result = dispatcher.ProcessEvent(&child, &mouse);
  EXPECT_FALSE(result & ER_CONSUMED);
  EXPECT_TRUE(result & ER_HANDLED);
  EXPECT_EQ(
      std::vector<int>(expected, expected + sizeof(expected) / sizeof(int)),
      child.handler_list());

  child.Reset();

  int nexpected[] = { 1, 2, 5 };
  h5.set_event_result(ER_CONSUMED);
  result = dispatcher.ProcessEvent(&child, &mouse);
  EXPECT_TRUE(result & ER_CONSUMED);
  EXPECT_TRUE(result & ER_HANDLED);
  EXPECT_EQ(
      std::vector<int>(nexpected, nexpected + sizeof(nexpected) / sizeof(int)),
      child.handler_list());

  child.Reset();

  int exp[] = { 1 };
  h1.set_event_result(ER_CONSUMED);
  result = dispatcher.ProcessEvent(&child, &mouse);
  EXPECT_TRUE(result & ER_CONSUMED);
  EXPECT_FALSE(result & ER_HANDLED);
  EXPECT_EQ(
      std::vector<int>(exp, exp + sizeof(exp) / sizeof(int)),
      child.handler_list());
}

}  // namespace ui
