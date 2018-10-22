// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"

#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

TrustedHTML::TrustedHTML(const String& html) : html_(html) {}

TrustedHTML* TrustedHTML::escape(ScriptState* script_state,
                                 const String& html) {
  // TODO(mkwst): This could be hugely optimized by scanning the string for any
  // of the interesting characters to see whether we need to do any replacement
  // at all, and by replacing all the characters in a single pass.
  String escapedHTML(html);
  escapedHTML.Replace("&", "&amp;")
      .Replace("<", "&lt;")
      .Replace(">", "&gt;")
      .Replace("\"", "&quot;")
      .Replace("'", "&#39;");

  return TrustedHTML::unsafelyCreate(script_state, escapedHTML);
}

TrustedHTML* TrustedHTML::unsafelyCreate(ScriptState* script_state,
                                         const String& html) {
  return TrustedHTML::Create(html);
}

String TrustedHTML::GetString(StringOrTrustedHTML stringOrHTML,
                              const Document* doc,
                              ExceptionState& exception_state) {
  DCHECK(stringOrHTML.IsString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
  DCHECK(!stringOrHTML.IsNull());

  if (!stringOrHTML.IsTrustedHTML() && doc && doc->RequireTrustedTypes()) {
    exception_state.ThrowTypeError(
        "This document requires `TrustedHTML` assignment.");
    return g_empty_string;
  }

  String markup = stringOrHTML.IsString()
                      ? stringOrHTML.GetAsString()
                      : stringOrHTML.GetAsTrustedHTML()->toString();
  return markup;
}

String TrustedHTML::toString() const {
  return html_;
}

}  // namespace blink
