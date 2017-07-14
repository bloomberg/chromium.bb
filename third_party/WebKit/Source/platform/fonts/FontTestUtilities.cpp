// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontTestUtilities.h"

namespace blink {

String To16Bit(const char* text, unsigned length) {
  return String::Make16BitFrom8BitSource(reinterpret_cast<const LChar*>(text),
                                         length);
}

}  // namespace blink
