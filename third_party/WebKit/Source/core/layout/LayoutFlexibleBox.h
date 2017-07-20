/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LayoutFlexibleBox_h
#define LayoutFlexibleBox_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/OrderIterator.h"

namespace blink {

class FlexItem;
class FlexLine;

class CORE_EXPORT LayoutFlexibleBox : public LayoutBlock {
 public:
  LayoutFlexibleBox(Element*);
  ~LayoutFlexibleBox() override;

  static LayoutFlexibleBox* CreateAnonymous(Document*);

  const char* GetName() const override { return "LayoutFlexibleBox"; }

  bool IsFlexibleBox() const final { return true; }
  void UpdateBlockLayout(bool relayout_children) final;

  int BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;

  static const StyleContentAlignmentData& ContentAlignmentNormalBehavior();
  static int SynthesizedBaselineFromContentBox(const LayoutBox&,
                                               LineDirectionMode);

  int FirstLineBoxBaseline() const override;
  int InlineBlockBaseline(LineDirectionMode) const override;
  IntSize OriginAdjustmentForScrollbars() const override;
  bool HasTopOverflow() const override;
  bool HasLeftOverflow() const override;

  void PaintChildren(const PaintInfo&, const LayoutPoint&) const final;

  bool IsHorizontalFlow() const;

  const OrderIterator& GetOrderIterator() const { return order_iterator_; }

  LayoutUnit CrossSizeForPercentageResolution(const LayoutBox& child);
  LayoutUnit MainSizeForPercentageResolution(const LayoutBox& child);
  LayoutUnit ChildLogicalHeightForPercentageResolution(const LayoutBox& child);

  void ClearCachedMainSizeForChild(const LayoutBox& child);

  LayoutUnit StaticMainAxisPositionForPositionedChild(const LayoutBox& child);
  LayoutUnit StaticCrossAxisPositionForPositionedChild(const LayoutBox& child);

  LayoutUnit StaticInlinePositionForPositionedChild(const LayoutBox& child);
  LayoutUnit StaticBlockPositionForPositionedChild(const LayoutBox& child);

  // Returns true if the position changed. In that case, the child will have to
  // be laid out again.
  bool SetStaticPositionForPositionedLayout(LayoutBox& child);

 protected:
  void ComputeIntrinsicLogicalWidths(
      LayoutUnit& min_logical_width,
      LayoutUnit& max_logical_width) const override;

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void RemoveChild(LayoutObject*) override;

 private:
  enum ChildLayoutType { kLayoutIfNeeded, kForceLayout, kNeverLayout };

  enum class TransformedWritingMode {
    kTopToBottomWritingMode,
    kRightToLeftWritingMode,
    kLeftToRightWritingMode,
    kBottomToTopWritingMode
  };

  enum class SizeDefiniteness { kDefinite, kIndefinite, kUnknown };

  bool HasOrthogonalFlow(const LayoutBox& child) const;
  bool IsColumnFlow() const;
  bool IsLeftToRightFlow() const;
  bool IsMultiline() const;
  Length FlexBasisForChild(const LayoutBox& child) const;
  LayoutUnit CrossAxisExtentForChild(const LayoutBox& child) const;
  LayoutUnit CrossAxisIntrinsicExtentForChild(const LayoutBox& child) const;
  LayoutUnit ChildIntrinsicLogicalHeight(const LayoutBox& child) const;
  LayoutUnit ChildIntrinsicLogicalWidth(const LayoutBox& child) const;
  LayoutUnit MainAxisExtentForChild(const LayoutBox& child) const;
  LayoutUnit MainAxisContentExtentForChildIncludingScrollbar(
      const LayoutBox& child) const;
  LayoutUnit CrossAxisExtent() const;
  LayoutUnit MainAxisExtent() const;
  LayoutUnit CrossAxisContentExtent() const;
  LayoutUnit MainAxisContentExtent(LayoutUnit content_logical_height);
  LayoutUnit ComputeMainAxisExtentForChild(const LayoutBox& child,
                                           SizeType,
                                           const Length& size);
  TransformedWritingMode GetTransformedWritingMode() const;
  StyleContentAlignmentData ResolvedJustifyContent() const;
  StyleContentAlignmentData ResolvedAlignContent() const;
  LayoutUnit FlowAwareBorderStart() const;
  LayoutUnit FlowAwareBorderEnd() const;
  LayoutUnit FlowAwareBorderBefore() const;
  LayoutUnit FlowAwareBorderAfter() const;
  LayoutUnit FlowAwarePaddingStart() const;
  LayoutUnit FlowAwarePaddingEnd() const;
  LayoutUnit FlowAwarePaddingBefore() const;
  LayoutUnit FlowAwarePaddingAfter() const;
  LayoutUnit FlowAwareMarginStartForChild(const LayoutBox& child) const;
  LayoutUnit FlowAwareMarginEndForChild(const LayoutBox& child) const;
  LayoutUnit FlowAwareMarginBeforeForChild(const LayoutBox& child) const;
  LayoutUnit CrossAxisMarginExtentForChild(const LayoutBox& child) const;
  LayoutUnit CrossAxisScrollbarExtent() const;
  LayoutUnit CrossAxisScrollbarExtentForChild(const LayoutBox& child) const;
  LayoutPoint FlowAwareLocationForChild(const LayoutBox& child) const;
  bool UseChildAspectRatio(const LayoutBox& child) const;
  LayoutUnit ComputeMainSizeFromAspectRatioUsing(
      const LayoutBox& child,
      Length cross_size_length) const;
  void SetFlowAwareLocationForChild(LayoutBox& child, const LayoutPoint&);
  LayoutUnit ComputeInnerFlexBaseSizeForChild(
      LayoutBox& child,
      LayoutUnit main_axis_border_and_padding,
      ChildLayoutType = kLayoutIfNeeded);
  void AdjustAlignmentForChild(LayoutBox& child, LayoutUnit);
  ItemPosition AlignmentForChild(const LayoutBox& child) const;
  bool MainAxisLengthIsDefinite(const LayoutBox& child,
                                const Length& flex_basis) const;
  bool CrossAxisLengthIsDefinite(const LayoutBox& child,
                                 const Length& flex_basis) const;
  bool NeedToStretchChildLogicalHeight(const LayoutBox& child) const;
  bool ChildHasIntrinsicMainAxisSize(const LayoutBox& child) const;
  EOverflow MainAxisOverflowForChild(const LayoutBox& child) const;
  EOverflow CrossAxisOverflowForChild(const LayoutBox& child) const;
  void CacheChildMainSize(const LayoutBox& child);

