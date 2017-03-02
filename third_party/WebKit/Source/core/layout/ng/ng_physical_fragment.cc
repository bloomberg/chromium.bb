// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_fragment.h"

#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_physical_text_fragment.h"

namespace blink {

NGPhysicalFragment::NGPhysicalFragment(LayoutObject* layout_object,
                                       NGPhysicalSize size,
                                       NGPhysicalSize overflow,
                                       NGFragmentType type,
                                       RefPtr<NGBreakToken> break_token)
    : layout_object_(layout_object),
      size_(size),
      overflow_(overflow),
      break_token_(std::move(break_token)),
      type_(type),
      is_placed_(false) {}

void NGPhysicalFragment::destroy() const {
  if (Type() == kFragmentText)
    delete static_cast<const NGPhysicalTextFragment*>(this);
  else
    delete static_cast<const NGPhysicalBoxFragment*>(this);
}

const ComputedStyle& NGPhysicalFragment::Style() const {
  DCHECK(layout_object_);
  return layout_object_->styleRef();
}

}  // namespace blink
