// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObserver.h"

#include "core/dom/IntersectionObserverCallback.h"
#include "core/dom/IntersectionObserverInit.h"
#include "core/frame/FrameView.h"
#include "platform/testing/UnitTestHelpers.h"
#include "web/WebViewImpl.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"
#include "wtf/CurrentTime.h"

namespace blink {

namespace {

class TestIntersectionObserverCallback : public IntersectionObserverCallback {
 public:
  TestIntersectionObserverCallback(Document& document)
      : m_document(document), m_callCount(0) {}
  void handleEvent(const HeapVector<Member<IntersectionObserverEntry>>&,
                   IntersectionObserver&) override {
    m_callCount++;
  }
  ExecutionContext* getExecutionContext() const override { return m_document; }
  int callCount() const { return m_callCount; }

  DEFINE_INLINE_TRACE() {
    IntersectionObserverCallback::trace(visitor);
    visitor->trace(m_document);
  }

 private:
  Member<Document> m_document;
  int m_callCount;
};

}  // namespace

class IntersectionObserverTest : public SimTest {};

TEST_F(IntersectionObserverTest, ObserveSchedulesFrame) {
  SimRequest mainResource("https://example.com/", "text/html");
  loadURL("https://example.com/");
  mainResource.complete("<div id='target'></div>");

  IntersectionObserverInit observerInit;
  DummyExceptionStateForTesting exceptionState;
  TestIntersectionObserverCallback* observerCallback =
      new TestIntersectionObserverCallback(document());
  IntersectionObserver* observer = IntersectionObserver::create(
      observerInit, *observerCallback, exceptionState);
  ASSERT_FALSE(exceptionState.hadException());

  compositor().beginFrame();
  ASSERT_FALSE(compositor().needsBeginFrame());
  EXPECT_TRUE(observer->takeRecords(exceptionState).isEmpty());
  EXPECT_EQ(observerCallback->callCount(), 0);

  Element* target = document().getElementById("target");
  ASSERT_TRUE(target);
  observer->observe(target, exceptionState);
  EXPECT_TRUE(compositor().needsBeginFrame());
}

TEST_F(IntersectionObserverTest, ResumePostsTask) {
  webView().resize(WebSize(800, 600));
  SimRequest mainResource("https://example.com/", "text/html");
  loadURL("https://example.com/");
  mainResource.complete(
      "<div id='leading-space' style='height: 700px;'></div>"
      "<div id='target'></div>"
      "<div id='trailing-space' style='height: 700px;'></div>");

  IntersectionObserverInit observerInit;
  DummyExceptionStateForTesting exceptionState;
  TestIntersectionObserverCallback* observerCallback =
      new TestIntersectionObserverCallback(document());
  IntersectionObserver* observer = IntersectionObserver::create(
      observerInit, *observerCallback, exceptionState);
  ASSERT_FALSE(exceptionState.hadException());

  Element* target = document().getElementById("target");
  ASSERT_TRUE(target);
  observer->observe(target, exceptionState);

  compositor().beginFrame();
  testing::runPendingTasks();
  EXPECT_EQ(observerCallback->callCount(), 1);

  // When document is not suspended, beginFrame() will generate notifications
  // and post a task to deliver them.
  document().view()->layoutViewportScrollableArea()->setScrollOffset(
      ScrollOffset(0, 300), ProgrammaticScroll);
  compositor().beginFrame();
  EXPECT_EQ(observerCallback->callCount(), 1);
  testing::runPendingTasks();
  EXPECT_EQ(observerCallback->callCount(), 2);

  // When a document is suspended, beginFrame() will generate a notification,
  // but it will not be delivered.  The notification will, however, be
  // available via takeRecords();
  document().suspendScheduledTasks();
  document().view()->layoutViewportScrollableArea()->setScrollOffset(
      ScrollOffset(0, 0), ProgrammaticScroll);
  compositor().beginFrame();
  EXPECT_EQ(observerCallback->callCount(), 2);
  testing::runPendingTasks();
  EXPECT_EQ(observerCallback->callCount(), 2);
  EXPECT_FALSE(observer->takeRecords(exceptionState).isEmpty());

  // Generate a notification while document is suspended; then resume document.
  // Notification should happen in a post task.
  document().view()->layoutViewportScrollableArea()->setScrollOffset(
      ScrollOffset(0, 300), ProgrammaticScroll);
  compositor().beginFrame();
  testing::runPendingTasks();
  EXPECT_EQ(observerCallback->callCount(), 2);
  document().resumeScheduledTasks();
  EXPECT_EQ(observerCallback->callCount(), 2);
  testing::runPendingTasks();
  EXPECT_EQ(observerCallback->callCount(), 3);
}

TEST_F(IntersectionObserverTest, DisconnectClearsNotifications) {
  webView().resize(WebSize(800, 600));
  SimRequest mainResource("https://example.com/", "text/html");
  loadURL("https://example.com/");
  mainResource.complete(
      "<div id='leading-space' style='height: 700px;'></div>"
      "<div id='target'></div>"
      "<div id='trailing-space' style='height: 700px;'></div>");

  IntersectionObserverInit observerInit;
  DummyExceptionStateForTesting exceptionState;
  TestIntersectionObserverCallback* observerCallback =
      new TestIntersectionObserverCallback(document());
  IntersectionObserver* observer = IntersectionObserver::create(
      observerInit, *observerCallback, exceptionState);
  ASSERT_FALSE(exceptionState.hadException());

  Element* target = document().getElementById("target");
  ASSERT_TRUE(target);
  observer->observe(target, exceptionState);

  compositor().beginFrame();
  testing::runPendingTasks();
  EXPECT_EQ(observerCallback->callCount(), 1);

  // If disconnect() is called while an observer has unsent notifications,
  // those notifications should be discarded.
  document().view()->layoutViewportScrollableArea()->setScrollOffset(
      ScrollOffset(0, 300), ProgrammaticScroll);
  compositor().beginFrame();
  observer->disconnect();
  testing::runPendingTasks();
  EXPECT_EQ(observerCallback->callCount(), 1);
}

}  // namespace blink
