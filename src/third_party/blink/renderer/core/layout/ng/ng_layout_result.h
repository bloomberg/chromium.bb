// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_RESULT_H_

#include "base/dcheck_is_on.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_margin_strut.h"
#include "third_party/blink/renderer/core/layout/ng/grid/layout_ng_grid.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_appeal.h"
#include "third_party/blink/renderer/core/layout/ng/ng_early_break.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGBoxFragmentBuilder;
class NGContainerFragmentBuilder;
class NGExclusionSpace;
class NGLineBoxFragmentBuilder;

// The NGLayoutResult stores the resulting data from layout. This includes
// geometry information in form of a NGPhysicalFragment, which is kept around
// for painting, hit testing, etc., as well as additional data which is only
// necessary during layout and stored on this object.
// Layout code should access the NGPhysicalFragment through the wrappers in
// NGFragment et al.
class CORE_EXPORT NGLayoutResult : public RefCounted<NGLayoutResult> {
 public:
  enum EStatus {
    kSuccess = 0,
    kBfcBlockOffsetResolved = 1,
    kNeedsEarlierBreak = 2,
    kOutOfFragmentainerSpace = 3,
    kNeedsRelayoutWithNoForcedTruncateAtLineClamp = 4,
    kDisableFragmentation = 5,
    // When adding new values, make sure the bit size of |Bitfields::status| is
    // large enough to store.
  };

  // Creates a copy of |other| but uses the "post-layout" fragments to ensure
  // fragment-tree consistency.
  static scoped_refptr<const NGLayoutResult> CloneWithPostLayoutFragments(
      const NGLayoutResult& other,
      const absl::optional<PhysicalRect> updated_layout_overflow =
          absl::nullopt);

  // Create a copy of NGLayoutResult with |BfcBlockOffset| replaced by the given
  // parameter. Note, when |bfc_block_offset| is |nullopt|, |BfcBlockOffset| is
  // still replaced with |nullopt|.
  NGLayoutResult(const NGLayoutResult& other,
                 const NGConstraintSpace& new_space,
                 const NGMarginStrut& new_end_margin_strut,
                 LayoutUnit bfc_line_offset,
                 absl::optional<LayoutUnit> bfc_block_offset,
                 LayoutUnit block_offset_delta);
  ~NGLayoutResult();

  const NGPhysicalFragment& PhysicalFragment() const {
    DCHECK(physical_fragment_);
    DCHECK_EQ(kSuccess, Status());
    return *physical_fragment_;
  }

  int LinesUntilClamp() const {
    return HasRareData() ? rare_data_->lines_until_clamp : 0;
  }

  // Return the adjustment baked into the fragment's block-offset that's caused
  // by ruby annotations.
  LayoutUnit AnnotationBlockOffsetAdjustment() const {
    if (!HasRareData())
      return LayoutUnit();
    return rare_data_->annotation_block_offset_adjustment;
  }

  // How much an annotation box overflow from this box.
  // This is for LayoutNGRubyRun and line boxes.
  // 0 : No overflow
  // -N : Overflowing by N px at block-start side
  //      This happens only for LayoutRubyRun.
  // N : Overflowing by N px at block-end side
  LayoutUnit AnnotationOverflow() const {
    return HasRareData() ? rare_data_->annotation_overflow : LayoutUnit();
  }

  // The amount of available space for block-start side annotations of the
  // next box.
  // This never be negative.
  LayoutUnit BlockEndAnnotationSpace() const {
    return HasRareData() ? rare_data_->block_end_annotation_space
                         : LayoutUnit();
  }

  LogicalOffset OutOfFlowPositionedOffset() const {
    DCHECK(bitfields_.has_oof_positioned_offset);
    return HasRareData() ? rare_data_->oof_positioned_offset
                         : oof_positioned_offset_;
  }

  // Returns if we can use the first-tier OOF-positioned cache.
  bool CanUseOutOfFlowPositionedFirstTierCache() const {
    DCHECK(physical_fragment_->IsOutOfFlowPositioned());
    return bitfields_.can_use_out_of_flow_positioned_first_tier_cache;
  }

