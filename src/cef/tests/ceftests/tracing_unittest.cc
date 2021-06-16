// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_bind.h"
#include "include/cef_file_util.h"
#include "include/cef_task.h"
#include "include/cef_trace.h"
#include "include/cef_waitable_event.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/file_util.h"

// Use the CEF version of the TRACE_* macros instead of the Chromium version.
#undef USING_CHROMIUM_INCLUDES
#include "include/base/cef_trace_event.h"

enum TracingTestType {
  TT_TRACE_EVENT0,
  TT_TRACE_EVENT1,
  TT_TRACE_EVENT2,
  TT_TRACE_EVENT_INSTANT0,
  TT_TRACE_EVENT_INSTANT1,
  TT_TRACE_EVENT_INSTANT2,
  TT_TRACE_EVENT_COPY_INSTANT0,
  TT_TRACE_EVENT_COPY_INSTANT1,
  TT_TRACE_EVENT_COPY_INSTANT2,
  TT_TRACE_EVENT_BEGIN0,
  TT_TRACE_EVENT_BEGIN1,
  TT_TRACE_EVENT_BEGIN2,
  TT_TRACE_EVENT_COPY_BEGIN0,
  TT_TRACE_EVENT_COPY_BEGIN1,
  TT_TRACE_EVENT_COPY_BEGIN2,
  TT_TRACE_EVENT_END0,
  TT_TRACE_EVENT_END1,
  TT_TRACE_EVENT_END2,
  TT_TRACE_EVENT_COPY_END0,
  TT_TRACE_EVENT_COPY_END1,
  TT_TRACE_EVENT_COPY_END2,
  TT_TRACE_COUNTER1,
  TT_TRACE_COPY_COUNTER1,
  TT_TRACE_COUNTER2,
  TT_TRACE_COPY_COUNTER2,
  TT_TRACE_COUNTER_ID1,
  TT_TRACE_COPY_COUNTER_ID1,
  TT_TRACE_COUNTER_ID2,
  TT_TRACE_COPY_COUNTER_ID2,
  TT_TRACE_EVENT_ASYNC_BEGIN0,
  TT_TRACE_EVENT_ASYNC_BEGIN1,
  TT_TRACE_EVENT_ASYNC_BEGIN2,
  TT_TRACE_EVENT_COPY_ASYNC_BEGIN0,
  TT_TRACE_EVENT_COPY_ASYNC_BEGIN1,
  TT_TRACE_EVENT_COPY_ASYNC_BEGIN2,
  TT_TRACE_EVENT_ASYNC_STEP_INTO0,
  TT_TRACE_EVENT_ASYNC_STEP_INTO1,
  TT_TRACE_EVENT_COPY_ASYNC_STEP_INTO0,
  TT_TRACE_EVENT_COPY_ASYNC_STEP_INTO1,
  TT_TRACE_EVENT_ASYNC_STEP_PAST0,
  TT_TRACE_EVENT_ASYNC_STEP_PAST1,
  TT_TRACE_EVENT_COPY_ASYNC_STEP_PAST0,
  TT_TRACE_EVENT_COPY_ASYNC_STEP_PAST1,
  TT_TRACE_EVENT_ASYNC_END0,
  TT_TRACE_EVENT_ASYNC_END1,
  TT_TRACE_EVENT_ASYNC_END2,
  TT_TRACE_EVENT_COPY_ASYNC_END0,
  TT_TRACE_EVENT_COPY_ASYNC_END1,
  TT_TRACE_EVENT_COPY_ASYNC_END2
};

const char kTraceTestCategory[] = "cef.client";

