// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "element_sanitizer.h"

#include "third_party/blink/renderer/modules/sanitizer_api/sanitizer.h"

namespace blink {

void ElementSanitizer::setHTML(ScriptState* script_state,
                               Element& element,
                               const String& markup,
                               Sanitizer* sanitizer,
                               ExceptionState& exception_state) {
  sanitizer->ElementSetHTML(script_state, element, markup, exception_state);
}

}  // namespace blink