  // Get the column spanner (if any) that interrupted column layout.
  NGBlockNode ColumnSpanner() const {
    return HasRareData() ? rare_data_->column_spanner : NGBlockNode(nullptr);
  }

  // True if this result is the parent of a column spanner and is empty (i.e.
  // has no children). This is used to determine whether the column spanner
  // margins should collapse. Note that |is_empty_spanner_parent| may be false
  // even if this column spanner parent is actually empty. This can happen in
  // the case where the spanner parent has no children but has not broken
  // previously - in which case, we shouldn't collapse the spanner margins since
  // we do not want to collapse margins with a column spanner outside of this
  // parent.
  bool IsEmptySpannerParent() const {
    return bitfields_.is_empty_spanner_parent;
  }

  const NGEarlyBreak* GetEarlyBreak() const {
    if (!HasRareData())
      return nullptr;
    return rare_data_->early_break;
  }

  const NGExclusionSpace& ExclusionSpace() const {
    if (bitfields_.has_rare_data_exclusion_space) {
      DCHECK(HasRareData());
      return rare_data_->exclusion_space;
    }

    return space_.ExclusionSpace();
  }

  EStatus Status() const { return static_cast<EStatus>(bitfields_.status); }

  LayoutUnit BfcLineOffset() const {
    if (HasRareData())
      return rare_data_->bfc_line_offset;

    if (bitfields_.has_oof_positioned_offset) {
      DCHECK(physical_fragment_->IsOutOfFlowPositioned());
      return LayoutUnit();
    }

    return bfc_offset_.line_offset;
  }

  const absl::optional<LayoutUnit> BfcBlockOffset() const {
    if (HasRareData())
      return rare_data_->bfc_block_offset;

    if (bitfields_.has_oof_positioned_offset) {
      DCHECK(physical_fragment_->IsOutOfFlowPositioned());
      return LayoutUnit();
    }

    if (bitfields_.is_bfc_block_offset_nullopt)
      return absl::nullopt;

    return bfc_offset_.block_offset;
  }

  // The BFC block-offset where a line-box has been placed. Will be nullopt if
  // it isn't a line-box, or an empty line-box.
  //
  // This can be different (but rarely) to where the |BfcBlockOffset()|
  // resolves to, when floats are present. E.g.
  //
  // <div style="width: 100px; display: flow-root;">
  //   <div style="float: left; width: 200px; height: 20px;"></div>
  //   text
  // </div>
  //
  // In the above example the |BfcBlockOffset()| will be at 0px, where-as the
  // |LineBoxBfcBlockOffset()| will be at 20px.
  absl::optional<LayoutUnit> LineBoxBfcBlockOffset() const {
    if (Status() != kSuccess || !PhysicalFragment().IsLineBox())
      return absl::nullopt;

    if (HasRareData() && rare_data_->line_box_bfc_block_offset)
      return rare_data_->line_box_bfc_block_offset;

    return BfcBlockOffset();
  }

  const NGMarginStrut EndMarginStrut() const {
    return HasRareData() ? rare_data_->end_margin_strut : NGMarginStrut();
  }

  // Get the intrinsic block-size of the fragment (i.e. the block-size the
  // fragment would get if no block-size constraints were applied). This is not
  // supported (and should not be needed [1]) if the node got split into
  // multiple fragments.
  //
  // [1] If a node gets block-fragmented, it means that it has possibly been
  // constrained and/or stretched by something extrinsic (i.e. the
  // fragmentainer), so the value returned here wouldn't be useful.
  const LayoutUnit IntrinsicBlockSize() const {
#if DCHECK_IS_ON()
    AssertSoleBoxFragment();
#endif
    return intrinsic_block_size_;
  }

  // Return the amount of clearance that we have to add after the fragment. This
  // is used for BR clear elements.
  LayoutUnit ClearanceAfterLine() const {
    return HasRareData() ? rare_data_->clearance_after_line : LayoutUnit();
  }

  LayoutUnit MinimalSpaceShortage() const {
    if (!HasRareData() || rare_data_->minimal_space_shortage == kIndefiniteSize)
      return LayoutUnit::Max();
    return rare_data_->minimal_space_shortage;
  }

