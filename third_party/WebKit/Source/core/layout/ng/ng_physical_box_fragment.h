// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalBoxFragment_h
#define NGPhysicalBoxFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/geometry/ng_margin_strut.h"
#include "core/layout/ng/ng_floating_object.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "wtf/Optional.h"

namespace blink {

struct NGFloatingObject;

class CORE_EXPORT NGPhysicalBoxFragment final : public NGPhysicalFragment {
 public:
  // This modifies the passed-in children vector.
  NGPhysicalBoxFragment(LayoutObject* layout_object,
                        NGPhysicalSize size,
                        NGPhysicalSize overflow,
                        Vector<RefPtr<NGPhysicalFragment>>& children,
                        Vector<RefPtr<NGFloatingObject>>& positioned_floats,
                        const WTF::Optional<NGLogicalOffset>& bfc_offset,
                        const NGMarginStrut& end_margin_strut,
                        RefPtr<NGBreakToken> break_token = nullptr);

  // Returns the total size, including the contents outside of the border-box.
  LayoutUnit WidthOverflow() const { return overflow_.width; }
  LayoutUnit HeightOverflow() const { return overflow_.height; }

  const Vector<RefPtr<NGPhysicalFragment>>& Children() const {
    return children_;
  }

  // List of positioned floats that need to be copied to the old layout tree.
  // TODO(layout-ng): remove this once we change painting code to handle floats
  // differently.
  const Vector<RefPtr<NGFloatingObject>>& PositionedFloats() const {
    return positioned_floats_;
  }

  const WTF::Optional<NGLogicalOffset>& BfcOffset() const {
    return bfc_offset_;
  }

  const NGMarginStrut& EndMarginStrut() const { return end_margin_strut_; }

 private:
  NGPhysicalSize overflow_;
  Vector<RefPtr<NGPhysicalFragment>> children_;
  Vector<RefPtr<NGFloatingObject>> positioned_floats_;
  const WTF::Optional<NGLogicalOffset> bfc_offset_;
  const NGMarginStrut end_margin_strut_;
};

DEFINE_TYPE_CASTS(NGPhysicalBoxFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentBox);

}  // namespace blink

#endif  // NGPhysicalBoxFragment_h
