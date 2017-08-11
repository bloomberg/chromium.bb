// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutOpportunityIterator_h
#define NGLayoutOpportunityIterator_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_exclusion.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_layout_opportunity_tree_node.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

typedef NGLogicalRect NGLayoutOpportunity;
typedef Vector<NGLayoutOpportunity> NGLayoutOpportunities;

NGLayoutOpportunity FindLayoutOpportunityForFragment(
    const NGExclusions* exclusions,
    const NGLogicalSize& available_size,
    const NGLogicalOffset& origin_point,
    const NGBoxStrut& margins,
    const NGLogicalSize& size);

class CORE_EXPORT NGLayoutOpportunityIterator final {
  STACK_ALLOCATED();

 public:
  // Default constructor.
  //
  // @param exclusions List of exclusions that should be avoided by this
  //                   iterator while generating layout opportunities.
  // @param available_size Available size that represents a rectangle where this
  //                       iterator searches layout opportunities.
  // @param offset Offset used as a default starting point for layout
  //               opportunities.
  NGLayoutOpportunityIterator(const NGExclusions* exclusions,
                              const NGLogicalSize& available_size,
                              const NGLogicalOffset& offset);

  // @return If there is no more opportunities to iterate.
  //         This also means that the last returned opportunity does not have
  //         exclusions, because the iterator adds an opportunity of the
  //         initial available size at the end.
  bool IsAtEnd() const { return opportunity_iter_ == opportunities_.end(); }

  // Gets the next Layout Opportunity or empty one if the search is exhausted.
  // TODO(chrome-layout-team): Refactor with using C++ <iterator> library.
  // TODO(glebl): Refactor the iterator to return unique_ptr here.
  const NGLayoutOpportunity Next();

  // Offset that specifies the starting point to search layout opportunities.
  // It's either {@code opt_offset} or space->BfcOffset().
  NGLogicalOffset Offset() const { return offset_; }

#ifndef NDEBUG
  // Prints Layout Opportunity tree for debug purposes.
  void ShowLayoutOpportunityTree() const;
#endif

 private:
  NGLayoutOpportunities opportunities_;
  NGLayoutOpportunities::const_iterator opportunity_iter_;
  NGLogicalOffset offset_;
};

}  // namespace blink

#endif  // NGLayoutOpportunityIterator_h