  LayoutUnit TallestUnbreakableBlockSize() const {
    if (!HasRareData() ||
        rare_data_->tallest_unbreakable_block_size == kIndefiniteSize)
      return LayoutUnit();
    return rare_data_->tallest_unbreakable_block_size;
  }

  // Return the (lowest) appeal among any unforced breaks inside the resulting
  // fragment (or kBreakAppealPerfect if there are no such breaks).
  //
  // A higher value is better. Violating breaking rules decreases appeal. Forced
  // breaks always have perfect appeal.
  //
  // If a node breaks, the resulting fragment usually carries an outgoing break
  // token, but this isn't necessarily the case if the break happened inside an
  // inner fragmentation context. The block-size of an inner multicol is
  // constrained by the available block-size in the outer fragmentation
  // context. This may cause suboptimal column breaks inside. The entire inner
  // multicol container may fit in the outer fragmentation context, but we may
  // also need to consider the inner column breaks (in an inner fragmentation
  // context). If there are any suboptimal breaks, we may want to push the
  // entire multicol container to the next outer fragmentainer, if it's likely
  // that we'll avoid suboptimal column breaks inside that way.
  NGBreakAppeal BreakAppeal() const {
    return static_cast<NGBreakAppeal>(bitfields_.break_appeal);
  }

  SerializedScriptValue* CustomLayoutData() const {
    return HasRareData() ? rare_data_->custom_layout_data.get() : nullptr;
  }

  wtf_size_t TableColumnCount() const {
    return HasRareData() ? rare_data_->table_column_count_ : 0;
  }

  const NGGridLayoutData* GridLayoutData() const {
    return HasRareData() ? rare_data_->grid_layout_data_.get() : nullptr;
  }

  LayoutUnit MathItalicCorrection() const {
    return HasRareData() && rare_data_->math_layout_data_
               ? rare_data_->math_layout_data_->italic_correction_
               : LayoutUnit();
  }

  // The break-before value on the first child needs to be propagated to the
  // container, in search of a valid class A break point.
  EBreakBetween InitialBreakBefore() const {
    return static_cast<EBreakBetween>(bitfields_.initial_break_before);
  }

  // The break-after value on the last child needs to be propagated to the
  // container, in search of a valid class A break point.
  EBreakBetween FinalBreakAfter() const {
    return static_cast<EBreakBetween>(bitfields_.final_break_after);
  }

  // Return true if the fragment broke because a forced break before a child.
  bool HasForcedBreak() const { return bitfields_.has_forced_break; }

  // Returns true if the fragment should be considered empty for margin
  // collapsing purposes (e.g. margins "collapse through").
  bool IsSelfCollapsing() const { return bitfields_.is_self_collapsing; }

  // Return true if this fragment got its block offset increased by the presence
  // of floats.
  bool IsPushedByFloats() const { return bitfields_.is_pushed_by_floats; }

  // Returns the types of preceding adjoining objects.
  // See |NGAdjoiningObjectTypes|.
  //
  // Adjoining floats should be treated differently when calculating clearance
  // on a block with adjoining block-start margin (in such cases we will know
  // up front that the block will need clearance, since, if it doesn't, the
  // float will be pulled along with the block, and the block will fail to
  // clear).
  NGAdjoiningObjectTypes AdjoiningObjectTypes() const {
    return bitfields_.adjoining_object_types;
  }

  // Returns true if the initial (pre-layout) block-size of this fragment was
  // indefinite. (e.g. it has "height: auto").
  bool IsInitialBlockSizeIndefinite() const {
    return bitfields_.is_initial_block_size_indefinite;
  }

  // Returns true if there is a descendant that depends on percentage
  // resolution block-size changes.
  // Some layout modes (flex-items, table-cells) have more complex child
  // percentage sizing behaviour (typically when their parent layout forces a
  // block-size on them).
  bool HasDescendantThatDependsOnPercentageBlockSize() const {
    return bitfields_.has_descendant_that_depends_on_percentage_block_size;
  }

  // Returns true if this subtree modified the incoming margin-strut (i.e.
  // appended a non-zero margin).
  bool SubtreeModifiedMarginStrut() const {
    return bitfields_.subtree_modified_margin_strut;
  }

