// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/element_internals.h"

#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

ElementInternals::ElementInternals(HTMLElement& target) : target_(target) {}

void ElementInternals::Trace(Visitor* visitor) {
  visitor->Trace(target_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