class TracingTestHandler : public CefEndTracingCallback,
                           public CefCompletionCallback {
 public:
  TracingTestHandler(TracingTestType type, const char* trace_type)
      : trace_type_(trace_type), type_(type) {
    completion_event_ = CefWaitableEvent::CreateWaitableEvent(true, false);
  }

  void ExecuteTest() {
    // Run the test.
    CefPostTask(TID_UI, base::Bind(&TracingTestHandler::BeginTracing, this));

    // Wait for the test to complete.
    completion_event_->Wait();

    // Verify the results.
    EXPECT_TRUE(!trace_data_.empty());
    EXPECT_TRUE(trace_type_ != nullptr);
    EXPECT_TRUE(strstr(trace_data_.c_str(), trace_type_) != nullptr);
  }

  void BeginTracing() {
    EXPECT_UI_THREAD();

    // Results in a call to OnComplete.
    CefBeginTracing(kTraceTestCategory, this);
  }

  void OnComplete() override {
    EXPECT_UI_THREAD();

    // Add some delay to avoid timing-related test failures.
    CefPostDelayedTask(TID_UI,
                       base::Bind(&TracingTestHandler::TestTracing, this), 50);
  }

  void TestTracing() {
    EXPECT_UI_THREAD();

    switch (type_) {
      case TT_TRACE_EVENT0: {
        TRACE_EVENT0(kTraceTestCategory, "TT_TRACE_EVENT0");
      } break;
      case TT_TRACE_EVENT1: {
        TRACE_EVENT1(kTraceTestCategory, "TT_TRACE_EVENT1", "arg1", 1);
      } break;
      case TT_TRACE_EVENT2: {
        TRACE_EVENT2(kTraceTestCategory, "TT_TRACE_EVENT2", "arg1", 1, "arg2",
                     2);
      } break;
      case TT_TRACE_EVENT_INSTANT0:
        TRACE_EVENT_INSTANT0(kTraceTestCategory, "TT_TRACE_EVENT_INSTANT0");
        break;
      case TT_TRACE_EVENT_INSTANT1:
        TRACE_EVENT_INSTANT1(kTraceTestCategory, "TT_TRACE_EVENT_INSTANT1",
                             "arg1", 1);
        break;
      case TT_TRACE_EVENT_INSTANT2:
        TRACE_EVENT_INSTANT2(kTraceTestCategory, "TT_TRACE_EVENT_INSTANT2",
                             "arg1", 1, "arg2", 2);
        break;
      case TT_TRACE_EVENT_COPY_INSTANT0:
        TRACE_EVENT_COPY_INSTANT0(kTraceTestCategory,
                                  "TT_TRACE_EVENT_COPY_INSTANT0");
        break;
      case TT_TRACE_EVENT_COPY_INSTANT1:
        TRACE_EVENT_COPY_INSTANT1(kTraceTestCategory,
                                  "TT_TRACE_EVENT_COPY_INSTANT1", "arg1", 1);
        break;
      case TT_TRACE_EVENT_COPY_INSTANT2:
        TRACE_EVENT_COPY_INSTANT2(kTraceTestCategory,
                                  "TT_TRACE_EVENT_COPY_INSTANT2", "arg1", 1,
                                  "arg2", 2);
        break;
      case TT_TRACE_EVENT_BEGIN0:
        TRACE_EVENT_BEGIN0(kTraceTestCategory, "TT_TRACE_EVENT_BEGIN0");
        break;
      case TT_TRACE_EVENT_BEGIN1:
        TRACE_EVENT_BEGIN1(kTraceTestCategory, "TT_TRACE_EVENT_BEGIN1", "arg1",
                           1);
        break;
      case TT_TRACE_EVENT_BEGIN2:
        TRACE_EVENT_BEGIN2(kTraceTestCategory, "TT_TRACE_EVENT_BEGIN2", "arg1",
                           1, "arg2", 2);
        break;
      case TT_TRACE_EVENT_COPY_BEGIN0:
        TRACE_EVENT_COPY_BEGIN0(kTraceTestCategory,
                                "TT_TRACE_EVENT_COPY_BEGIN0");
        break;
      case TT_TRACE_EVENT_COPY_BEGIN1:
        TRACE_EVENT_COPY_BEGIN1(kTraceTestCategory,
                                "TT_TRACE_EVENT_COPY_BEGIN1", "arg1", 1);
        break;
      case TT_TRACE_EVENT_COPY_BEGIN2:
        TRACE_EVENT_COPY_BEGIN2(kTraceTestCategory,
                                "TT_TRACE_EVENT_COPY_BEGIN2", "arg1", 1, "arg2",
                                2);
        break;
      case TT_TRACE_EVENT_END0:
        TRACE_EVENT_BEGIN0(kTraceTestCategory, "TT_TRACE_EVENT_END0");
        TRACE_EVENT_END0(kTraceTestCategory, "TT_TRACE_EVENT_END0");
        break;
      case TT_TRACE_EVENT_END1:
        TRACE_EVENT_BEGIN1(kTraceTestCategory, "TT_TRACE_EVENT_END1", "arg1",
                           1);
        TRACE_EVENT_END1(kTraceTestCategory, "TT_TRACE_EVENT_END1", "arg1", 1);
        break;
      case TT_TRACE_EVENT_END2:
        TRACE_EVENT_BEGIN2(kTraceTestCategory, "TT_TRACE_EVENT_END2", "arg1", 1,
                           "arg2", 2);
        TRACE_EVENT_END2(kTraceTestCategory, "TT_TRACE_EVENT_END2", "arg1", 1,
                         "arg2", 2);
        break;
      case TT_TRACE_EVENT_COPY_END0:
        TRACE_EVENT_COPY_BEGIN0(kTraceTestCategory, "TT_TRACE_EVENT_COPY_END0");
        TRACE_EVENT_COPY_END0(kTraceTestCategory, "TT_TRACE_EVENT_COPY_END0");
        break;
      case TT_TRACE_EVENT_COPY_END1:
        TRACE_EVENT_COPY_BEGIN1(kTraceTestCategory, "TT_TRACE_EVENT_COPY_END1",
                                "arg1", 1);
        TRACE_EVENT_COPY_END1(kTraceTestCategory, "TT_TRACE_EVENT_COPY_END1",
                              "arg1", 1);
        break;
      case TT_TRACE_EVENT_COPY_END2:
        TRACE_EVENT_COPY_BEGIN2(kTraceTestCategory, "TT_TRACE_EVENT_COPY_END2",
                                "arg1", 1, "arg2", 2);
        TRACE_EVENT_COPY_END2(kTraceTestCategory, "TT_TRACE_EVENT_COPY_END2",
                              "arg1", 1, "arg2", 2);
        break;
      case TT_TRACE_COUNTER1:
        TRACE_COUNTER1(kTraceTestCategory, "TT_TRACE_COUNTER1", 5);
        break;
      case TT_TRACE_COPY_COUNTER1:
        TRACE_COPY_COUNTER1(kTraceTestCategory, "TT_TRACE_COPY_COUNTER1", 5);
        break;
      case TT_TRACE_COUNTER2:
        TRACE_COUNTER2(kTraceTestCategory, "TT_TRACE_COUNTER2", "val1", 5,
                       "val2", 10);
        break;
      case TT_TRACE_COPY_COUNTER2:
        TRACE_COPY_COUNTER2(kTraceTestCategory, "TT_TRACE_COPY_COUNTER2",
                            "val1", 5, "val2", 10);
        break;
      case TT_TRACE_COUNTER_ID1:
        TRACE_COUNTER_ID1(kTraceTestCategory, "TT_TRACE_COUNTER_ID1", 100, 5);
        break;
      case TT_TRACE_COPY_COUNTER_ID1:
        TRACE_COPY_COUNTER_ID1(kTraceTestCategory, "TT_TRACE_COPY_COUNTER_ID1",
                               100, 5);
        break;
      case TT_TRACE_COUNTER_ID2:
        TRACE_COUNTER_ID2(kTraceTestCategory, "TT_TRACE_COUNTER_ID2", 100,
                          "val1", 5, "val2", 10);
        break;
      case TT_TRACE_COPY_COUNTER_ID2:
        TRACE_COPY_COUNTER_ID2(kTraceTestCategory, "TT_TRACE_COPY_COUNTER_ID2",
                               100, "val1", 5, "val2", 10);
        break;
      case TT_TRACE_EVENT_ASYNC_BEGIN0:
        TRACE_EVENT_ASYNC_BEGIN0(kTraceTestCategory,
                                 "TT_TRACE_EVENT_ASYNC_BEGIN0", 100);
        break;
      case TT_TRACE_EVENT_ASYNC_BEGIN1:
        TRACE_EVENT_ASYNC_BEGIN1(kTraceTestCategory,
                                 "TT_TRACE_EVENT_ASYNC_BEGIN1", 100, "arg1", 1);
        break;
      case TT_TRACE_EVENT_ASYNC_BEGIN2:
        TRACE_EVENT_ASYNC_BEGIN2(kTraceTestCategory,
                                 "TT_TRACE_EVENT_ASYNC_BEGIN2", 100, "arg1", 1,
                                 "arg2", 2);
        break;
      case TT_TRACE_EVENT_COPY_ASYNC_BEGIN0:
        TRACE_EVENT_COPY_ASYNC_BEGIN0(kTraceTestCategory,
                                      "TT_TRACE_EVENT_COPY_ASYNC_BEGIN0", 100);
        break;
      case TT_TRACE_EVENT_COPY_ASYNC_BEGIN1:
        TRACE_EVENT_COPY_ASYNC_BEGIN1(kTraceTestCategory,
                                      "TT_TRACE_EVENT_COPY_ASYNC_BEGIN1", 100,
                                      "arg1", 1);
        break;
      case TT_TRACE_EVENT_COPY_ASYNC_BEGIN2:
        TRACE_EVENT_COPY_ASYNC_BEGIN2(kTraceTestCategory,
                                      "TT_TRACE_EVENT_COPY_ASYNC_BEGIN2", 100,
                                      "arg1", 1, "arg2", 2);
        break;
      case TT_TRACE_EVENT_ASYNC_STEP_INTO0:
        TRACE_EVENT_ASYNC_STEP_INTO0(
            kTraceTestCategory, "TT_TRACE_EVENT_ASYNC_STEP_INTO0", 100, 1000);
        break;
      case TT_TRACE_EVENT_ASYNC_STEP_INTO1:
        TRACE_EVENT_ASYNC_STEP_INTO1(kTraceTestCategory,
                                     "TT_TRACE_EVENT_ASYNC_STEP_INTO1", 100,
                                     1000, "arg1", 1);
        break;
      case TT_TRACE_EVENT_COPY_ASYNC_STEP_INTO0:
        TRACE_EVENT_COPY_ASYNC_STEP_INTO0(
            kTraceTestCategory, "TT_TRACE_EVENT_COPY_ASYNC_STEP_INTO0", 100,
            1000);
        break;
      case TT_TRACE_EVENT_COPY_ASYNC_STEP_INTO1:
        TRACE_EVENT_COPY_ASYNC_STEP_INTO1(
            kTraceTestCategory, "TT_TRACE_EVENT_COPY_ASYNC_STEP_INTO1", 100,
            1000, "arg1", 1);
        break;
      case TT_TRACE_EVENT_ASYNC_STEP_PAST0:
        TRACE_EVENT_ASYNC_STEP_PAST0(
            kTraceTestCategory, "TT_TRACE_EVENT_ASYNC_STEP_PAST0", 100, 1000);
        break;
      case TT_TRACE_EVENT_ASYNC_STEP_PAST1:
        TRACE_EVENT_ASYNC_STEP_PAST1(kTraceTestCategory,
                                     "TT_TRACE_EVENT_ASYNC_STEP_PAST1", 100,
                                     1000, "arg1", 1);
        break;
      case TT_TRACE_EVENT_COPY_ASYNC_STEP_PAST0:
        TRACE_EVENT_COPY_ASYNC_STEP_PAST0(
            kTraceTestCategory, "TT_TRACE_EVENT_COPY_ASYNC_STEP_PAST0", 100,
            1000);
        break;
      case TT_TRACE_EVENT_COPY_ASYNC_STEP_PAST1:
        TRACE_EVENT_COPY_ASYNC_STEP_PAST1(
            kTraceTestCategory, "TT_TRACE_EVENT_COPY_ASYNC_STEP_PAST1", 100,
            1000, "arg1", 1);
        break;
      case TT_TRACE_EVENT_ASYNC_END0:
        TRACE_EVENT_ASYNC_BEGIN0(kTraceTestCategory,
                                 "TT_TRACE_EVENT_ASYNC_END0", 100);
        TRACE_EVENT_ASYNC_END0(kTraceTestCategory, "TT_TRACE_EVENT_ASYNC_END0",
                               100);
        break;
      case TT_TRACE_EVENT_ASYNC_END1:
        TRACE_EVENT_ASYNC_BEGIN1(kTraceTestCategory,
                                 "TT_TRACE_EVENT_ASYNC_END1", 100, "arg1", 1);
        TRACE_EVENT_ASYNC_END1(kTraceTestCategory, "TT_TRACE_EVENT_ASYNC_END1",
                               100, "arg1", 1);
        break;
      case TT_TRACE_EVENT_ASYNC_END2:
        TRACE_EVENT_ASYNC_BEGIN2(kTraceTestCategory,
                                 "TT_TRACE_EVENT_ASYNC_END2", 100, "arg1", 1,
                                 "arg2", 2);
        TRACE_EVENT_ASYNC_END2(kTraceTestCategory, "TT_TRACE_EVENT_ASYNC_END2",
                               100, "arg1", 1, "arg2", 2);
        break;
      case TT_TRACE_EVENT_COPY_ASYNC_END0:
        TRACE_EVENT_COPY_ASYNC_BEGIN0(kTraceTestCategory,
                                      "TT_TRACE_EVENT_COPY_ASYNC_END0", 100);
        TRACE_EVENT_COPY_ASYNC_END0(kTraceTestCategory,
                                    "TT_TRACE_EVENT_COPY_ASYNC_END0", 100);
        break;
      case TT_TRACE_EVENT_COPY_ASYNC_END1:
        TRACE_EVENT_COPY_ASYNC_BEGIN1(kTraceTestCategory,
                                      "TT_TRACE_EVENT_COPY_ASYNC_END1", 100,
                                      "arg1", 1);
        TRACE_EVENT_COPY_ASYNC_END1(kTraceTestCategory,
                                    "TT_TRACE_EVENT_COPY_ASYNC_END1", 100,
                                    "arg1", 1);
        break;
      case TT_TRACE_EVENT_COPY_ASYNC_END2:
        TRACE_EVENT_COPY_ASYNC_BEGIN2(kTraceTestCategory,
                                      "TT_TRACE_EVENT_COPY_ASYNC_END2", 100,
                                      "arg1", 1, "arg2", 2);
        TRACE_EVENT_COPY_ASYNC_END2(kTraceTestCategory,
                                    "TT_TRACE_EVENT_COPY_ASYNC_END2", 100,
                                    "arg1", 1, "arg2", 2);
        break;
    }

    // Results in a call to OnEndTracingComplete.
    CefEndTracing(CefString(), this);
  }

  void OnEndTracingComplete(const CefString& tracing_file) override {
    EXPECT_UI_THREAD();

    CefPostTask(
        TID_FILE_USER_VISIBLE,
        base::Bind(&TracingTestHandler::ReadTracingFile, this, tracing_file));
  }

  void ReadTracingFile(const std::string& file_path) {
    EXPECT_FILE_THREAD();

    EXPECT_TRUE(client::file_util::ReadFileToString(file_path, &trace_data_));
    EXPECT_TRUE(CefDeleteFile(file_path, false));

    completion_event_->Signal();
  }

 private:
  ~TracingTestHandler() override {}

  // Handle used to notify when the test is complete.
  CefRefPtr<CefWaitableEvent> completion_event_;

  const char* trace_type_;
  TracingTestType type_;
  std::string trace_data_;

  IMPLEMENT_REFCOUNTING(TracingTestHandler);
};

