// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/platform_event_source.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform/scoped_event_dispatcher.h"

namespace ui {

namespace {

scoped_ptr<PlatformEvent> CreatePlatformEvent() {
  scoped_ptr<PlatformEvent> event(new PlatformEvent());
  memset(event.get(), 0, sizeof(PlatformEvent));
  return event.Pass();
}

template <typename T>
void DestroyScopedPtr(scoped_ptr<T> object) {}

}  // namespace

class TestPlatformEventSource : public PlatformEventSource {
 public:
  TestPlatformEventSource() {}
  virtual ~TestPlatformEventSource() {}

  uint32_t Dispatch(const PlatformEvent& event) { return DispatchEvent(event); }

  // Dispatches the stream of events, and returns the number of events that are
  // dispatched before it is requested to stop.
  size_t DispatchEventStream(const ScopedVector<PlatformEvent>& events) {
    for (size_t count = 0; count < events.size(); ++count) {
      uint32_t action = DispatchEvent(*events[count]);
      if (action & POST_DISPATCH_QUIT_LOOP)
        return count + 1;
    }
    return events.size();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPlatformEventSource);
};

class TestPlatformEventDispatcher : public PlatformEventDispatcher {
 public:
  TestPlatformEventDispatcher(int id, std::vector<int>* list)
      : id_(id), list_(list), post_dispatch_action_(POST_DISPATCH_NONE) {
    PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  }
  virtual ~TestPlatformEventDispatcher() {
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  }

  void set_post_dispatch_action(uint32_t action) {
    post_dispatch_action_ = action;
  }

 protected:
  // PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const PlatformEvent& event) OVERRIDE {
    return true;
  }

  virtual uint32_t DispatchEvent(const PlatformEvent& event) OVERRIDE {
    list_->push_back(id_);
    return post_dispatch_action_;
  }

 private:
  int id_;
  std::vector<int>* list_;
  uint32_t post_dispatch_action_;

  DISALLOW_COPY_AND_ASSIGN(TestPlatformEventDispatcher);
};

class TestPlatformEventObserver : public PlatformEventObserver {
 public:
  TestPlatformEventObserver(int id, std::vector<int>* list)
      : id_(id), list_(list) {
    PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
  }
  virtual ~TestPlatformEventObserver() {
    PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
  }

 protected:
  // PlatformEventObserver:
  virtual void WillProcessEvent(const PlatformEvent& event) OVERRIDE {
    list_->push_back(id_);
  }

  virtual void DidProcessEvent(const PlatformEvent& event) OVERRIDE {}

 private:
  int id_;
  std::vector<int>* list_;

  DISALLOW_COPY_AND_ASSIGN(TestPlatformEventObserver);
};

class PlatformEventTest : public testing::Test {
 public:
  PlatformEventTest() {}
  virtual ~PlatformEventTest() {}

  TestPlatformEventSource* source() { return source_.get(); }

 protected:
  // testing::Test:
  virtual void SetUp() OVERRIDE {
    source_.reset(new TestPlatformEventSource());
  }

 private:
  scoped_ptr<TestPlatformEventSource> source_;

  DISALLOW_COPY_AND_ASSIGN(PlatformEventTest);
};

// Tests that a dispatcher receives an event.
TEST_F(PlatformEventTest, DispatcherBasic) {
  std::vector<int> list_dispatcher;
  scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
  source()->Dispatch(*event);
  EXPECT_EQ(0u, list_dispatcher.size());
  {
    TestPlatformEventDispatcher dispatcher(1, &list_dispatcher);

    scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
    source()->Dispatch(*event);
    ASSERT_EQ(1u, list_dispatcher.size());
    EXPECT_EQ(1, list_dispatcher[0]);
  }

  list_dispatcher.clear();
  event = CreatePlatformEvent();
  source()->Dispatch(*event);
  EXPECT_EQ(0u, list_dispatcher.size());
}

