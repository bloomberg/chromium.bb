// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGConstraintSpace_h
#define NGConstraintSpace_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_macros.h"
#include "core/layout/ng/ng_units.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"
#include "wtf/Vector.h"

namespace blink {

class ComputedStyle;
class LayoutBox;
class NGFragment;
class NGLayoutOpportunityIterator;

// TODO(glebl@): unused, delete.
enum NGExclusionType {
  kNGClearNone = 0,
  kNGClearFloatLeft = 1,
  kNGClearFloatRight = 2,
  kNGClearFragment = 4
};

enum NGFragmentationType {
  kFragmentNone,
  kFragmentPage,
  kFragmentColumn,
  kFragmentRegion
};

// The NGConstraintSpace represents a set of constraints and available space
// which a layout algorithm may produce a NGFragment within.
class CORE_EXPORT NGConstraintSpace final
    : public GarbageCollectedFinalized<NGConstraintSpace> {
 public:
  // This should live on NGBlockNode or another layout bridge and probably take
  // a root NGConstraintSpace.
  static NGConstraintSpace* CreateFromLayoutObject(const LayoutBox&);

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

  // Offset relative to the root constraint space.
  NGLogicalOffset Offset() const { return offset_; }
  // TODO(layout-ng): Set offset via NGConstraintSpacebuilder.
  void SetOffset(const NGLogicalOffset& offset) { offset_ = offset; }

  // Whether the current constraint space is for the newly established
  // Formatting Context.
  bool IsNewFormattingContext() const { return is_new_fc_; }

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
  bool IsFixedSizeInline() const { return is_fixed_size_inline_; }

  bool IsFixedSizeBlock() const { return is_fixed_size_block_; }

  // If specified a layout should produce a Fragment which fragments at the
  // blockSize if possible.
  NGFragmentationType BlockFragmentationType() const;

  // Modifies constraint space to account for a placed fragment. Depending on
  // the shape of the fragment this will either modify the inline or block
  // size, or add an exclusion.
  void Subtract(const NGFragment*);

  NGLayoutOpportunityIterator* LayoutOpportunities(
      unsigned clear = kNGClearNone,
      bool for_inline_or_bfc = false);

  DEFINE_INLINE_VIRTUAL_TRACE() {}

  NGConstraintSpace* ChildSpace(const ComputedStyle* style) const;
  String ToString() const;

 private:
  friend class NGConstraintSpaceBuilder;
  // Default constructor.
  NGConstraintSpace(NGWritingMode,
                    TextDirection,
                    NGLogicalSize available_size,
                    NGLogicalSize percentage_resolution_size,
                    bool is_fixed_size_inline,
                    bool is_fixed_size_block,
                    bool is_inline_direction_triggers_scrollbar,
                    bool is_block_direction_triggers_scrollbar,
                    NGFragmentationType block_direction_fragmentation_type,
                    bool is_new_fc,
                    const std::shared_ptr<NGExclusions>& exclusions_);

  NGLogicalSize available_size_;
  NGLogicalSize percentage_resolution_size_;

  unsigned is_fixed_size_inline_ : 1;
  unsigned is_fixed_size_block_ : 1;

  unsigned is_inline_direction_triggers_scrollbar_ : 1;
  unsigned is_block_direction_triggers_scrollbar_ : 1;

  unsigned block_direction_fragmentation_type_ : 2;

  // Whether the current constraint space is for the newly established
  // formatting Context
  unsigned is_new_fc_ : 1;

  NGLogicalOffset offset_;
  unsigned writing_mode_ : 3;
  unsigned direction_ : 1;

  const std::shared_ptr<NGExclusions> exclusions_;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const NGConstraintSpace& value) {
  return stream << value.ToString();
}

}  // namespace blink

#endif  // NGConstraintSpace_h
