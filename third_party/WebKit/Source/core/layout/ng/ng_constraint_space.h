// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGConstraintSpace_h
#define NGConstraintSpace_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/geometry/ng_margin_strut.h"
#include "core/layout/ng/geometry/ng_physical_size.h"
#include "core/layout/ng/ng_exclusion.h"
#include "core/layout/ng/ng_unpositioned_float.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/heap/Handle.h"
#include "platform/text/TextDirection.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class LayoutBox;

enum NGFragmentationType {
  kFragmentNone,
  kFragmentPage,
  kFragmentColumn,
  kFragmentRegion
};

// The NGConstraintSpace represents a set of constraints and available space
// which a layout algorithm may produce a NGFragment within.
class CORE_EXPORT NGConstraintSpace final
    : public RefCounted<NGConstraintSpace> {
 public:
  // This should live on NGBlockNode or another layout bridge and probably take
  // a root NGConstraintSpace.
  static RefPtr<NGConstraintSpace> CreateFromLayoutObject(const LayoutBox&);

  const std::shared_ptr<NGExclusions>& Exclusions() const {
    return exclusions_;
  }

  TextDirection Direction() const {
    return static_cast<TextDirection>(direction_);
  }

  NGWritingMode WritingMode() const {
    return static_cast<NGWritingMode>(writing_mode_);
  }

  void AddExclusion(const NGExclusion& exclusion);

  // The size to use for percentage resolution.
  // See: https://drafts.csswg.org/css-sizing/#percentage-sizing
  NGLogicalSize PercentageResolutionSize() const {
    return percentage_resolution_size_;
  }

  // The available space size.
  // See: https://drafts.csswg.org/css-sizing/#available
  NGLogicalSize AvailableSize() const { return available_size_; }

  // Return the block-direction space available in the current fragmentainer.
  LayoutUnit FragmentainerSpaceAvailable() const {
    DCHECK(HasBlockFragmentation());
    return fragmentainer_space_available_;
  }

  // Whether the current constraint space is for the newly established
  // Formatting Context.
  bool IsNewFormattingContext() const { return is_new_fc_; }

  // Whether the fragment produced from layout should be anonymous, (e.g. it
  // may be a column in a multi-column layout). In such cases it shouldn't have
  // any borders or padding.
  bool IsAnonymous() const { return is_anonymous_; }

  // Whether exceeding the AvailableSize() triggers the presence of a scrollbar
  // for the indicated direction.
  // If exceeded the current layout should be aborted and invoked again with a
  // constraint space modified to reserve space for a scrollbar.
  bool IsInlineDirectionTriggersScrollbar() const {
    return is_inline_direction_triggers_scrollbar_;
  }

  bool IsBlockDirectionTriggersScrollbar() const {
    return is_block_direction_triggers_scrollbar_;
  }

  // Some layout modes “stretch” their children to a fixed size (e.g. flex,
  // grid). These flags represented whether a layout needs to produce a
  // fragment that satisfies a fixed constraint in the inline and block
  // direction respectively.
  // If these flags are true, the AvailableSize() is interpreted as the fixed
  // border-box size of this box in the respective dimension.
  bool IsFixedSizeInline() const { return is_fixed_size_inline_; }

  bool IsFixedSizeBlock() const { return is_fixed_size_block_; }

  // Whether an auto inline-size should be interpreted as shrink-to-fit
  // (ie. fit-content). This is used for inline-block, floats, etc.
  bool IsShrinkToFit() const { return is_shrink_to_fit_; }

  // If specified a layout should produce a Fragment which fragments at the
  // blockSize if possible.
  NGFragmentationType BlockFragmentationType() const;

  // Return true if this contraint space participates in a fragmentation
  // context.
  bool HasBlockFragmentation() const {
    return BlockFragmentationType() != kFragmentNone;
  }

  NGMarginStrut MarginStrut() const { return margin_strut_; }

  NGLogicalOffset BfcOffset() const { return bfc_offset_; }

  Vector<RefPtr<NGUnpositionedFloat>>& UnpositionedFloats() {
    return unpositioned_floats_;
  }

  WTF::Optional<LayoutUnit> ClearanceOffset() const {
    return clearance_offset_;
  }

  bool operator==(const NGConstraintSpace&) const;
  bool operator!=(const NGConstraintSpace&) const;

  String ToString() const;

 private:
  friend class NGConstraintSpaceBuilder;
  // Default constructor.
  NGConstraintSpace(NGWritingMode,
                    TextDirection,
                    NGLogicalSize available_size,
                    NGLogicalSize percentage_resolution_size,
                    NGPhysicalSize initial_containing_block_size,
                    LayoutUnit fragmentainer_space_available,
                    bool is_fixed_size_inline,
                    bool is_fixed_size_block,
                    bool is_shrink_to_fit,
                    bool is_inline_direction_triggers_scrollbar,
                    bool is_block_direction_triggers_scrollbar,
                    NGFragmentationType block_direction_fragmentation_type,
                    bool is_new_fc,
                    bool is_anonymous,
                    const NGMarginStrut& margin_strut,
                    const NGLogicalOffset& bfc_offset,
                    const std::shared_ptr<NGExclusions>& exclusions,
                    Vector<RefPtr<NGUnpositionedFloat>>& unpositioned_floats,
                    const WTF::Optional<LayoutUnit>& clearance_offset);

  NGPhysicalSize InitialContainingBlockSize() const {
    return initial_containing_block_size_;
  }

  NGLogicalSize available_size_;
  NGLogicalSize percentage_resolution_size_;
  NGPhysicalSize initial_containing_block_size_;

  LayoutUnit fragmentainer_space_available_;

  unsigned is_fixed_size_inline_ : 1;
  unsigned is_fixed_size_block_ : 1;

  unsigned is_shrink_to_fit_ : 1;

  unsigned is_inline_direction_triggers_scrollbar_ : 1;
  unsigned is_block_direction_triggers_scrollbar_ : 1;

  unsigned block_direction_fragmentation_type_ : 2;

  // Whether the current constraint space is for the newly established
  // formatting Context
  unsigned is_new_fc_ : 1;

  unsigned is_anonymous_ : 1;

  unsigned writing_mode_ : 3;
  unsigned direction_ : 1;

  NGMarginStrut margin_strut_;
  NGLogicalOffset bfc_offset_;
  const std::shared_ptr<NGExclusions> exclusions_;
  WTF::Optional<LayoutUnit> clearance_offset_;
  Vector<RefPtr<NGUnpositionedFloat>> unpositioned_floats_;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const NGConstraintSpace& value) {
  return stream << value.ToString();
}

}  // namespace blink

#endif  // NGConstraintSpace_h
