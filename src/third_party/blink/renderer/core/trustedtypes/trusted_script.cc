// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"

#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

TrustedScript::TrustedScript(const String& script) : script_(script) {}

String TrustedScript::GetString(StringOrTrustedScript string_or_trusted_script,
                                const Document* doc,
                                ExceptionState& exception_state) {
  DCHECK(string_or_trusted_script.IsString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
  DCHECK(!string_or_trusted_script.IsNull());

  if (!string_or_trusted_script.IsTrustedScript() && doc &&
      doc->RequireTrustedTypes()) {
    exception_state.ThrowTypeError(
        "This document requires `TrustedScript` assignment.");
    return g_empty_string;
  }

  String markup =
      string_or_trusted_script.IsString()
          ? string_or_trusted_script.GetAsString()
          : string_or_trusted_script.GetAsTrustedScript()->toString();
  return markup;
}

String TrustedScript::toString() const {
  return script_;
}

}  // namespace blink
