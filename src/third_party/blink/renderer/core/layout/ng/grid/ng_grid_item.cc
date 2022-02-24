// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_item.h"

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_placement.h"

namespace blink {

namespace {

// Given an |item_style| determines the correct |AxisEdge| alignment.
// Additionally will determine:
//  - The behavior of 'auto' via the |auto_behavior| out-parameter.
//  - If the alignment is safe via the |is_overflow_safe| out-parameter.
AxisEdge AxisEdgeFromItemPosition(const bool is_inline_axis,
                                  const bool is_replaced,
                                  const bool is_out_of_flow,
                                  const ComputedStyle& item_style,
                                  const ComputedStyle& container_style,
                                  NGAutoBehavior* auto_behavior,
                                  bool* is_overflow_safe) {
  DCHECK(auto_behavior && is_overflow_safe);

  const auto& alignment = is_inline_axis
                              ? item_style.ResolvedJustifySelf(
                                    ItemPosition::kNormal, &container_style)
                              : item_style.ResolvedAlignSelf(
                                    ItemPosition::kNormal, &container_style);

  *auto_behavior = NGAutoBehavior::kFitContent;
  *is_overflow_safe = alignment.Overflow() == OverflowAlignment::kSafe;

  // Auto-margins take precedence over any alignment properties.
  if (item_style.MayHaveMargin() && !is_out_of_flow) {
    const bool is_start_auto =
        is_inline_axis ? item_style.MarginStartUsing(container_style).IsAuto()
                       : item_style.MarginBeforeUsing(container_style).IsAuto();
    const bool is_end_auto =
        is_inline_axis ? item_style.MarginEndUsing(container_style).IsAuto()
                       : item_style.MarginAfterUsing(container_style).IsAuto();

    // 'auto' margin alignment is always "safe".
    if (is_start_auto || is_end_auto)
      *is_overflow_safe = true;

    if (is_start_auto && is_end_auto)
      return AxisEdge::kCenter;
    else if (is_start_auto)
      return AxisEdge::kEnd;
    else if (is_end_auto)
      return AxisEdge::kStart;
  }

  const auto container_writing_direction =
      container_style.GetWritingDirection();
  const auto item_position = alignment.GetPosition();

  switch (item_position) {
    case ItemPosition::kSelfStart:
    case ItemPosition::kSelfEnd: {
      // In order to determine the correct "self" axis-edge without a
      // complicated set of if-branches we use two converters.

      // First use the grid-item's writing-direction to convert the logical
      // edge into the physical coordinate space.
      LogicalToPhysical<AxisEdge> physical(item_style.GetWritingDirection(),
                                           AxisEdge::kStart, AxisEdge::kEnd,
                                           AxisEdge::kStart, AxisEdge::kEnd);

      // Then use the container's writing-direction to convert the physical
      // edges, into our logical coordinate space.
      PhysicalToLogical<AxisEdge> logical(container_writing_direction,
                                          physical.Top(), physical.Right(),
                                          physical.Bottom(), physical.Left());

      if (is_inline_axis) {
        return item_position == ItemPosition::kSelfStart ? logical.InlineStart()
                                                         : logical.InlineEnd();
      }
      return item_position == ItemPosition::kSelfStart ? logical.BlockStart()
                                                       : logical.BlockEnd();
    }
    case ItemPosition::kCenter:
      return AxisEdge::kCenter;
    case ItemPosition::kFlexStart:
    case ItemPosition::kStart:
      return AxisEdge::kStart;
    case ItemPosition::kFlexEnd:
    case ItemPosition::kEnd:
      return AxisEdge::kEnd;
    case ItemPosition::kStretch:
      *auto_behavior = NGAutoBehavior::kStretchExplicit;
      return AxisEdge::kStart;
    case ItemPosition::kBaseline:
    case ItemPosition::kLastBaseline:
      return AxisEdge::kBaseline;
    case ItemPosition::kLeft:
      DCHECK(is_inline_axis);
      return container_writing_direction.IsLtr() ? AxisEdge::kStart
                                                 : AxisEdge::kEnd;
    case ItemPosition::kRight:
      DCHECK(is_inline_axis);
      return container_writing_direction.IsRtl() ? AxisEdge::kStart
                                                 : AxisEdge::kEnd;
    case ItemPosition::kNormal:
      *auto_behavior = is_replaced ? NGAutoBehavior::kFitContent
                                   : NGAutoBehavior::kStretchImplicit;
      return AxisEdge::kStart;
    case ItemPosition::kLegacy:
    case ItemPosition::kAuto:
      NOTREACHED();
      return AxisEdge::kStart;
  }
}

// Determines whether the track direction, grid container writing mode, and
// grid item writing mode are part of the same alignment context as specified in
// https://www.w3.org/TR/css-align-3/#baseline-sharing-group
// In particular, 'Boxes share an alignment context, along a particular axis,
// and established by a particular box, when they are grid items in the same
// row, along the grid’s row (inline) axis, established by the grid container.'
//
// TODO(kschmi): Some of these conditions are non-intuitive, so investigate
// whether these conditions are correct or if the test expectations are off.
BaselineType DetermineBaselineType(
    const GridTrackSizingDirection track_direction,
    const WritingMode container_writing_mode,
    const WritingMode child_writing_mode) {
  bool is_major = false;
  switch (container_writing_mode) {
    case WritingMode::kHorizontalTb:
      is_major = (track_direction == kForRows)
                     ? true
                     : (child_writing_mode == WritingMode::kVerticalLr ||
                        child_writing_mode == WritingMode::kHorizontalTb);
      break;
    case WritingMode::kVerticalLr:
      is_major = (track_direction == kForRows)
                     ? (child_writing_mode == WritingMode::kVerticalLr ||
                        child_writing_mode == WritingMode::kHorizontalTb)
                     : true;
      break;
    case WritingMode::kVerticalRl:
      is_major = (track_direction == kForRows)
                     ? (child_writing_mode == WritingMode::kVerticalRl ||
                        child_writing_mode == WritingMode::kHorizontalTb)
                     : true;
      break;
    default:
      is_major = true;
      break;
  }
  return is_major ? BaselineType::kMajor : BaselineType::kMinor;
}

}  // namespace

GridItemData::GridItemData(const NGBlockNode node,
                           const ComputedStyle& container_style,
                           const WritingMode container_writing_mode)
    : node(node), is_sizing_dependent_on_block_size(false) {
  const auto& style = node.Style();

  const bool is_replaced = node.IsReplaced();
  const bool is_out_of_flow = node.IsOutOfFlowPositioned();

  // Determine the alignment for the grid item ahead of time (we may need to
  // know if it stretches to correctly determine any block axis contribution).
  bool is_overflow_safe;
  inline_axis_alignment = AxisEdgeFromItemPosition(
      /* is_inline_axis */ true, is_replaced, is_out_of_flow, style,
      container_style, &inline_auto_behavior, &is_overflow_safe);
  is_inline_axis_overflow_safe = is_overflow_safe;

  block_axis_alignment = AxisEdgeFromItemPosition(
      /* is_inline_axis */ false, is_replaced, is_out_of_flow, style,
      container_style, &block_auto_behavior, &is_overflow_safe);
  is_block_axis_overflow_safe = is_overflow_safe;

  const auto item_writing_mode = style.GetWritingDirection().GetWritingMode();
  column_baseline_type = DetermineBaselineType(
      kForColumns, container_writing_mode, item_writing_mode);
  row_baseline_type = DetermineBaselineType(kForRows, container_writing_mode,
                                            item_writing_mode);
}

void GridItemData::SetAlignmentFallback(
    const GridTrackSizingDirection track_direction,
    const ComputedStyle& container_style,
    const bool has_synthesized_baseline) {
  // Alignment fallback is only possible when baseline alignment is specified.
  if (!IsBaselineSpecifiedForDirection(track_direction))
    return;

  auto CanParticipateInBaselineAlignment =
      [&](const ComputedStyle& container_style,
          const GridTrackSizingDirection track_direction) -> bool {
    // "If baseline alignment is specified on a grid item whose size in that
    // axis depends on the size of an intrinsically-sized track (whose size is
    // therefore dependent on both the item’s size and baseline alignment,
    // creating a cyclic dependency), that item does not participate in
    // baseline alignment, and instead uses its fallback alignment as if that
    // were originally specified. For this purpose, <flex> track sizes count
    // as “intrinsically-sized” when the grid container has an indefinite size
    // in the relevant axis."
    // https://drafts.csswg.org/css-grid-2/#row-align
    if (has_synthesized_baseline &&
        (IsSpanningIntrinsicTrack(track_direction) ||
         IsSpanningFlexibleTrack(track_direction))) {
      // Parallel grid items with a synthesized baseline support baseline
      // alignment only of the height doesn't depend on the track size.
      const auto& item_style = node.Style();
      const bool is_parallel_to_baseline_axis =
          (track_direction == kForRows) ==
          IsParallelWritingMode(container_style.GetWritingMode(),
                                item_style.GetWritingMode());
      if (is_parallel_to_baseline_axis) {
        const bool logical_height_depends_on_container =
            item_style.LogicalHeight().IsPercentOrCalc() ||
            item_style.LogicalMinHeight().IsPercentOrCalc() ||
            item_style.LogicalMaxHeight().IsPercentOrCalc() ||
            item_style.LogicalHeight().IsAuto();
        return !logical_height_depends_on_container;
      } else {
        // Orthogonal items with synthesized baselines never support baseline
        // alignment when they span intrinsic or flex tracks.
        return false;
      }
    }
    return true;
  };

  // Set fallback alignment to start edges if an item requests baseline
  // alignment but does not meet requirements for it.
  if (!CanParticipateInBaselineAlignment(container_style, track_direction)) {
    if (track_direction == kForColumns &&
        inline_axis_alignment == AxisEdge::kBaseline) {
      inline_axis_alignment_fallback = AxisEdge::kStart;
    } else if (track_direction == kForRows &&
               block_axis_alignment == AxisEdge::kBaseline) {
      block_axis_alignment_fallback = AxisEdge::kStart;
    }
  } else {
    // Reset the alignment fallback if eligibility has changed.
    if (track_direction == kForColumns &&
        inline_axis_alignment_fallback.has_value()) {
      inline_axis_alignment_fallback.reset();
    } else if (track_direction == kForRows &&
               block_axis_alignment_fallback.has_value()) {
      block_axis_alignment_fallback.reset();
    }
  }
}

void GridItemData::ComputeSetIndices(
    const NGGridLayoutAlgorithmTrackCollection& track_collection) {
  DCHECK(!IsOutOfFlow());
  GridItemIndices range_indices = RangeIndices(track_collection.Direction());

#if DCHECK_IS_ON()
  const wtf_size_t start_line = StartLine(track_collection.Direction());
  const wtf_size_t end_line = EndLine(track_collection.Direction());
  DCHECK_LE(end_line, track_collection.EndLineOfImplicitGrid());
  DCHECK_LT(start_line, end_line);

  // Check the range index caching was correct by running a binary search.
  DCHECK_EQ(track_collection.RangeIndexFromTrackNumber(start_line),
            range_indices.begin);
  DCHECK_EQ(track_collection.RangeIndexFromTrackNumber(end_line - 1),
            range_indices.end);
#endif

  auto& set_indices =
      track_collection.IsForColumns() ? column_set_indices : row_set_indices;
  set_indices.begin =
      track_collection.RangeStartingSetIndex(range_indices.begin);
  set_indices.end = track_collection.RangeStartingSetIndex(range_indices.end) +
                    track_collection.RangeSetCount(range_indices.end);

  DCHECK_LE(set_indices.end, track_collection.SetCount());
  DCHECK_LT(set_indices.begin, set_indices.end);
}

void GridItemData::ComputeOutOfFlowItemPlacement(
    const NGGridLayoutAlgorithmTrackCollection& track_collection,
    const NGGridPlacement& grid_placement) {
  DCHECK(IsOutOfFlow());

  auto& start_offset = track_collection.IsForColumns()
                           ? column_placement.offset_in_range.begin
                           : row_placement.offset_in_range.begin;
  auto& end_offset = track_collection.IsForColumns()
                         ? column_placement.offset_in_range.end
                         : row_placement.offset_in_range.end;

  if (IsGridContainingBlock()) {
    grid_placement.ResolveOutOfFlowItemGridLines(track_collection, node.Style(),
                                                 &start_offset, &end_offset);
  } else {
    start_offset = kNotFound;
    end_offset = kNotFound;
  }

#if DCHECK_IS_ON()
  if (start_offset != kNotFound && end_offset != kNotFound) {
    DCHECK_LE(end_offset, track_collection.EndLineOfImplicitGrid());
    DCHECK_LT(start_offset, end_offset);
  } else if (start_offset != kNotFound) {
    DCHECK_LE(start_offset, track_collection.EndLineOfImplicitGrid());
  } else if (end_offset != kNotFound) {
    DCHECK_LE(end_offset, track_collection.EndLineOfImplicitGrid());
  }
#endif

  // We only calculate the range placement if the line was not defined as 'auto'
  // and it is within the bounds of the grid, since an out of flow item cannot
  // create grid lines.
  const wtf_size_t range_count = track_collection.RangeCount();
  auto& start_range_index = track_collection.IsForColumns()
                                ? column_placement.range_index.begin
                                : row_placement.range_index.begin;
  if (start_offset != kNotFound) {
    if (!range_count) {
      // An undefined and empty grid has a single start/end grid line and no
      // ranges. Therefore, if the start offset isn't 'auto', the only valid
      // offset is zero.
      DCHECK_EQ(start_offset, 0u);
      start_range_index = 0;
    } else {
      // If the start line of an out of flow item is the last line of the grid,
      // we can just subtract one unit to the range count.
      start_range_index =
          (start_offset < track_collection.EndLineOfImplicitGrid())
              ? track_collection.RangeIndexFromTrackNumber(start_offset)
              : range_count - 1;
      start_offset -= track_collection.RangeTrackNumber(start_range_index);
    }
  }

  auto& end_range_index = track_collection.IsForColumns()
                              ? column_placement.range_index.end
                              : row_placement.range_index.end;
  if (end_offset != kNotFound) {
    if (!range_count) {
      // Similarly to the start offset, if we have an undefined, empty grid and
      // the end offset isn't 'auto', the only valid offset is zero.
      DCHECK_EQ(end_offset, 0u);
      end_range_index = 0;
    } else {
      // If the end line of an out of flow item is the first line of the grid,
      // then |last_spanned_range| is set to zero.
      end_range_index =
          end_offset
              ? track_collection.RangeIndexFromTrackNumber(end_offset - 1)
              : 0;
      end_offset -= track_collection.RangeTrackNumber(end_range_index);
    }
  }
}

}  // namespace blink