  // Returns the space which generated this object for caching purposes.
  const NGConstraintSpace& GetConstraintSpaceForCaching() const {
#if DCHECK_IS_ON()
    DCHECK(has_valid_space_);
#endif
    return space_;
  }

  // This exposes a mutable part of the layout result just for the
  // |NGOutOfFlowLayoutPart|.
  class MutableForOutOfFlow final {
    STACK_ALLOCATED();

   protected:
    friend class NGOutOfFlowLayoutPart;

    void SetOutOfFlowPositionedOffset(
        const LogicalOffset& offset,
        bool can_use_out_of_flow_positioned_first_tier_cache) {
      // OOF-positioned nodes *must* always have an initial BFC-offset.
      DCHECK(layout_result_->physical_fragment_->IsOutOfFlowPositioned());
      DCHECK_EQ(layout_result_->BfcLineOffset(), LayoutUnit());
      DCHECK_EQ(layout_result_->BfcBlockOffset().value_or(LayoutUnit()),
                LayoutUnit());

      layout_result_->bitfields_
          .can_use_out_of_flow_positioned_first_tier_cache =
          can_use_out_of_flow_positioned_first_tier_cache;
      layout_result_->bitfields_.has_oof_positioned_offset = true;
      if (layout_result_->HasRareData())
        layout_result_->rare_data_->oof_positioned_offset = offset;
      else
        layout_result_->oof_positioned_offset_ = offset;
    }

   private:
    friend class NGLayoutResult;
    MutableForOutOfFlow(const NGLayoutResult* layout_result)
        : layout_result_(const_cast<NGLayoutResult*>(layout_result)) {}

    NGLayoutResult* layout_result_;
  };

  MutableForOutOfFlow GetMutableForOutOfFlow() const {
    return MutableForOutOfFlow(this);
  }

  class MutableForLayoutBoxCachedResults final {
    STACK_ALLOCATED();

   protected:
    friend class LayoutBox;

    void SetFragmentChildrenInvalid() {
      layout_result_->physical_fragment_->SetChildrenInvalid();
    }

   private:
    friend class NGLayoutResult;
    explicit MutableForLayoutBoxCachedResults(
        const NGLayoutResult* layout_result)
        : layout_result_(const_cast<NGLayoutResult*>(layout_result)) {}

    NGLayoutResult* layout_result_;
  };

  MutableForLayoutBoxCachedResults GetMutableForLayoutBoxCachedResults() const {
    return MutableForLayoutBoxCachedResults(this);
  }

#if DCHECK_IS_ON()
  void CheckSameForSimplifiedLayout(const NGLayoutResult&,
                                    bool check_same_block_size = true) const;
#endif

  using NGContainerFragmentBuilderPassKey =
      base::PassKey<NGContainerFragmentBuilder>;
  // This constructor is for a non-success status.
  NGLayoutResult(NGContainerFragmentBuilderPassKey,
                 EStatus,
                 NGContainerFragmentBuilder*);

  // This constructor requires a non-null fragment and sets a success status.
  using NGBoxFragmentBuilderPassKey = base::PassKey<NGBoxFragmentBuilder>;
  NGLayoutResult(NGBoxFragmentBuilderPassKey,
                 scoped_refptr<const NGPhysicalFragment> physical_fragment,
                 NGBoxFragmentBuilder*);

  using NGLineBoxFragmentBuilderPassKey =
      base::PassKey<NGLineBoxFragmentBuilder>;
  // This constructor requires a non-null fragment and sets a success status.
  NGLayoutResult(NGLineBoxFragmentBuilderPassKey,
                 scoped_refptr<const NGPhysicalFragment> physical_fragment,
                 NGLineBoxFragmentBuilder*);

  // See https://w3c.github.io/mathml-core/#box-model
  struct MathData {
    LayoutUnit italic_correction_;
  };

 private:
  friend class MutableForOutOfFlow;

  // Creates a copy of NGLayoutResult with a new (but "identical") fragment.
  NGLayoutResult(const NGLayoutResult& other,
                 scoped_refptr<const NGPhysicalFragment> physical_fragment);

