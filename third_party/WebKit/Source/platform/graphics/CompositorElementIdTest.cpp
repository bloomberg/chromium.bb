// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorElementId.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CompositorElementIdTest : public ::testing::Test {};

TEST_F(CompositorElementIdTest, EncodeDecode) {
  CompositorElementId element_id =
      CreateCompositorElementId(1, CompositorSubElementId::kPrimary);
  EXPECT_EQ(1u, DomNodeIdFromCompositorElementId(element_id));
  EXPECT_EQ(CompositorSubElementId::kPrimary,
            SubElementIdFromCompositorElementId(element_id));

  element_id =
      CreateCompositorElementId(1, CompositorSubElementId::kLinkHighlight);
  EXPECT_EQ(1u, DomNodeIdFromCompositorElementId(element_id));
  EXPECT_EQ(CompositorSubElementId::kLinkHighlight,
            SubElementIdFromCompositorElementId(element_id));

  element_id = CreateCompositorElementId(23, CompositorSubElementId::kScroll);
  EXPECT_EQ(23u, DomNodeIdFromCompositorElementId(element_id));
  EXPECT_EQ(CompositorSubElementId::kScroll,
            SubElementIdFromCompositorElementId(element_id));
}

}  // namespace blink
