// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalBoxFragment_h
#define NGPhysicalBoxFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "core/layout/ng/inline/ng_baseline.h"
#include "core/layout/ng/ng_physical_container_fragment.h"

namespace blink {

class CORE_EXPORT NGPhysicalBoxFragment final
    : public NGPhysicalContainerFragment {
 public:
  // This modifies the passed-in children vector.
  NGPhysicalBoxFragment(LayoutObject* layout_object,
                        const ComputedStyle& style,
                        NGPhysicalSize size,
                        const NGPhysicalOffsetRect& contents_visual_rect,
                        Vector<RefPtr<NGPhysicalFragment>>& children,
                        Vector<NGBaseline>& baselines,
                        NGBoxType box_type,
                        unsigned,  // NGBorderEdges::Physical
                        RefPtr<NGBreakToken> break_token = nullptr);

  const NGBaseline* Baseline(const NGBaselineRequest&) const;

  // Visual rect of this box in the local coordinate. Does not include children
  // even if they overflow this box.
  const NGPhysicalOffsetRect LocalVisualRect() const;

  // Visual rect of children in the local coordinate.
  const NGPhysicalOffsetRect& ContentsVisualRect() const {
    return contents_visual_rect_;
  }

  RefPtr<NGPhysicalFragment> CloneWithoutOffset() const;

 private:
  NGPhysicalOffsetRect contents_visual_rect_;
  Vector<NGBaseline> baselines_;
};

DEFINE_TYPE_CASTS(NGPhysicalBoxFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentBox);

}  // namespace blink

#endif  // NGPhysicalBoxFragment_h
