// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"

#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html_or_trusted_script_or_trusted_script_url_or_trusted_url.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/usv_string_or_trusted_url.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_url.h"

namespace blink {

String GetStringFromTrustedType(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL&
        string_or_trusted_type,
    const Document* doc,
    ExceptionState& exception_state) {
  DCHECK(string_or_trusted_type.IsString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
  DCHECK(!string_or_trusted_type.IsNull());

  if (string_or_trusted_type.IsString() && doc && doc->RequireTrustedTypes()) {
    exception_state.ThrowTypeError(
        "This document requires a Trusted Type assignment.");
    return g_empty_string;
  }

  if (string_or_trusted_type.IsTrustedHTML())
    return string_or_trusted_type.GetAsTrustedHTML()->toString();
  if (string_or_trusted_type.IsTrustedScript())
    return string_or_trusted_type.GetAsTrustedScript()->toString();
  if (string_or_trusted_type.IsTrustedScriptURL())
    return string_or_trusted_type.GetAsTrustedScriptURL()->toString();
  if (string_or_trusted_type.IsTrustedURL())
    return string_or_trusted_type.GetAsTrustedURL()->toString();

  return string_or_trusted_type.GetAsString();
}

String GetStringFromTrustedTypeWithoutCheck(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL&
        string_or_trusted_type) {
  if (string_or_trusted_type.IsTrustedHTML())
    return string_or_trusted_type.GetAsTrustedHTML()->toString();
  if (string_or_trusted_type.IsTrustedScript())
    return string_or_trusted_type.GetAsTrustedScript()->toString();
  if (string_or_trusted_type.IsTrustedScriptURL())
    return string_or_trusted_type.GetAsTrustedScriptURL()->toString();
  if (string_or_trusted_type.IsTrustedURL())
    return string_or_trusted_type.GetAsTrustedURL()->toString();
  if (string_or_trusted_type.IsString())
    return string_or_trusted_type.GetAsString();

  return g_empty_string;
}

String GetStringFromTrustedHTML(StringOrTrustedHTML string_or_trusted_html,
                                const Document* doc,
                                ExceptionState& exception_state) {
  DCHECK(string_or_trusted_html.IsString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
  DCHECK(!string_or_trusted_html.IsNull());

  if (!string_or_trusted_html.IsTrustedHTML() && doc &&
      doc->RequireTrustedTypes()) {
    exception_state.ThrowTypeError(
        "This document requires `TrustedHTML` assignment.");
    return g_empty_string;
  }

  String markup = string_or_trusted_html.IsString()
                      ? string_or_trusted_html.GetAsString()
                      : string_or_trusted_html.GetAsTrustedHTML()->toString();
  return markup;
}

String GetStringFromTrustedScript(
    StringOrTrustedScript string_or_trusted_script,
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

String GetStringFromTrustedScriptURL(
    StringOrTrustedScriptURL string_or_trusted_script_url,
    const Document* doc,
    ExceptionState& exception_state) {
  DCHECK(string_or_trusted_script_url.IsString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
  DCHECK(!string_or_trusted_script_url.IsNull());

  if (!string_or_trusted_script_url.IsTrustedScriptURL() && doc &&
      doc->RequireTrustedTypes()) {
    exception_state.ThrowTypeError(
        "This document requires `TrustedScriptURL` assignment.");
    return g_empty_string;
  }

  String markup =
      string_or_trusted_script_url.IsString()
          ? string_or_trusted_script_url.GetAsString()
          : string_or_trusted_script_url.GetAsTrustedScriptURL()->toString();
  return markup;
}

String GetStringFromTrustedURL(USVStringOrTrustedURL string_or_trusted_url,
                               const Document* doc,
                               ExceptionState& exception_state) {
  DCHECK(string_or_trusted_url.IsUSVString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled());
  DCHECK(!string_or_trusted_url.IsNull());

  if (!string_or_trusted_url.IsTrustedURL() && doc &&
      doc->RequireTrustedTypes()) {
    exception_state.ThrowTypeError(
        "This document requires `TrustedURL` assignment.");
    return g_empty_string;
  }

  String markup = string_or_trusted_url.IsUSVString()
                      ? string_or_trusted_url.GetAsUSVString()
                      : string_or_trusted_url.GetAsTrustedURL()->toString();
  return markup;
}
}  // namespace blink
