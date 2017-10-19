// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/credentialmanager/Credential.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

Credential::~Credential() {}

Credential::Credential(PlatformCredential* credential)
    : platform_credential_(credential) {}

Credential::Credential(const String& id)
    : platform_credential_(PlatformCredential::Create(id)) {}

KURL Credential::ParseStringAsURL(const String& url,
                                  ExceptionState& exception_state) {
  if (url.IsEmpty())
    return KURL();
  KURL parsed_url = KURL(NullURL(), url);
  if (!parsed_url.IsValid())
    exception_state.ThrowDOMException(kSyntaxError,
                                      "'" + url + "' is not a valid URL.");
  return parsed_url;
}

void Credential::Trace(blink::Visitor* visitor) {
  visitor->Trace(platform_credential_);
}

}  // namespace blink
