// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"

#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/mojom/reporting/reporting.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html_or_trusted_script_or_trusted_script_url_or_trusted_url.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/usv_string_or_trusted_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_url.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

enum TrustedTypeViolationKind {
  kAnyTrustedTypeAssignment,
  kTrustedHTMLAssignment,
  kTrustedScriptAssignment,
  kTrustedURLAssignment,
  kTrustedScriptURLAssignment,
  kTrustedHTMLAssignmentAndDefaultPolicyFailed,
  kTrustedScriptAssignmentAndDefaultPolicyFailed,
  kTrustedURLAssignmentAndDefaultPolicyFailed,
  kTrustedScriptURLAssignmentAndDefaultPolicyFailed,
};

const char* GetMessage(TrustedTypeViolationKind kind) {
  switch (kind) {
    case kAnyTrustedTypeAssignment:
      return "This document requires any trusted type assignment.";
    case kTrustedHTMLAssignment:
      return "This document requires 'TrustedHTML' assignment.";
    case kTrustedScriptAssignment:
      return "This document requires 'TrustedScript' assignment.";
    case kTrustedURLAssignment:
      return "This document requires 'TrustedURL' assignment.";
    case kTrustedScriptURLAssignment:
      return "This document requires 'TrustedScriptURL' assignment.";
    case kTrustedHTMLAssignmentAndDefaultPolicyFailed:
      return "This document requires 'TrustedHTML' assignment and the "
             "'default' policy failed to execute.";
    case kTrustedScriptAssignmentAndDefaultPolicyFailed:
      return "This document requires 'TrustedScript' assignment and the "
             "'default' policy failed to execute.";
    case kTrustedURLAssignmentAndDefaultPolicyFailed:
      return "This document requires 'TrustedURL' assignment and the 'default' "
             "policy failed to execute.";
    case kTrustedScriptURLAssignmentAndDefaultPolicyFailed:
      return "This document requires 'TrustedScriptURL' assignment and the "
             "'default' policy failed to execute.";
  }
  NOTREACHED();
  return "";
}

// Handle failure of a Trusted Type assignment.
//
// If trusted type assignment fails, we need to
// - report the violation via CSP
// - increment the appropriate counter,
// - raise a JavaScript exception (if enforced).
//
// Returns whether the failure should be enforced.
bool TrustedTypeFail(TrustedTypeViolationKind kind,
                     const Document* doc,
                     ExceptionState& exception_state) {
  if (!doc)
    return true;

  // Test case docs (Document::CreateForTest) might not have a window.
  if (doc->ExecutingWindow())
    doc->ExecutingWindow()->trustedTypes()->CountTrustedTypeAssignmentError();

  const char* message = GetMessage(kind);
  bool allow =
      doc->GetContentSecurityPolicy()->AllowTrustedTypeAssignmentFailure(
          message);
  if (!allow) {
    exception_state.ThrowTypeError(message);
  }
  return !allow;
}

}  // namespace

