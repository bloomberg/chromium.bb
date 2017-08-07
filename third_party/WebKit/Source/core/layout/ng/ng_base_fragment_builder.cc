// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_base_fragment_builder.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_layout_result.h"

namespace blink {

NGBaseFragmentBuilder::NGBaseFragmentBuilder(const ComputedStyle& style,
                                             NGWritingMode writing_mode,
                                             TextDirection direction)
    : style_(&style), writing_mode_(writing_mode), direction_(direction) {
  DCHECK(&style);
}

NGBaseFragmentBuilder::NGBaseFragmentBuilder(NGWritingMode writing_mode,
                                             TextDirection direction)
    : writing_mode_(writing_mode), direction_(direction) {}

NGBaseFragmentBuilder& NGBaseFragmentBuilder::SetStyle(
    const ComputedStyle& style) {
  DCHECK(&style);
  style_ = &style;
  return *this;
}

}  // namespace blink
