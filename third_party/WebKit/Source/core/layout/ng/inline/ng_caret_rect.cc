// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_caret_rect.h"

#include "core/editing/LocalCaretRect.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "core/layout/ng/inline/ng_inline_break_token.h"
#include "core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "core/layout/ng/inline/ng_offset_mapping.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_physical_box_fragment.h"

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

// TODO(xiaochengh): Try to avoid passing the seemingly redundant line boxes.
// TODO(xiaochengh): Move this function to NGPhysicalFragment.
bool IsFragmentAfterLineWrap(const NGPhysicalFragment& fragment,
                             const NGPhysicalLineBoxFragment& current_line,
                             const NGPhysicalLineBoxFragment* last_line) {
  if (!last_line)
    return false;
  // A fragment after line wrap must be the first logical leaf in its line.
  if (&fragment != current_line.FirstLogicalLeaf())
    return false;
  DCHECK(last_line->BreakToken());
  DCHECK(last_line->BreakToken()->IsInlineType());
  DCHECK(!last_line->BreakToken()->IsFinished());
  return !ToNGInlineBreakToken(last_line->BreakToken())->IsForcedBreak();
}

bool CanResolveCaretPositionBeforeFragment(
    const NGPhysicalFragment& fragment,
    TextAffinity affinity,
    const NGPhysicalLineBoxFragment& current_line,
    const NGPhysicalLineBoxFragment* last_line) {
  if (affinity == TextAffinity::kDownstream)
    return true;
  return !IsFragmentAfterLineWrap(fragment, current_line, last_line);
}

// TODO(xiaochengh): Try to avoid passing the seemingly redundant line box.
// TODO(xiaochengh): Move this function to NGPhysicalFragment.
bool IsFragmentBeforeLineWrap(const NGPhysicalFragment& fragment,
                              const NGPhysicalLineBoxFragment& current_line) {
  // A fragment before line wrap must be the last logical leaf in its line.
  if (&fragment != current_line.LastLogicalLeaf())
    return false;
  DCHECK(current_line.BreakToken());
  DCHECK(current_line.BreakToken()->IsInlineType());
  const NGInlineBreakToken& break_token =
      ToNGInlineBreakToken(*current_line.BreakToken());
  return !break_token.IsFinished() && !break_token.IsForcedBreak();
}

bool CanResolveCaretPositionAfterFragment(
    const NGPhysicalFragment& fragment,
    TextAffinity affinity,
    const NGPhysicalLineBoxFragment& current_line) {
  if (affinity == TextAffinity::kUpstream)
    return true;
  return !IsFragmentBeforeLineWrap(fragment, current_line);
}

CaretPositionResolution TryResolveCaretPositionInTextFragment(
    const NGPhysicalTextFragment& fragment,
    const NGPhysicalLineBoxFragment& current_line,
    const NGPhysicalLineBoxFragment* last_line,
    unsigned offset,
    TextAffinity affinity) {
  if (fragment.IsAnonymousText())
    return CaretPositionResolution();

  // [StartOffset(), EndOffset()] is the range allowing caret placement.
  // For example, "foo" has 4 offsets allowing caret placement.
  if (offset < fragment.StartOffset() || offset > fragment.EndOffset()) {
    // TODO(xiaochengh): This may introduce false negatives. Investigate.
    return CaretPositionResolution();
  }
  NGCaretPosition candidate = {&fragment, NGCaretPositionType::kAtTextOffset,
                               offset};

  // Offsets in the interior of a fragment can be resolved directly.
  if (offset > fragment.StartOffset() && offset < fragment.EndOffset())
    return {ResolutionType::kResolved, candidate};

  if (offset == fragment.StartOffset() &&
      CanResolveCaretPositionBeforeFragment(fragment, affinity, current_line,
                                            last_line)) {
    return {ResolutionType::kResolved, candidate};
  }

  if (offset == fragment.EndOffset() && !fragment.IsLineBreak() &&
      CanResolveCaretPositionAfterFragment(fragment, affinity, current_line)) {
    return {ResolutionType::kResolved, candidate};
  }

  // We may have a better candidate
  return {ResolutionType::kFoundCandidate, candidate};
}

unsigned GetTextOffsetBefore(const NGPhysicalBoxFragment& fragment) {
  // TODO(xiaochengh): Design more straightforward way to get text offset of
  // atomic inline box.
  DCHECK(fragment.IsInlineBlock());
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
    const NGPhysicalBoxFragment& fragment,
    const NGPhysicalLineBoxFragment& current_line,
    const NGPhysicalLineBoxFragment* last_line,
    unsigned offset,
    TextAffinity affinity) {
  if (!fragment.GetNode()) {
    // TODO(xiaochengh): This leads to false negatives for, e.g., RUBY, where an
    // anonymous wrapping inline block is created.
    return CaretPositionResolution();
  }

  const unsigned offset_before = GetTextOffsetBefore(fragment);
  const unsigned offset_after = offset_before + 1;
  if (offset != offset_before && offset != offset_after)
    return CaretPositionResolution();
  const NGCaretPositionType position_type =
      offset == offset_before ? NGCaretPositionType::kBeforeBox
                              : NGCaretPositionType::kAfterBox;
  NGCaretPosition candidate{&fragment, position_type, WTF::nullopt};

  if (offset == offset_before &&
      CanResolveCaretPositionBeforeFragment(fragment, affinity, current_line,
                                            last_line)) {
    return {ResolutionType::kResolved, candidate};
  }

  if (offset == offset_after &&
      CanResolveCaretPositionAfterFragment(fragment, affinity, current_line)) {
    return {ResolutionType::kResolved, candidate};
  }

  return {ResolutionType::kFoundCandidate, candidate};
}

CaretPositionResolution TryResolveCaretPositionWithFragment(
    const NGPhysicalFragment& fragment,
    const NGPhysicalLineBoxFragment& current_line,
    const NGPhysicalLineBoxFragment* last_line,
    unsigned offset,
    TextAffinity affinity) {
  if (fragment.IsText()) {
    return TryResolveCaretPositionInTextFragment(
        ToNGPhysicalTextFragment(fragment), current_line, last_line, offset,
        affinity);
  }
  if (fragment.IsBox() && fragment.IsInlineBlock()) {
    return TryResolveCaretPositionByBoxFragmentSide(
        ToNGPhysicalBoxFragment(fragment), current_line, last_line, offset,
        affinity);
  }
  return CaretPositionResolution();
}

}  // namespace

// The main function for compute an NGCaretPosition. See the comments at the top
// of this file for details.
NGCaretPosition ComputeNGCaretPosition(const LayoutBlockFlow& context,
                                       unsigned offset,
                                       TextAffinity affinity) {
  const NGPhysicalBoxFragment* box_fragment = context.CurrentFragment();
  // TODO(kojii): CurrentFragment isn't always available after layout clean.
  // Investigate why.
  if (!box_fragment)
    return NGCaretPosition();

  const NGPhysicalLineBoxFragment* last_line = nullptr;
  const NGPhysicalLineBoxFragment* current_line = nullptr;

  NGCaretPosition candidate;
  for (const auto& child :
       NGInlineFragmentTraversal::DescendantsOf(*box_fragment)) {
    if (child.fragment->IsLineBox()) {
      last_line = current_line;
      current_line = ToNGPhysicalLineBoxFragment(child.fragment.get());
    }

    // Every inline fragment should have a line box (inclusive) ancestor.
    DCHECK(current_line);

    const CaretPositionResolution resolution =
        TryResolveCaretPositionWithFragment(*child.fragment, *current_line,
                                            last_line, offset, affinity);

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

}  // namespace blink
