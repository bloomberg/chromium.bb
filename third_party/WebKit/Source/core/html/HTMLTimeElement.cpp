// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLTimeElement.h"

#include "core/frame/UseCounter.h"

namespace blink {

HTMLTimeElement::HTMLTimeElement(Document& document)
    : HTMLElement(HTMLNames::timeTag, document) {
  UseCounter::Count(document, WebFeature::kTimeElement);
}

HTMLTimeElement* HTMLTimeElement::Create(Document& document) {
  return new HTMLTimeElement(document);
}

}  // namespace blink
