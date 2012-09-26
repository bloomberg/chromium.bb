// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events/event_dispatcher.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

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
        event_result_(ER_UNHANDLED),
        expect_pre_target_(false),
        expect_post_target_(false),
        received_pre_target_(false) {
  }

  virtual ~TestEventHandler() {}

  void ReceivedEvent(Event* event) {
    static_cast<TestTarget*>(event->target())->AddHandlerId(id_);
    if (event->phase() == ui::EP_POSTTARGET) {
      EXPECT_TRUE(expect_post_target_);
      if (expect_pre_target_)
        EXPECT_TRUE(received_pre_target_);
    } else if (event->phase() == ui::EP_PRETARGET) {
      EXPECT_TRUE(expect_pre_target_);
      received_pre_target_ = true;
    } else {
      NOTREACHED();
    }
  }

  void set_event_result(EventResult result) { event_result_ = result; }

  void set_expect_pre_target(bool expect) { expect_pre_target_ = expect; }
  void set_expect_post_target(bool expect) { expect_post_target_ = expect; }

 private:
  // Overridden from EventHandler:
  virtual EventResult OnKeyEvent(KeyEvent* event) OVERRIDE {
    ReceivedEvent(event);
    return event_result_;
  }

  virtual EventResult OnMouseEvent(MouseEvent* event) OVERRIDE {
    ReceivedEvent(event);
    return event_result_;
  }

  virtual EventResult OnScrollEvent(ScrollEvent* event) OVERRIDE {
    ReceivedEvent(event);
    return event_result_;
  }

  virtual TouchStatus OnTouchEvent(TouchEvent* event) OVERRIDE {
    ReceivedEvent(event);
    return ui::TOUCH_STATUS_UNKNOWN;
  }

  virtual EventResult OnGestureEvent(GestureEvent* event) OVERRIDE {
    ReceivedEvent(event);
    return event_result_;
  }

  int id_;
  EventResult event_result_;
  bool expect_pre_target_;
  bool expect_post_target_;
  bool received_pre_target_;

  DISALLOW_COPY_AND_ASSIGN(TestEventHandler);
};

class TestEventDispatcher : public EventDispatcher {
 public:
  TestEventDispatcher() {}
  virtual ~TestEventDispatcher() {}

 private:
  // Overridden from EventDispatcher:
  virtual bool CanDispatchToTarget(EventTarget* target) OVERRIDE {
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(TestEventDispatcher);
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

  h1.set_expect_pre_target(true);
  h2.set_expect_pre_target(true);
  h3.set_expect_pre_target(true);
  h4.set_expect_pre_target(true);

  child.AddPostTargetHandler(&h5);
  child.AddPostTargetHandler(&h6);

  parent.AddPostTargetHandler(&h7);
  parent.AddPostTargetHandler(&h8);

  h5.set_expect_post_target(true);
  h6.set_expect_post_target(true);
  h7.set_expect_post_target(true);
  h8.set_expect_post_target(true);

  MouseEvent mouse(ui::ET_MOUSE_MOVED, gfx::Point(3, 4),
      gfx::Point(3, 4), 0);
  Event::DispatcherApi event_mod(&mouse);
  int result = dispatcher.ProcessEvent(&child, &mouse);
  EXPECT_FALSE(result & ER_CONSUMED);
  EXPECT_FALSE(result & ER_HANDLED);

  int expected[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
  EXPECT_EQ(
      std::vector<int>(expected, expected + sizeof(expected) / sizeof(int)),
      child.handler_list());

  child.Reset();
  event_mod.set_phase(EP_PREDISPATCH);
  event_mod.set_result(ER_UNHANDLED);

  h1.set_event_result(ER_HANDLED);
  result = dispatcher.ProcessEvent(&child, &mouse);
  EXPECT_EQ(result, mouse.result());
  EXPECT_EQ(EP_POSTDISPATCH, mouse.phase());
  EXPECT_FALSE(result & ER_CONSUMED);
  EXPECT_TRUE(result & ER_HANDLED);
  EXPECT_EQ(
      std::vector<int>(expected, expected + sizeof(expected) / sizeof(int)),
      child.handler_list());

  child.Reset();
  event_mod.set_phase(EP_PREDISPATCH);
  event_mod.set_result(ER_UNHANDLED);

  int nexpected[] = { 1, 2, 3, 4, 5 };
  h5.set_event_result(ER_CONSUMED);
  result = dispatcher.ProcessEvent(&child, &mouse);
  EXPECT_EQ(result, mouse.result());
  EXPECT_EQ(EP_POSTDISPATCH, mouse.phase());
  EXPECT_TRUE(result & ER_CONSUMED);
  EXPECT_TRUE(result & ER_HANDLED);
  EXPECT_EQ(
      std::vector<int>(nexpected, nexpected + sizeof(nexpected) / sizeof(int)),
      child.handler_list());

  child.Reset();
  event_mod.set_phase(EP_PREDISPATCH);
  event_mod.set_result(ER_UNHANDLED);

  int exp[] = { 1 };
  h1.set_event_result(ER_CONSUMED);
  result = dispatcher.ProcessEvent(&child, &mouse);
  EXPECT_EQ(EP_POSTDISPATCH, mouse.phase());
  EXPECT_EQ(result, mouse.result());
  EXPECT_TRUE(result & ER_CONSUMED);
  EXPECT_FALSE(result & ER_HANDLED);
  EXPECT_EQ(
      std::vector<int>(exp, exp + sizeof(exp) / sizeof(int)),
      child.handler_list());
}

// Tests that the event-phases are correct.
TEST(EventDispatcherTest, EventDispatchPhase) {
  TestEventDispatcher dispatcher;
  TestTarget target;

  TestEventHandler handler(11);

  target.AddPreTargetHandler(&handler);
  target.AddPostTargetHandler(&handler);
  handler.set_expect_pre_target(true);
  handler.set_expect_post_target(true);

  MouseEvent mouse(ui::ET_MOUSE_MOVED, gfx::Point(3, 4),
      gfx::Point(3, 4), 0);
  Event::DispatcherApi event_mod(&mouse);
  int result = dispatcher.ProcessEvent(&target, &mouse);
  EXPECT_EQ(ER_UNHANDLED, result);

  int handlers[] = { 11, 11 };
  EXPECT_EQ(
      std::vector<int>(handlers, handlers + sizeof(handlers) / sizeof(int)),
      target.handler_list());
}

}  // namespace ui
