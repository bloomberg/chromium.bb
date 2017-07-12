// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalBoxFragment_h
#define NGPhysicalBoxFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/geometry/ng_margin_strut.h"
#include "core/layout/ng/inline/ng_baseline.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_positioned_float.h"
#include "platform/wtf/Optional.h"

namespace blink {

class CORE_EXPORT NGPhysicalBoxFragment final : public NGPhysicalFragment {
 public:
  // This modifies the passed-in children vector.
  NGPhysicalBoxFragment(LayoutObject* layout_object,
                        NGPhysicalSize size,
                        NGPhysicalSize overflow,
                        Vector<RefPtr<NGPhysicalFragment>>& children,
                        Vector<NGPositionedFloat>& positioned_floats,
                        Vector<NGBaseline>& baselines,
                        unsigned,  // NGBorderEdges::Physical
                        RefPtr<NGBreakToken> break_token = nullptr);

  // Returns the total size, including the contents outside of the border-box.
  NGPhysicalSize OverflowSize() const { return overflow_; }

  const Vector<RefPtr<NGPhysicalFragment>>& Children() const {
    return children_;
  }

  // List of positioned floats that need to be copied to the old layout tree.
  // TODO(layout-ng): remove this once we change painting code to handle floats
  // differently.
  const Vector<NGPositionedFloat>& PositionedFloats() const {
    return positioned_floats_;
  }

  const NGBaseline* Baseline(const NGBaselineRequest&) const;

 private:
  NGPhysicalSize overflow_;
  Vector<RefPtr<NGPhysicalFragment>> children_;
  Vector<NGPositionedFloat> positioned_floats_;
  Vector<NGBaseline> baselines_;
};

DEFINE_TYPE_CASTS(NGPhysicalBoxFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentBox);

}  // namespace blink

#endif  // NGPhysicalBoxFragment_h
