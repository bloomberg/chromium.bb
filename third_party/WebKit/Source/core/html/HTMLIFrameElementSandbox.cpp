// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLIFrameElementSandbox.h"

#include "core/html/HTMLIFrameElement.h"

namespace blink {

namespace {

const char* const kSupportedSandboxTokens[] = {"allow-forms",
                                               "allow-modals",
                                               "allow-pointer-lock",
                                               "allow-popups",
                                               "allow-popups-to-escape-sandbox",
                                               "allow-same-origin",
                                               "allow-scripts",
                                               "allow-top-navigation"};

bool IsTokenSupported(const AtomicString& token) {
  for (const char* supported_token : kSupportedSandboxTokens) {
    if (token == supported_token)
      return true;
  }
  return false;
}

}  // namespace

HTMLIFrameElementSandbox::HTMLIFrameElementSandbox(HTMLIFrameElement* element)
    : DOMTokenList(*element, HTMLNames::sandboxAttr) {}

bool HTMLIFrameElementSandbox::ValidateTokenValue(
    const AtomicString& token_value,
    ExceptionState&) const {
  return IsTokenSupported(token_value);
}

}  // namespace blink
