// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalLineBoxFragment_h
#define NGPhysicalLineBoxFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/inline/ng_line_height_metrics.h"
#include "core/layout/ng/ng_physical_container_fragment.h"
#include "platform/fonts/FontBaseline.h"

namespace blink {

class CORE_EXPORT NGPhysicalLineBoxFragment final
    : public NGPhysicalContainerFragment {
 public:
  // This modifies the passed-in children vector.
  NGPhysicalLineBoxFragment(const ComputedStyle&,
                            NGPhysicalSize size,
                            Vector<scoped_refptr<NGPhysicalFragment>>& children,
                            const NGPhysicalOffsetRect& contents_visual_rect,
                            const NGLineHeightMetrics&,
                            scoped_refptr<NGBreakToken> break_token = nullptr);

  const NGLineHeightMetrics& Metrics() const { return metrics_; }

  // Compute baseline for the specified baseline type.
  LayoutUnit BaselinePosition(FontBaseline) const;

  // VisualRect of itself including contents, in the local coordinate.
  NGPhysicalOffsetRect VisualRectWithContents() const;

  scoped_refptr<NGPhysicalFragment> CloneWithoutOffset() const {
    Vector<scoped_refptr<NGPhysicalFragment>> children_copy(children_);
    return base::AdoptRef(new NGPhysicalLineBoxFragment(
        Style(), size_, children_copy, contents_visual_rect_, metrics_,
        break_token_));
  }

 private:
  NGLineHeightMetrics metrics_;
};

DEFINE_TYPE_CASTS(NGPhysicalLineBoxFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentLineBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentLineBox);

}  // namespace blink

#endif  // NGPhysicalBoxFragment_h
