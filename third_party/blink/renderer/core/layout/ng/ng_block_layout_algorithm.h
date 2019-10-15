// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_LAYOUT_ALGORITHM_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_margin_strut.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_unpositioned_float.h"

namespace blink {

class NGConstraintSpace;
class NGEarlyBreak;
class NGFragment;
class NGPhysicalLineBoxFragment;

// This struct is used for communicating to a child the position of the previous
// inflow child. This will be used to calculate the position of the next child.
struct NGPreviousInflowPosition {
  LayoutUnit logical_block_offset;
  NGMarginStrut margin_strut;
  bool self_collapsing_child_had_clearance;
};

// This strut holds information for the current inflow child. The data is not
// useful outside of handling this single inflow child.
struct NGInflowChildData {
  NGBfcOffset bfc_offset_estimate;
  NGMarginStrut margin_strut;
  NGBoxStrut margins;
  bool margins_fully_resolved;
  bool is_resuming_after_break;
};

// A class for general block layout (e.g. a <div> with no special style).
// Lays out the children in sequence.
class CORE_EXPORT NGBlockLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  // Default constructor.
  NGBlockLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  ~NGBlockLayoutAlgorithm() override;

  void SetBoxType(NGPhysicalFragment::NGBoxType type);

  base::Optional<MinMaxSize> ComputeMinMaxSize(
      const MinMaxSizeInput&) const override;
  scoped_refptr<const NGLayoutResult> Layout() override;

 private:
  NOINLINE scoped_refptr<const NGLayoutResult>
  LayoutWithInlineChildLayoutContext();
  NOINLINE scoped_refptr<const NGLayoutResult> LayoutWithItemsBuilder(
      NGInlineChildLayoutContext* context);

  // Lay out again, this time with a predefined good breakpoint that we
  // discovered in the first pass. This happens when we run out of space in a
  // fragmentainer at an less-than-ideal location, due to breaking restrictions,
  // such as orphans, widows, break-before:avoid or break-after:avoid.
  NOINLINE scoped_refptr<const NGLayoutResult> RelayoutAndBreakEarlier(
      const NGEarlyBreak&);

  inline scoped_refptr<const NGLayoutResult> Layout(
      NGInlineChildLayoutContext* inline_child_layout_context);

  scoped_refptr<const NGLayoutResult> FinishLayout(NGPreviousInflowPosition*);

  // Return the BFC block offset of this block.
  LayoutUnit BfcBlockOffset() const {
    // If we have resolved our BFC block offset, use that.
    if (container_builder_.BfcBlockOffset())
      return *container_builder_.BfcBlockOffset();
    // Otherwise fall back to the BFC block offset assigned by the parent
    // algorithm.
    return ConstraintSpace().BfcOffset().block_offset;
  }

  // Return the BFC block offset of the next block-start border edge (for some
  // child) we'd get if we commit pending margins.
  LayoutUnit NextBorderEdge(
      const NGPreviousInflowPosition& previous_inflow_position) const {
    return BfcBlockOffset() + previous_inflow_position.logical_block_offset +
           previous_inflow_position.margin_strut.Sum();
  }

  NGBoxStrut CalculateMargins(NGLayoutInputNode child,
                              bool is_new_fc,
                              bool* margins_fully_resolved);

  void StopMarginCollapsing(EMarginCollapse collapse_value,
                            LayoutUnit this_margin,
                            LayoutUnit* logical_block_offset,
                            NGMarginStrut* margin_strut);

  // Creates a new constraint space for the current child.
  NGConstraintSpace CreateConstraintSpaceForChild(
      const NGLayoutInputNode child,
      const NGInflowChildData& child_data,
      const LogicalSize child_available_size,
      bool is_new_fc,
      const base::Optional<LayoutUnit> forced_bfc_block_offset = base::nullopt,
      bool has_clearance_past_adjoining_floats = false);

  // @return Estimated BFC block offset for the "to be layout" child.
  NGInflowChildData ComputeChildData(const NGPreviousInflowPosition&,
                                     NGLayoutInputNode,
                                     const NGBreakToken* child_break_token,
                                     bool is_new_fc);

  NGPreviousInflowPosition ComputeInflowPosition(
      const NGPreviousInflowPosition&,
      const NGLayoutInputNode child,
      const NGInflowChildData&,
      const base::Optional<LayoutUnit>& child_bfc_block_offset,
      const LogicalOffset&,
      const NGLayoutResult&,
      const NGFragment&,
      bool self_collapsing_child_had_clearance);

