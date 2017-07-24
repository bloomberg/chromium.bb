// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLUnknownElement.h"

#include "core/frame/UseCounter.h"

namespace blink {

HTMLUnknownElement::HTMLUnknownElement(const QualifiedName& tag_name,
                                       Document& document)
    : HTMLElement(tag_name, document) {
  if (tag_name.LocalName() == "time")
    UseCounter::Count(document, WebFeature::kTimeElement);
}

}  // namespace blink
