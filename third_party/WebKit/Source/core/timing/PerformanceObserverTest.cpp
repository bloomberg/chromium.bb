// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceObserver.h"

#include "bindings/core/v8/PerformanceObserverCallback.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/timing/Performance.h"
#include "core/timing/PerformanceBase.h"
#include "core/timing/PerformanceMark.h"
#include "core/timing/PerformanceObserverInit.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MockPerformanceBase : public PerformanceBase {
 public:
  explicit MockPerformanceBase(ScriptState* script_state)
      : PerformanceBase(0,
                        TaskRunnerHelper::Get(TaskType::kPerformanceTimeline,
                                              script_state)) {}
  ~MockPerformanceBase() {}

  ExecutionContext* GetExecutionContext() const override { return nullptr; }
};

class PerformanceObserverTest : public ::testing::Test {
 protected:
  void Initialize(ScriptState* script_state) {
    v8::Local<v8::Function> callback =
        v8::Function::New(script_state->GetContext(), nullptr).ToLocalChecked();
    base_ = new MockPerformanceBase(script_state);
    cb_ = PerformanceObserverCallback::Create(script_state, callback);
    observer_ = new PerformanceObserver(ExecutionContext::From(script_state),
                                        base_, cb_);
  }

  bool IsRegistered() { return observer_->is_registered_; }
  int NumPerformanceEntries() { return observer_->performance_entries_.size(); }
  void Deliver() { observer_->Deliver(); }

  Persistent<MockPerformanceBase> base_;
  Persistent<PerformanceObserverCallback> cb_;
  Persistent<PerformanceObserver> observer_;
};

TEST_F(PerformanceObserverTest, Observe) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  NonThrowableExceptionState exception_state;
  PerformanceObserverInit options;
  Vector<String> entry_type_vec;
  entry_type_vec.push_back("mark");
  options.setEntryTypes(entry_type_vec);

  observer_->observe(options, exception_state);
  EXPECT_TRUE(IsRegistered());
}

TEST_F(PerformanceObserverTest, Enqueue) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  Persistent<PerformanceEntry> entry = PerformanceMark::Create("m", 1234);
  EXPECT_EQ(0, NumPerformanceEntries());

  observer_->EnqueuePerformanceEntry(*entry);
  EXPECT_EQ(1, NumPerformanceEntries());
}

TEST_F(PerformanceObserverTest, Deliver) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  Persistent<PerformanceEntry> entry = PerformanceMark::Create("m", 1234);
  EXPECT_EQ(0, NumPerformanceEntries());

  observer_->EnqueuePerformanceEntry(*entry);
  EXPECT_EQ(1, NumPerformanceEntries());

  Deliver();
  EXPECT_EQ(0, NumPerformanceEntries());
}

TEST_F(PerformanceObserverTest, Disconnect) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  Persistent<PerformanceEntry> entry = PerformanceMark::Create("m", 1234);
  EXPECT_EQ(0, NumPerformanceEntries());

  observer_->EnqueuePerformanceEntry(*entry);
  EXPECT_EQ(1, NumPerformanceEntries());

  observer_->disconnect();
  EXPECT_FALSE(IsRegistered());
  EXPECT_EQ(0, NumPerformanceEntries());
}
}
