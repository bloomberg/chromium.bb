// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/frame/UseCounter.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MainThreadWorkletGlobalScopeForTest
    : public MainThreadWorkletGlobalScope {
 public:
  MainThreadWorkletGlobalScopeForTest(LocalFrame* frame,
                                      const KURL& url,
                                      const String& user_agent,
                                      RefPtr<SecurityOrigin> security_origin,
                                      v8::Isolate* isolate)
      : MainThreadWorkletGlobalScope(frame,
                                     url,
                                     user_agent,
                                     std::move(security_origin),
                                     isolate),
        reported_features_(UseCounter::kNumberOfFeatures) {}

  void ReportFeature(UseCounter::Feature feature) override {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_.QuickGet(feature));
    reported_features_.QuickSet(feature);
    MainThreadWorkletGlobalScope::ReportFeature(feature);
  }

  void ReportDeprecation(UseCounter::Feature feature) final {
    // Any feature should be reported only one time.
    EXPECT_FALSE(reported_features_.QuickGet(feature));
    reported_features_.QuickSet(feature);
    MainThreadWorkletGlobalScope::ReportDeprecation(feature);
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
    global_scope_ = new MainThreadWorkletGlobalScope(
        &page_->GetFrame(), url, "fake user agent", security_origin_.Get(),
        ToIsolate(page_->GetFrame().GetDocument()));
  }

  void TearDown() override { global_scope_->Terminate(); }

 protected:
  RefPtr<SecurityOrigin> security_origin_;
  std::unique_ptr<DummyPageHolder> page_;
  Persistent<MainThreadWorkletGlobalScope> global_scope_;
};

TEST_F(MainThreadWorkletTest, UseCounter) {
  Document& document = *page_->GetFrame().GetDocument();

  // This feature is randomly selected.
  const UseCounter::Feature kFeature1 = UseCounter::Feature::kRequestFileSystem;

  // API use on the MainThreadWorkletGlobalScope should be recorded in
  // UseCounter on the Document.
  EXPECT_FALSE(UseCounter::IsCounted(document, kFeature1));
  UseCounter::Count(global_scope_, kFeature1);
  EXPECT_TRUE(UseCounter::IsCounted(document, kFeature1));

  // API use should be reported to the Document only one time. See comments in
  // MainThreadGlobalScopeForTest::ReportFeature.
  UseCounter::Count(global_scope_, kFeature1);

  // This feature is randomly selected from Deprecation::deprecationMessage().
  const UseCounter::Feature kFeature2 =
      UseCounter::Feature::kPrefixedStorageInfo;

  // Deprecated API use on the MainThreadWorkletGlobalScope should be recorded
  // in UseCounter on the Document.
  EXPECT_FALSE(UseCounter::IsCounted(document, kFeature2));
  Deprecation::CountDeprecation(global_scope_, kFeature2);
  EXPECT_TRUE(UseCounter::IsCounted(document, kFeature2));

  // API use should be reported to the Document only one time. See comments in
  // MainThreadWorkletGlobalScopeForTest::ReportDeprecation.
  Deprecation::CountDeprecation(global_scope_, kFeature2);
}

}  // namespace blink
