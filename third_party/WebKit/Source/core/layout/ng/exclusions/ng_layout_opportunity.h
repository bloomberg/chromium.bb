// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLayoutOpportunity_h
#define NGLayoutOpportunity_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_bfc_rect.h"

namespace blink {

struct CORE_EXPORT NGLayoutOpportunity {
  NGLayoutOpportunity()
      : rect(NGBfcOffset(LayoutUnit::Min(), LayoutUnit::Min()),
             NGBfcOffset(LayoutUnit::Max(), LayoutUnit::Max())) {}
  explicit NGLayoutOpportunity(const NGBfcRect& rect) : rect(rect) {}

  // Rectangle in BFC coordinates that represents this opportunity.
  NGBfcRect rect;

  // TODO(ikilpatrick): This will also need to hold all the adjacent exclusions
  // on each edge for implementing css-shapes correctly. It'll need to have
  // methods which use these adjacent exclusions to determine the available
  // inline-size for a line-box.
};

}  // namespace blink

#endif  // NGLayoutOpportunity_h
