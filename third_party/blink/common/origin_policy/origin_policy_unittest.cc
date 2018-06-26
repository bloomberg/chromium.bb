// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/origin_policy/origin_policy.h"

#include "testing/gtest/include/gtest/gtest.h"

// Unit tests for OriginPolicy / OriginPolicyParser.
//
// These are fairly simple "smoke tests". The majority of test coverage is
// expected from wpt/origin-policy/ end-to-end tests.

TEST(OriginPolicy, Empty) {
  auto policy = blink::OriginPolicy::From("");
  ASSERT_FALSE(policy);
}

TEST(OriginPolicy, Invalid) {
  auto policy = blink::OriginPolicy::From("potato potato potato");
  ASSERT_FALSE(policy);
}

TEST(OriginPolicy, ValidButEmpty) {
  auto policy = blink::OriginPolicy::From(R"({"headers":[]})");
  ASSERT_TRUE(policy);
  ASSERT_TRUE(policy->GetContentSecurityPolicy().empty());
}

TEST(OriginPolicy, SimpleCSP) {
  auto policy = blink::OriginPolicy::From(R"(
      { "headers": [{
          "name": "Content-Security-Policy",
          "value": "script-src 'self' 'unsafe-inline'",
          "type": "fallback"
      }] }
  )");
  ASSERT_TRUE(policy);
  ASSERT_EQ(policy->GetContentSecurityPolicy(),
            "script-src 'self' 'unsafe-inline'");
}

TEST(OriginPolicy, ExtraFieldsDontBreakParsing) {
  auto policy = blink::OriginPolicy::From(R"(
      { "potatoes": "are better than kale",
        "headers": [{
          "name": "Content-Security-Policy",
          "potatos": "are best",
          "value": "script-src 'self' 'unsafe-inline'",
          "type": "fallback"
        },{
          "name": "Sieglinde",
          "value": "best of potatos"
      }]}
  )");
  ASSERT_TRUE(policy);
  ASSERT_EQ(policy->GetContentSecurityPolicy(),
            "script-src 'self' 'unsafe-inline'");
}
