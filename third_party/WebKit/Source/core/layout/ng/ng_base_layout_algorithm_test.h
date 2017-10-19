// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NG_BASE_LAYOUT_ALGORITHM_TEST_H_
#define NG_BASE_LAYOUT_ALGORITHM_TEST_H_

#include "core/layout/LayoutTestHelper.h"

#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/text/TextDirection.h"
#include "platform/wtf/Allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class Element;

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

  static const NGPhysicalBoxFragment* CurrentFragmentFor(
      const LayoutNGBlockFlow*);
};

class FragmentChildIterator {
  STACK_ALLOCATED();

 public:
  explicit FragmentChildIterator(const NGPhysicalBoxFragment* parent) {
    SetParent(parent);
  }
  void SetParent(const NGPhysicalBoxFragment* parent) {
    parent_ = parent;
    index_ = 0;
  }

  const NGPhysicalBoxFragment* NextChild();

 private:
  const NGPhysicalBoxFragment* parent_;
  unsigned index_;
};

RefPtr<NGConstraintSpace> ConstructBlockLayoutTestConstraintSpace(
    NGWritingMode writing_mode,
    TextDirection direction,
    NGLogicalSize size,
    bool shrink_to_fit = false,
    bool is_new_formatting_context = false,
    LayoutUnit fragmentainer_space_available = LayoutUnit());

}  // namespace blink

#endif  // NG_BASE_LAYOUT_ALGORITHM_TEST_H_
