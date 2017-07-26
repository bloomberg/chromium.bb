// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_offset_mapping_result.h"

namespace blink {

NGOffsetMappingUnit::NGOffsetMappingUnit(NGOffsetMappingUnitType type,
                                         const LayoutText* owner,
                                         unsigned dom_start,
                                         unsigned dom_end,
                                         unsigned text_content_start,
                                         unsigned text_content_end)
    : type_(type),
      owner_(owner),
      dom_start_(dom_start),
      dom_end_(dom_end),
      text_content_start_(text_content_start),
      text_content_end_(text_content_end) {}

}  // namespace blink
