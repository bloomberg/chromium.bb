// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/Performance.h"

#include "bindings/core/v8/PerformanceObserverCallback.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/testing/DummyPageHolder.h"
#include "core/testing/NullExecutionContext.h"
#include "core/timing/PerformanceBase.h"
#include "core/timing/PerformanceLongTaskTiming.h"
#include "core/timing/PerformanceObserver.h"
#include "core/timing/PerformanceObserverInit.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TestPerformanceBase : public PerformanceBase {
 public:
  explicit TestPerformanceBase(ScriptState* script_state)
      : PerformanceBase(0,
                        TaskRunnerHelper::Get(TaskType::kPerformanceTimeline,
                                              script_state)) {}
  ~TestPerformanceBase() {}

  ExecutionContext* GetExecutionContext() const override { return nullptr; }

  int NumActiveObservers() { return active_observers_.size(); }

  int NumObservers() { return observers_.size(); }

  bool HasPerformanceObserverFor(PerformanceEntry::EntryType entry_type) {
    return HasObserverFor(entry_type);
  }

  DEFINE_INLINE_TRACE() { PerformanceBase::Trace(visitor); }
};

class PerformanceBaseTest : public ::testing::Test {
 protected:
  void Initialize(ScriptState* script_state) {
    v8::Local<v8::Function> callback =
        v8::Function::New(script_state->GetContext(), nullptr).ToLocalChecked();
    base_ = new TestPerformanceBase(script_state);
    cb_ = PerformanceObserverCallback::Create(script_state, callback);
    observer_ = new PerformanceObserver(script_state, base_, cb_);
  }

  void SetUp() override {
    page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
    execution_context_ = new NullExecutionContext();
  }

  ExecutionContext* GetExecutionContext() { return execution_context_.Get(); }

  int NumPerformanceEntriesInObserver() {
    return observer_->performance_entries_.size();
  }

  static bool AllowsTimingRedirect(
      const Vector<ResourceResponse>& redirect_chain,
      const ResourceResponse& final_response,
      const SecurityOrigin& initiator_security_origin,
      ExecutionContext* context) {
    return PerformanceBase::AllowsTimingRedirect(
        redirect_chain, final_response, initiator_security_origin, context);
  }

  Persistent<TestPerformanceBase> base_;
  Persistent<ExecutionContext> execution_context_;
  Persistent<PerformanceObserver> observer_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<PerformanceObserverCallback> cb_;
};

TEST_F(PerformanceBaseTest, Register) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  EXPECT_EQ(0, base_->NumObservers());
  EXPECT_EQ(0, base_->NumActiveObservers());

  base_->RegisterPerformanceObserver(*observer_.Get());
  EXPECT_EQ(1, base_->NumObservers());
  EXPECT_EQ(0, base_->NumActiveObservers());

  base_->UnregisterPerformanceObserver(*observer_.Get());
  EXPECT_EQ(0, base_->NumObservers());
  EXPECT_EQ(0, base_->NumActiveObservers());
}

TEST_F(PerformanceBaseTest, Activate) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  EXPECT_EQ(0, base_->NumObservers());
  EXPECT_EQ(0, base_->NumActiveObservers());

  base_->RegisterPerformanceObserver(*observer_.Get());
  EXPECT_EQ(1, base_->NumObservers());
  EXPECT_EQ(0, base_->NumActiveObservers());

  base_->ActivateObserver(*observer_.Get());
  EXPECT_EQ(1, base_->NumObservers());
  EXPECT_EQ(1, base_->NumActiveObservers());

  base_->UnregisterPerformanceObserver(*observer_.Get());
  EXPECT_EQ(0, base_->NumObservers());
  EXPECT_EQ(0, base_->NumActiveObservers());
}

TEST_F(PerformanceBaseTest, AddLongTaskTiming) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  // Add a long task entry, but no observer registered.
  base_->AddLongTaskTiming(1234, 5678, "same-origin", "www.foo.com/bar", "",
                           "");
  EXPECT_FALSE(base_->HasPerformanceObserverFor(PerformanceEntry::kLongTask));
  EXPECT_EQ(0, NumPerformanceEntriesInObserver());  // has no effect

  // Make an observer for longtask
  NonThrowableExceptionState exception_state;
  PerformanceObserverInit options;
  Vector<String> entry_type_vec;
  entry_type_vec.push_back("longtask");
  options.setEntryTypes(entry_type_vec);
  observer_->observe(options, exception_state);

  EXPECT_TRUE(base_->HasPerformanceObserverFor(PerformanceEntry::kLongTask));
  // Add a long task entry
  base_->AddLongTaskTiming(1234, 5678, "same-origin", "www.foo.com/bar", "",
                           "");
  EXPECT_EQ(1, NumPerformanceEntriesInObserver());  // added an entry
}

TEST_F(PerformanceBaseTest, AllowsTimingRedirect) {
  // When there are no cross-origin redirects.
  AtomicString origin_domain = "http://127.0.0.1:8000";
  Vector<ResourceResponse> redirect_chain;
  KURL url(kParsedURLString, origin_domain + "/foo.html");
  ResourceResponse final_response;
  ResourceResponse redirect_response1;
  redirect_response1.SetURL(url);
  ResourceResponse redirect_response2;
  redirect_response2.SetURL(url);
  redirect_chain.push_back(redirect_response1);
  redirect_chain.push_back(redirect_response2);
  RefPtr<SecurityOrigin> security_origin = SecurityOrigin::Create(url);
  // When finalResponse is an empty object.
  EXPECT_FALSE(AllowsTimingRedirect(redirect_chain, final_response,
                                    *security_origin.Get(),
                                    GetExecutionContext()));
  final_response.SetURL(url);
  EXPECT_TRUE(AllowsTimingRedirect(redirect_chain, final_response,
                                   *security_origin.Get(),
                                   GetExecutionContext()));
  // When there exist cross-origin redirects.
  AtomicString cross_origin_domain = "http://126.0.0.1:8000";
  KURL redirect_url(kParsedURLString, cross_origin_domain + "/bar.html");
  ResourceResponse redirect_response3;
  redirect_response3.SetURL(redirect_url);
  redirect_chain.push_back(redirect_response3);
  EXPECT_FALSE(AllowsTimingRedirect(redirect_chain, final_response,
                                    *security_origin.Get(),
                                    GetExecutionContext()));

  // When cross-origin redirect opts in.
  redirect_chain.back().SetHTTPHeaderField(HTTPNames::Timing_Allow_Origin,
                                           origin_domain);
  EXPECT_TRUE(AllowsTimingRedirect(redirect_chain, final_response,
                                   *security_origin.Get(),
                                   GetExecutionContext()));
}

}  // namespace blink
