// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/ImeTextSpan.h"

#include <algorithm>
#include "public/web/WebImeTextSpan.h"

namespace blink {

ImeTextSpan::ImeTextSpan(unsigned start_offset,
                         unsigned end_offset,
                         const Color& underline_color,
                         bool thick,
                         const Color& background_color)
    : underline_color_(underline_color),
      thick_(thick),
      background_color_(background_color) {
  // Sanitize offsets by ensuring a valid range corresponding to the last
  // possible position.
  // TODO(wkorman): Consider replacing with DCHECK_LT(startOffset, endOffset).
  start_offset_ =
      std::min(start_offset, std::numeric_limits<unsigned>::max() - 1u);
  end_offset_ = std::max(start_offset_ + 1u, end_offset);
}

ImeTextSpan::ImeTextSpan(const WebImeTextSpan& ime_text_span)
    : ImeTextSpan(ime_text_span.start_offset,
                  ime_text_span.end_offset,
                  Color(ime_text_span.underline_color),
                  ime_text_span.thick,
                  Color(ime_text_span.background_color)) {}

}  // namespace blink
