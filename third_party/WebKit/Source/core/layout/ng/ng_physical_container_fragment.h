// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalContainerFragment_h
#define NGPhysicalContainerFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/inline/ng_baseline.h"
#include "core/layout/ng/ng_physical_fragment.h"

namespace blink {

class CORE_EXPORT NGPhysicalContainerFragment : public NGPhysicalFragment {
 public:
  const Vector<RefPtr<NGPhysicalFragment>>& Children() const {
    return children_;
  }

 protected:
  // This modifies the passed-in children vector.
  NGPhysicalContainerFragment(LayoutObject*,
                              const ComputedStyle&,
                              NGPhysicalSize,
                              NGFragmentType,
                              Vector<RefPtr<NGPhysicalFragment>>& children,
                              RefPtr<NGBreakToken> = nullptr);

  Vector<RefPtr<NGPhysicalFragment>> children_;
};

DEFINE_TYPE_CASTS(NGPhysicalContainerFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->IsContainer(),
                  fragment.IsContainer());

}  // namespace blink

#endif  // NGPhysicalContainerFragment_h
