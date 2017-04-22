// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8Binding.h"
#include "core/frame/UseCounter.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class WorkletObjectProxyForTest final
    : public GarbageCollectedFinalized<WorkletObjectProxyForTest>,
      public WorkletObjectProxy {
  USING_GARBAGE_COLLECTED_MIXIN(WorkletObjectProxyForTest);

 public:
  void DidFetchAndInvokeScript(int32_t request_id, bool success) {}
};

}  // namespace

class MainThreadWorkletTest : public ::testing::Test {
 public:
  void SetUp() override {
    KURL url(kParsedURLString, "https://example.com/");
    page_ = DummyPageHolder::Create();
    security_origin_ = SecurityOrigin::Create(url);
    global_scope_ = new MainThreadWorkletGlobalScope(
        &page_->GetFrame(), url, "fake user agent", security_origin_.Get(),
        ToIsolate(page_->GetFrame().GetDocument()),
        new WorkletObjectProxyForTest);
  }

  void TearDown() override { global_scope_->TerminateWorkletGlobalScope(); }

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

  // This feature is randomly selected from Deprecation::deprecationMessage().
  const UseCounter::Feature kFeature2 =
      UseCounter::Feature::kPrefixedStorageInfo;

  // Deprecated API use on the MainThreadWorkletGlobalScope should be recorded
  // in UseCounter on the Document.
  EXPECT_FALSE(UseCounter::IsCounted(document, kFeature2));
  Deprecation::CountDeprecation(global_scope_, kFeature2);
  EXPECT_TRUE(UseCounter::IsCounted(document, kFeature2));
}

}  // namespace blink
