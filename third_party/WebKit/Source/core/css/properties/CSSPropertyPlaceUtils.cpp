// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyPlaceUtils.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

bool CSSPropertyPlaceUtils::ConsumePlaceAlignment(
    CSSParserTokenRange& range,
    ConsumePlaceAlignmentValue consume_alignment_value,
    CSSValue*& align_value,
    CSSValue*& justify_value) {
  DCHECK(consume_alignment_value);
  DCHECK(!align_value);
  DCHECK(!justify_value);

  align_value = consume_alignment_value(range);
  if (!align_value)
    return false;

  justify_value = range.AtEnd() ? align_value : consume_alignment_value(range);

  return justify_value && range.AtEnd();
}

}  // namespace blink
