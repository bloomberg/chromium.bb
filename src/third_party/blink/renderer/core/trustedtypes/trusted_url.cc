// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_url.h"

#include "third_party/blink/renderer/bindings/core/v8/usv_string_or_trusted_url.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

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

String TrustedURL::GetString(USVStringOrTrustedURL stringOrURL,
                             const Document* doc,
                             ExceptionState& exception_state) {
  DCHECK(stringOrURL.IsUSVString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
  DCHECK(!stringOrURL.IsNull());

  if (!stringOrURL.IsTrustedURL() && doc && doc->RequireTrustedTypes()) {
    exception_state.ThrowTypeError(
        "This document requires `TrustedURL` assignment.");
    return g_empty_string;
  }

  String markup = stringOrURL.IsUSVString()
                      ? stringOrURL.GetAsUSVString()
                      : stringOrURL.GetAsTrustedURL()->toString();
  return markup;
}

String TrustedURL::toString() const {
  return url_.GetString();
}

}  // namespace blink
