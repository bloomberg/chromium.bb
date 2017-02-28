// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/PerformanceMonitor.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/Location.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"

#include <memory>

namespace blink {

class PerformanceMonitorTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;
  LocalFrame* frame() const { return m_pageHolder->document().frame(); }
  ExecutionContext* executionContext() const {
    return &m_pageHolder->document();
  }
  LocalFrame* anotherFrame() const {
    return m_anotherPageHolder->document().frame();
  }
  ExecutionContext* anotherExecutionContext() const {
    return &m_anotherPageHolder->document();
  }

  void willExecuteScript(ExecutionContext* executionContext) {
    m_monitor->willExecuteScript(executionContext);
  }

  // scheduler::TaskTimeObserver implementation
  void willProcessTask(scheduler::TaskQueue* queue, double startTime) {
    m_monitor->willProcessTask(queue, startTime);
  }

  void didProcessTask(scheduler::TaskQueue* queue,
                      double startTime,
                      double endTime) {
    m_monitor->didProcessTask(queue, startTime, endTime);
  }

  String frameContextURL();
  int numUniqueFrameContextsSeen();

  Persistent<PerformanceMonitor> m_monitor;
  std::unique_ptr<DummyPageHolder> m_pageHolder;
  std::unique_ptr<DummyPageHolder> m_anotherPageHolder;
};

void PerformanceMonitorTest::SetUp() {
  m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
  m_pageHolder->document().setURL(KURL(KURL(), "https://example.com/foo"));
  m_monitor = new PerformanceMonitor(frame());

  // Create another dummy page holder and pretend this is the iframe.
  m_anotherPageHolder = DummyPageHolder::create(IntSize(400, 300));
  m_anotherPageHolder->document().setURL(
      KURL(KURL(), "https://iframed.com/bar"));
}

void PerformanceMonitorTest::TearDown() {
  m_monitor->shutdown();
}

String PerformanceMonitorTest::frameContextURL() {
  // This is reported only if there is a single frameContext URL.
  if (m_monitor->m_taskHasMultipleContexts)
    return "";
  Frame* frame = toDocument(m_monitor->m_taskExecutionContext)->frame();
  return toLocalFrame(frame)->document()->location()->href();
}

int PerformanceMonitorTest::numUniqueFrameContextsSeen() {
  if (!m_monitor->m_taskExecutionContext)
    return 0;
  if (!m_monitor->m_taskHasMultipleContexts)
    return 1;
  return 2;
}

TEST_F(PerformanceMonitorTest, SingleScriptInTask) {
  willProcessTask(nullptr, 3719349.445172);
  EXPECT_EQ(0, numUniqueFrameContextsSeen());
  willExecuteScript(executionContext());
  EXPECT_EQ(1, numUniqueFrameContextsSeen());
  didProcessTask(nullptr, 3719349.445172, 3719349.5561923);  // Long task
  EXPECT_EQ(1, numUniqueFrameContextsSeen());
  EXPECT_EQ("https://example.com/foo", frameContextURL());
}

TEST_F(PerformanceMonitorTest, MultipleScriptsInTask_SingleContext) {
  willProcessTask(nullptr, 3719349.445172);
  EXPECT_EQ(0, numUniqueFrameContextsSeen());
  willExecuteScript(executionContext());
  EXPECT_EQ(1, numUniqueFrameContextsSeen());
  EXPECT_EQ("https://example.com/foo", frameContextURL());

  willExecuteScript(executionContext());
  EXPECT_EQ(1, numUniqueFrameContextsSeen());
  didProcessTask(nullptr, 3719349.445172, 3719349.5561923);  // Long task
  EXPECT_EQ(1, numUniqueFrameContextsSeen());
  EXPECT_EQ("https://example.com/foo", frameContextURL());
}

TEST_F(PerformanceMonitorTest, MultipleScriptsInTask_MultipleContexts) {
  willProcessTask(nullptr, 3719349.445172);
  EXPECT_EQ(0, numUniqueFrameContextsSeen());
  willExecuteScript(executionContext());
  EXPECT_EQ(1, numUniqueFrameContextsSeen());
  EXPECT_EQ("https://example.com/foo", frameContextURL());

  willExecuteScript(anotherExecutionContext());
  EXPECT_EQ(2, numUniqueFrameContextsSeen());
  didProcessTask(nullptr, 3719349.445172, 3719349.5561923);  // Long task
  EXPECT_EQ(2, numUniqueFrameContextsSeen());
  EXPECT_EQ("", frameContextURL());
}

TEST_F(PerformanceMonitorTest, NoScriptInLongTask) {
  willProcessTask(nullptr, 3719349.445172);
  willExecuteScript(executionContext());
  didProcessTask(nullptr, 3719349.445172, 3719349.445182);

  willProcessTask(nullptr, 3719349.445172);
  didProcessTask(nullptr, 3719349.445172, 3719349.5561923);  // Long task
  // Without presence of Script, FrameContext URL is not available
  EXPECT_EQ(0, numUniqueFrameContextsSeen());
}

}  // namespace blink
