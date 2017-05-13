// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorElementId.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CompositorElementIdTest : public ::testing::Test {};

TEST_F(CompositorElementIdTest, EncodeDecode) {
  CompositorElementId element_id = CompositorElementIdFromDOMNodeId(
      1, CompositorElementIdNamespace::kPrimary);
  EXPECT_EQ(1u, IdFromCompositorElementId(element_id));
  EXPECT_EQ(CompositorElementIdNamespace::kPrimary,
            NamespaceFromCompositorElementId(element_id));

  element_id = CompositorElementIdFromDOMNodeId(
      1, CompositorElementIdNamespace::kLinkHighlight);
  EXPECT_EQ(1u, IdFromCompositorElementId(element_id));
  EXPECT_EQ(CompositorElementIdNamespace::kLinkHighlight,
            NamespaceFromCompositorElementId(element_id));

  element_id = CompositorElementIdFromDOMNodeId(
      23, CompositorElementIdNamespace::kScroll);
  EXPECT_EQ(23u, IdFromCompositorElementId(element_id));
  EXPECT_EQ(CompositorElementIdNamespace::kScroll,
            NamespaceFromCompositorElementId(element_id));
}

}  // namespace blink
