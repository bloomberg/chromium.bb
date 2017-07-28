/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SelectionModifier_h
#define SelectionModifier_h

#include "base/macros.h"
#include "core/editing/VisibleSelection.h"
#include "platform/LayoutUnit.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LocalFrame;

enum class SelectionModifyAlteration { kMove, kExtend };
enum class SelectionModifyVerticalDirection { kUp, kDown };
enum class SelectionModifyDirection { kBackward, kForward, kLeft, kRight };

class SelectionModifier {
  STACK_ALLOCATED();

 public:
  // |frame| is used for providing settings.
  SelectionModifier(const LocalFrame& /* frame */,
                    const VisibleSelection&,
                    LayoutUnit);
  SelectionModifier(const LocalFrame&, const VisibleSelection&);

  LayoutUnit XPosForVerticalArrowNavigation() const {
    return x_pos_for_vertical_arrow_navigation_;
  }
  const VisibleSelection& Selection() const { return selection_; }

  bool Modify(SelectionModifyAlteration,
              SelectionModifyDirection,
              TextGranularity);
  bool ModifyWithPageGranularity(SelectionModifyAlteration,
                                 unsigned vertical_distance,
                                 SelectionModifyVerticalDirection);

 private:
  LocalFrame* GetFrame() const { return frame_; }

  static bool ShouldAlwaysUseDirectionalSelection(LocalFrame*);
  TextDirection DirectionOfEnclosingBlock() const;
  TextDirection DirectionOfSelection() const;
  VisiblePosition PositionForPlatform(bool is_get_start) const;
  VisiblePosition StartForPlatform() const;
  VisiblePosition EndForPlatform() const;
  LayoutUnit LineDirectionPointForBlockDirectionNavigation(const Position&);
  VisiblePosition ModifyExtendingRight(TextGranularity);
  VisiblePosition ModifyExtendingRightInternal(TextGranularity);
  VisiblePosition ModifyExtendingForward(TextGranularity);
  VisiblePosition ModifyExtendingForwardInternal(TextGranularity);
  VisiblePosition ModifyMovingRight(TextGranularity);
  VisiblePosition ModifyMovingForward(TextGranularity);
  VisiblePosition ModifyExtendingLeft(TextGranularity);
  VisiblePosition ModifyExtendingLeftInternal(TextGranularity);
  VisiblePosition ModifyExtendingBackward(TextGranularity);
  VisiblePosition ModifyExtendingBackwardInternal(TextGranularity);
  VisiblePosition ModifyMovingLeft(TextGranularity);
  VisiblePosition ModifyMovingBackward(TextGranularity);
  VisiblePosition NextWordPositionForPlatform(const VisiblePosition&);

  // TODO(editing-dev): We should handle |skips_spaces_when_moving_right| in
  // another way, e.g. pass |EditingBehavior()|.
  static VisiblePosition LeftWordPosition(const VisiblePosition&,
                                          bool skips_space_when_moving_right);
  static VisiblePosition RightWordPosition(const VisiblePosition&,
                                           bool skips_space_when_moving_right);

  Member<LocalFrame> frame_;
  VisibleSelection selection_;
  LayoutUnit x_pos_for_vertical_arrow_navigation_;

  DISALLOW_COPY_AND_ASSIGN(SelectionModifier);
};

LayoutUnit NoXPosForVerticalArrowNavigation();

// Following functions are exported for using in SelectionModifier and
// testing only.

// TODO(yosin) Since return value of |leftPositionOf()| with |VisiblePosition|
// isn't defined well on flat tree, we should not use it for a position in
// flat tree.
CORE_EXPORT VisiblePosition LeftPositionOf(const VisiblePosition&);
CORE_EXPORT VisiblePositionInFlatTree
LeftPositionOf(const VisiblePositionInFlatTree&);
// TODO(yosin) Since return value of |rightPositionOf()| with |VisiblePosition|
// isn't defined well on flat tree, we should not use it for a position in
// flat tree.
CORE_EXPORT VisiblePosition RightPositionOf(const VisiblePosition&);
CORE_EXPORT VisiblePositionInFlatTree
RightPositionOf(const VisiblePositionInFlatTree&);

}  // namespace blink

#endif  // SelectionModifier_h
