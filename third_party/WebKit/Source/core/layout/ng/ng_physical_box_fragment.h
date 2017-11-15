// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalBoxFragment_h
#define NGPhysicalBoxFragment_h

#include "core/CoreExport.h"
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
                        Vector<scoped_refptr<NGPhysicalFragment>>& children,
                        const NGPhysicalOffsetRect& contents_visual_rect,
                        Vector<NGBaseline>& baselines,
                        NGBoxType box_type,
                        unsigned,  // NGBorderEdges::Physical
                        scoped_refptr<NGBreakToken> break_token = nullptr);

  const NGBaseline* Baseline(const NGBaselineRequest&) const;

  bool HasSelfPaintingLayer() const;

  // True if overflow != 'visible', except for certain boxes that do not allow
  // overflow clip; i.e., AllowOverflowClip() returns false.
  bool HasOverflowClip() const;
  bool ShouldClipOverflow() const;

  // Visual rect of this box in the local coordinate. Does not include children
  // even if they overflow this box.
  NGPhysicalOffsetRect SelfVisualRect() const;

  // VisualRect of itself including contents, in the local coordinate.
  NGPhysicalOffsetRect VisualRectWithContents() const;

  scoped_refptr<NGPhysicalFragment> CloneWithoutOffset() const;

 private:
  Vector<NGBaseline> baselines_;
};

DEFINE_TYPE_CASTS(NGPhysicalBoxFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentBox);

}  // namespace blink

#endif  // NGPhysicalBoxFragment_h
