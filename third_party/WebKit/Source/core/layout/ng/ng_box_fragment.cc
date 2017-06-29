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

}  // namespace blink
