// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalBoxFragment_h
#define NGPhysicalBoxFragment_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"

namespace blink {

class CORE_EXPORT NGPhysicalBoxFragment final
    : public NGPhysicalContainerFragment {
 public:
  // This modifies the passed-in children vector.
  NGPhysicalBoxFragment(LayoutObject* layout_object,
                        const ComputedStyle& style,
                        NGPhysicalSize size,
                        Vector<scoped_refptr<NGPhysicalFragment>>& children,
                        const NGPixelSnappedPhysicalBoxStrut& padding,
                        const NGPhysicalOffsetRect& contents_visual_rect,
                        Vector<NGBaseline>& baselines,
                        NGBoxType box_type,
                        bool is_old_layout_root,
                        unsigned,  // NGBorderEdges::Physical
                        scoped_refptr<NGBreakToken> break_token = nullptr);

  const NGBaseline* Baseline(const NGBaselineRequest&) const;

  const NGPixelSnappedPhysicalBoxStrut& Padding() const { return padding_; }

  bool HasSelfPaintingLayer() const;
  bool ChildrenInline() const;

  // True if overflow != 'visible', except for certain boxes that do not allow
  // overflow clip; i.e., AllowOverflowClip() returns false.
  bool HasOverflowClip() const;
  bool ShouldClipOverflow() const;

  // Visual rect of this box in the local coordinate. Does not include children
  // even if they overflow this box.
  NGPhysicalOffsetRect SelfVisualRect() const;

  // VisualRect of itself including contents, in the local coordinate.
  NGPhysicalOffsetRect VisualRectWithContents() const;

  void AddSelfOutlineRects(Vector<LayoutRect>*,
                           const LayoutPoint& additional_offset) const;

  PositionWithAffinity PositionForPoint(const NGPhysicalOffset&) const override;

  scoped_refptr<NGPhysicalFragment> CloneWithoutOffset() const;

 private:
  Vector<NGBaseline> baselines_;
  NGPixelSnappedPhysicalBoxStrut padding_;
};

DEFINE_TYPE_CASTS(NGPhysicalBoxFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentBox);

}  // namespace blink

#endif  // NGPhysicalBoxFragment_h
