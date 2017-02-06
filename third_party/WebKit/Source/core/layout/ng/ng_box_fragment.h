// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBoxFragment_h
#define NGBoxFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_units.h"
#include "core/layout/ng/ng_writing_mode.h"

namespace blink {

class CORE_EXPORT NGBoxFragment final : public NGFragment {
 public:
  NGBoxFragment(NGWritingMode writing_mode,
                NGPhysicalBoxFragment* physical_fragment)
      : NGFragment(writing_mode, physical_fragment) {}

  const WTF::Optional<NGLogicalOffset>& BfcOffset() const;

  const NGMarginStrut& EndMarginStrut() const;
};

DEFINE_TYPE_CASTS(NGBoxFragment,
                  NGFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentBox);

}  // namespace blink

#endif  // NGBoxFragment_h
