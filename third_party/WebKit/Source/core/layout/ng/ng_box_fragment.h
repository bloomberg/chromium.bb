// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBoxFragment_h
#define NGBoxFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_writing_mode.h"

namespace blink {

class NGPhysicalBoxFragment;
struct NGBaselineRequest;
struct NGLineHeightMetrics;
struct NGLogicalSize;

class CORE_EXPORT NGBoxFragment final : public NGFragment {
 public:
  NGBoxFragment(NGWritingMode writing_mode,
                const NGPhysicalBoxFragment& physical_fragment)
      : NGFragment(writing_mode, physical_fragment) {}

  // Returns the total size, including the contents outside of the border-box.
  NGLogicalSize OverflowSize() const;

  // Compute baseline metrics (ascent/descent) for this box.
  //
  // Baseline requests must be added to constraint space when this fragment was
  // laid out.
  NGLineHeightMetrics BaselineMetrics(const NGBaselineRequest&,
                                      const NGConstraintSpace&) const;
};

DEFINE_TYPE_CASTS(NGBoxFragment,
                  NGFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentBox);

}  // namespace blink

#endif  // NGBoxFragment_h
