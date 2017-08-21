// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_base_fragment_builder.h"

namespace blink {

NGBaseFragmentBuilder::NGBaseFragmentBuilder(RefPtr<const ComputedStyle> style,
                                             NGWritingMode writing_mode,
                                             TextDirection direction)
    : style_(std::move(style)),
      writing_mode_(writing_mode),
      direction_(direction) {
  DCHECK(style_);
}

NGBaseFragmentBuilder::NGBaseFragmentBuilder(NGWritingMode writing_mode,
                                             TextDirection direction)
    : writing_mode_(writing_mode), direction_(direction) {}

NGBaseFragmentBuilder& NGBaseFragmentBuilder::SetStyle(
    RefPtr<const ComputedStyle> style) {
  DCHECK(style);
  style_ = std::move(style);
  return *this;
}

}  // namespace blink
