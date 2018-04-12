// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_rect.h"

#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"

namespace blink {

namespace {

// -------------------------------------

// Caret position calculation and its helpers.

// The calculation takes the following input:
// - An inline formatting context as a |LayoutBlockFlow|
// - An offset in the |text_content_| string of the above context
// - A TextAffinity
//
// The calculation iterates all inline fragments in the context, and tries to
// compute an NGCaretPosition using the "caret resolution process" below:
//
// The (offset, affinity) pair is compared against each inline fragment to see
// if the corresponding caret should be placed in the fragment, using the
// |TryResolveCaretPositionInXXX()| functions. These functions may return:
// - Failed, indicating that the caret must not be placed in the fragment;
// - Resolved, indicating that the care should be placed in the fragment, and
//   no further search is required. The result NGCaretPosition is returned
//   together.
// - FoundCandidate, indicating that the caret may be placed in the fragment;
//   however, further search may find a better position. The candidate
//   NGCaretPosition is also returned together.

enum class ResolutionType { kFailed, kFoundCandidate, kResolved };
struct CaretPositionResolution {
  ResolutionType type = ResolutionType::kFailed;
  NGCaretPosition caret_position;
};

bool CanResolveCaretPositionBeforeFragment(const NGPaintFragment& fragment,
                                           TextAffinity affinity) {
  if (affinity == TextAffinity::kDownstream)
    return true;
  const NGPaintFragment* current_line_paint = fragment.ContainerLineBox();
  const NGPhysicalLineBoxFragment& current_line =
      ToNGPhysicalLineBoxFragment(current_line_paint->PhysicalFragment());
  // A fragment after line wrap must be the first logical leaf in its line.
  if (&fragment.PhysicalFragment() != current_line.FirstLogicalLeaf())
    return true;
  const NGPaintFragment* last_line_paint =
      NGPaintFragmentTraversal::PreviousLineOf(*current_line_paint);
  return !last_line_paint ||
         !ToNGPhysicalLineBoxFragment(last_line_paint->PhysicalFragment())
              .HasSoftWrapToNextLine();
}

bool CanResolveCaretPositionAfterFragment(const NGPaintFragment& fragment,
                                          TextAffinity affinity) {
  if (affinity == TextAffinity::kUpstream)
    return true;
  const NGPaintFragment* current_line_paint = fragment.ContainerLineBox();
  const NGPhysicalLineBoxFragment& current_line =
      ToNGPhysicalLineBoxFragment(current_line_paint->PhysicalFragment());
  // A fragment before line wrap must be the last logical leaf in its line.
  if (&fragment.PhysicalFragment() != current_line.LastLogicalLeaf())
    return true;
  return !current_line.HasSoftWrapToNextLine();
}

CaretPositionResolution TryResolveCaretPositionInTextFragment(
    const NGPaintFragment& paint_fragment,
    unsigned offset,
    TextAffinity affinity) {
  DCHECK(paint_fragment.PhysicalFragment().IsText());
  const NGPhysicalTextFragment& fragment =
      ToNGPhysicalTextFragment(paint_fragment.PhysicalFragment());
  if (fragment.IsAnonymousText())
    return CaretPositionResolution();

  // [StartOffset(), EndOffset()] is the range allowing caret placement.
  // For example, "foo" has 4 offsets allowing caret placement.
  if (offset < fragment.StartOffset() || offset > fragment.EndOffset()) {
    // TODO(xiaochengh): This may introduce false negatives. Investigate.
    return CaretPositionResolution();
  }
  NGCaretPosition candidate = {&paint_fragment,
                               NGCaretPositionType::kAtTextOffset, offset};

  // Offsets in the interior of a fragment can be resolved directly.
  if (offset > fragment.StartOffset() && offset < fragment.EndOffset())
    return {ResolutionType::kResolved, candidate};

  if (offset == fragment.StartOffset() &&
      CanResolveCaretPositionBeforeFragment(paint_fragment, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  if (offset == fragment.EndOffset() && !fragment.IsLineBreak() &&
      CanResolveCaretPositionAfterFragment(paint_fragment, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  // We may have a better candidate
  return {ResolutionType::kFoundCandidate, candidate};
}

unsigned GetTextOffsetBefore(const NGPhysicalFragment& fragment) {
  // TODO(xiaochengh): Design more straightforward way to get text offset of
  // atomic inline box.
  DCHECK(fragment.IsAtomicInline());
  const Node* node = fragment.GetNode();
  DCHECK(node);
  const Position before_node = Position::BeforeNode(*node);
  Optional<unsigned> maybe_offset_before =
      NGOffsetMapping::GetFor(before_node)->GetTextContentOffset(before_node);
  // We should have offset mapping for atomic inline boxes.
  DCHECK(maybe_offset_before.has_value());
  return maybe_offset_before.value();
}

CaretPositionResolution TryResolveCaretPositionByBoxFragmentSide(
    const NGPaintFragment& fragment,
    unsigned offset,
    TextAffinity affinity) {
  if (!fragment.GetNode()) {
    // TODO(xiaochengh): This leads to false negatives for, e.g., RUBY, where an
    // anonymous wrapping inline block is created.
    return CaretPositionResolution();
  }

  const unsigned offset_before =
      GetTextOffsetBefore(fragment.PhysicalFragment());
  const unsigned offset_after = offset_before + 1;
  if (offset != offset_before && offset != offset_after)
    return CaretPositionResolution();
  const NGCaretPositionType position_type =
      offset == offset_before ? NGCaretPositionType::kBeforeBox
                              : NGCaretPositionType::kAfterBox;
  NGCaretPosition candidate{&fragment, position_type, WTF::nullopt};

  if (offset == offset_before &&
      CanResolveCaretPositionBeforeFragment(fragment, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  if (offset == offset_after &&
      CanResolveCaretPositionAfterFragment(fragment, affinity)) {
    return {ResolutionType::kResolved, candidate};
  }

  return {ResolutionType::kFoundCandidate, candidate};
}

CaretPositionResolution TryResolveCaretPositionWithFragment(
    const NGPaintFragment& paint_fragment,
    unsigned offset,
    TextAffinity affinity) {
  const NGPhysicalFragment& fragment = paint_fragment.PhysicalFragment();
  if (fragment.IsText()) {
    return TryResolveCaretPositionInTextFragment(paint_fragment, offset,
                                                 affinity);
  }
  if (fragment.IsBox() && fragment.IsAtomicInline()) {
    return TryResolveCaretPositionByBoxFragmentSide(paint_fragment, offset,
                                                    affinity);
  }
  return CaretPositionResolution();
}

// -------------------------------------

// Helpers for converting NGCaretPositions to caret rects.

NGPhysicalOffsetRect ComputeLocalCaretRectByBoxSide(
    const LayoutBlockFlow& context,
    const NGPaintFragment& fragment,
    NGCaretPositionType position_type) {
  const bool is_horizontal = fragment.Style().IsHorizontalWritingMode();
  DCHECK(fragment.ContainerLineBox());
  const NGPaintFragment& line_box = *fragment.ContainerLineBox();
  const NGPhysicalOffset offset_to_line_box =
      fragment.InlineOffsetToContainerBox() -
      line_box.InlineOffsetToContainerBox();
  LayoutUnit caret_height =
      is_horizontal ? line_box.Size().height : line_box.Size().width;
  LayoutUnit caret_top =
      is_horizontal ? -offset_to_line_box.top : -offset_to_line_box.left;

  const LocalFrameView* frame_view =
      fragment.GetLayoutObject()->GetDocument().View();
  LayoutUnit caret_width = frame_view->CaretWidth();

  const bool is_ltr = fragment.Style().Direction() == TextDirection::kLtr;
  LayoutUnit caret_left;
  if (is_ltr != (position_type == NGCaretPositionType::kBeforeBox)) {
    if (is_horizontal)
      caret_left = fragment.Size().width - caret_width;
    else
      caret_left = fragment.Size().height - caret_width;
  }

  if (!is_horizontal) {
    std::swap(caret_top, caret_left);
    std::swap(caret_width, caret_height);
  }

  const NGPhysicalOffset caret_location(caret_left, caret_top);
  const NGPhysicalSize caret_size(caret_width, caret_height);
  return NGPhysicalOffsetRect(caret_location, caret_size);
}

NGPhysicalOffsetRect ComputeLocalCaretRectAtTextOffset(
    const LayoutBlockFlow& context,
    const NGPaintFragment& paint_fragment,
    unsigned offset) {
  const NGPhysicalTextFragment& fragment =
      ToNGPhysicalTextFragment(paint_fragment.PhysicalFragment());
  DCHECK_GE(offset, fragment.StartOffset());
  DCHECK_LE(offset, fragment.EndOffset());

  const LocalFrameView* frame_view =
      fragment.GetLayoutObject()->GetDocument().View();
  LayoutUnit caret_width = frame_view->CaretWidth();

  const bool is_horizontal = fragment.Style().IsHorizontalWritingMode();

  LayoutUnit caret_height =
      is_horizontal ? fragment.Size().height : fragment.Size().width;
  LayoutUnit caret_top;

  LayoutUnit caret_left = fragment.InlinePositionForOffset(offset);
  if (!fragment.IsLineBreak())
    caret_left -= caret_width / 2;

  if (!is_horizontal) {
    std::swap(caret_top, caret_left);
    std::swap(caret_width, caret_height);
  }

  // Adjust the location to be relative to the inline formatting context.
  NGPhysicalOffset caret_location = NGPhysicalOffset(caret_left, caret_top) +
                                    paint_fragment.InlineOffsetToContainerBox();
  NGPhysicalSize caret_size(caret_width, caret_height);

  const NGPhysicalBoxFragment& context_fragment = *context.CurrentFragment();
  const NGPaintFragment* line_box = paint_fragment.ContainerLineBox();
  const NGPhysicalOffset line_box_offset =
      line_box->InlineOffsetToContainerBox();
  const NGPhysicalOffsetRect line_box_rect(line_box_offset, line_box->Size());

  // For horizontal text, adjust the location in the x direction to ensure that
  // it completely falls in the union of line box and containing block, and
  // then round it to the nearest pixel.
  if (is_horizontal) {
    const LayoutUnit min_x = std::min(LayoutUnit(), line_box_offset.left);
    caret_location.left = std::max(caret_location.left, min_x);
    const LayoutUnit max_x =
        std::max(context_fragment.Size().width, line_box_rect.Right());
    caret_location.left = std::min(caret_location.left, max_x - caret_width);
    caret_location.left = LayoutUnit(caret_location.left.Round());
    return NGPhysicalOffsetRect(caret_location, caret_size);
  }

  // Similar adjustment and rounding for vertical text.
  const LayoutUnit min_y = std::min(LayoutUnit(), line_box_offset.top);
  caret_location.top = std::max(caret_location.top, min_y);
  const LayoutUnit max_y =
      std::max(context_fragment.Size().height, line_box_rect.Bottom());
  caret_location.top = std::min(caret_location.top, max_y - caret_height);
  caret_location.top = LayoutUnit(caret_location.top.Round());
  return NGPhysicalOffsetRect(caret_location, caret_size);
}

LocalCaretRect ComputeLocalCaretRect(const LayoutBlockFlow& context,
                                     const NGCaretPosition& caret_position) {
  if (caret_position.IsNull())
    return LocalCaretRect();

  switch (caret_position.position_type) {
    case NGCaretPositionType::kBeforeBox:
    case NGCaretPositionType::kAfterBox: {
      DCHECK(caret_position.fragment->PhysicalFragment().IsBox());
      const NGPhysicalOffsetRect fragment_local_rect =
          ComputeLocalCaretRectByBoxSide(context, *caret_position.fragment,
                                         caret_position.position_type);
      return {caret_position.fragment->GetLayoutObject(),
              fragment_local_rect.ToLayoutRect()};
    }
    case NGCaretPositionType::kAtTextOffset: {
      DCHECK(caret_position.fragment->PhysicalFragment().IsText());
      DCHECK(caret_position.text_offset.has_value());
      const NGPhysicalOffsetRect caret_rect = ComputeLocalCaretRectAtTextOffset(
          context, *caret_position.fragment, *caret_position.text_offset);

      return {caret_position.fragment->GetLayoutObject(),
              caret_rect.ToLayoutRect()};
    }
  }

  NOTREACHED();
  return {caret_position.fragment->GetLayoutObject(), LayoutRect()};
}

// -------------------------------------

void AssertValidPositionForCaretRectComputation(
    const PositionWithAffinity& position) {
#if DCHECK_IS_ON()
  DCHECK(NGOffsetMapping::AcceptsPosition(position.GetPosition()));
  const LayoutObject* layout_object = position.AnchorNode()->GetLayoutObject();
  DCHECK(layout_object);
  DCHECK(layout_object->IsText() || layout_object->IsAtomicInlineLevel());
#endif
}

}  // namespace

// The main function for compute an NGCaretPosition. See the comments at the top
// of this file for details.
NGCaretPosition ComputeNGCaretPosition(const LayoutBlockFlow& context,
                                       unsigned offset,
                                       TextAffinity affinity) {
  const NGPaintFragment* root_fragment = context.PaintFragment();
  DCHECK(root_fragment);

  NGCaretPosition candidate;
  for (const auto& child :
       NGPaintFragmentTraversal::InlineDescendantsOf(*root_fragment)) {
    const CaretPositionResolution resolution =
        TryResolveCaretPositionWithFragment(*child.fragment, offset, affinity);

    if (resolution.type == ResolutionType::kFailed)
      continue;

    // TODO(xiaochengh): Handle caret poisition in empty container (e.g. empty
    // line box).

    if (resolution.type == ResolutionType::kResolved)
      return resolution.caret_position;

    DCHECK_EQ(ResolutionType::kFoundCandidate, resolution.type);
    // TODO(xiaochengh): We are not sure if we can ever find multiple
    // candidates. Handle it once reached.
    DCHECK(candidate.IsNull());
    candidate = resolution.caret_position;
  }

  return candidate;
}

LocalCaretRect ComputeNGLocalCaretRect(const LayoutBlockFlow& context,
                                       const PositionWithAffinity& position) {
  AssertValidPositionForCaretRectComputation(position);
  DCHECK_EQ(&context, NGInlineFormattingContextOf(position.GetPosition()));
  const NGOffsetMapping* mapping = NGOffsetMapping::GetFor(&context);
  DCHECK(mapping);
  const Optional<unsigned> maybe_offset =
      mapping->GetTextContentOffset(position.GetPosition());
  if (!maybe_offset.has_value()) {
    // TODO(xiaochengh): Investigate if we reach here.
    NOTREACHED();
    return LocalCaretRect();
  }

  const unsigned offset = maybe_offset.value();
  const TextAffinity affinity = position.Affinity();
  return ComputeLocalCaretRect(
      context, ComputeNGCaretPosition(context, offset, affinity));
}

}  // namespace blink
