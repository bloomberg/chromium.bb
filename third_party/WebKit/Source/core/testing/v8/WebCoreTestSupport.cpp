/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/testing/v8/WebCoreTestSupport.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8OriginTrialsTest.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/testing/InternalSettings.h"
#include "core/testing/Internals.h"
#include "core/testing/WorkerInternals.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/OriginTrialFeatures.h"

namespace WebCoreTestSupport {

namespace {

blink::InstallOriginTrialFeaturesFunction
    s_original_install_origin_trial_features_function = nullptr;
blink::InstallPendingOriginTrialFeatureFunction
    s_original_install_pending_origin_trial_feature_function = nullptr;

v8::Local<v8::Value> CreateInternalsObject(v8::Local<v8::Context> context) {
  blink::ScriptState* script_state = blink::ScriptState::From(context);
  v8::Local<v8::Object> global = script_state->GetContext()->Global();
  blink::ExecutionContext* execution_context =
      blink::ExecutionContext::From(script_state);
  if (execution_context->IsDocument()) {
    return blink::ToV8(blink::Internals::Create(execution_context), global,
                       script_state->GetIsolate());
  }
  if (execution_context->IsWorkerGlobalScope()) {
    return blink::ToV8(blink::WorkerInternals::Create(), global,
                       script_state->GetIsolate());
  }
  return v8::Local<v8::Value>();
}

}  // namespace

void InjectInternalsObject(v8::Local<v8::Context> context) {
  RegisterInstallOriginTrialFeaturesForTesting();

  blink::ScriptState* script_state = blink::ScriptState::From(context);
  blink::ScriptState::Scope scope(script_state);
  v8::Local<v8::Object> global = script_state->GetContext()->Global();
  v8::Local<v8::Value> internals = CreateInternalsObject(context);
  if (internals.IsEmpty())
    return;

  global
      ->CreateDataProperty(
          script_state->GetContext(),
          blink::V8AtomicString(script_state->GetIsolate(), "internals"),
          internals)
      .ToChecked();
}

void InstallOriginTrialFeaturesForTesting(
    const blink::WrapperTypeInfo* type,
    const blink::ScriptState* script_state,
    v8::Local<v8::Object> prototype_object,
    v8::Local<v8::Function> interface_object) {
  (*s_original_install_origin_trial_features_function)(
      type, script_state, prototype_object, interface_object);

  blink::ExecutionContext* execution_context =
      blink::ExecutionContext::From(script_state);
  blink::OriginTrialContext* originTrialContext =
      blink::OriginTrialContext::From(
          execution_context, blink::OriginTrialContext::kDontCreateIfNotExists);

  if (type == &blink::V8OriginTrialsTest::wrapperTypeInfo) {
    if (originTrialContext && originTrialContext->IsTrialEnabled("Frobulate")) {
      blink::V8OriginTrialsTest::installOriginTrialsSampleAPI(
          script_state->GetIsolate(), script_state->World(),
          v8::Local<v8::Object>(), prototype_object, interface_object);
    }
  }
}

void ResetInternalsObject(v8::Local<v8::Context> context) {
  // This can happen if JavaScript is disabled in the main frame.
  if (context.IsEmpty())
    return;

  blink::ScriptState* script_state = blink::ScriptState::From(context);
  blink::ScriptState::Scope scope(script_state);
  blink::Document* document =
      ToDocument(blink::ExecutionContext::From(script_state));
  DCHECK(document);
  blink::LocalFrame* frame = document->GetFrame();
  // Should the document have been detached, the page is assumed being destroyed
  // (=> no reset required.)
  if (!frame)
    return;
  blink::Page* page = frame->GetPage();
  DCHECK(page);
  blink::Internals::ResetToConsistentState(page);
  blink::InternalSettings::From(*page)->ResetToConsistentState();
}

void InstallPendingOriginTrialFeatureForTesting(
    const String& feature,
    const blink::ScriptState* script_state) {
  (*s_original_install_pending_origin_trial_feature_function)(feature,
                                                              script_state);
  v8::Local<v8::Object> prototype_object;
  v8::Local<v8::Function> interface_object;
  if (feature == "Frobulate") {
    if (script_state->PerContextData()
            ->GetExistingConstructorAndPrototypeForType(
                &blink::V8OriginTrialsTest::wrapperTypeInfo, &prototype_object,
                &interface_object)) {
      blink::V8OriginTrialsTest::installOriginTrialsSampleAPI(
          script_state->GetIsolate(), script_state->World(),
          v8::Local<v8::Object>(), prototype_object, interface_object);
    }
    return;
  }
}

void RegisterInstallOriginTrialFeaturesForTesting() {
  if (!s_original_install_origin_trial_features_function) {
    s_original_install_origin_trial_features_function =
        SetInstallOriginTrialFeaturesFunction(
            InstallOriginTrialFeaturesForTesting);
  }
  if (!s_original_install_pending_origin_trial_feature_function) {
    s_original_install_pending_origin_trial_feature_function =
        SetInstallPendingOriginTrialFeatureFunction(
            &InstallPendingOriginTrialFeatureForTesting);
  }
}

}  // namespace WebCoreTestSupport
