// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalBoxFragment_h
#define NGPhysicalBoxFragment_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"

namespace blink {

enum class NGOutlineType;
class CORE_EXPORT NGPhysicalBoxFragment final
    : public NGPhysicalContainerFragment {
 public:
  // This modifies the passed-in children vector.
  NGPhysicalBoxFragment(LayoutObject* layout_object,
                        const ComputedStyle& style,
                        NGStyleVariant style_variant,
                        NGPhysicalSize size,
                        Vector<NGLink>& children,
                        const NGPhysicalBoxStrut& border,
                        const NGPhysicalBoxStrut& padding,
                        const NGPhysicalOffsetRect& contents_ink_overflow,
                        Vector<NGBaseline>& baselines,
                        NGBoxType box_type,
                        bool is_old_layout_root,
                        unsigned,  // NGBorderEdges::Physical
                        scoped_refptr<NGBreakToken> break_token = nullptr);

  const NGBaseline* Baseline(const NGBaselineRequest&) const;

  const NGPhysicalBoxStrut Borders() const { return borders_; }

  const NGPhysicalBoxStrut Padding() const { return padding_; }

  NGPixelSnappedPhysicalBoxStrut PixelSnappedPadding() const {
    return padding_.SnapToDevicePixels();
  }

  bool HasSelfPaintingLayer() const;
  bool ChildrenInline() const { return children_inline_; }

  // True if overflow != 'visible', except for certain boxes that do not allow
  // overflow clip; i.e., AllowOverflowClip() returns false.
  bool HasOverflowClip() const;
  bool ShouldClipOverflow() const;

  NGPhysicalOffsetRect ScrollableOverflow() const;

  // TODO(layout-dev): These three methods delegate to legacy layout for now,
  // update them to use LayoutNG based overflow information from the fragment
  // and change them to use NG geometry types once LayoutNG supports overflow.
  LayoutRect OverflowClipRect(
      const LayoutPoint& location,
      OverlayScrollbarClipBehavior = kIgnorePlatformOverlayScrollbarSize) const;
  IntSize ScrolledContentOffset() const;
  LayoutSize ScrollSize() const;

  // Visual rect of this box in the local coordinate. Does not include children
  // even if they overflow this box.
  NGPhysicalOffsetRect SelfInkOverflow() const;

  // Ink overflow including contents, in the local coordinates.
  NGPhysicalOffsetRect InkOverflow(bool apply_clip) const;

  // Fragment offset is this fragment's offset from parent.
  // Needed to compensate for LayoutInline Legacy code offsets.
  void AddSelfOutlineRects(Vector<LayoutRect>* outline_rects,
                           const LayoutPoint& additional_offset,
                           NGOutlineType include_block_overflows) const;

  UBiDiLevel BidiLevel() const override;

  scoped_refptr<const NGPhysicalFragment> CloneWithoutOffset() const;

 private:
  Vector<NGBaseline> baselines_;
  NGPhysicalBoxStrut borders_;
  NGPhysicalBoxStrut padding_;
  NGPhysicalOffsetRect descendant_outlines_;
};

DEFINE_TYPE_CASTS(NGPhysicalBoxFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentBox);

}  // namespace blink

#endif  // NGPhysicalBoxFragment_h
