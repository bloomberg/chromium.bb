// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ScriptedAnimationController.h"

#include "core/dom/Document.h"
#include "core/dom/FrameRequestCallback.h"
#include "core/events/Event.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Functional.h"
#include <memory>

namespace blink {

class ScriptedAnimationControllerTest : public ::testing::Test {
 protected:
  void SetUp() override;

  Document& document() const { return m_dummyPageHolder->document(); }
  ScriptedAnimationController& controller() { return *m_controller; }

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<ScriptedAnimationController> m_controller;
};

void ScriptedAnimationControllerTest::SetUp() {
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));

  // Note: The document doesn't know about this ScriptedAnimationController
  // instance, and will create another if
  // Document::ensureScriptedAnimationController is called.
  m_controller =
      wrapPersistent(ScriptedAnimationController::create(&document()));
}

namespace {

class TaskOrderObserver {
 public:
  std::unique_ptr<WTF::Closure> createTask(int id) {
    return WTF::bind(&TaskOrderObserver::runTask, WTF::unretained(this), id);
  }
  const Vector<int>& order() const { return m_order; }

 private:
  void runTask(int id) { m_order.push_back(id); }
  Vector<int> m_order;
};

}  // anonymous namespace

TEST_F(ScriptedAnimationControllerTest, EnqueueOneTask) {
  TaskOrderObserver observer;

  controller().enqueueTask(observer.createTask(1));
  EXPECT_EQ(0u, observer.order().size());

  controller().serviceScriptedAnimations(0);
  EXPECT_EQ(1u, observer.order().size());
  EXPECT_EQ(1, observer.order()[0]);
}

TEST_F(ScriptedAnimationControllerTest, EnqueueTwoTasks) {
  TaskOrderObserver observer;

  controller().enqueueTask(observer.createTask(1));
  controller().enqueueTask(observer.createTask(2));
  EXPECT_EQ(0u, observer.order().size());

  controller().serviceScriptedAnimations(0);
  EXPECT_EQ(2u, observer.order().size());
  EXPECT_EQ(1, observer.order()[0]);
  EXPECT_EQ(2, observer.order()[1]);
}

namespace {

void enqueueTask(ScriptedAnimationController* controller,
                 TaskOrderObserver* observer,
                 int id) {
  controller->enqueueTask(observer->createTask(id));
}

}  // anonymous namespace

// A task enqueued while running tasks should not be run immediately after, but
// the next time tasks are run.
TEST_F(ScriptedAnimationControllerTest, EnqueueWithinTask) {
  TaskOrderObserver observer;

  controller().enqueueTask(observer.createTask(1));
  controller().enqueueTask(WTF::bind(&enqueueTask,
                                     wrapPersistent(&controller()),
                                     WTF::unretained(&observer), 2));
  controller().enqueueTask(observer.createTask(3));
  EXPECT_EQ(0u, observer.order().size());

  controller().serviceScriptedAnimations(0);
  EXPECT_EQ(2u, observer.order().size());
  EXPECT_EQ(1, observer.order()[0]);
  EXPECT_EQ(3, observer.order()[1]);

  controller().serviceScriptedAnimations(0);
  EXPECT_EQ(3u, observer.order().size());
  EXPECT_EQ(1, observer.order()[0]);
  EXPECT_EQ(3, observer.order()[1]);
  EXPECT_EQ(2, observer.order()[2]);
}

namespace {

class RunTaskEventListener final : public EventListener {
 public:
  RunTaskEventListener(std::unique_ptr<WTF::Closure> task)
      : EventListener(CPPEventListenerType), m_task(std::move(task)) {}
  void handleEvent(ExecutionContext*, Event*) override { (*m_task)(); }
  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

 private:
  std::unique_ptr<WTF::Closure> m_task;
};

}  // anonymous namespace

// Tasks should be run after events are dispatched, even if they were enqueued
// first.
TEST_F(ScriptedAnimationControllerTest, EnqueueTaskAndEvent) {
  TaskOrderObserver observer;

  controller().enqueueTask(observer.createTask(1));
  document().addEventListener("test",
                              new RunTaskEventListener(observer.createTask(2)));
  Event* event = Event::create("test");
  event->setTarget(&document());
  controller().enqueueEvent(event);
  EXPECT_EQ(0u, observer.order().size());

  controller().serviceScriptedAnimations(0);
  EXPECT_EQ(2u, observer.order().size());
  EXPECT_EQ(2, observer.order()[0]);
  EXPECT_EQ(1, observer.order()[1]);
}

namespace {

class RunTaskCallback final : public FrameRequestCallback {
 public:
  RunTaskCallback(std::unique_ptr<WTF::Closure> task)
      : m_task(std::move(task)) {}
  void handleEvent(double) override { (*m_task)(); }

 private:
  std::unique_ptr<WTF::Closure> m_task;
};

}  // anonymous namespace

// Animation frame callbacks should be run after tasks, even if they were
// enqueued first.
TEST_F(ScriptedAnimationControllerTest, RegisterCallbackAndEnqueueTask) {
  TaskOrderObserver observer;

  Event* event = Event::create("test");
  event->setTarget(&document());

  controller().registerCallback(new RunTaskCallback(observer.createTask(1)));
  controller().enqueueTask(observer.createTask(2));
  EXPECT_EQ(0u, observer.order().size());

  controller().serviceScriptedAnimations(0);
  EXPECT_EQ(2u, observer.order().size());
  EXPECT_EQ(2, observer.order()[0]);
  EXPECT_EQ(1, observer.order()[1]);
}

}  // namespace blink