// Tests that dispatchers receive events in the correct order.
TEST_F(PlatformEventTest, DispatcherOrder) {
  std::vector<int> list_dispatcher;
  int sequence[] = {21, 3, 6, 45};
  ScopedVector<TestPlatformEventDispatcher> dispatchers;
  for (size_t i = 0; i < arraysize(sequence); ++i) {
    dispatchers.push_back(
        new TestPlatformEventDispatcher(sequence[i], &list_dispatcher));
  }
  scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
  source()->Dispatch(*event);
  ASSERT_EQ(arraysize(sequence), list_dispatcher.size());
  EXPECT_EQ(std::vector<int>(sequence, sequence + arraysize(sequence)),
            list_dispatcher);
}

// Tests that if a dispatcher consumes the event, the subsequent dispatchers do
// not receive the event.
TEST_F(PlatformEventTest, DispatcherConsumesEventToStopDispatch) {
  std::vector<int> list_dispatcher;
  TestPlatformEventDispatcher first(12, &list_dispatcher);
  TestPlatformEventDispatcher second(23, &list_dispatcher);

  scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
  source()->Dispatch(*event);
  ASSERT_EQ(2u, list_dispatcher.size());
  EXPECT_EQ(12, list_dispatcher[0]);
  EXPECT_EQ(23, list_dispatcher[1]);
  list_dispatcher.clear();

  first.set_post_dispatch_action(POST_DISPATCH_STOP_PROPAGATION);
  event = CreatePlatformEvent();
  source()->Dispatch(*event);
  ASSERT_EQ(1u, list_dispatcher.size());
  EXPECT_EQ(12, list_dispatcher[0]);
}

// Tests that observers receive events.
TEST_F(PlatformEventTest, ObserverBasic) {
  std::vector<int> list_observer;
  scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
  source()->Dispatch(*event);
  EXPECT_EQ(0u, list_observer.size());
  {
    TestPlatformEventObserver observer(31, &list_observer);

    scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
    source()->Dispatch(*event);
    ASSERT_EQ(1u, list_observer.size());
    EXPECT_EQ(31, list_observer[0]);
  }

  list_observer.clear();
  event = CreatePlatformEvent();
  source()->Dispatch(*event);
  EXPECT_EQ(0u, list_observer.size());
}

// Tests that observers receive events in the correct order.
TEST_F(PlatformEventTest, ObserverOrder) {
  std::vector<int> list_observer;
  const int sequence[] = {21, 3, 6, 45};
  ScopedVector<TestPlatformEventObserver> observers;
  for (size_t i = 0; i < arraysize(sequence); ++i) {
    observers.push_back(
        new TestPlatformEventObserver(sequence[i], &list_observer));
  }
  scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
  source()->Dispatch(*event);
  ASSERT_EQ(arraysize(sequence), list_observer.size());
  EXPECT_EQ(std::vector<int>(sequence, sequence + arraysize(sequence)),
            list_observer);
}

// Tests that observers and dispatchers receive events in the correct order.
TEST_F(PlatformEventTest, DispatcherAndObserverOrder) {
  std::vector<int> list;
  TestPlatformEventDispatcher first_d(12, &list);
  TestPlatformEventObserver first_o(10, &list);
  TestPlatformEventDispatcher second_d(23, &list);
  TestPlatformEventObserver second_o(20, &list);
  scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
  source()->Dispatch(*event);
  const int expected[] = {10, 20, 12, 23};
  EXPECT_EQ(std::vector<int>(expected, expected + arraysize(expected)), list);
}

// Tests that an overridden dispatcher receives events before the default
// dispatchers.
TEST_F(PlatformEventTest, OverriddenDispatcherBasic) {
  std::vector<int> list;
  TestPlatformEventDispatcher dispatcher(10, &list);
  TestPlatformEventObserver observer(15, &list);
  scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
  source()->Dispatch(*event);
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(15, list[0]);
  EXPECT_EQ(10, list[1]);
  list.clear();

  TestPlatformEventDispatcher overriding_dispatcher(20, &list);
  source()->RemovePlatformEventDispatcher(&overriding_dispatcher);
  scoped_ptr<ScopedEventDispatcher> handle =
      source()->OverrideDispatcher(&overriding_dispatcher);
  source()->Dispatch(*event);
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(15, list[0]);
  EXPECT_EQ(20, list[1]);
}

