// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLDataElement.h"

#include "core/frame/UseCounter.h"

namespace blink {

HTMLDataElement::HTMLDataElement(Document& document)
    : HTMLElement(HTMLNames::dataTag, document) {
  UseCounter::Count(document, WebFeature::kDataElement);
}

HTMLDataElement* HTMLDataElement::Create(Document& document) {
  return new HTMLDataElement(document);
}

}  // namespace blink
