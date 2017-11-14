// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalContainerFragment_h
#define NGPhysicalContainerFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "core/layout/ng/inline/ng_baseline.h"
#include "core/layout/ng/ng_physical_fragment.h"

namespace blink {

class CORE_EXPORT NGPhysicalContainerFragment : public NGPhysicalFragment {
 public:
  const Vector<scoped_refptr<NGPhysicalFragment>>& Children() const {
    return children_;
  }

  // Visual rect of children in the local coordinate.
  const NGPhysicalOffsetRect& ContentsVisualRect() const {
    return contents_visual_rect_;
  }

 protected:
  // This modifies the passed-in children vector.
  NGPhysicalContainerFragment(
      LayoutObject*,
      const ComputedStyle&,
      NGPhysicalSize,
      NGFragmentType,
      Vector<scoped_refptr<NGPhysicalFragment>>& children,
      const NGPhysicalOffsetRect& contents_visual_rect,
      scoped_refptr<NGBreakToken> = nullptr);

  Vector<scoped_refptr<NGPhysicalFragment>> children_;
  NGPhysicalOffsetRect contents_visual_rect_;
};

DEFINE_TYPE_CASTS(NGPhysicalContainerFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->IsContainer(),
                  fragment.IsContainer());

}  // namespace blink

#endif  // NGPhysicalContainerFragment_h
