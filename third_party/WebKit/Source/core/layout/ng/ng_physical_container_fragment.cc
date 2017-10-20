// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_physical_container_fragment.h"

namespace blink {

NGPhysicalContainerFragment::NGPhysicalContainerFragment(
    LayoutObject* layout_object,
    const ComputedStyle& style,
    NGPhysicalSize size,
    NGFragmentType type,
    Vector<scoped_refptr<NGPhysicalFragment>>& children,
    scoped_refptr<NGBreakToken> break_token)
    : NGPhysicalFragment(layout_object,
                         style,
                         size,
                         type,
                         std::move(break_token)),
      children_(std::move(children)) {
  DCHECK(children.IsEmpty());  // Ensure move semantics is used.
}

}  // namespace blink
