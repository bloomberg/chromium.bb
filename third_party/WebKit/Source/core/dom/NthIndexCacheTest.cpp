// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/NthIndexCache.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/html/HTMLElement.h"
#include "core/testing/PageTestBase.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class NthIndexCacheTest : public PageTestBase {};

TEST_F(NthIndexCacheTest, NthIndex) {
  GetDocument().documentElement()->SetInnerHTMLFromString(R"HTML(
    <body>
    <span
    id=first></span><span></span><span></span><span></span><span></span>
    <span></span><span></span><span></span><span></span><span></span>
    Text does not count
    <span id=nth-last-child></span>
    <span id=nth-child></span>
    <span></span><span></span><span></span><span></span><span></span>
    <span></span><span></span><span></span><span></span><span
    id=last></span>
    </body>
  )HTML");

  NthIndexCache nth_index_cache(GetDocument());

  EXPECT_EQ(nth_index_cache.NthChildIndex(*GetElementById("nth-child")), 12U);
  EXPECT_EQ(
      nth_index_cache.NthLastChildIndex(*GetElementById("nth-last-child")),
      12U);
}

}  // namespace blink
