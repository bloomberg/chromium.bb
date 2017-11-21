// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/intersection_observer/IntersectionObserver.h"

#include "core/exported/WebViewImpl.h"
#include "core/frame/LocalFrameView.h"
#include "core/intersection_observer/IntersectionObserverDelegate.h"
#include "core/intersection_observer/IntersectionObserverInit.h"
#include "core/testing/sim/SimCompositor.h"
#include "core/testing/sim/SimDisplayItemList.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/Time.h"

namespace blink {

namespace {

class TestIntersectionObserverDelegate : public IntersectionObserverDelegate {
 public:
  TestIntersectionObserverDelegate(Document& document)
      : document_(document), call_count_(0) {}
  void Deliver(const HeapVector<Member<IntersectionObserverEntry>>&,
               IntersectionObserver&) override {
    call_count_++;
  }
  ExecutionContext* GetExecutionContext() const override { return document_; }
  int CallCount() const { return call_count_; }

  void Trace(blink::Visitor* visitor) {
    IntersectionObserverDelegate::Trace(visitor);
    visitor->Trace(document_);
  }

 private:
  Member<Document> document_;
  int call_count_;
};

}  // namespace

class IntersectionObserverTest : public SimTest {};

TEST_F(IntersectionObserverTest, ObserveSchedulesFrame) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<div id='target'></div>");

  IntersectionObserverInit observer_init;
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      new TestIntersectionObserverDelegate(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_TRUE(observer->takeRecords(exception_state).IsEmpty());
  EXPECT_EQ(observer_delegate->CallCount(), 0);

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);
  EXPECT_TRUE(Compositor().NeedsBeginFrame());
}

TEST_F(IntersectionObserverTest, ResumePostsTask) {
  WebView().Resize(WebSize(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='leading-space' style='height: 700px;'></div>
    <div id='target'></div>
    <div id='trailing-space' style='height: 700px;'></div>
  )HTML");

  IntersectionObserverInit observer_init;
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      new TestIntersectionObserverDelegate(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);

  Compositor().BeginFrame();
  testing::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);

  // When document is not suspended, beginFrame() will generate notifications
  // and post a task to deliver them.
  GetDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 300), kProgrammaticScroll);
  Compositor().BeginFrame();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  testing::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);

  // When a document is suspended, beginFrame() will generate a notification,
  // but it will not be delivered.  The notification will, however, be
  // available via takeRecords();
  GetDocument().PauseScheduledTasks();
  GetDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 0), kProgrammaticScroll);
  Compositor().BeginFrame();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  testing::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_FALSE(observer->takeRecords(exception_state).IsEmpty());

  // Generate a notification while document is suspended; then resume document.
  // Notification should happen in a post task.
  GetDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 300), kProgrammaticScroll);
  Compositor().BeginFrame();
  testing::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  GetDocument().UnpauseScheduledTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  testing::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 3);
}

TEST_F(IntersectionObserverTest, DisconnectClearsNotifications) {
  WebView().Resize(WebSize(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='leading-space' style='height: 700px;'></div>
    <div id='target'></div>
    <div id='trailing-space' style='height: 700px;'></div>
  )HTML");

  IntersectionObserverInit observer_init;
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      new TestIntersectionObserverDelegate(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);

  Compositor().BeginFrame();
  testing::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);

  // If disconnect() is called while an observer has unsent notifications,
  // those notifications should be discarded.
  GetDocument().View()->LayoutViewportScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 300), kProgrammaticScroll);
  Compositor().BeginFrame();
  observer->disconnect();
  testing::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
}

}  // namespace blink
