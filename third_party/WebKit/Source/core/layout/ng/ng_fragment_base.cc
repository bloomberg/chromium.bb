// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragment_base.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_text_fragment.h"

namespace blink {

NGFragmentBase::NGFragmentBase(NGLogicalSize size,
                               NGLogicalSize overflow,
                               NGWritingMode writing_mode,
                               NGDirection direction,
                               NGFragmentType type)
    : size_(size),
      overflow_(overflow),
      type_(type),
      writing_mode_(writing_mode),
      direction_(direction),
      has_been_placed_(false) {}

void NGFragmentBase::SetOffset(LayoutUnit inline_offset,
                               LayoutUnit block_offset) {
  // setOffset should only be called once.
  DCHECK(!has_been_placed_);
  offset_.inlineOffset = inline_offset;
  offset_.blockOffset = block_offset;
  has_been_placed_ = true;
}

DEFINE_TRACE(NGFragmentBase) {
  if (Type() == FragmentText)
    static_cast<NGTextFragment*>(this)->traceAfterDispatch(visitor);
  else
    static_cast<NGFragment*>(this)->traceAfterDispatch(visitor);
}

}  // namespace blink