  // Delegate constructor that sets up what it can, based on the builder.
  NGLayoutResult(scoped_refptr<const NGPhysicalFragment> physical_fragment,
                 NGContainerFragmentBuilder* builder);

  // We don't need the copy constructor, move constructor, copy
  // assigmnment-operator, or move assignment-operator today.
  // Delete these to clarify that they will not work because a |RefCounted|
  // object can't be copied directly.
  //
  // If at some point we do need these constructors particular care will need
  // to be taken with the |rare_data_| field which is manually memory managed.
  NGLayoutResult(const NGLayoutResult&) = delete;
  NGLayoutResult(NGLayoutResult&&) = delete;
  NGLayoutResult& operator=(const NGLayoutResult& other) = delete;
  NGLayoutResult& operator=(NGLayoutResult&& other) = delete;
  NGLayoutResult() = delete;

  static NGExclusionSpace MergeExclusionSpaces(
      const NGLayoutResult& other,
      const NGExclusionSpace& new_input_exclusion_space,
      LayoutUnit bfc_line_offset,
      LayoutUnit block_offset_delta);

  struct RareData {
    USING_FAST_MALLOC(RareData);

   public:
    RareData(LayoutUnit bfc_line_offset,
             absl::optional<LayoutUnit> bfc_block_offset)
        : bfc_line_offset(bfc_line_offset),
          bfc_block_offset(bfc_block_offset) {}
    RareData(const RareData& rare_data)
        : bfc_line_offset(rare_data.bfc_line_offset),
          bfc_block_offset(rare_data.bfc_block_offset),
          early_break(rare_data.early_break),
          oof_positioned_offset(rare_data.oof_positioned_offset),
          end_margin_strut(rare_data.end_margin_strut),
          // This will initialize "both" members of the union.
          tallest_unbreakable_block_size(
              rare_data.tallest_unbreakable_block_size),
          exclusion_space(rare_data.exclusion_space),
          custom_layout_data(rare_data.custom_layout_data),
          clearance_after_line(rare_data.clearance_after_line),
          line_box_bfc_block_offset(rare_data.line_box_bfc_block_offset),
          annotation_block_offset_adjustment(
              rare_data.annotation_block_offset_adjustment),
          annotation_overflow(rare_data.annotation_overflow),
          block_end_annotation_space(rare_data.block_end_annotation_space),
          lines_until_clamp(rare_data.lines_until_clamp),
          table_column_count_(rare_data.table_column_count_),
          math_layout_data_(rare_data.math_layout_data_) {
      if (rare_data.grid_layout_data_) {
        grid_layout_data_ =
            std::make_unique<NGGridLayoutData>(*rare_data.grid_layout_data_);
      }
    }

    LayoutUnit bfc_line_offset;
    absl::optional<LayoutUnit> bfc_block_offset;

    Persistent<const NGEarlyBreak> early_break;
    LogicalOffset oof_positioned_offset;
    NGMarginStrut end_margin_strut;
    NGBlockNode column_spanner = nullptr;
    union {
      // Only set in the initial column balancing layout pass, when we have no
      // clue what the column block-size is going to be.
      LayoutUnit tallest_unbreakable_block_size;

      // Only set in subsequent column balancing passes, when we have set a
      // tentative column block-size. At every column boundary we'll record
      // space shortage, and store the smallest one here. If the columns
      // couldn't fit all the content, and we're allowed to stretch columns
      // further, we'll perform another pass with the column block-size
      // increased by this amount.
      LayoutUnit minimal_space_shortage = kIndefiniteSize;
    };
    NGExclusionSpace exclusion_space;
    scoped_refptr<SerializedScriptValue> custom_layout_data;

    LayoutUnit clearance_after_line;
    absl::optional<LayoutUnit> line_box_bfc_block_offset;
    LayoutUnit annotation_block_offset_adjustment;
    LayoutUnit annotation_overflow;
    LayoutUnit block_end_annotation_space;
    int lines_until_clamp = 0;
    wtf_size_t table_column_count_ = 0;
    std::unique_ptr<const NGGridLayoutData> grid_layout_data_;
    absl::optional<MathData> math_layout_data_;
  };

