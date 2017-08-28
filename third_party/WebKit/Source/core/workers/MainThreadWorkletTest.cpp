// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/UseCounter.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "core/workers/MainThreadWorkletReportingProxy.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MainThreadWorkletReportingProxyForTest final
    : public MainThreadWorkletReportingProxy {
 public:
  explicit MainThreadWorkletReportingProxyForTest(Document* document)
      : MainThreadWorkletReportingProxy(document),
        reported_features_(static_cast<int>(WebFeature::kNumberOfFeatures)) {}

  void CountFeature(WebFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_.QuickGet(static_cast<int>(feature)));
    reported_features_.QuickSet(static_cast<int>(feature));
    MainThreadWorkletReportingProxy::CountFeature(feature);
  }

  void CountDeprecation(WebFeature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_.QuickGet(static_cast<int>(feature)));
    reported_features_.QuickSet(static_cast<int>(feature));
    MainThreadWorkletReportingProxy::CountDeprecation(feature);
  }

 private:
  BitVector reported_features_;
};

class MainThreadWorkletTest : public ::testing::Test {
 public:
  void SetUp() override {
    KURL url(kParsedURLString, "https://example.com/");
    page_ = DummyPageHolder::Create();
    security_origin_ = SecurityOrigin::Create(url);
    reporting_proxy_ = WTF::MakeUnique<MainThreadWorkletReportingProxyForTest>(
        page_->GetFrame().GetDocument());
    global_scope_ = new MainThreadWorkletGlobalScope(
        &page_->GetFrame(), url, "fake user agent", security_origin_.Get(),
        ToIsolate(page_->GetFrame().GetDocument()), *reporting_proxy_);
  }

  void TearDown() override { global_scope_->Terminate(); }

 protected:
  RefPtr<SecurityOrigin> security_origin_;
  std::unique_ptr<DummyPageHolder> page_;
  std::unique_ptr<MainThreadWorkletReportingProxyForTest> reporting_proxy_;
  Persistent<MainThreadWorkletGlobalScope> global_scope_;
};

TEST_F(MainThreadWorkletTest, UseCounter) {
  Document& document = *page_->GetFrame().GetDocument();

  // This feature is randomly selected.
  const WebFeature kFeature1 = WebFeature::kRequestFileSystem;

  // API use on the MainThreadWorkletGlobalScope should be recorded in
  // UseCounter on the Document.
  EXPECT_FALSE(UseCounter::IsCounted(document, kFeature1));
  UseCounter::Count(global_scope_, kFeature1);
  EXPECT_TRUE(UseCounter::IsCounted(document, kFeature1));

  // API use should be reported to the Document only one time. See comments in
  // MainThreadWorkletReportingProxyForTest::ReportFeature.
  UseCounter::Count(global_scope_, kFeature1);

  // This feature is randomly selected from Deprecation::deprecationMessage().
  const WebFeature kFeature2 = WebFeature::kPrefixedStorageInfo;

  // Deprecated API use on the MainThreadWorkletGlobalScope should be recorded
  // in UseCounter on the Document.
  EXPECT_FALSE(UseCounter::IsCounted(document, kFeature2));
  Deprecation::CountDeprecation(global_scope_, kFeature2);
  EXPECT_TRUE(UseCounter::IsCounted(document, kFeature2));

  // API use should be reported to the Document only one time. See comments in
  // MainThreadWorkletReportingProxyForTest::ReportDeprecation.
  Deprecation::CountDeprecation(global_scope_, kFeature2);
}

TEST_F(MainThreadWorkletTest, TaskRunner) {
  RefPtr<WebTaskRunner> task_runner =
      TaskRunnerHelper::Get(TaskType::kUnthrottled, global_scope_);
  EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
}

}  // namespace blink
