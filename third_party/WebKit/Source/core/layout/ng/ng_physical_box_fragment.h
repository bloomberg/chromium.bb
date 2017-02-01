// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalBoxFragment_h
#define NGPhysicalBoxFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_units.h"
#include "platform/heap/Handle.h"
#include "wtf/Optional.h"

namespace blink {

class NGBlockNode;
struct NGFloatingObject;

class CORE_EXPORT NGPhysicalBoxFragment final : public NGPhysicalFragment {
 public:
  // This modifies the passed-in children vector.
  NGPhysicalBoxFragment(
      LayoutObject* layout_object,
      NGPhysicalSize size,
      NGPhysicalSize overflow,
      HeapVector<Member<NGPhysicalFragment>>& children,
      HeapLinkedHashSet<WeakMember<NGBlockNode>>& out_of_flow_descendants,
      Vector<NGStaticPosition>& out_of_flow_positions,
      HeapVector<Member<NGFloatingObject>>& unpositioned_floats,
      HeapVector<Member<NGFloatingObject>>& positioned_floats,
      const WTF::Optional<NGLogicalOffset>& bfc_offset,
      const NGMarginStrut& end_margin_strut,
      NGBreakToken* break_token = nullptr);

  const HeapVector<Member<NGPhysicalFragment>>& Children() const {
    return children_;
  }

  const WTF::Optional<NGLogicalOffset>& BfcOffset() const {
    return bfc_offset_;
  }

  const NGMarginStrut& EndMarginStrut() const { return end_margin_strut_; }

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  HeapVector<Member<NGPhysicalFragment>> children_;
  const WTF::Optional<NGLogicalOffset> bfc_offset_;
  const NGMarginStrut end_margin_strut_;
};

WILL_NOT_BE_EAGERLY_TRACED_CLASS(NGPhysicalBoxFragment);

DEFINE_TYPE_CASTS(NGPhysicalBoxFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentBox);

}  // namespace blink

#endif  // NGPhysicalBoxFragment_h
