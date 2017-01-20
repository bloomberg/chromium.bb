// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/Performance.h"

#include "bindings/core/v8/PerformanceObserverCallback.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/testing/DummyPageHolder.h"
#include "core/testing/NullExecutionContext.h"
#include "core/timing/PerformanceBase.h"
#include "core/timing/PerformanceLongTaskTiming.h"
#include "core/timing/PerformanceObserver.h"
#include "core/timing/PerformanceObserverInit.h"
#include "platform/network/ResourceResponse.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TestPerformanceBase : public PerformanceBase {
 public:
  explicit TestPerformanceBase(ScriptState* scriptState)
      : PerformanceBase(
            0,
            TaskRunnerHelper::get(TaskType::PerformanceTimeline, scriptState)) {
  }
  ~TestPerformanceBase() {}

  ExecutionContext* getExecutionContext() const override { return nullptr; }

  int numActiveObservers() { return m_activeObservers.size(); }

  int numObservers() { return m_observers.size(); }

  bool hasPerformanceObserverFor(PerformanceEntry::EntryType entryType) {
    return hasObserverFor(entryType);
  }

  DEFINE_INLINE_TRACE() { PerformanceBase::trace(visitor); }
};

class PerformanceBaseTest : public ::testing::Test {
 protected:
  void initialize(ScriptState* scriptState) {
    v8::Local<v8::Function> callback =
        v8::Function::New(scriptState->context(), nullptr).ToLocalChecked();
    m_base = new TestPerformanceBase(scriptState);
    m_cb = PerformanceObserverCallback::create(scriptState, callback);
    m_observer = PerformanceObserver::create(scriptState->getExecutionContext(),
                                             m_base, m_cb);
  }

  void SetUp() override {
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_executionContext = new NullExecutionContext();
  }

  ExecutionContext* getExecutionContext() { return m_executionContext.get(); }

  int numPerformanceEntriesInObserver() {
    return m_observer->m_performanceEntries.size();
  }

  PerformanceNavigationTiming::NavigationType getNavigationType(
      NavigationType type,
      Document* document) {
    return PerformanceBase::getNavigationType(type, document);
  }

  static bool allowsTimingRedirect(
      const Vector<ResourceResponse>& redirectChain,
      const ResourceResponse& finalResponse,
      const SecurityOrigin& initiatorSecurityOrigin,
      ExecutionContext* context) {
    return PerformanceBase::allowsTimingRedirect(
        redirectChain, finalResponse, initiatorSecurityOrigin, context);
  }

  Persistent<TestPerformanceBase> m_base;
  Persistent<ExecutionContext> m_executionContext;
  Persistent<PerformanceObserver> m_observer;
  std::unique_ptr<DummyPageHolder> m_pageHolder;
  Persistent<PerformanceObserverCallback> m_cb;
};

TEST_F(PerformanceBaseTest, Register) {
  V8TestingScope scope;
  initialize(scope.getScriptState());

  EXPECT_EQ(0, m_base->numObservers());
  EXPECT_EQ(0, m_base->numActiveObservers());

  m_base->registerPerformanceObserver(*m_observer.get());
  EXPECT_EQ(1, m_base->numObservers());
  EXPECT_EQ(0, m_base->numActiveObservers());

  m_base->unregisterPerformanceObserver(*m_observer.get());
  EXPECT_EQ(0, m_base->numObservers());
  EXPECT_EQ(0, m_base->numActiveObservers());
}

TEST_F(PerformanceBaseTest, Activate) {
  V8TestingScope scope;
  initialize(scope.getScriptState());

  EXPECT_EQ(0, m_base->numObservers());
  EXPECT_EQ(0, m_base->numActiveObservers());

  m_base->registerPerformanceObserver(*m_observer.get());
  EXPECT_EQ(1, m_base->numObservers());
  EXPECT_EQ(0, m_base->numActiveObservers());

  m_base->activateObserver(*m_observer.get());
  EXPECT_EQ(1, m_base->numObservers());
  EXPECT_EQ(1, m_base->numActiveObservers());

  m_base->unregisterPerformanceObserver(*m_observer.get());
  EXPECT_EQ(0, m_base->numObservers());
  EXPECT_EQ(0, m_base->numActiveObservers());
}

