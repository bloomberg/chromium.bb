// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBoxFragment_h
#define NGBoxFragment_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

class NGPhysicalBoxFragment;
struct NGBaselineRequest;
struct NGLineHeightMetrics;

class CORE_EXPORT NGBoxFragment final : public NGFragment {
 public:
  NGBoxFragment(WritingMode writing_mode,
                const NGPhysicalBoxFragment& physical_fragment)
      : NGFragment(writing_mode, physical_fragment) {}

  // Compute baseline metrics (ascent/descent) for this box.
  //
  // Baseline requests must be added to constraint space when this fragment was
  // laid out.
  //
  // The "WithoutSynthesize" version returns an empty metrics if this box does
  // not have any baselines, while the other version synthesize the baseline
  // from the box.
  NGLineHeightMetrics BaselineMetricsWithoutSynthesize(
      const NGBaselineRequest&,
      const NGConstraintSpace&) const;
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
