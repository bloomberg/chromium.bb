// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

struct SameSizeAsNGPhysicalLineBoxFragment : NGPhysicalContainerFragment {
  void* pointer;
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
          builder->children_.size() * sizeof(NGLinkStorage),
      ::WTF::GetStringWithTypeName<NGPhysicalLineBoxFragment>());
  new (data) NGPhysicalLineBoxFragment(builder);
  return base::AdoptRef(static_cast<NGPhysicalLineBoxFragment*>(data));
}

NGPhysicalLineBoxFragment::NGPhysicalLineBoxFragment(
    NGLineBoxFragmentBuilder* builder)
    : NGPhysicalContainerFragment(builder,
                                  builder->GetWritingMode(),
                                  children_,
                                  kFragmentLineBox,
                                  builder->line_box_type_),
      metrics_(builder->metrics_) {
  style_ = std::move(builder->style_);
  has_propagated_descendants_ = builder->HasPropagatedDescendants();
  base_direction_ = static_cast<unsigned>(builder->base_direction_);
}

NGLineHeightMetrics NGPhysicalLineBoxFragment::BaselineMetrics(
    FontBaseline) const {
  // TODO(kojii): Computing other baseline types than the used one is not
  // implemented yet.
  // TODO(kojii): We might need locale/script to look up OpenType BASE table.
  return metrics_;
}

PhysicalRect NGPhysicalLineBoxFragment::ScrollableOverflow(
    const LayoutObject* container,
    const ComputedStyle* container_style,
    PhysicalSize container_physical_size) const {
  WritingMode container_writing_mode = container_style->GetWritingMode();
  TextDirection container_direction = container_style->Direction();
  PhysicalRect overflow({}, Size());
  for (const auto& child : Children()) {
    PhysicalRect child_scroll_overflow =
        child->ScrollableOverflowForPropagation(container);
    child_scroll_overflow.offset += child.Offset();
    // If child has the same style as parent, parent will compute relative
    // offset.
    if (&child->Style() != container_style) {
      child_scroll_overflow.offset +=
          ComputeRelativeOffset(child->Style(), container_writing_mode,
                                container_direction, container_physical_size);
    }
    overflow.Unite(child_scroll_overflow);
  }
  return overflow;
}

const NGPhysicalFragment* NGPhysicalLineBoxFragment::FirstLogicalLeaf() const {
  if (Children().IsEmpty())
    return nullptr;
  // TODO(xiaochengh): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  const TextDirection direction = Style().Direction();
  const NGPhysicalFragment* runner = this;
  while (const auto* runner_as_container =
             DynamicTo<NGPhysicalContainerFragment>(runner)) {
    if (runner->IsBlockFormattingContextRoot())
      break;
    if (runner_as_container->Children().IsEmpty())
      break;
    runner = direction == TextDirection::kLtr
                 ? runner_as_container->Children().front().get()
                 : runner_as_container->Children().back().get();
  }
  DCHECK_NE(runner, this);
  return runner;
}

const NGPhysicalFragment* NGPhysicalLineBoxFragment::LastLogicalLeaf() const {
  if (Children().IsEmpty())
    return nullptr;
  // TODO(xiaochengh): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  const TextDirection direction = Style().Direction();
  const NGPhysicalFragment* runner = this;
  while (const auto* runner_as_container =
             DynamicTo<NGPhysicalContainerFragment>(runner)) {
    if (runner->IsBlockFormattingContextRoot())
      break;
    if (runner_as_container->Children().IsEmpty())
      break;
    runner = direction == TextDirection::kLtr
                 ? runner_as_container->Children().back().get()
                 : runner_as_container->Children().front().get();
  }
  DCHECK_NE(runner, this);
  return runner;
}

bool NGPhysicalLineBoxFragment::HasSoftWrapToNextLine() const {
  const auto& break_token = To<NGInlineBreakToken>(*BreakToken());
  return !break_token.IsFinished() && !break_token.IsForcedBreak();
}

PhysicalOffset NGPhysicalLineBoxFragment::LineStartPoint() const {
  const LogicalOffset logical_start;  // (0, 0)
  const PhysicalSize pixel_size(LayoutUnit(1), LayoutUnit(1));
  return logical_start.ConvertToPhysical(Style().GetWritingMode(),
                                         BaseDirection(), Size(), pixel_size);
}

PhysicalOffset NGPhysicalLineBoxFragment::LineEndPoint() const {
  const LayoutUnit inline_size =
      NGFragment(Style().GetWritingMode(), *this).InlineSize();
  const LogicalOffset logical_end(inline_size, LayoutUnit());
  const PhysicalSize pixel_size(LayoutUnit(1), LayoutUnit(1));
  return logical_end.ConvertToPhysical(Style().GetWritingMode(),
                                       BaseDirection(), Size(), pixel_size);
}

}  // namespace blink
