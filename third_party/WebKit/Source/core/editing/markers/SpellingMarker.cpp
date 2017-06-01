// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/SpellingMarker.h"

namespace blink {

SpellingMarker::SpellingMarker(unsigned start_offset,
                               unsigned end_offset,
                               const String& description)
    : SpellCheckMarker(DocumentMarker::kSpelling,
                       start_offset,
                       end_offset,
                       description) {
  DCHECK_LT(start_offset, end_offset);
}

}  // namespace blink
