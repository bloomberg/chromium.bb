// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalBoxFragment_h
#define NGPhysicalBoxFragment_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/platform/graphics/scroll_types.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class NGBoxFragmentBuilder;
enum class NGOutlineType;

class CORE_EXPORT NGPhysicalBoxFragment final
    : public NGPhysicalContainerFragment {
 public:
  static scoped_refptr<const NGPhysicalBoxFragment> Create(
      NGBoxFragmentBuilder* builder,
      WritingMode block_or_line_writing_mode);

  ~NGPhysicalBoxFragment() {
    for (const NGLinkStorage& child : Children())
      child.fragment->Release();
  }

  base::Optional<LayoutUnit> Baseline(const NGBaselineRequest& request) const {
    return baselines_.Offset(request);
  }

  const NGPhysicalBoxStrut Borders() const { return borders_; }

  const NGPhysicalBoxStrut Padding() const { return padding_; }

  NGPixelSnappedPhysicalBoxStrut PixelSnappedPadding() const {
    return padding_.SnapToDevicePixels();
  }

  bool HasSelfPaintingLayer() const;
  bool ChildrenInline() const { return children_inline_; }

  PhysicalRect ScrollableOverflow() const;

  // TODO(layout-dev): These three methods delegate to legacy layout for now,
  // update them to use LayoutNG based overflow information from the fragment
  // and change them to use NG geometry types once LayoutNG supports overflow.
  LayoutRect OverflowClipRect(
      const LayoutPoint& location,
      OverlayScrollbarClipBehavior = kIgnorePlatformOverlayScrollbarSize) const;
  IntSize ScrolledContentOffset() const;
  LayoutSize ScrollSize() const;

  // Compute visual overflow of this box in the local coordinate.
  PhysicalRect ComputeSelfInkOverflow() const;

  // Fragment offset is this fragment's offset from parent.
  // Needed to compensate for LayoutInline Legacy code offsets.
  void AddSelfOutlineRects(Vector<LayoutRect>* outline_rects,
                           const LayoutPoint& additional_offset,
                           NGOutlineType include_block_overflows) const;

  UBiDiLevel BidiLevel() const;

 private:
  NGPhysicalBoxFragment(NGBoxFragmentBuilder* builder,
                        WritingMode block_or_line_writing_mode);

  NGBaselineList baselines_;
  NGPhysicalBoxStrut borders_;
  NGPhysicalBoxStrut padding_;
  NGLinkStorage children_[];
};

template <>
struct DowncastTraits<NGPhysicalBoxFragment> {
  static bool AllowFrom(const NGPhysicalFragment& fragment) {
    return fragment.Type() == NGPhysicalFragment::kFragmentBox ||
           fragment.Type() == NGPhysicalFragment::kFragmentRenderedLegend;
  }
};

}  // namespace blink

#endif  // NGPhysicalBoxFragment_h
