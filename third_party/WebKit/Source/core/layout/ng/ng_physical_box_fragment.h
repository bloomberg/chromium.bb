// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalBoxFragment_h
#define NGPhysicalBoxFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/inline/ng_baseline.h"
#include "core/layout/ng/ng_physical_fragment.h"

namespace blink {

class CORE_EXPORT NGPhysicalBoxFragment final : public NGPhysicalFragment {
 public:
  // This modifies the passed-in children vector.
  NGPhysicalBoxFragment(LayoutObject* layout_object,
                        const ComputedStyle& style,
                        NGPhysicalSize size,
                        NGPhysicalSize overflow,
                        Vector<RefPtr<NGPhysicalFragment>>& children,
                        Vector<NGBaseline>& baselines,
                        unsigned,  // NGBorderEdges::Physical
                        RefPtr<NGBreakToken> break_token = nullptr);

  // Returns the total size, including the contents outside of the border-box.
  NGPhysicalSize OverflowSize() const { return overflow_; }

  const Vector<RefPtr<NGPhysicalFragment>>& Children() const {
    return children_;
  }

  const NGBaseline* Baseline(const NGBaselineRequest&) const;

  RefPtr<NGPhysicalFragment> CloneWithoutOffset() const;

 private:
  NGPhysicalSize overflow_;
  Vector<RefPtr<NGPhysicalFragment>> children_;
  Vector<NGBaseline> baselines_;
};

DEFINE_TYPE_CASTS(NGPhysicalBoxFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->Type() == NGPhysicalFragment::kFragmentBox,
                  fragment.Type() == NGPhysicalFragment::kFragmentBox);

}  // namespace blink

#endif  // NGPhysicalBoxFragment_h
