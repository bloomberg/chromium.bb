// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLIFrameElementSandbox_h
#define HTMLIFrameElementSandbox_h

#include "core/dom/DOMTokenList.h"
#include "platform/heap/Handle.h"

namespace blink {

class HTMLIFrameElement;

class HTMLIFrameElementSandbox final : public DOMTokenList {
 public:
  static HTMLIFrameElementSandbox* Create(HTMLIFrameElement* element) {
    return new HTMLIFrameElementSandbox(element);
  }

 private:
  explicit HTMLIFrameElementSandbox(HTMLIFrameElement*);
  bool ValidateTokenValue(const AtomicString&, ExceptionState&) const override;
};

}  // namespace blink

#endif
