// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLegacyBlockLayoutAlgorithm_h
#define NGLegacyBlockLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "wtf/RefPtr.h"

namespace blink {

// A class for legacy block layout (blocks that can't be handled by NG yet).
// Defers to the old layout method of the block
class CORE_EXPORT NGLegacyBlockLayoutAlgorithm : public NGLayoutAlgorithm {
 public:
  NGLegacyBlockLayoutAlgorithm(NGBlockNode*, const NGConstraintSpace*);

  NGLayoutStatus Layout(NGPhysicalFragmentBase*,
                        NGPhysicalFragmentBase**,
                        NGLayoutAlgorithm**) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  Member<NGBlockNode> block_;
  Member<const NGConstraintSpace> constraint_space_;
};
}

#endif  // NGLegacyBlockLayoutAlgorithm_h
