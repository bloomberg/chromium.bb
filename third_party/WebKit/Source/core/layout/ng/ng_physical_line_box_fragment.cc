// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_line_box_fragment.h"

#include "core/layout/ng/ng_floating_object.h"

namespace blink {

NGPhysicalLineBoxFragment::NGPhysicalLineBoxFragment(
    NGPhysicalSize size,
    Vector<RefPtr<NGPhysicalFragment>>& children,
    const NGLineHeightMetrics& metrics,
    RefPtr<NGBreakToken> break_token)
    : NGPhysicalFragment(nullptr,
                         size,
                         kFragmentLineBox,
                         std::move(break_token)),
      metrics_(metrics) {
  children_.swap(children);
}

}  // namespace blink