TEST_F(PerformanceBaseTest, AddLongTaskTiming) {
  V8TestingScope scope;
  initialize(scope.getScriptState());

  // Add a long task entry, but no observer registered.
  m_base->addLongTaskTiming(1234, 5678, "same-origin", "www.foo.com/bar", "",
                            "");
  EXPECT_FALSE(m_base->hasPerformanceObserverFor(PerformanceEntry::LongTask));
  EXPECT_EQ(0, numPerformanceEntriesInObserver());  // has no effect

  // Make an observer for longtask
  NonThrowableExceptionState exceptionState;
  PerformanceObserverInit options;
  Vector<String> entryTypeVec;
  entryTypeVec.push_back("longtask");
  options.setEntryTypes(entryTypeVec);
  m_observer->observe(options, exceptionState);

  EXPECT_TRUE(m_base->hasPerformanceObserverFor(PerformanceEntry::LongTask));
  // Add a long task entry
  m_base->addLongTaskTiming(1234, 5678, "same-origin", "www.foo.com/bar", "",
                            "");
  EXPECT_EQ(1, numPerformanceEntriesInObserver());  // added an entry
}

TEST_F(PerformanceBaseTest, GetNavigationType) {
  m_pageHolder->page().setVisibilityState(PageVisibilityStatePrerender, false);
  PerformanceNavigationTiming::NavigationType returnedType =
      getNavigationType(NavigationTypeBackForward, &m_pageHolder->document());
  EXPECT_EQ(returnedType,
            PerformanceNavigationTiming::NavigationType::Prerender);

  m_pageHolder->page().setVisibilityState(PageVisibilityStateHidden, false);
  returnedType =
      getNavigationType(NavigationTypeBackForward, &m_pageHolder->document());
  EXPECT_EQ(returnedType,
            PerformanceNavigationTiming::NavigationType::BackForward);

  m_pageHolder->page().setVisibilityState(PageVisibilityStateVisible, false);
  returnedType = getNavigationType(NavigationTypeFormResubmitted,
                                   &m_pageHolder->document());
  EXPECT_EQ(returnedType,
            PerformanceNavigationTiming::NavigationType::Navigate);
}

TEST_F(PerformanceBaseTest, AllowsTimingRedirect) {
  // When there are no cross-origin redirects.
  AtomicString originDomain = "http://127.0.0.1:8000";
  Vector<ResourceResponse> redirectChain;
  KURL url(ParsedURLString, originDomain + "/foo.html");
  ResourceResponse finalResponse;
  finalResponse.setURL(url);
  ResourceResponse redirectResponse1;
  redirectResponse1.setURL(url);
  ResourceResponse redirectResponse2;
  redirectResponse2.setURL(url);
  redirectChain.push_back(redirectResponse1);
  redirectChain.push_back(redirectResponse2);
  RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::create(url);
  EXPECT_TRUE(allowsTimingRedirect(redirectChain, finalResponse,
                                   *securityOrigin.get(),
                                   getExecutionContext()));
  // When there exist cross-origin redirects.
  AtomicString crossOriginDomain = "http://126.0.0.1:8000";
  KURL redirectUrl(ParsedURLString, crossOriginDomain + "/bar.html");
  ResourceResponse redirectResponse3;
  redirectResponse3.setURL(redirectUrl);
  redirectChain.push_back(redirectResponse3);
  EXPECT_FALSE(allowsTimingRedirect(redirectChain, finalResponse,
                                    *securityOrigin.get(),
                                    getExecutionContext()));

  // When cross-origin redirect opts in.
  redirectChain.back().setHTTPHeaderField(HTTPNames::Timing_Allow_Origin,
                                          originDomain);
  EXPECT_TRUE(allowsTimingRedirect(redirectChain, finalResponse,
                                   *securityOrigin.get(),
                                   getExecutionContext()));
}

}  // namespace blink
