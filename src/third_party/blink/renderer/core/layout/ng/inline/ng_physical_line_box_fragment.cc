// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

struct SameSizeAsNGPhysicalLineBoxFragment : NGPhysicalContainerFragment {
  NGLineHeightMetrics metrics;
};

static_assert(sizeof(NGPhysicalLineBoxFragment) ==
                  sizeof(SameSizeAsNGPhysicalLineBoxFragment),
              "NGPhysicalLineBoxFragment should stay small");

}  // namespace

scoped_refptr<const NGPhysicalLineBoxFragment>
NGPhysicalLineBoxFragment::Create(NGLineBoxFragmentBuilder* builder) {
  // We store the children list inline in the fragment as a flexible
  // array. Therefore, we need to make sure to allocate enough space for
  // that array here, which requires a manual allocation + placement new.
  // The initialization of the array is done by NGPhysicalContainerFragment;
  // we pass the buffer as a constructor argument.
  void* data = ::WTF::Partitions::FastMalloc(
      sizeof(NGPhysicalLineBoxFragment) +
          builder->children_.size() * sizeof(NGLink),
      ::WTF::GetStringWithTypeName<NGPhysicalLineBoxFragment>());
  new (data) NGPhysicalLineBoxFragment(PassKey(), builder);
  return base::AdoptRef(static_cast<NGPhysicalLineBoxFragment*>(data));
}

NGPhysicalLineBoxFragment::NGPhysicalLineBoxFragment(
    PassKey key,
    NGLineBoxFragmentBuilder* builder)
    : NGPhysicalContainerFragment(builder,
                                  builder->GetWritingMode(),
                                  children_,
                                  kFragmentLineBox,
                                  builder->line_box_type_),
      metrics_(builder->metrics_) {
  // A line box must have a metrics unless it's an empty line box.
  DCHECK(!metrics_.IsEmpty() || IsEmptyLineBox());
  base_or_resolved_direction_ = static_cast<unsigned>(builder->base_direction_);
  has_hanging_ = builder->hang_inline_size_ != 0;
  has_propagated_descendants_ = has_floating_descendants_for_paint_ ||
                                HasOutOfFlowPositionedDescendants() ||
                                builder->unpositioned_list_marker_;
}

NGLineHeightMetrics NGPhysicalLineBoxFragment::BaselineMetrics() const {
  // TODO(kojii): Computing other baseline types than the used one is not
  // implemented yet.
  // TODO(kojii): We might need locale/script to look up OpenType BASE table.
  return metrics_;
}

namespace {

// Include the inline-size of the line-box in the overflow.
inline void AddInlineSizeToOverflow(const PhysicalRect& rect,
                                    const WritingMode container_writing_mode,
                                    PhysicalRect* overflow) {
  PhysicalRect inline_rect;
  inline_rect.offset = rect.offset;
  if (IsHorizontalWritingMode(container_writing_mode))
    inline_rect.size.width = rect.size.width;
  else
    inline_rect.size.height = rect.size.height;
  overflow->UniteEvenIfEmpty(inline_rect);
}

}  // namespace

PhysicalRect NGPhysicalLineBoxFragment::ScrollableOverflow(
    const NGPhysicalBoxFragment& container,
    const ComputedStyle& container_style) const {
  const WritingMode container_writing_mode = container_style.GetWritingMode();
  const TextDirection container_direction = container_style.Direction();
  PhysicalRect overflow;
  for (const auto& child : Children()) {
    PhysicalRect child_scroll_overflow =
        child->ScrollableOverflowForPropagation(container);
    child_scroll_overflow.offset += child.Offset();

    if (UNLIKELY(has_hanging_ && !child->IsFloatingOrOutOfFlowPositioned())) {
      AdjustScrollableOverflowForHanging(LocalRect(), container_writing_mode,
                                         &child_scroll_overflow);
    }

    // For implementation reasons, text nodes inherit computed style from their
    // container, including everything, also non-inherited properties. So, if
    // the container has a relative offset, this will be falsely reflected on
    // text children. We need to guard against this.
    if (!child->IsText()) {
      child_scroll_overflow.offset +=
          ComputeRelativeOffset(child->Style(), container_writing_mode,
                                container_direction, container.Size());
    }
    overflow.Unite(child_scroll_overflow);
  }

  // Make sure we include the inline-size of the line-box in the overflow.
  AddInlineSizeToOverflow(LocalRect(), container_writing_mode, &overflow);

  return overflow;
}

PhysicalRect NGPhysicalLineBoxFragment::ScrollableOverflowForLine(
    const NGPhysicalBoxFragment& container,
    const ComputedStyle& container_style,
    const NGFragmentItem& line,
    const NGInlineCursor& cursor) const {
  DCHECK(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  DCHECK_EQ(&line, cursor.CurrentItem());
  DCHECK_EQ(line.LineBoxFragment(), this);

  PhysicalRect overflow;
  AddScrollableOverflowForInlineChild(container, container_style, line,
                                      has_hanging_, cursor, &overflow);

  // Make sure we include the inline-size of the line-box in the overflow.
  // Note, the bottom half-leading should not be included. crbug.com/996847
  const WritingMode container_writing_mode = container_style.GetWritingMode();
  AddInlineSizeToOverflow(line.RectInContainerBlock(), container_writing_mode,
                          &overflow);

  return overflow;
}

bool NGPhysicalLineBoxFragment::HasSoftWrapToNextLine() const {
  const auto& break_token = To<NGInlineBreakToken>(*BreakToken());
  return !break_token.IsFinished() && !break_token.IsForcedBreak();
}

}  // namespace blink
