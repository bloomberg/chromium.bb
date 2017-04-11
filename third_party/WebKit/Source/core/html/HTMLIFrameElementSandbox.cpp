// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElementSandbox.h"

#include "core/html/HTMLIFrameElement.h"

namespace blink {

namespace {

const char* const kSupportedTokens[] = {"allow-forms",
                                        "allow-modals",
                                        "allow-pointer-lock",
                                        "allow-popups",
                                        "allow-popups-to-escape-sandbox",
                                        "allow-same-origin",
                                        "allow-scripts",
                                        "allow-top-navigation"};

bool IsTokenSupported(const AtomicString& token) {
  for (const char* supported_token : kSupportedTokens) {
    if (token == supported_token)
      return true;
  }
  return false;
}

}  // namespace

HTMLIFrameElementSandbox::HTMLIFrameElementSandbox(HTMLIFrameElement* element)
    : DOMTokenList(this), element_(element) {}

HTMLIFrameElementSandbox::~HTMLIFrameElementSandbox() {}

DEFINE_TRACE(HTMLIFrameElementSandbox) {
  visitor->Trace(element_);
  DOMTokenList::Trace(visitor);
  DOMTokenListObserver::Trace(visitor);
}

bool HTMLIFrameElementSandbox::ValidateTokenValue(
    const AtomicString& token_value,
    ExceptionState&) const {
  return IsTokenSupported(token_value);
}

void HTMLIFrameElementSandbox::ValueWasSet() {
  element_->SandboxValueWasSet();
}

}  // namespace blink