  // Position an self-collapsing child using the parent BFC block-offset.
  // The fragment doesn't know its offset, but we can still calculate its BFC
  // position because the parent fragment's BFC is known.
  // Example:
  //   BFC Offset is known here because of the padding.
  //   <div style="padding: 1px">
  //     <div id="zero" style="margin: 1px"></div>
  LayoutUnit PositionSelfCollapsingChildWithParentBfc(
      const NGLayoutInputNode& child,
      const NGConstraintSpace& child_space,
      const NGInflowChildData& child_data,
      const NGLayoutResult&) const;

  // Try to reuse part of cached fragments. When reusing is possible, this
  // function adds part of cached fragments to |container_builder_|, update
  // |break_token_| to continue layout from the last reused fragment, and
  // returns |true|. Otherwise returns |false|.
  const NGInlineBreakToken* TryReuseFragmentsFromCache(
      NGInlineNode child,
      NGPreviousInflowPosition*,
      bool* abort_out);

  void HandleOutOfFlowPositioned(const NGPreviousInflowPosition&, NGBlockNode);
  void HandleFloat(const NGPreviousInflowPosition&,
                   NGBlockNode,
                   const NGBlockBreakToken*);

  // This uses the NGLayoutOpporunityIterator to position the fragment.
  //
  // An element that establishes a new formatting context must not overlap the
  // margin box of any floats within the current BFC.
  //
  // Example:
  // <div id="container">
  //   <div id="float"></div>
  //   <div id="new-fc" style="margin-top: 20px;"></div>
  // </div>
  // 1) If #new-fc is small enough to fit the available space right from #float
  //    then it will be placed there and we collapse its margin.
  // 2) If #new-fc is too big then we need to clear its position and place it
  //    below #float ignoring its vertical margin.
  //
  // Returns false if we need to abort layout, because a previously unknown BFC
  // block offset has now been resolved.
  NGLayoutResult::EStatus HandleNewFormattingContext(
      NGLayoutInputNode child,
      const NGBreakToken* child_break_token,
      NGPreviousInflowPosition*);

  // Performs the actual layout of a new formatting context. This may be called
  // multiple times from HandleNewFormattingContext.
  scoped_refptr<const NGLayoutResult> LayoutNewFormattingContext(
      NGLayoutInputNode child,
      const NGBreakToken* child_break_token,
      const NGInflowChildData&,
      NGBfcOffset origin_offset,
      bool abort_if_cleared,
      NGBfcOffset* out_child_bfc_offset);

  // Handle an in-flow child.
  // Returns false if we need to abort layout, because a previously unknown BFC
  // block offset has now been resolved. (Same as HandleNewFormattingContext).
  NGLayoutResult::EStatus HandleInflow(
      NGLayoutInputNode child,
      const NGBreakToken* child_break_token,
      NGPreviousInflowPosition*,
      NGInlineChildLayoutContext*,
      scoped_refptr<const NGInlineBreakToken>* previous_inline_break_token);

  NGLayoutResult::EStatus FinishInflow(
      NGLayoutInputNode child,
      const NGBreakToken* child_break_token,
      const NGConstraintSpace&,
      bool has_clearance_past_adjoining_floats,
      scoped_refptr<const NGLayoutResult>,
      NGInflowChildData*,
      NGPreviousInflowPosition*,
      NGInlineChildLayoutContext*,
      scoped_refptr<const NGInlineBreakToken>* previous_inline_break_token);

  // Return the amount of block space available in the current fragmentainer
  // for the node being laid out by this algorithm.
  LayoutUnit FragmentainerSpaceAvailable() const;

  // Return true if the node being laid out by this fragmentainer has used all
  // the available space in the current fragmentainer.
  // |block_offset| is the border-edge relative block offset we want to check
  // whether fits within the fragmentainer or not.
  bool IsFragmentainerOutOfSpace(LayoutUnit block_offset) const;

  // Return the block-offset from the start of the fragmentainer, to this node.
  LayoutUnit OffsetFromFragmentainerStart() const;

  // Return the block-size of the portion that intersects with the
  // fragmentainer. The block-offset is relative to this node.
  LayoutUnit PortionIntersectingWithFragmentainer(LayoutUnit block_offset,
                                                  LayoutUnit block_size) const;

  // Propagate the block-size of unbreakable content. This is used to inflate
  // the initial minimal column block-size when balancing columns. Unbreakable
  // content will actually fragment if the columns aren't large enough, and we
  // want to prevent that, if possible.
  void PropagateUnbreakableBlockSize(LayoutUnit block_offset,
                                     LayoutUnit block_size);

