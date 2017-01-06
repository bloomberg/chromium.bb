// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalBoxFragment_h
#define NGPhysicalBoxFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_units.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "platform/heap/Handle.h"

namespace blink {

class NGBlockNode;

class CORE_EXPORT NGPhysicalBoxFragment final : public NGPhysicalFragment {
 public:
  // This modifies the passed-in children vector.
  NGPhysicalBoxFragment(
      NGPhysicalSize size,
      NGPhysicalSize overflow,
      HeapVector<Member<const NGPhysicalFragment>>& children,
      HeapLinkedHashSet<WeakMember<NGBlockNode>>& out_of_flow_descendants,
      Vector<NGStaticPosition>& out_of_flow_positions,
      NGMarginStrut margin_strut);

  const HeapVector<Member<const NGPhysicalFragment>>& Children() const {
    return children_;
  }

  NGMarginStrut MarginStrut() const { return margin_strut_; }

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  HeapVector<Member<const NGPhysicalFragment>> children_;
  NGMarginStrut margin_strut_;
};

WILL_NOT_BE_EAGERLY_TRACED_CLASS(NGPhysicalBoxFragment);

DEFINE_TYPE_CASTS(NGPhysicalBoxFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentBox);

}  // namespace blink

#endif  // NGPhysicalBoxFragment_h
