// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class PrefetchTypeTest : public ::testing::Test {};

TEST_F(PrefetchTypeTest, GetPrefetchTypeParams) {
  PrefetchType prefetch_type1(/*use_isolated_network_context=*/true,
                              /*use_prefetch_proxy=*/true);
  PrefetchType prefetch_type2(/*use_isolated_network_context=*/true,
                              /*use_prefetch_proxy=*/false);
  PrefetchType prefetch_type3(/*use_isolated_network_context=*/false,
                              /*use_prefetch_proxy=*/false);

  EXPECT_TRUE(prefetch_type1.IsIsolatedNetworkContextRequired());
  EXPECT_TRUE(prefetch_type1.IsProxyRequired());

  EXPECT_TRUE(prefetch_type2.IsIsolatedNetworkContextRequired());
  EXPECT_FALSE(prefetch_type2.IsProxyRequired());

  EXPECT_FALSE(prefetch_type3.IsIsolatedNetworkContextRequired());
  EXPECT_FALSE(prefetch_type3.IsProxyRequired());
}

TEST_F(PrefetchTypeTest, ComparePrefetchTypes) {
  PrefetchType prefetch_type1(/*use_isolated_network_context=*/true,
                              /*use_prefetch_proxy=*/true);
  PrefetchType prefetch_type2(/*use_isolated_network_context=*/true,
                              /*use_prefetch_proxy=*/true);
  PrefetchType prefetch_type3(/*use_isolated_network_context=*/true,
                              /*use_prefetch_proxy=*/false);
  PrefetchType prefetch_type4(/*use_isolated_network_context=*/false,
                              /*use_prefetch_proxy=*/false);

  // Explicitly test the == and != operators for |PrefetchType|.
  EXPECT_TRUE(prefetch_type1 == prefetch_type1);
  EXPECT_TRUE(prefetch_type1 == prefetch_type2);
  EXPECT_TRUE(prefetch_type1 != prefetch_type3);
  EXPECT_TRUE(prefetch_type1 != prefetch_type4);
}

}  // namespace
}  // namespace content