// Helper for defining tracing tests.
#define TRACING_TEST(name, test_type)                  \
  TEST(TracingTest, name) {                            \
    CefRefPtr<TracingTestHandler> handler =            \
        new TracingTestHandler(test_type, #test_type); \
    handler->ExecuteTest();                            \
  }

// Define the tests.
TRACING_TEST(TraceEvent0, TT_TRACE_EVENT0)
TRACING_TEST(TraceEvent1, TT_TRACE_EVENT1)
TRACING_TEST(TraceEvent2, TT_TRACE_EVENT2)
TRACING_TEST(TraceEventInstant0, TT_TRACE_EVENT_INSTANT0)
TRACING_TEST(TraceEventInstant1, TT_TRACE_EVENT_INSTANT1)
TRACING_TEST(TraceEventInstant2, TT_TRACE_EVENT_INSTANT2)
TRACING_TEST(TraceEventCopyInstant0, TT_TRACE_EVENT_COPY_INSTANT0)
TRACING_TEST(TraceEventCopyInstant1, TT_TRACE_EVENT_COPY_INSTANT1)
TRACING_TEST(TraceEventCopyInstant2, TT_TRACE_EVENT_COPY_INSTANT2)
TRACING_TEST(TraceEventBegin0, TT_TRACE_EVENT_BEGIN0)
TRACING_TEST(TraceEventBegin1, TT_TRACE_EVENT_BEGIN1)
TRACING_TEST(TraceEventBegin2, TT_TRACE_EVENT_BEGIN2)
TRACING_TEST(TraceEventCopyBegin0, TT_TRACE_EVENT_COPY_BEGIN0)
TRACING_TEST(TraceEventCopyBegin1, TT_TRACE_EVENT_COPY_BEGIN1)
TRACING_TEST(TraceEventCopyBegin2, TT_TRACE_EVENT_COPY_BEGIN2)
TRACING_TEST(TraceEventEnd0, TT_TRACE_EVENT_END0)
TRACING_TEST(TraceEventEnd1, TT_TRACE_EVENT_END1)
TRACING_TEST(TraceEventEnd2, TT_TRACE_EVENT_END2)
TRACING_TEST(TraceEventCopyEnd0, TT_TRACE_EVENT_COPY_END0)
TRACING_TEST(TraceEventCopyEnd1, TT_TRACE_EVENT_COPY_END1)
TRACING_TEST(TraceEventCopyEnd2, TT_TRACE_EVENT_COPY_END1)
TRACING_TEST(TraceCounter1, TT_TRACE_COUNTER1)
TRACING_TEST(TraceCopyCounter1, TT_TRACE_COPY_COUNTER1)
TRACING_TEST(TraceCounter2, TT_TRACE_COUNTER2)
TRACING_TEST(TraceCopyCounter2, TT_TRACE_COPY_COUNTER2)
TRACING_TEST(TraceCounterId1, TT_TRACE_COUNTER_ID1)
TRACING_TEST(TraceCopyCounterId1, TT_TRACE_COPY_COUNTER_ID1)
TRACING_TEST(TraceCounterId2, TT_TRACE_COUNTER_ID2)
TRACING_TEST(TraceCopyCounterId2, TT_TRACE_COPY_COUNTER_ID1)
TRACING_TEST(TraceEventAsyncBegin0, TT_TRACE_EVENT_ASYNC_BEGIN0)
TRACING_TEST(TraceEventAsyncBegin1, TT_TRACE_EVENT_ASYNC_BEGIN1)
TRACING_TEST(TraceEventAsyncBegin2, TT_TRACE_EVENT_ASYNC_BEGIN2)
TRACING_TEST(TraceEventCopyAsyncBegin0, TT_TRACE_EVENT_COPY_ASYNC_BEGIN0)
TRACING_TEST(TraceEventCopyAsyncBegin1, TT_TRACE_EVENT_COPY_ASYNC_BEGIN1)
TRACING_TEST(TraceEventCopyAsyncBegin2, TT_TRACE_EVENT_COPY_ASYNC_BEGIN2)
TRACING_TEST(TraceEventAsyncStepInto0, TT_TRACE_EVENT_ASYNC_STEP_INTO0)
TRACING_TEST(TraceEventAsyncStepInto1, TT_TRACE_EVENT_ASYNC_STEP_INTO1)
TRACING_TEST(TraceEventCopyAsyncStepInto0, TT_TRACE_EVENT_COPY_ASYNC_STEP_INTO0)
TRACING_TEST(TraceEventCopyAsyncStepInto1, TT_TRACE_EVENT_COPY_ASYNC_STEP_INTO1)
TRACING_TEST(TraceEventAsyncStepPast0, TT_TRACE_EVENT_ASYNC_STEP_PAST0)
TRACING_TEST(TraceEventAsyncStepPast1, TT_TRACE_EVENT_ASYNC_STEP_PAST1)
TRACING_TEST(TraceEventCopyAsyncStepPast0, TT_TRACE_EVENT_COPY_ASYNC_STEP_PAST0)
TRACING_TEST(TraceEventCopyAsyncStepPast1, TT_TRACE_EVENT_COPY_ASYNC_STEP_PAST1)
TRACING_TEST(TraceEventAsyncEnd0, TT_TRACE_EVENT_ASYNC_END0)
TRACING_TEST(TraceEventAsyncEnd1, TT_TRACE_EVENT_ASYNC_END1)
TRACING_TEST(TraceEventAsyncEnd2, TT_TRACE_EVENT_ASYNC_END2)
TRACING_TEST(TraceEventCopyAsyncEnd0, TT_TRACE_EVENT_COPY_ASYNC_END0)
TRACING_TEST(TraceEventCopyAsyncEnd1, TT_TRACE_EVENT_COPY_ASYNC_END1)
TRACING_TEST(TraceEventCopyAsyncEnd2, TT_TRACE_EVENT_COPY_ASYNC_END2)

TEST(TracingTest, NowFromSystemTraceTime) {
  int64 val = CefNowFromSystemTraceTime();
  EXPECT_NE(val, 0);
}
