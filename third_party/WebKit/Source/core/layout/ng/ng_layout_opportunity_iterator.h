// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutOpportunityIterator_h
#define NGLayoutOpportunityIterator_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_layout_opportunity_tree_node.h"
#include "wtf/Optional.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

class NGConstraintSpace;
typedef NGLogicalRect NGLayoutOpportunity;
typedef Vector<NGLayoutOpportunity> NGLayoutOpportunities;

class CORE_EXPORT NGLayoutOpportunityIterator final {
 public:
  // Default constructor.
  //
  // @param space Constraint space with exclusions for which this iterator needs
  //              to generate layout opportunities.
  // @param opt_offset Optional offset parameter that is used as a
  //                   default start point for layout opportunities.
  // @param opt_leader_point Optional 'leader' parameter that is used to specify
  //                         the ending point of temporary excluded rectangle
  //                         which starts from 'origin'. This rectangle may
  //                         represent a text fragment for example.
  NGLayoutOpportunityIterator(
      const NGConstraintSpace* space,
      const NGLogicalSize& available_size,
      const WTF::Optional<NGLogicalOffset>& opt_offset = WTF::nullopt,
      const WTF::Optional<NGLogicalOffset>& opt_leader_point = WTF::nullopt);

  // Gets the next Layout Opportunity or nullptr if the search is exhausted.
  // TODO(chrome-layout-team): Refactor with using C++ <iterator> library.
  const NGLayoutOpportunity Next();

  // Offset that specifies the starting point to search layout opportunities.
  // It's either {@code opt_offset} or space->BfcOffset().
  NGLogicalOffset Offset() const { return offset_; }

#ifndef NDEBUG
  // Prints Layout Opportunity tree for debug purposes.
  void ShowLayoutOpportunityTree() const;
#endif

 private:
  // Mutable Getters.
  NGLayoutOpportunityTreeNode* MutableOpportunityTreeRoot() {
    return opportunity_tree_root_.get();
  }

  // Read-only Getters.
  const NGLayoutOpportunityTreeNode* OpportunityTreeRoot() const {
    return opportunity_tree_root_.get();
  }

  const NGConstraintSpace* constraint_space_;

  NGLayoutOpportunities opportunities_;
  NGLayoutOpportunities::const_iterator opportunity_iter_;
  std::unique_ptr<NGLayoutOpportunityTreeNode> opportunity_tree_root_;
  NGLogicalOffset offset_;
};

}  // namespace blink

#endif  // NGLayoutOpportunityIterator_h
