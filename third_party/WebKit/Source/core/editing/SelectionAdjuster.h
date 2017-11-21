// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SelectionAdjuster_h
#define SelectionAdjuster_h

#include "core/CoreExport.h"
#include "core/editing/Forward.h"
#include "core/editing/TextGranularity.h"
#include "platform/wtf/Allocator.h"

namespace blink {

// |SelectionAdjuster| adjusts positions in |VisibleSelection| directly without
// calling |validate()|. Users of |SelectionAdjuster| should keep invariant of
// |VisibleSelection|, e.g. all positions are canonicalized.
class CORE_EXPORT SelectionAdjuster final {
  STATIC_ONLY(SelectionAdjuster);

 public:
  static Position AdjustSelectionStartToAvoidCrossingShadowBoundaries(
      const EphemeralRange&);
  static Position AdjustSelectionEndToAvoidCrossingShadowBoundaries(
      const EphemeralRange&);
  static PositionInFlatTree AdjustSelectionStartToAvoidCrossingShadowBoundaries(
      const EphemeralRangeInFlatTree&);
  static PositionInFlatTree AdjustSelectionEndToAvoidCrossingShadowBoundaries(
      const EphemeralRangeInFlatTree&);
  static SelectionInDOMTree AdjustSelectionRespectingGranularity(
      const SelectionInDOMTree&,
      TextGranularity);
  static SelectionInFlatTree AdjustSelectionRespectingGranularity(
      const SelectionInFlatTree&,
      TextGranularity);
};

}  // namespace blink

#endif  // SelectionAdjuster_h
