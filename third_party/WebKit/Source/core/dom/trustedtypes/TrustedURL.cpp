// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/trustedtypes/TrustedURL.h"

#include "core/dom/ExecutionContext.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

TrustedURL::TrustedURL(const KURL& url) : url_(url) {}

TrustedURL* TrustedURL::create(ScriptState* script_state, const String& url) {
  KURL result(ExecutionContext::From(script_state)->CompleteURL(url));

  if (!result.IsValid() || !result.ProtocolIsInHTTPFamily())
    result = KURL("about:invalid");

  return TrustedURL::Create(result);
}

TrustedURL* TrustedURL::unsafelyCreate(ScriptState* script_state,
                                       const String& url) {
  return TrustedURL::Create(
      ExecutionContext::From(script_state)->CompleteURL(url));
}

String TrustedURL::toString() const {
  return url_.GetString();
}

}  // namespace blink