// Tests that an overridden dispatcher can request that the default dispatchers
// can dispatch the events.
TEST_F(PlatformEventTest, OverriddenDispatcherInvokeDefaultDispatcher) {
  std::vector<int> list;
  TestPlatformEventDispatcher dispatcher(10, &list);
  TestPlatformEventObserver observer(15, &list);
  TestPlatformEventDispatcher overriding_dispatcher(20, &list);
  source()->RemovePlatformEventDispatcher(&overriding_dispatcher);
  scoped_ptr<ScopedEventDispatcher> handle =
      source()->OverrideDispatcher(&overriding_dispatcher);
  overriding_dispatcher.set_post_dispatch_action(POST_DISPATCH_PERFORM_DEFAULT);

  scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
  source()->Dispatch(*event);
  // First the observer, then the overriding dispatcher, then the default
  // dispatcher.
  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(15, list[0]);
  EXPECT_EQ(20, list[1]);
  EXPECT_EQ(10, list[2]);
  list.clear();

  // Install a second overriding dispatcher.
  TestPlatformEventDispatcher second_overriding(50, &list);
  source()->RemovePlatformEventDispatcher(&second_overriding);
  scoped_ptr<ScopedEventDispatcher> second_override_handle =
      source()->OverrideDispatcher(&second_overriding);
  source()->Dispatch(*event);
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(15, list[0]);
  EXPECT_EQ(50, list[1]);
  list.clear();

  second_overriding.set_post_dispatch_action(POST_DISPATCH_PERFORM_DEFAULT);
  source()->Dispatch(*event);
  // First the observer, then the second overriding dispatcher, then the default
  // dispatcher.
  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(15, list[0]);
  EXPECT_EQ(50, list[1]);
  EXPECT_EQ(10, list[2]);
}

// Provides mechanism for running tests from inside an active message-loop.
class PlatformEventTestWithMessageLoop : public PlatformEventTest {
 public:
  PlatformEventTestWithMessageLoop() {}
  virtual ~PlatformEventTestWithMessageLoop() {}

  void Run() {
    message_loop_.PostTask(
        FROM_HERE,
        base::Bind(&PlatformEventTestWithMessageLoop::RunTest,
                   base::Unretained(this)));
    message_loop_.Run();
  }

 protected:
  void RunTest() {
    RunTestImpl();
    message_loop_.Quit();
  }

  virtual void RunTestImpl() = 0;

 private:
  base::MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(PlatformEventTestWithMessageLoop);
};

#define RUN_TEST_IN_MESSAGE_LOOP(name) \
  TEST_F(name, Run) { Run(); }

// Tests that a ScopedEventDispatcher restores the previous dispatcher when
// destroyed.
class ScopedDispatcherRestoresAfterDestroy
    : public PlatformEventTestWithMessageLoop {
 public:
  // PlatformEventTestWithMessageLoop:
  virtual void RunTestImpl() OVERRIDE {
    std::vector<int> list;
    TestPlatformEventDispatcher dispatcher(10, &list);
    TestPlatformEventObserver observer(15, &list);

    TestPlatformEventDispatcher first_overriding(20, &list);
    source()->RemovePlatformEventDispatcher(&first_overriding);
    scoped_ptr<ScopedEventDispatcher> first_override_handle =
        source()->OverrideDispatcher(&first_overriding);

    // Install a second overriding dispatcher.
    TestPlatformEventDispatcher second_overriding(50, &list);
    source()->RemovePlatformEventDispatcher(&second_overriding);
    scoped_ptr<ScopedEventDispatcher> second_override_handle =
        source()->OverrideDispatcher(&second_overriding);

    scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
    source()->Dispatch(*event);
    ASSERT_EQ(2u, list.size());
    EXPECT_EQ(15, list[0]);
    EXPECT_EQ(50, list[1]);
    list.clear();

    second_override_handle.reset();
    source()->Dispatch(*event);
    ASSERT_EQ(2u, list.size());
    EXPECT_EQ(15, list[0]);
    EXPECT_EQ(20, list[1]);
  }
};

