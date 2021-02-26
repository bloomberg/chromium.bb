// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_text_offset.h"

#include <ostream>

namespace blink {

std::ostream& operator<<(std::ostream& ostream, const NGTextOffset& offset) {
  return ostream << "{" << offset.start << ", " << offset.end << "}";
}

}  // namespace blink
