// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class HTMLElementTest : public PageTestBase {};

TEST_F(HTMLElementTest, AdjustDirectionalityInFlatTree) {
  SetBodyContent("<bdi><summary><i id=target></i></summary></bdi>");
  UpdateAllLifecyclePhasesForTest();
  GetDocument().getElementById("target")->remove();
  // Pass if not crashed.
}

}  // namespace blink