RUN_TEST_IN_MESSAGE_LOOP(ScopedDispatcherRestoresAfterDestroy)

// This dispatcher destroys the handle to the ScopedEventDispatcher when
// dispatching an event.
class DestroyScopedHandleDispatcher : public TestPlatformEventDispatcher {
 public:
  DestroyScopedHandleDispatcher(int id, std::vector<int>* list)
      : TestPlatformEventDispatcher(id, list) {}
  virtual ~DestroyScopedHandleDispatcher() {}

  void SetScopedHandle(scoped_ptr<ScopedEventDispatcher> handler) {
    handler_ = handler.Pass();
  }

 private:
  // PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const PlatformEvent& event) OVERRIDE {
    return true;
  }

  virtual uint32_t DispatchEvent(const PlatformEvent& event) OVERRIDE {
    handler_.reset();
    return TestPlatformEventDispatcher::DispatchEvent(event);
  }

  scoped_ptr<ScopedEventDispatcher> handler_;

  DISALLOW_COPY_AND_ASSIGN(DestroyScopedHandleDispatcher);
};

// Tests that resetting an overridden dispatcher causes the nested message-loop
// iteration to stop and the rest of the events are dispatched in the next
// iteration.
class DestroyedNestedOverriddenDispatcherQuitsNestedLoopIteration
    : public PlatformEventTestWithMessageLoop {
 public:
  void NestedTask(std::vector<int>* list,
                  TestPlatformEventDispatcher* dispatcher) {
    ScopedVector<PlatformEvent> events;
    scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
    events.push_back(event.release());
    event = CreatePlatformEvent();
    events.push_back(event.release());

    // Attempt to dispatch a couple of events. Dispatching the first event will
    // have terminated the ScopedEventDispatcher object, which will terminate
    // the current iteration of the message-loop.
    size_t count = source()->DispatchEventStream(events);
    EXPECT_EQ(1u, count);
    ASSERT_EQ(2u, list->size());
    EXPECT_EQ(15, (*list)[0]);
    EXPECT_EQ(20, (*list)[1]);
    list->clear();

    ASSERT_LT(count, events.size());
    events.erase(events.begin(), events.begin() + count);

    count = source()->DispatchEventStream(events);
    EXPECT_EQ(1u, count);
    ASSERT_EQ(2u, list->size());
    EXPECT_EQ(15, (*list)[0]);
    EXPECT_EQ(10, (*list)[1]);
    list->clear();

    // Terminate the message-loop.
    base::MessageLoopForUI::current()->QuitNow();
  }

  // PlatformEventTestWithMessageLoop:
  virtual void RunTestImpl() OVERRIDE {
    std::vector<int> list;
    TestPlatformEventDispatcher dispatcher(10, &list);
    TestPlatformEventObserver observer(15, &list);

    DestroyScopedHandleDispatcher overriding(20, &list);
    source()->RemovePlatformEventDispatcher(&overriding);
    scoped_ptr<ScopedEventDispatcher> override_handle =
        source()->OverrideDispatcher(&overriding);

    scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
    source()->Dispatch(*event);
    ASSERT_EQ(2u, list.size());
    EXPECT_EQ(15, list[0]);
    EXPECT_EQ(20, list[1]);
    list.clear();

    overriding.SetScopedHandle(override_handle.Pass());
    base::RunLoop run_loop;
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoopForUI::ScopedNestableTaskAllower allow_nested(loop);
    loop->PostTask(
        FROM_HERE,
        base::Bind(
            &DestroyedNestedOverriddenDispatcherQuitsNestedLoopIteration::
                NestedTask,
            base::Unretained(this),
            base::Unretained(&list),
            base::Unretained(&overriding)));
    run_loop.Run();

    // Dispatching the event should now reach the default dispatcher.
    source()->Dispatch(*event);
    ASSERT_EQ(2u, list.size());
    EXPECT_EQ(15, list[0]);
    EXPECT_EQ(10, list[1]);
  }
};

