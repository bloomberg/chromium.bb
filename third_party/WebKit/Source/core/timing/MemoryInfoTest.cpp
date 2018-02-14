/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/timing/MemoryInfo.h"

#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(MemoryInfo, quantizeMemorySize) {
  EXPECT_EQ(10000000u, QuantizeMemorySize(1024));
  EXPECT_EQ(10000000u, QuantizeMemorySize(1024 * 1024));
  EXPECT_EQ(410000000u, QuantizeMemorySize(389472983));
  EXPECT_EQ(39600000u, QuantizeMemorySize(38947298));
  EXPECT_EQ(29400000u, QuantizeMemorySize(28947298));
  EXPECT_EQ(19300000u, QuantizeMemorySize(18947298));
  EXPECT_EQ(14300000u, QuantizeMemorySize(13947298));
  EXPECT_EQ(10000000u, QuantizeMemorySize(3894729));
  EXPECT_EQ(10000000u, QuantizeMemorySize(389472));
  EXPECT_EQ(10000000u, QuantizeMemorySize(38947));
  EXPECT_EQ(10000000u, QuantizeMemorySize(3894));
  EXPECT_EQ(10000000u, QuantizeMemorySize(389));
  EXPECT_EQ(10000000u, QuantizeMemorySize(38));
  EXPECT_EQ(10000000u, QuantizeMemorySize(3));
  EXPECT_EQ(10000000u, QuantizeMemorySize(1));
  EXPECT_EQ(10000000u, QuantizeMemorySize(0));
}

TEST(MemoryInfo, nullableValues) {
  RuntimeEnabledFeatures::SetPreciseMemoryInfoEnabled(true);
  // Having a single page, MemoryInfo provides all values.
  std::unique_ptr<DummyPageHolder> page_holder = DummyPageHolder::Create();
  Page::OrdinaryPages().insert(&page_holder->GetPage());
  Persistent<MemoryInfo> info(MemoryInfo::Create());
  bool is_null;
  EXPECT_LT(0U, info->totalJSHeapSize(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_LT(0U, info->usedJSHeapSize(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_LT(0U, info->jsHeapSizeLimit());

  // Now two pages total. MemoryInfo should only provide jsHeapSizeLimit.
  std::unique_ptr<DummyPageHolder> other_page_holder =
      DummyPageHolder::Create();
  Page::OrdinaryPages().insert(&other_page_holder->GetPage());
  info = MemoryInfo::Create();
  EXPECT_EQ(0U, info->totalJSHeapSize(is_null));
  EXPECT_TRUE(is_null);
  EXPECT_EQ(0U, info->usedJSHeapSize(is_null));
  EXPECT_TRUE(is_null);
  EXPECT_LT(0U, info->jsHeapSizeLimit());

  // Remove both pages. MemoryInfo should now provide all values.
  Page::OrdinaryPages().clear();
  info = MemoryInfo::Create();
  EXPECT_LT(0U, info->totalJSHeapSize(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_LT(0U, info->usedJSHeapSize(is_null));
  EXPECT_FALSE(is_null);
  EXPECT_LT(0U, info->jsHeapSizeLimit());
}

}  // namespace blink