  void LayoutFlexItems(bool relayout_children, SubtreeLayoutScope&);
  LayoutUnit AutoMarginOffsetInMainAxis(const Vector<FlexItem>&,
                                        LayoutUnit& available_free_space);
  void UpdateAutoMarginsInMainAxis(LayoutBox& child,
                                   LayoutUnit auto_margin_offset);
  bool HasAutoMarginsInCrossAxis(const LayoutBox& child) const;
  bool UpdateAutoMarginsInCrossAxis(LayoutBox& child,
                                    LayoutUnit available_alignment_space);
  void RepositionLogicalHeightDependentFlexItems(Vector<FlexLine>&);
  LayoutUnit ClientLogicalBottomAfterRepositioning();

  LayoutUnit AvailableAlignmentSpaceForChild(LayoutUnit line_cross_axis_extent,
                                             const LayoutBox& child);
  LayoutUnit MarginBoxAscentForChild(const LayoutBox& child);

  LayoutUnit ComputeChildMarginValue(Length margin);
  void PrepareOrderIteratorAndMargins();
  LayoutUnit AdjustChildSizeForMinAndMax(const LayoutBox& child,
                                         LayoutUnit child_size);
  LayoutUnit AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
      const LayoutBox& child,
      LayoutUnit child_size);
  FlexItem ConstructFlexItem(LayoutBox& child, ChildLayoutType);

  bool ResolveFlexibleLengths(FlexLine*,
                              LayoutUnit initial_free_space,
                              LayoutUnit& remaining_free_space);

  void ResetAutoMarginsAndLogicalTopInCrossAxis(LayoutBox& child);
  void SetOverrideMainAxisContentSizeForChild(LayoutBox& child,
                                              LayoutUnit child_preferred_size);
  void PrepareChildForPositionedLayout(LayoutBox& child);
  void LayoutAndPlaceChildren(LayoutUnit& cross_axis_offset,
                              FlexLine*,
                              LayoutUnit available_free_space,
                              bool relayout_children,
                              SubtreeLayoutScope&);
  void LayoutColumnReverse(const Vector<FlexItem>&,
                           LayoutUnit cross_axis_offset,
                           LayoutUnit available_free_space);
  void AlignFlexLines(Vector<FlexLine>&);
  void AlignChildren(const Vector<FlexLine>&);
  void ApplyStretchAlignmentToChild(LayoutBox& child,
                                    LayoutUnit line_cross_axis_extent);
  void FlipForRightToLeftColumn(const Vector<FlexLine>& line_contexts);
  void FlipForWrapReverse(const Vector<FlexLine>&,
                          LayoutUnit cross_axis_start_edge);

  float CountIntrinsicSizeForAlgorithmChange(
      LayoutUnit max_preferred_width,
      LayoutBox* child,
      float previous_max_content_flex_fraction) const;

  // This is used to cache the preferred size for orthogonal flow children so we
  // don't have to relayout to get it
  HashMap<const LayoutObject*, LayoutUnit> intrinsic_size_along_main_axis_;

  // This set is used to keep track of which children we laid out in this
  // current layout iteration. We need it because the ones in this set may
  // need an additional layout pass for correct stretch alignment handling, as
  // the first layout likely did not use the correct value for percentage
  // sizing of children.
  HashSet<const LayoutObject*> relaid_out_children_;

  mutable OrderIterator order_iterator_;
  int number_of_in_flow_children_on_first_line_;

  // This is SizeIsUnknown outside of layoutBlock()
  mutable SizeDefiniteness has_definite_height_;
  bool in_layout_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutFlexibleBox, IsFlexibleBox());

}  // namespace blink

#endif  // LayoutFlexibleBox_h
