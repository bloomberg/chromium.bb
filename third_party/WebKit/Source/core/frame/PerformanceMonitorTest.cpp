// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/PerformanceMonitor.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/Location.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/wtf/PtrUtil.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace blink {

class PerformanceMonitorTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;
  LocalFrame* GetFrame() const {
    return page_holder_->GetDocument().GetFrame();
  }
  ExecutionContext* GetExecutionContext() const {
    return &page_holder_->GetDocument();
  }
  LocalFrame* AnotherFrame() const {
    return another_page_holder_->GetDocument().GetFrame();
  }
  ExecutionContext* AnotherExecutionContext() const {
    return &another_page_holder_->GetDocument();
  }

  void WillExecuteScript(ExecutionContext* execution_context) {
    monitor_->WillExecuteScript(execution_context);
  }

  // scheduler::TaskTimeObserver implementation
  void WillProcessTask(double start_time) {
    monitor_->WillProcessTask(start_time);
  }

  void DidProcessTask(double start_time, double end_time) {
    monitor_->DidProcessTask(start_time, end_time);
  }

  String FrameContextURL();
  int NumUniqueFrameContextsSeen();

  Persistent<PerformanceMonitor> monitor_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  std::unique_ptr<DummyPageHolder> another_page_holder_;
};

void PerformanceMonitorTest::SetUp() {
  page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
  page_holder_->GetDocument().SetURL(
      KURL(NullURL(), "https://example.com/foo"));
  monitor_ = new PerformanceMonitor(GetFrame());

  // Create another dummy page holder and pretend this is the iframe.
  another_page_holder_ = DummyPageHolder::Create(IntSize(400, 300));
  another_page_holder_->GetDocument().SetURL(
      KURL(NullURL(), "https://iframed.com/bar"));
}

void PerformanceMonitorTest::TearDown() {
  monitor_->Shutdown();
}

String PerformanceMonitorTest::FrameContextURL() {
  // This is reported only if there is a single frameContext URL.
  if (monitor_->task_has_multiple_contexts_)
    return "";
  Frame* frame = ToDocument(monitor_->task_execution_context_)->GetFrame();
  return ToLocalFrame(frame)->GetDocument()->location()->href();
}

int PerformanceMonitorTest::NumUniqueFrameContextsSeen() {
  if (!monitor_->task_execution_context_)
    return 0;
  if (!monitor_->task_has_multiple_contexts_)
    return 1;
  return 2;
}

TEST_F(PerformanceMonitorTest, SingleScriptInTask) {
  WillProcessTask(3719349.445172);
  EXPECT_EQ(0, NumUniqueFrameContextsSeen());
  WillExecuteScript(GetExecutionContext());
  EXPECT_EQ(1, NumUniqueFrameContextsSeen());
  DidProcessTask(3719349.445172, 3719349.5561923);  // Long task
  EXPECT_EQ(1, NumUniqueFrameContextsSeen());
  EXPECT_EQ("https://example.com/foo", FrameContextURL());
}

TEST_F(PerformanceMonitorTest, MultipleScriptsInTask_SingleContext) {
  WillProcessTask(3719349.445172);
  EXPECT_EQ(0, NumUniqueFrameContextsSeen());
  WillExecuteScript(GetExecutionContext());
  EXPECT_EQ(1, NumUniqueFrameContextsSeen());
  EXPECT_EQ("https://example.com/foo", FrameContextURL());

  WillExecuteScript(GetExecutionContext());
  EXPECT_EQ(1, NumUniqueFrameContextsSeen());
  DidProcessTask(3719349.445172, 3719349.5561923);  // Long task
  EXPECT_EQ(1, NumUniqueFrameContextsSeen());
  EXPECT_EQ("https://example.com/foo", FrameContextURL());
}

TEST_F(PerformanceMonitorTest, MultipleScriptsInTask_MultipleContexts) {
  WillProcessTask(3719349.445172);
  EXPECT_EQ(0, NumUniqueFrameContextsSeen());
  WillExecuteScript(GetExecutionContext());
  EXPECT_EQ(1, NumUniqueFrameContextsSeen());
  EXPECT_EQ("https://example.com/foo", FrameContextURL());

  WillExecuteScript(AnotherExecutionContext());
  EXPECT_EQ(2, NumUniqueFrameContextsSeen());
  DidProcessTask(3719349.445172, 3719349.5561923);  // Long task
  EXPECT_EQ(2, NumUniqueFrameContextsSeen());
  EXPECT_EQ("", FrameContextURL());
}

TEST_F(PerformanceMonitorTest, NoScriptInLongTask) {
  WillProcessTask(3719349.445172);
  WillExecuteScript(GetExecutionContext());
  DidProcessTask(3719349.445172, 3719349.445182);

  WillProcessTask(3719349.445172);
  DidProcessTask(3719349.445172, 3719349.5561923);  // Long task
  // Without presence of Script, FrameContext URL is not available
  EXPECT_EQ(0, NumUniqueFrameContextsSeen());
}

}  // namespace blink
