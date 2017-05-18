// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_box_fragment.h"

#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/ng_macros.h"
#include "core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGLogicalSize NGBoxFragment::OverflowSize() const {
  auto* physical_fragment = ToNGPhysicalBoxFragment(physical_fragment_);
  return physical_fragment->OverflowSize().ConvertToLogical(WritingMode());
}

const WTF::Optional<NGLogicalOffset>& NGBoxFragment::BfcOffset() const {
  WRITING_MODE_IGNORED(
      "Accessing BFC offset is allowed here because writing"
      "modes are irrelevant in this case.");
  return ToNGPhysicalBoxFragment(physical_fragment_)->BfcOffset();
}

const NGMarginStrut& NGBoxFragment::EndMarginStrut() const {
  WRITING_MODE_IGNORED(
      "Accessing the margin strut is fine here. Changing the writing mode"
      "establishes a new formatting context, for which a margin strut is"
      "never set for a fragment.");
  return ToNGPhysicalBoxFragment(physical_fragment_)->EndMarginStrut();
}

}  // namespace blink
