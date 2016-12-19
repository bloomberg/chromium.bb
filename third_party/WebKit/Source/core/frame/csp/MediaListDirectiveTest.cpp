// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/MediaListDirective.h"

#include "core/frame/csp/ContentSecurityPolicy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MediaListDirectiveTest : public ::testing::Test {
 public:
  MediaListDirectiveTest() : csp(ContentSecurityPolicy::create()) {}

 protected:
  Persistent<ContentSecurityPolicy> csp;
};

TEST_F(MediaListDirectiveTest, GetIntersect) {
  MediaListDirective A(
      "plugin-types",
      "application/x-shockwave-flash application/pdf text/plain", csp.get());
  MediaListDirective emptyA("plugin-types", "", csp.get());

  struct TestCase {
    const char* policyB;
    const std::vector<const char*> expected;
  } cases[] = {
      {"", std::vector<const char*>()},
      {"text/", std::vector<const char*>()},
      {"text/*", std::vector<const char*>()},
      {"*/plain", std::vector<const char*>()},
      {"text/plain */plain", {"text/plain"}},
      {"text/plain application/*", {"text/plain"}},
      {"text/plain", {"text/plain"}},
      {"application/pdf", {"application/pdf"}},
      {"application/x-shockwave-flash", {"application/x-shockwave-flash"}},
      {"application/x-shockwave-flash text/plain",
       {"application/x-shockwave-flash", "text/plain"}},
      {"application/pdf text/plain", {"text/plain", "application/pdf"}},
      {"application/x-shockwave-flash application/pdf text/plain",
       {"application/x-shockwave-flash", "application/pdf", "text/plain"}},
  };

  for (const auto& test : cases) {
    MediaListDirective B("plugin-types", test.policyB, csp.get());

    HashSet<String> result = A.getIntersect(B.m_pluginTypes);
    EXPECT_EQ(result.size(), test.expected.size());

    for (const auto& type : test.expected)
      EXPECT_TRUE(result.contains(type));

    // If we change the order of `A` and `B`, intersection should not change.
    result = B.getIntersect(A.m_pluginTypes);
    EXPECT_EQ(result.size(), test.expected.size());

    for (const auto& type : test.expected)
      EXPECT_TRUE(result.contains(type));

    // When `A` is empty, there should not be any intersection.
    result = emptyA.getIntersect(B.m_pluginTypes);
    EXPECT_FALSE(result.size());
  }
}

TEST_F(MediaListDirectiveTest, Subsumes) {
  MediaListDirective A(
      "plugin-types",
      "application/x-shockwave-flash application/pdf text/plain text/*",
      csp.get());

  struct TestCase {
    const std::vector<const char*> policiesB;
    bool subsumed;
    bool subsumedByEmptyA;
  } cases[] = {
      // `A` subsumes `policiesB`.
      {{""}, true, true},
      {{"text/"}, true, true},
      {{"text/*"}, true, false},
      {{"application/*"}, false, false},
      {{"application/"}, true, true},
      {{"*/plain"}, false, false},
      {{"application"}, true, true},
      {{"text/plain"}, true, false},
      {{"application/pdf"}, true, false},
      {{"application/x-shockwave-flash"}, true, false},
      {{"application/x-shockwave-flash text/plain"}, true, false},
      {{"application/pdf text/plain"}, true, false},
      {{"application/x-shockwave-flash text/plain application/pdf"},
       true,
       false},
      {{"application/x-shockwave-flash text "}, true, false},
      {{"text/* application/x-shockwave-flash"}, true, false},
      {{"application/ application/x-shockwave-flash"}, true, false},
      {{"*/plain application/x-shockwave-flash"}, false, false},
      {{"text/ application/x-shockwave-flash"}, true, false},
      {{"application application/x-shockwave-flash"}, true, false},
      {{"application/x-shockwave-flash text/plain "
        "application/x-blink-test-plugin",
        "application/x-shockwave-flash text/plain"},
       true,
       false},
      {{"application/x-shockwave-flash text/plain "
        "application/x-blink-test-plugin",
        "text/plain"},
       true,
       false},
      {{"application/x-blink-test-plugin", "text/plain"}, true, true},
      {{"application/x-shockwave-flash",
        "text/plain application/x-shockwave-flash"},
       true,
       false},
      {{"application/x-shockwave-flash text/plain",
        "application/x-blink-test-plugin", "text/plain"},
       true,
       true},
      // `A` does not subsumes `policiesB`.
      {std::vector<const char*>(), false, false},
      {{"application/x-blink-test-plugin"}, false, false},
      {{"application/x-shockwave-flash text/plain "
        "application/x-blink-test-plugin"},
       false,
       false},
      {{"application/x-shockwave-flash text application/x-blink-test-plugin"},
       false,
       false},
      {{"application/x-invalid-type text application/"}, false, false},
      {{"application/x-blink-test-plugin text application/",
        "application/x-blink-test-plugin"},
       false,
       false},
  };

  MediaListDirective emptyA("plugin-types", "", csp.get());

  for (const auto& test : cases) {
    HeapVector<Member<MediaListDirective>> policiesB;
    for (const auto& policy : test.policiesB) {
      policiesB.push_back(
          new MediaListDirective("plugin-types", policy, csp.get()));
    }

    EXPECT_EQ(A.subsumes(policiesB), test.subsumed);
    EXPECT_EQ(emptyA.subsumes(policiesB), test.subsumedByEmptyA);
  }
}

}  // namespace blink