String GetStringFromTrustedType(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL&
        string_or_trusted_type,
    const Document* doc,
    ExceptionState& exception_state) {
  DCHECK(string_or_trusted_type.IsString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled(doc));
  DCHECK(!string_or_trusted_type.IsNull());

  if (string_or_trusted_type.IsString() && doc && doc->RequireTrustedTypes()) {
    TrustedTypeFail(kAnyTrustedTypeAssignment, doc, exception_state);
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

String GetStringFromSpecificTrustedType(
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL&
        string_or_trusted_type,
    SpecificTrustedType specific_trusted_type,
    const Document* doc,
    ExceptionState& exception_state) {
  switch (specific_trusted_type) {
    case SpecificTrustedType::kTrustedHTML: {
      StringOrTrustedHTML string_or_trusted_html =
          string_or_trusted_type.IsTrustedHTML()
              ? StringOrTrustedHTML::FromTrustedHTML(
                    string_or_trusted_type.GetAsTrustedHTML())
              : StringOrTrustedHTML::FromString(
                    GetStringFromTrustedTypeWithoutCheck(
                        string_or_trusted_type));
      return GetStringFromTrustedHTML(string_or_trusted_html, doc,
                                      exception_state);
    }
    case SpecificTrustedType::kTrustedScript: {
      StringOrTrustedScript string_or_trusted_script =
          string_or_trusted_type.IsTrustedScript()
              ? StringOrTrustedScript::FromTrustedScript(
                    string_or_trusted_type.GetAsTrustedScript())
              : StringOrTrustedScript::FromString(
                    GetStringFromTrustedTypeWithoutCheck(
                        string_or_trusted_type));
      return GetStringFromTrustedScript(string_or_trusted_script, doc,
                                        exception_state);
    }
    case SpecificTrustedType::kTrustedScriptURL: {
      StringOrTrustedScriptURL string_or_trusted_script_url =
          string_or_trusted_type.IsTrustedScriptURL()
              ? StringOrTrustedScriptURL::FromTrustedScriptURL(
                    string_or_trusted_type.GetAsTrustedScriptURL())
              : StringOrTrustedScriptURL::FromString(
                    GetStringFromTrustedTypeWithoutCheck(
                        string_or_trusted_type));
      return GetStringFromTrustedScriptURL(string_or_trusted_script_url, doc,
                                           exception_state);
    }
    case SpecificTrustedType::kTrustedURL: {
      USVStringOrTrustedURL string_or_trusted_url =
          string_or_trusted_type.IsTrustedURL()
              ? USVStringOrTrustedURL::FromTrustedURL(
                    string_or_trusted_type.GetAsTrustedURL())
              : USVStringOrTrustedURL::FromUSVString(
                    GetStringFromTrustedTypeWithoutCheck(
                        string_or_trusted_type));
      return GetStringFromTrustedURL(string_or_trusted_url, doc,
                                     exception_state);
    }
  }
}

String GetStringFromTrustedHTML(StringOrTrustedHTML string_or_trusted_html,
                                const Document* doc,
                                ExceptionState& exception_state) {
  DCHECK(string_or_trusted_html.IsString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled(doc));
  DCHECK(!string_or_trusted_html.IsNull());

  if (string_or_trusted_html.IsTrustedHTML()) {
    return string_or_trusted_html.GetAsTrustedHTML()->toString();
  }

  return GetStringFromTrustedHTML(string_or_trusted_html.GetAsString(), doc,
                                  exception_state);
}

String GetStringFromTrustedHTML(const String& string,
                                const Document* doc,
                                ExceptionState& exception_state) {
  bool require_trusted_type = doc && doc->RequireTrustedTypes();
  if (!require_trusted_type) {
    return string;
  }

  TrustedTypePolicy* default_policy =
      doc->ExecutingWindow()->trustedTypes()->getExposedPolicy("default");
  if (!default_policy) {
    if (TrustedTypeFail(kTrustedHTMLAssignment, doc, exception_state)) {
      return g_empty_string;
    }
    return string;
  }

  TrustedHTML* result =
      default_policy->CreateHTML(doc->GetIsolate(), string, exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    TrustedTypeFail(kTrustedHTMLAssignmentAndDefaultPolicyFailed, doc,
                    exception_state);
    return g_empty_string;
  }

  return result->toString();
}

String GetStringFromTrustedScript(
    StringOrTrustedScript string_or_trusted_script,
    const Document* doc,
    ExceptionState& exception_state) {
  DCHECK(string_or_trusted_script.IsString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled(doc));

  // To remain compatible with legacy behaviour, HTMLElement uses extended IDL
  // attributes to allow for nullable union of (DOMString or TrustedScript).
  // Thus, this method is required to handle the case where
  // string_or_trusted_script.IsNull(), unlike the various similar methods in
  // this file.

  bool require_trusted_type = doc && doc->RequireTrustedTypes();
  if (!require_trusted_type) {
    if (string_or_trusted_script.IsString()) {
      return string_or_trusted_script.GetAsString();
    }
    if (string_or_trusted_script.IsNull()) {
      return g_empty_string;
    }
  }

  if (string_or_trusted_script.IsTrustedScript()) {
    return string_or_trusted_script.GetAsTrustedScript()->toString();
  }

  DCHECK(require_trusted_type);
  DCHECK(string_or_trusted_script.IsNull() ||
         string_or_trusted_script.IsString());

  TrustedTypePolicy* default_policy =
      doc->ExecutingWindow()->trustedTypes()->getExposedPolicy("default");
  if (!default_policy) {
    if (TrustedTypeFail(kTrustedScriptAssignment, doc, exception_state)) {
      return g_empty_string;
    }
    if (string_or_trusted_script.IsNull())
      return g_empty_string;
    return string_or_trusted_script.GetAsString();
  }

  const String& string_value_or_empty =
      string_or_trusted_script.IsNull()
          ? g_empty_string
          : string_or_trusted_script.GetAsString();
  TrustedScript* result = default_policy->CreateScript(
      doc->GetIsolate(), string_value_or_empty, exception_state);
  DCHECK_EQ(!result, exception_state.HadException());
  if (exception_state.HadException()) {
    exception_state.ClearException();
    TrustedTypeFail(kTrustedScriptAssignmentAndDefaultPolicyFailed, doc,
                    exception_state);
    return g_empty_string;
  }

  return result->toString();
}

String GetStringFromTrustedScriptURL(
    StringOrTrustedScriptURL string_or_trusted_script_url,
    const Document* doc,
    ExceptionState& exception_state) {
  DCHECK(string_or_trusted_script_url.IsString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled(doc));
  DCHECK(!string_or_trusted_script_url.IsNull());

  bool require_trusted_type =
      doc && RuntimeEnabledFeatures::TrustedDOMTypesEnabled(doc) &&
      doc->RequireTrustedTypes();
  if (!require_trusted_type && string_or_trusted_script_url.IsString()) {
    return string_or_trusted_script_url.GetAsString();
  }

  if (string_or_trusted_script_url.IsTrustedScriptURL()) {
    return string_or_trusted_script_url.GetAsTrustedScriptURL()->toString();
  }

  TrustedTypePolicy* default_policy =
      doc->ExecutingWindow()->trustedTypes()->getExposedPolicy("default");
  if (!default_policy) {
    if (TrustedTypeFail(kTrustedScriptURLAssignment, doc, exception_state)) {
      return g_empty_string;
    }
    return string_or_trusted_script_url.GetAsString();
  }

  TrustedScriptURL* result = default_policy->CreateScriptURL(
      doc->GetIsolate(), string_or_trusted_script_url.GetAsString(),
      exception_state);

  if (exception_state.HadException()) {
    exception_state.ClearException();
    TrustedTypeFail(kTrustedScriptURLAssignmentAndDefaultPolicyFailed, doc,
                    exception_state);
    return g_empty_string;
  }

  return result->toString();
}

String GetStringFromTrustedURL(USVStringOrTrustedURL string_or_trusted_url,
                               const Document* doc,
                               ExceptionState& exception_state) {
  DCHECK(string_or_trusted_url.IsUSVString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled(doc));
  DCHECK(!string_or_trusted_url.IsNull());

  bool require_trusted_type = doc && doc->RequireTrustedTypes();
  if (!require_trusted_type && string_or_trusted_url.IsUSVString()) {
    return string_or_trusted_url.GetAsUSVString();
  }

  if (string_or_trusted_url.IsTrustedURL()) {
    return string_or_trusted_url.GetAsTrustedURL()->toString();
  }

  TrustedTypePolicy* default_policy =
      doc->ExecutingWindow()->trustedTypes()->getExposedPolicy("default");
  if (!default_policy) {
    if (TrustedTypeFail(kTrustedURLAssignment, doc, exception_state)) {
      return g_empty_string;
    }
    return string_or_trusted_url.GetAsUSVString();
  }

  TrustedURL* result = default_policy->CreateURL(
      doc->GetIsolate(), string_or_trusted_url.GetAsUSVString(),
      exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    TrustedTypeFail(kTrustedURLAssignmentAndDefaultPolicyFailed, doc,
                    exception_state);
    return g_empty_string;
  }

  return result->toString();
}

}  // namespace blink
