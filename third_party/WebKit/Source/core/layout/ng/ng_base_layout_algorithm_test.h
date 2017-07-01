// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NG_BASE_LAYOUT_ALGORITHM_TEST_H_
#define NG_BASE_LAYOUT_ALGORITHM_TEST_H_

#include "core/layout/LayoutTestHelper.h"

#include "core/dom/Element.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// Base class for all LayoutNG Algorithms unit test classes.
typedef bool TestParamLayoutNG;
class NGBaseLayoutAlgorithmTest
    : public ::testing::WithParamInterface<TestParamLayoutNG>,
      public RenderingTest {
 public:
  NGBaseLayoutAlgorithmTest();
  ~NGBaseLayoutAlgorithmTest();

 protected:
  void SetUp() override;

  std::pair<RefPtr<NGPhysicalBoxFragment>, RefPtr<NGConstraintSpace>>
  RunBlockLayoutAlgorithmForElement(Element* element);
};

}  // namespace blink

#endif  // NG_BASE_LAYOUT_ALGORITHM_TEST_H_
