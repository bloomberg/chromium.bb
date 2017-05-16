// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_item_result.h"

namespace blink {

NGInlineItemResult::NGInlineItemResult() {}

NGInlineItemResult::NGInlineItemResult(unsigned index,
                                       unsigned start,
                                       unsigned end)
    : item_index(index), start_offset(start), end_offset(end) {}

}  // namespace blink
