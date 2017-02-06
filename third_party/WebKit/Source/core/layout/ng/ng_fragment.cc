// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragment.h"

namespace blink {

LayoutUnit NGFragment::InlineSize() const {
  return writing_mode_ == kHorizontalTopBottom ? physical_fragment_->Width()
                                               : physical_fragment_->Height();
}

LayoutUnit NGFragment::BlockSize() const {
  return writing_mode_ == kHorizontalTopBottom ? physical_fragment_->Height()
                                               : physical_fragment_->Width();
}

LayoutUnit NGFragment::InlineOverflow() const {
  return writing_mode_ == kHorizontalTopBottom
             ? physical_fragment_->WidthOverflow()
             : physical_fragment_->HeightOverflow();
}

LayoutUnit NGFragment::BlockOverflow() const {
  return writing_mode_ == kHorizontalTopBottom
             ? physical_fragment_->HeightOverflow()
             : physical_fragment_->WidthOverflow();
}

LayoutUnit NGFragment::InlineOffset() const {
  return writing_mode_ == kHorizontalTopBottom
             ? physical_fragment_->LeftOffset()
             : physical_fragment_->TopOffset();
}

LayoutUnit NGFragment::BlockOffset() const {
  return writing_mode_ == kHorizontalTopBottom
             ? physical_fragment_->TopOffset()
             : physical_fragment_->LeftOffset();
}

NGPhysicalFragment::NGFragmentType NGFragment::Type() const {
  return physical_fragment_->Type();
}

}  // namespace blink
