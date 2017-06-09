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
#include "platform/bindings/ConditionalFeatures.h"
#include "platform/bindings/DOMWrapperWorld.h"

namespace WebCoreTestSupport {

namespace {

blink::InstallConditionalFeaturesFunction
    s_originalInstallConditionalFeaturesFunction = nullptr;
blink::InstallPendingConditionalFeatureFunction
    s_originalInstallPendingConditionalFeatureFunction = nullptr;

v8::Local<v8::Value> createInternalsObject(v8::Local<v8::Context> context) {
  blink::ScriptState* scriptState = blink::ScriptState::From(context);
  v8::Local<v8::Object> global = scriptState->GetContext()->Global();
  blink::ExecutionContext* executionContext =
      blink::ExecutionContext::From(scriptState);
  if (executionContext->IsDocument()) {
    return blink::ToV8(blink::Internals::Create(executionContext), global,
                       scriptState->GetIsolate());
  }
  if (executionContext->IsWorkerGlobalScope()) {
    return blink::ToV8(blink::WorkerInternals::Create(), global,
                       scriptState->GetIsolate());
  }
  return v8::Local<v8::Value>();
}
}

void injectInternalsObject(v8::Local<v8::Context> context) {
  registerInstallConditionalFeaturesForTesting();

  blink::ScriptState* scriptState = blink::ScriptState::From(context);
  blink::ScriptState::Scope scope(scriptState);
  v8::Local<v8::Object> global = scriptState->GetContext()->Global();
  v8::Local<v8::Value> internals = createInternalsObject(context);
  if (internals.IsEmpty())
    return;

  global
      ->CreateDataProperty(
          scriptState->GetContext(),
          blink::V8AtomicString(scriptState->GetIsolate(), "internals"),
          internals)
      .ToChecked();
}

void installConditionalFeaturesForTesting(
    const blink::WrapperTypeInfo* type,
    const blink::ScriptState* scriptState,
    v8::Local<v8::Object> prototypeObject,
    v8::Local<v8::Function> interfaceObject) {
  (*s_originalInstallConditionalFeaturesFunction)(
      type, scriptState, prototypeObject, interfaceObject);

  blink::ExecutionContext* executionContext =
      blink::ExecutionContext::From(scriptState);
  blink::OriginTrialContext* originTrialContext =
      blink::OriginTrialContext::From(
          executionContext, blink::OriginTrialContext::kDontCreateIfNotExists);

  if (type == &blink::V8OriginTrialsTest::wrapperTypeInfo) {
    if (originTrialContext && originTrialContext->IsTrialEnabled("Frobulate")) {
      blink::V8OriginTrialsTest::installOriginTrialsSampleAPI(
          scriptState->GetIsolate(), scriptState->World(),
          v8::Local<v8::Object>(), prototypeObject, interfaceObject);
    }
  }
}

void resetInternalsObject(v8::Local<v8::Context> context) {
  // This can happen if JavaScript is disabled in the main frame.
  if (context.IsEmpty())
    return;

  blink::ScriptState* scriptState = blink::ScriptState::From(context);
  blink::ScriptState::Scope scope(scriptState);
  blink::Document* document =
      ToDocument(blink::ExecutionContext::From(scriptState));
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

void installPendingConditionalFeatureForTesting(
    const String& feature,
    const blink::ScriptState* scriptState) {
  (*s_originalInstallPendingConditionalFeatureFunction)(feature, scriptState);
  v8::Local<v8::Object> prototypeObject;
  v8::Local<v8::Function> interfaceObject;
  if (feature == "Frobulate") {
    if (scriptState->PerContextData()
            ->GetExistingConstructorAndPrototypeForType(
                &blink::V8OriginTrialsTest::wrapperTypeInfo, &prototypeObject,
                &interfaceObject)) {
      blink::V8OriginTrialsTest::installOriginTrialsSampleAPI(
          scriptState->GetIsolate(), scriptState->World(),
          v8::Local<v8::Object>(), prototypeObject, interfaceObject);
    }
    return;
  }
}

void registerInstallConditionalFeaturesForTesting() {
  if (!s_originalInstallConditionalFeaturesFunction) {
    s_originalInstallConditionalFeaturesFunction =
        SetInstallConditionalFeaturesFunction(
            installConditionalFeaturesForTesting);
  }
  if (!s_originalInstallPendingConditionalFeatureFunction) {
    s_originalInstallPendingConditionalFeatureFunction =
        SetInstallPendingConditionalFeatureFunction(
            &installPendingConditionalFeatureForTesting);
  }
}

}  // namespace WebCoreTestSupport
