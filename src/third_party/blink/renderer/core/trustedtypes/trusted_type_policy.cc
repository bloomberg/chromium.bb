// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"

#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_url.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

TrustedTypePolicy::TrustedTypePolicy(
    const String& policy_name,
    const TrustedTypePolicyOptions& policy_options,
    bool exposed)
    : name_(policy_name), policy_options_(policy_options) {
  policy_options_.setExposed(exposed);
}

TrustedTypePolicy* TrustedTypePolicy::Create(
    const String& policy_name,
    const TrustedTypePolicyOptions& policy_options,
    bool exposed) {
  return new TrustedTypePolicy(policy_name, policy_options, exposed);
}

TrustedHTML* TrustedTypePolicy::createHTML(ScriptState* script_state,
                                           const String& input,
                                           ExceptionState& exception_state) {
  if (!policy_options_.createHTML()) {
    exception_state.ThrowTypeError(
        "Policy " + name_ +
        "'s TrustedTypePolicyOptions did not specify a 'createHTML' member.");
    return nullptr;
  }
  v8::TryCatch try_catch(script_state->GetIsolate());
  String html;
  if (!policy_options_.createHTML()->Invoke(nullptr, input).To(&html)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return nullptr;
  }
  return TrustedHTML::Create(html);
}

TrustedScript* TrustedTypePolicy::createScript(
    ScriptState* script_state,
    const String& input,
    ExceptionState& exception_state) {
  if (!policy_options_.createScript()) {
    exception_state.ThrowTypeError(
        "Policy " + name_ +
        "'s TrustedTypePolicyOptions did not specify a 'createScript' member.");
    return nullptr;
  }
  v8::TryCatch try_catch(script_state->GetIsolate());
  String script;
  if (!policy_options_.createScript()->Invoke(nullptr, input).To(&script)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return nullptr;
  }
  return TrustedScript::Create(script);
}

TrustedScriptURL* TrustedTypePolicy::createScriptURL(
    ScriptState* script_state,
    const String& input,
    ExceptionState& exception_state) {
  if (!policy_options_.createScriptURL()) {
    exception_state.ThrowTypeError("Policy " + name_ +
                                   "'s TrustedTypePolicyOptions did not "
                                   "specify a 'createScriptURL' member.");
    return nullptr;
  }
  v8::TryCatch try_catch(script_state->GetIsolate());
  String script_url;
  if (!policy_options_.createScriptURL()
           ->Invoke(nullptr, input)
           .To(&script_url)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return nullptr;
  }
  return TrustedScriptURL::Create(KURL(script_url));
}

TrustedURL* TrustedTypePolicy::createURL(ScriptState* script_state,
                                         const String& input,
                                         ExceptionState& exception_state) {
  if (!policy_options_.createURL()) {
    exception_state.ThrowTypeError(
        "Policy " + name_ +
        "'s TrustedTypePolicyOptions did not specify a 'createURL' member.");
    return nullptr;
  }
  v8::TryCatch try_catch(script_state->GetIsolate());
  String url;
  if (!policy_options_.createURL()->Invoke(nullptr, input).To(&url)) {
    DCHECK(try_catch.HasCaught());
    exception_state.RethrowV8Exception(try_catch.Exception());
    return nullptr;
  }
  return TrustedURL::Create(KURL(url));
}

String TrustedTypePolicy::name() const {
  return name_;
}

bool TrustedTypePolicy::exposed() const {
  return policy_options_.exposed();
}

void TrustedTypePolicy::Trace(blink::Visitor* visitor) {
  visitor->Trace(policy_options_);
  ScriptWrappable::Trace(visitor);
}
}  // namespace blink
