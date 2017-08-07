// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalLineBoxFragment_h
#define NGPhysicalLineBoxFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/inline/ng_line_height_metrics.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "platform/fonts/FontBaseline.h"

namespace blink {

class CORE_EXPORT NGPhysicalLineBoxFragment final : public NGPhysicalFragment {
 public:
  // This modifies the passed-in children vector.
  NGPhysicalLineBoxFragment(const ComputedStyle&,
                            NGPhysicalSize size,
                            Vector<RefPtr<NGPhysicalFragment>>& children,
                            const NGLineHeightMetrics&,
                            RefPtr<NGBreakToken> break_token = nullptr);

  const Vector<RefPtr<NGPhysicalFragment>>& Children() const {
    return children_;
  }

  const NGLineHeightMetrics& Metrics() const { return metrics_; }

  // Compute baseline for the specified baseline type.
  LayoutUnit BaselinePosition(FontBaseline) const;

  RefPtr<NGPhysicalFragment> CloneWithoutOffset() const {
    Vector<RefPtr<NGPhysicalFragment>> children_copy(children_);
    return AdoptRef(new NGPhysicalLineBoxFragment(Style(), size_, children_copy,
                                                  metrics_, break_token_));
  }

 private:
  Vector<RefPtr<NGPhysicalFragment>> children_;

  NGLineHeightMetrics metrics_;
};

DEFINE_TYPE_CASTS(NGPhysicalLineBoxFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentLineBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentLineBox);

}  // namespace blink

#endif  // NGPhysicalBoxFragment_h