  bool HasRareData() const { return bitfields_.has_rare_data; }
  RareData* EnsureRareData();

#if DCHECK_IS_ON()
  void AssertSoleBoxFragment() const;
#endif

  struct Bitfields {
    DISALLOW_NEW();

   public:
    // We define the default constructor so that the |has_rare_data| bit is
    // never uninitialized (potentially allowing a dangling pointer).
    Bitfields()
        : Bitfields(
              /* is_self_collapsing */ false,
              /* is_pushed_by_floats */ false,
              /* adjoining_object_types */ kAdjoiningNone,
              /* has_descendant_that_depends_on_percentage_block_size */
              false,
              /* subtree_modified_margin_strut */ false) {}
    Bitfields(bool is_self_collapsing,
              bool is_pushed_by_floats,
              NGAdjoiningObjectTypes adjoining_object_types,
              bool has_descendant_that_depends_on_percentage_block_size,
              bool subtree_modified_margin_strut)
        : has_rare_data(false),
          has_rare_data_exclusion_space(false),
          has_oof_positioned_offset(false),
          can_use_out_of_flow_positioned_first_tier_cache(false),
          is_bfc_block_offset_nullopt(false),
          has_forced_break(false),
          break_appeal(kBreakAppealPerfect),
          is_empty_spanner_parent(false),
          is_self_collapsing(is_self_collapsing),
          is_pushed_by_floats(is_pushed_by_floats),
          adjoining_object_types(static_cast<unsigned>(adjoining_object_types)),
          is_initial_block_size_indefinite(false),
          has_descendant_that_depends_on_percentage_block_size(
              has_descendant_that_depends_on_percentage_block_size),
          subtree_modified_margin_strut(subtree_modified_margin_strut),
          initial_break_before(static_cast<unsigned>(EBreakBetween::kAuto)),
          final_break_after(static_cast<unsigned>(EBreakBetween::kAuto)),
          status(static_cast<unsigned>(kSuccess)) {}

    unsigned has_rare_data : 1;
    unsigned has_rare_data_exclusion_space : 1;
    unsigned has_oof_positioned_offset : 1;
    unsigned can_use_out_of_flow_positioned_first_tier_cache : 1;
    unsigned is_bfc_block_offset_nullopt : 1;

    unsigned has_forced_break : 1;
    unsigned break_appeal : kNGBreakAppealBitsNeeded;
    unsigned is_empty_spanner_parent : 1;

    unsigned is_self_collapsing : 1;
    unsigned is_pushed_by_floats : 1;
    unsigned adjoining_object_types : 3;  // NGAdjoiningObjectTypes

    unsigned is_initial_block_size_indefinite : 1;
    unsigned has_descendant_that_depends_on_percentage_block_size : 1;

    unsigned subtree_modified_margin_strut : 1;

    unsigned initial_break_before : 4;  // EBreakBetween
    unsigned final_break_after : 4;     // EBreakBetween

    unsigned status : 3;  // EStatus
  };

  // The constraint space which generated this layout result, may not be valid
  // as indicated by |has_valid_space_|.
  const NGConstraintSpace space_;

  scoped_refptr<const NGPhysicalFragment> physical_fragment_;

  // To save space, we union these fields.
  //  - |rare_data_| is valid if the |Bitfields::has_rare_data| bit is set.
  //    |bfc_offset_| and |oof_positioned_offset_| are stored within the
  //    |RareData| object for this case.
  //  - |oof_positioned_offset_| is valid if the
  //    |Bitfields::has_oof_positioned_offset| bit is set. As the node is
  //    OOF-positioned the |bfc_offset_| is *always* the initial value.
  //  - Otherwise |bfc_offset_| is valid.
  union {
    NGBfcOffset bfc_offset_;
    // This is the final position of an OOF-positioned object in its parent's
    // writing-mode. This is set by the |NGOutOfFlowLayoutPart| while
    // generating this layout result.
    LogicalOffset oof_positioned_offset_;
    RareData* rare_data_;
  };

  LayoutUnit intrinsic_block_size_;
  Bitfields bitfields_;

#if DCHECK_IS_ON()
  bool has_valid_space_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_RESULT_H_