RUN_TEST_IN_MESSAGE_LOOP(
    DestroyedNestedOverriddenDispatcherQuitsNestedLoopIteration)

// Tests that resetting an overridden dispatcher, and installing another
// overridden dispatcher before the nested message-loop completely unwinds
// function correctly.
class ConsecutiveOverriddenDispatcherInTheSameMessageLoopIteration
    : public PlatformEventTestWithMessageLoop {
 public:
  void NestedTask(scoped_ptr<ScopedEventDispatcher> dispatch_handle,
                  std::vector<int>* list) {
    scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
    source()->Dispatch(*event);
    ASSERT_EQ(2u, list->size());
    EXPECT_EQ(15, (*list)[0]);
    EXPECT_EQ(20, (*list)[1]);
    list->clear();

    // Reset the override dispatcher. This should restore the default
    // dispatcher.
    dispatch_handle.reset();
    source()->Dispatch(*event);
    ASSERT_EQ(2u, list->size());
    EXPECT_EQ(15, (*list)[0]);
    EXPECT_EQ(10, (*list)[1]);
    list->clear();

    // Install another override-dispatcher.
    DestroyScopedHandleDispatcher second_overriding(70, list);
    source()->RemovePlatformEventDispatcher(&second_overriding);
    scoped_ptr<ScopedEventDispatcher> second_override_handle =
        source()->OverrideDispatcher(&second_overriding);

    source()->Dispatch(*event);
    ASSERT_EQ(2u, list->size());
    EXPECT_EQ(15, (*list)[0]);
    EXPECT_EQ(70, (*list)[1]);
    list->clear();

    second_overriding.SetScopedHandle(second_override_handle.Pass());
    second_overriding.set_post_dispatch_action(POST_DISPATCH_QUIT_LOOP);
    base::RunLoop run_loop;
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoopForUI::ScopedNestableTaskAllower allow_nested(loop);
    loop->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&TestPlatformEventSource::Dispatch),
                   base::Unretained(source()),
                   *event));
    run_loop.Run();
    ASSERT_EQ(2u, list->size());
    EXPECT_EQ(15, (*list)[0]);
    EXPECT_EQ(70, (*list)[1]);
    list->clear();

    // Terminate the message-loop.
    base::MessageLoopForUI::current()->QuitNow();
  }

  // PlatformEventTestWithMessageLoop:
  virtual void RunTestImpl() OVERRIDE {
    std::vector<int> list;
    TestPlatformEventDispatcher dispatcher(10, &list);
    TestPlatformEventObserver observer(15, &list);

    TestPlatformEventDispatcher overriding(20, &list);
    source()->RemovePlatformEventDispatcher(&overriding);
    scoped_ptr<ScopedEventDispatcher> override_handle =
        source()->OverrideDispatcher(&overriding);

    scoped_ptr<PlatformEvent> event(CreatePlatformEvent());
    source()->Dispatch(*event);
    ASSERT_EQ(2u, list.size());
    EXPECT_EQ(15, list[0]);
    EXPECT_EQ(20, list[1]);
    list.clear();

    // Start a nested message-loop, and destroy |override_handle| in the nested
    // loop. That should terminate the nested loop, restore the previous
    // dispatchers, and return control to this function.
    base::RunLoop run_loop;
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoopForUI::ScopedNestableTaskAllower allow_nested(loop);
    loop->PostTask(
        FROM_HERE,
        base::Bind(
            &ConsecutiveOverriddenDispatcherInTheSameMessageLoopIteration::
                NestedTask,
            base::Unretained(this),
            base::Passed(&override_handle),
            base::Unretained(&list)));
    run_loop.Run();

    // Dispatching the event should now reach the default dispatcher.
    source()->Dispatch(*event);
    ASSERT_EQ(2u, list.size());
    EXPECT_EQ(15, list[0]);
    EXPECT_EQ(10, list[1]);
  }
};

RUN_TEST_IN_MESSAGE_LOOP(
    ConsecutiveOverriddenDispatcherInTheSameMessageLoopIteration)

}  // namespace ui
