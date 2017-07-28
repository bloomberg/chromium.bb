// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebMetaElement.h"

#include "core/HTMLNames.h"
#include "core/html/HTMLMetaElement.h"
#include "platform/wtf/PassRefPtr.h"
#include "public/platform/WebString.h"

namespace blink {

WebString WebMetaElement::ComputeEncoding() const {
  return String(ConstUnwrap<HTMLMetaElement>()->ComputeEncoding().GetName());
}

WebMetaElement::WebMetaElement(HTMLMetaElement* element)
    : WebElement(element) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebMetaElement,
                           isHTMLMetaElement(ConstUnwrap<Node>()));

WebMetaElement& WebMetaElement::operator=(HTMLMetaElement* element) {
  private_ = element;
  return *this;
}

WebMetaElement::operator HTMLMetaElement*() const {
  return toHTMLMetaElement(private_.Get());
}

}  // namespace blink
