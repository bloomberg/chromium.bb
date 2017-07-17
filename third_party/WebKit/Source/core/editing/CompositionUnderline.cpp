// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/CompositionUnderline.h"

#include <algorithm>
#include "public/web/WebCompositionUnderline.h"

namespace blink {

CompositionUnderline::CompositionUnderline(unsigned start_offset,
                                           unsigned end_offset,
                                           const Color& color,
                                           bool thick,
                                           const Color& background_color)
    : color_(color), thick_(thick), background_color_(background_color) {
  // Sanitize offsets by ensuring a valid range corresponding to the last
  // possible position.
  // TODO(wkorman): Consider replacing with DCHECK_LT(startOffset, endOffset).
  start_offset_ =
      std::min(start_offset, std::numeric_limits<unsigned>::max() - 1u);
  end_offset_ = std::max(start_offset_ + 1u, end_offset);
}

CompositionUnderline::CompositionUnderline(
    const WebCompositionUnderline& underline)
    : CompositionUnderline(underline.start_offset,
                           underline.end_offset,
                           Color(underline.color),
                           underline.thick,
                           Color(underline.background_color)) {}
}  // namespace blink
