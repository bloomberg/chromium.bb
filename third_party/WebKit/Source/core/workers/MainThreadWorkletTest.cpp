// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/UseCounter.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MainThreadWorkletTest : public ::testing::Test {
 public:
  void SetUp() override {
    KURL url(ParsedURLString, "https://example.com/");
    m_page = DummyPageHolder::create();
    m_securityOrigin = SecurityOrigin::create(url);
    m_globalScope = new MainThreadWorkletGlobalScope(
        &m_page->frame(), url, "fake user agent", m_securityOrigin.get(),
        toIsolate(m_page->frame().document()));
  }

  void TearDown() override { m_globalScope->terminateWorkletGlobalScope(); }

 protected:
  RefPtr<SecurityOrigin> m_securityOrigin;
  std::unique_ptr<DummyPageHolder> m_page;
  Persistent<MainThreadWorkletGlobalScope> m_globalScope;
};

TEST_F(MainThreadWorkletTest, UseCounter) {
  Document& document = *m_page->frame().document();

  // This feature is randomly selected.
  const UseCounter::Feature feature1 = UseCounter::Feature::RequestFileSystem;

  // API use on the MainThreadWorkletGlobalScope should be recorded in
  // UseCounter on the Document.
  EXPECT_FALSE(UseCounter::isCounted(document, feature1));
  UseCounter::count(m_globalScope, feature1);
  EXPECT_TRUE(UseCounter::isCounted(document, feature1));

  // This feature is randomly selected from Deprecation::deprecationMessage().
  const UseCounter::Feature feature2 = UseCounter::Feature::PrefixedStorageInfo;

  // Deprecated API use on the MainThreadWorkletGlobalScope should be recorded
  // in UseCounter on the Document.
  EXPECT_FALSE(UseCounter::isCounted(document, feature2));
  Deprecation::countDeprecation(m_globalScope, feature2);
  EXPECT_TRUE(UseCounter::isCounted(document, feature2));
}

}  // namespace blink
