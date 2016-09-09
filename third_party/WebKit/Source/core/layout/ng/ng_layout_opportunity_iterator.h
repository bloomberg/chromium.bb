// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutOpportunityIterator_h
#define NGLayoutOpportunityIterator_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"
#include "wtf/Vector.h"

namespace blink {

class CORE_EXPORT NGLayoutOpportunityIterator final
    : public GarbageCollectedFinalized<NGLayoutOpportunityIterator> {
 public:
  NGLayoutOpportunityIterator(NGConstraintSpace* space,
                              unsigned clear,
                              bool for_inline_or_bfc);
  ~NGLayoutOpportunityIterator() {}

  NGConstraintSpace* Next();

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(constraint_space_);
    visitor->trace(current_opportunities_);
  }

 private:
  void computeForExclusion(unsigned index);
  LayoutUnit heightForOpportunity(LayoutUnit left,
                                  LayoutUnit top,
                                  LayoutUnit right,
                                  LayoutUnit bottom);
  void addLayoutOpportunity(LayoutUnit left,
                            LayoutUnit top,
                            LayoutUnit right,
                            LayoutUnit bottom);

  Member<NGConstraintSpace> constraint_space_;
  unsigned clear_;
  bool for_inline_or_bfc_;
  Vector<NGExclusion> filtered_exclusions_;
  HeapVector<Member<NGConstraintSpace>> current_opportunities_;
  unsigned current_exclusion_idx_;
};

}  // namespace blink

#endif  // NGLayoutOpportunityIterator_h
