// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutOpportunityIterator_h
#define NGLayoutOpportunityIterator_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_layout_opportunity_tree_node.h"
#include "core/layout/ng/ng_units.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"
#include "wtf/Vector.h"

namespace blink {

typedef NGLogicalRect NGLayoutOpportunity;
typedef Vector<NGLayoutOpportunity> NGLayoutOpportunities;

class CORE_EXPORT NGLayoutOpportunityIterator final
    : public GarbageCollectedFinalized<NGLayoutOpportunityIterator> {
 public:
  NGLayoutOpportunityIterator(NGConstraintSpace* space,
                              unsigned clear,
                              bool for_inline_or_bfc);

  // Gets the next Layout Opportunity or nullptr if the search is exhausted.
  // TODO(chrome-layout-team): Refactor with using C++ <iterator> library.
  const NGLayoutOpportunity Next();

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(constraint_space_);
    visitor->trace(opportunity_tree_root_);
  }

 private:
  // Mutable Getters.
  NGLayoutOpportunityTreeNode* MutableOpportunityTreeRoot() {
    return opportunity_tree_root_.get();
  }

  // Read-only Getters.
  const NGLayoutOpportunityTreeNode* OpportunityTreeRoot() const {
    return opportunity_tree_root_.get();
  }

  Member<NGConstraintSpace> constraint_space_;

  NGLayoutOpportunities opportunities_;
  NGLayoutOpportunities::const_iterator opportunity_iter_;
  Member<NGLayoutOpportunityTreeNode> opportunity_tree_root_;
};

}  // namespace blink

#endif  // NGLayoutOpportunityIterator_h