  // Final adjustments before fragment creation. We need to prevent the fragment
  // from crossing fragmentainer boundaries, and rather create a break token if
  // we're out of space. As part of finalizing we may also discover that we need
  // to abort layout, because we've run out of space at a less-than-ideal
  // location. In this case, false will be returned. Otherwise, true will be
  // returned.
  bool FinalizeForFragmentation();

  // Outcome of considering (and possibly attempting) breaking before a child.
  enum BreakStatus {
    // Continue layout. No break was inserted before the child (but there may be
    // a break inside).
    kContinueWithoutBreaking,

    // A break was inserted before the child. Discard the child fragment and
    // finish layout of the container. If there was a break inside the child, it
    // will be discarded along with the child fragment.
    kBrokeBefore,

    // The child couldn't fit here, but no break was inserted before the child,
    // as it was an unappealing place to break, and we have a better earlier
    // breakpoint. We now need to abort the current layout, and go back and
    // re-layout to said earlier breakpoint.
    kNeedsEarlierBreak
  };

  // Insert a fragmentainer break before the child if necessary. In that case,
  // the previous in-flow position will be updated, we'll return |kBrokeBefore|.
  // If we don't break inside, we'll consider the appeal of doing so anyway (and
  // store it as the most appealing break point so far if that's the case),
  // since we might have to go back and break here. Return
  // |kContinueWithoutBreaking| if we're to continue laying out. If
  // |kNeedsEarlierBreak| is returned, it means that we ran out of space, but
  // shouldn't break before the child, but rather abort layout, and re-layout to
  // a previously found good breakpoint.
  // If |has_container_separation| is true, it means that we're at a valid
  // breakpoint. We obviously prefer valid breakpoints, but sometimes we need to
  // break at undesirable locations. Class A breakpoints occur between block
  // siblings. Class B breakpoints between line boxes. Both these breakpoint
  // classes imply that we're already past the first in-flow child in the
  // container, but there's also another way of achieving container separation:
  // class C breakpoints. Those occur if there's a positive gap between the
  // block-start content edge of the container and the block-start margin edge
  // of the first in-flow child. This can happen when in-flow content is pushed
  // down by floats. https://www.w3.org/TR/css-break-3/#possible-breaks
  BreakStatus BreakBeforeChildIfNeeded(NGLayoutInputNode child,
                                       const NGLayoutResult&,
                                       NGPreviousInflowPosition*,
                                       LayoutUnit block_offset,
                                       bool has_container_separation);

  // Insert either a soft or forced break before the child.
  void BreakBeforeChild(NGLayoutInputNode child,
                        const NGLayoutResult&,
                        LayoutUnit block_offset,
                        NGBreakAppeal,
                        bool is_forced_break,
                        NGPreviousInflowPosition*);
  void BreakBeforeChild(NGLayoutInputNode child,
                        NGBreakAppeal,
                        bool is_forced_break,
                        NGPreviousInflowPosition*);

  // Propagate the minimal space shortage from a child.
  void PropagateSpaceShortage(const NGLayoutResult&, LayoutUnit block_offset);

  // Look for a better breakpoint (than we already have) either before the child
  // (class A breakpoint), or inside it (class A or B breakpoint), and store it.
  void UpdateEarlyBreakAtBlockChild(NGBlockNode child,
                                    const NGLayoutResult& layout_result,
                                    NGBreakAppeal break_before);

  // Look for a better breakpoint (than we already have) between lines (i.e. a
  // class B breakpoint), and store it.
  void UpdateEarlyBreakBetweenLines();

  void PropagateBaselinesFromChildren();
  bool AddBaseline(const NGBaselineRequest&,
                   const NGPhysicalFragment&,
                   LayoutUnit child_offset);

  // Compute the baseline offset of a line box from the content box.
  // Line boxes are in line-relative coordinates. This function returns the
  // offset in flow-relative coordinates.
  LayoutUnit ComputeLineBoxBaselineOffset(
      const NGBaselineRequest&,
      const NGPhysicalLineBoxFragment&,
      LayoutUnit line_box_block_offset) const;

  // If still unresolved, resolve the fragment's BFC block offset.
  //
  // This includes applying clearance, so the |bfc_block_offset| passed won't
  // be the final BFC block-offset, if it wasn't large enough to get past all
  // relevant floats. The updated BFC block-offset can be read out with
  // |ContainerBfcBlockOffset()|.
  //
  // If the |forced_bfc_block_offset| has a value, it will override the given
  // |bfc_block_offset|. Typically this comes from the input constraints, when
  // the current node has clearance past adjoining floats, or has a re-layout
  // due to a child resolving the BFC block-offset.
  //
  // In addition to resolving our BFC block offset, this will also position
  // pending floats, and update our in-flow layout state.
  //
  // Returns false if resolving the BFC block-offset resulted in needing to
  // abort layout. It will always return true otherwise. If the BFC
  // block-offset was already resolved, this method does nothing (and returns
  // true).
  bool ResolveBfcBlockOffset(
      NGPreviousInflowPosition*,
      LayoutUnit bfc_block_offset,
      const base::Optional<LayoutUnit> forced_bfc_block_offset);

  // This passes in the |forced_bfc_block_offset| from the input constraints,
  // which is almost always desired.
  bool ResolveBfcBlockOffset(NGPreviousInflowPosition* previous_inflow_position,
                             LayoutUnit bfc_block_offset) {
    return ResolveBfcBlockOffset(previous_inflow_position, bfc_block_offset,
                                 ConstraintSpace().ForcedBfcBlockOffset());
  }

  // A very common way to resolve the BFC block offset is to simply commit the
  // pending margin, so here's a convenience overload for that.
  bool ResolveBfcBlockOffset(
      NGPreviousInflowPosition* previous_inflow_position) {
    return ResolveBfcBlockOffset(previous_inflow_position,
                                 NextBorderEdge(*previous_inflow_position));
  }

  // Mark this fragment as modifying its incoming margin-strut if it hasn't
  // resolved its BFC block-offset yet.
  void SetSubtreeModifiedMarginStrutIfNeeded(const Length* margin = nullptr) {
    if (container_builder_.BfcBlockOffset())
      return;

    if (margin && margin->IsZero())
      return;

    container_builder_.SetSubtreeModifiedMarginStrut();
  }

  // Return true if the BFC block offset has changed and this means that we
  // need to abort layout.
  bool NeedsAbortOnBfcBlockOffsetChange() const;

  // Positions a list marker for the specified block content.
  // Return false if it aborts when resolving BFC block offset for LI.
  bool PositionOrPropagateListMarker(const NGLayoutResult&,
                                     LogicalOffset*,
                                     NGPreviousInflowPosition*);

  // Positions a list marker when the block does not have any line boxes.
  // Return false if it aborts when resolving BFC block offset for LI.
  bool PositionListMarkerWithoutLineBoxes(NGPreviousInflowPosition*);

  // Calculates logical offset for the current fragment using either {@code
  // intrinsic_block_size_} when the fragment doesn't know it's offset or
  // {@code known_fragment_offset} if the fragment knows it's offset
  // @return Fragment's offset relative to the fragment's parent.
  LogicalOffset CalculateLogicalOffset(
      const NGFragment& fragment,
      LayoutUnit child_bfc_line_offset,
      const base::Optional<LayoutUnit>& child_bfc_block_offset);

  // Computes minimum size for HTML and BODY elements in quirks mode.
  // Returns kIndefiniteSize in all other cases.
  LayoutUnit CalculateMinimumBlockSize(const NGMarginStrut& end_margin_strut);

  const NGBoxStrut border_padding_;
  NGBoxStrut border_scrollbar_padding_;

  LogicalSize child_available_size_;
  LogicalSize child_percentage_size_;
  LogicalSize replaced_child_percentage_size_;

  // Intrinsic block size based on child layout and containment.
  LayoutUnit intrinsic_block_size_;

  // The line box index at which we ran out of space. This where we'll actually
  // end up breaking, unless we determine that we should break earlier in order
  // to satisfy the widows request.
  int first_overflowing_line_ = 0;

  // Set if we should fit as many lines as there's room for, i.e. no early
  // break. In that case we'll break before first_overflowing_line_. In this
  // case there'll either be enough widows for the next fragment, or we have
  // determined that we're unable to fulfill the widows request.
  bool fit_all_lines_ = false;

  // Set if we're resuming layout of a node that has already produced fragments.
  bool is_resuming_;

  // Set when we're to abort if the BFC block offset gets resolved or updated.
  // Sometimes we walk past elements (i.e. floats) that depend on the BFC block
  // offset being known (in order to position and lay themselves out properly).
  // When this happens, and we finally manage to resolve (or update) the BFC
  // block offset at some subsequent element, we need to check if this flag is
  // set, and abort layout if it is.
  bool abort_when_bfc_block_offset_updated_ = false;

  // This will be set during block fragmentation once we've processed the first
  // in-flow child of a container. It is used to check if we're at a valid class
  // A or B breakpoint (between block-level siblings or line box siblings).
  bool has_processed_first_child_ = false;

  bool did_break_before_child_ = false;

  NGExclusionSpace exclusion_space_;

  // When set, this will specify where to break before or inside.
  const NGEarlyBreak* early_break_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_LAYOUT_ALGORITHM_H_
