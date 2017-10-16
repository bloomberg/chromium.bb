/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/ScriptController.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/WindowProxy.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/NavigationScheduler.h"
#include "core/loader/ProgressTracker.h"
#include "core/plugins/PluginView.h"
#include "core/probe/CoreProbes.h"
#include "platform/Histogram.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/StringExtras.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/web/WebSettings.h"

namespace blink {

DEFINE_TRACE(ScriptController) {
  visitor->Trace(frame_);
  visitor->Trace(window_proxy_manager_);
}

void ScriptController::ClearForClose() {
  window_proxy_manager_->ClearForClose();
  MainThreadDebugger::Instance()->DidClearContextsForFrame(GetFrame());
}

void ScriptController::UpdateSecurityOrigin(SecurityOrigin* security_origin) {
  window_proxy_manager_->UpdateSecurityOrigin(security_origin);
}

namespace {

V8CacheOptions CacheOptions(const ScriptResource* resource,
                            const Settings* settings) {
  V8CacheOptions v8_cache_options(kV8CacheOptionsDefault);
  if (settings)
    v8_cache_options = settings->GetV8CacheOptions();
  if (resource && !resource->GetResponse().CacheStorageCacheName().IsNull()) {
    switch (settings->GetV8CacheStrategiesForCacheStorage()) {
      case V8CacheStrategiesForCacheStorage::kNone:
        v8_cache_options = kV8CacheOptionsNone;
        break;
      case V8CacheStrategiesForCacheStorage::kNormal:
        v8_cache_options = kV8CacheOptionsCode;
        break;
      case V8CacheStrategiesForCacheStorage::kDefault:
      case V8CacheStrategiesForCacheStorage::kAggressive:
        v8_cache_options = kV8CacheOptionsAlways;
        break;
    }
  }
  return v8_cache_options;
}

}  // namespace

v8::Local<v8::Value> ScriptController::ExecuteScriptAndReturnValue(
    v8::Local<v8::Context> context,
    const ScriptSourceCode& source,
    const ScriptFetchOptions& fetch_options,
    AccessControlStatus access_control_status) {
  TRACE_EVENT1(
      "devtools.timeline", "EvaluateScript", "data",
      InspectorEvaluateScriptEvent::Data(GetFrame(), source.Url().GetString(),
                                         source.StartPosition()));
  v8::Local<v8::Value> result;
  {
    V8CacheOptions v8_cache_options =
        CacheOptions(source.GetResource(), GetFrame()->GetSettings());

    // Isolate exceptions that occur when compiling and executing
    // the code. These exceptions should not interfere with
    // javascript code we might evaluate from C++ when returning
    // from here.
    v8::TryCatch try_catch(GetIsolate());
    try_catch.SetVerbose(true);

    v8::Local<v8::Script> script;
    if (!V8ScriptRunner::CompileScript(ScriptState::From(context), source,
                                       fetch_options, access_control_status,
                                       v8_cache_options)
             .ToLocal(&script))
      return result;

    if (!V8ScriptRunner::RunCompiledScript(GetIsolate(), script,
                                           GetFrame()->GetDocument())
             .ToLocal(&result))
      return result;
  }

  return result;
}

bool ScriptController::ShouldBypassMainWorldCSP() {
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> context = GetIsolate()->GetCurrentContext();
  if (context.IsEmpty() || !ToLocalDOMWindow(context))
    return false;
  DOMWrapperWorld& world = DOMWrapperWorld::Current(GetIsolate());
  return world.IsIsolatedWorld() ? world.IsolatedWorldHasContentSecurityPolicy()
                                 : false;
}

TextPosition ScriptController::EventHandlerPosition() const {
  ScriptableDocumentParser* parser =
      GetFrame()->GetDocument()->GetScriptableDocumentParser();
  if (parser)
    return parser->GetTextPosition();
  return TextPosition::MinimumPosition();
}

void ScriptController::EnableEval() {
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> v8_context =
      window_proxy_manager_->MainWorldProxyMaybeUninitialized()
          ->ContextIfInitialized();
  if (v8_context.IsEmpty())
    return;
  v8_context->AllowCodeGenerationFromStrings(true);
}

void ScriptController::DisableEval(const String& error_message) {
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Context> v8_context =
      window_proxy_manager_->MainWorldProxyMaybeUninitialized()
          ->ContextIfInitialized();
  if (v8_context.IsEmpty())
    return;
  v8_context->AllowCodeGenerationFromStrings(false);
  v8_context->SetErrorMessageForCodeGenerationFromStrings(
      V8String(GetIsolate(), error_message));
}

V8Extensions& ScriptController::RegisteredExtensions() {
  DEFINE_STATIC_LOCAL(V8Extensions, extensions, ());
  return extensions;
}

void ScriptController::RegisterExtensionIfNeeded(v8::Extension* extension) {
  const V8Extensions& extensions = RegisteredExtensions();
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (extensions[i] == extension)
      return;
  }
  v8::RegisterExtension(extension);
  RegisteredExtensions().push_back(extension);
}

void ScriptController::ClearWindowProxy() {
  // V8 binding expects ScriptController::clearWindowProxy only be called when a
  // frame is loading a new page. This creates a new context for the new page.
  window_proxy_manager_->ClearForNavigation();
  MainThreadDebugger::Instance()->DidClearContextsForFrame(GetFrame());
}

void ScriptController::UpdateDocument() {
  window_proxy_manager_->MainWorldProxyMaybeUninitialized()->UpdateDocument();
  EnableEval();
}

bool ScriptController::ExecuteScriptIfJavaScriptURL(const KURL& url,
                                                    Element* element) {
  if (!url.ProtocolIsJavaScript())
    return false;

  const int kJavascriptSchemeLength = sizeof("javascript:") - 1;
  String script_source = DecodeURLEscapeSequences(url.GetString())
                             .Substring(kJavascriptSchemeLength);

  bool should_bypass_main_world_content_security_policy =
      ContentSecurityPolicy::ShouldBypassMainWorld(GetFrame()->GetDocument());
  if (!GetFrame()->GetPage() ||
      (!should_bypass_main_world_content_security_policy &&
       !GetFrame()
            ->GetDocument()
            ->GetContentSecurityPolicy()
            ->AllowJavaScriptURLs(element, script_source,
                                  GetFrame()->GetDocument()->Url(),
                                  EventHandlerPosition().line_))) {
    return true;
  }

  bool progress_notifications_needed =
      GetFrame()->Loader().StateMachine()->IsDisplayingInitialEmptyDocument() &&
      !GetFrame()->IsLoading();
  if (progress_notifications_needed)
    GetFrame()->Loader().Progress().ProgressStarted(kFrameLoadTypeStandard);

  Document* owner_document = GetFrame()->GetDocument();

  bool location_change_before =
      GetFrame()->GetNavigationScheduler().LocationChangePending();

  v8::HandleScope handle_scope(GetIsolate());
  ScriptFetchOptions fetch_options;
  // TODO(kouhei): set up |fetch_options| properly.
  v8::Local<v8::Value> result = EvaluateScriptInMainWorld(
      ScriptSourceCode(script_source), fetch_options, kNotSharableCrossOrigin,
      kDoNotExecuteScriptWhenScriptsDisabled);

  // If executing script caused this frame to be removed from the page, we
  // don't want to try to replace its document!
  if (!GetFrame()->GetPage())
    return true;

  if (result.IsEmpty() || !result->IsString()) {
    if (progress_notifications_needed)
      GetFrame()->Loader().Progress().ProgressCompleted();
    return true;
  }
  String script_result = ToCoreString(v8::Local<v8::String>::Cast(result));

  // We're still in a frame, so there should be a DocumentLoader.
  DCHECK(GetFrame()->GetDocument()->Loader());
  if (!location_change_before &&
      GetFrame()->GetNavigationScheduler().LocationChangePending())
    return true;

  GetFrame()->Loader().ReplaceDocumentWhileExecutingJavaScriptURL(
      script_result, owner_document);
  return true;
}

void ScriptController::ExecuteScriptInMainWorld(const String& script,
                                                ExecuteScriptPolicy policy) {
  v8::HandleScope handle_scope(GetIsolate());
  EvaluateScriptInMainWorld(ScriptSourceCode(script), ScriptFetchOptions(),
                            kNotSharableCrossOrigin, policy);
}

void ScriptController::ExecuteScriptInMainWorld(
    const ScriptSourceCode& source_code,
    const ScriptFetchOptions& fetch_options,
    AccessControlStatus access_control_status) {
  v8::HandleScope handle_scope(GetIsolate());
  EvaluateScriptInMainWorld(source_code, fetch_options, access_control_status,
                            kDoNotExecuteScriptWhenScriptsDisabled);
}

v8::Local<v8::Value> ScriptController::ExecuteScriptInMainWorldAndReturnValue(
    const ScriptSourceCode& source_code,
    const ScriptFetchOptions& fetch_options,
    ExecuteScriptPolicy policy) {
  return EvaluateScriptInMainWorld(source_code, fetch_options,
                                   kNotSharableCrossOrigin, policy);
}

v8::Local<v8::Value> ScriptController::EvaluateScriptInMainWorld(
    const ScriptSourceCode& source_code,
    const ScriptFetchOptions& fetch_options,
    AccessControlStatus access_control_status,
    ExecuteScriptPolicy policy) {
  if (policy == kDoNotExecuteScriptWhenScriptsDisabled &&
      !GetFrame()->GetDocument()->CanExecuteScripts(kAboutToExecuteScript))
    return v8::Local<v8::Value>();

  // TODO(dcheng): Clean this up to not use ScriptState, to match
  // executeScriptInIsolatedWorld.
  ScriptState* script_state = ToScriptStateForMainWorld(GetFrame());
  if (!script_state)
    return v8::Local<v8::Value>();
  v8::EscapableHandleScope handle_scope(GetIsolate());
  ScriptState::Scope scope(script_state);

  if (GetFrame()->Loader().StateMachine()->IsDisplayingInitialEmptyDocument())
    GetFrame()->Loader().DidAccessInitialDocument();

  v8::Local<v8::Value> object =
      ExecuteScriptAndReturnValue(script_state->GetContext(), source_code,
                                  fetch_options, access_control_status);

  if (object.IsEmpty())
    return v8::Local<v8::Value>();

  return handle_scope.Escape(object);
}

void ScriptController::ExecuteScriptInIsolatedWorld(
    int world_id,
    const HeapVector<ScriptSourceCode>& sources,
    Vector<v8::Local<v8::Value>>* results) {
  DCHECK_GT(world_id, 0);

  RefPtr<DOMWrapperWorld> world =
      DOMWrapperWorld::EnsureIsolatedWorld(GetIsolate(), world_id);
  LocalWindowProxy* isolated_world_window_proxy = WindowProxy(*world);
  // TODO(dcheng): Context must always be initialized here, due to the call to
  // windowProxy() on the previous line. Add a helper which makes that obvious?
  v8::Local<v8::Context> context =
      isolated_world_window_proxy->ContextIfInitialized();
  v8::Context::Scope scope(context);
  v8::Local<v8::Array> result_array =
      v8::Array::New(GetIsolate(), sources.size());

  for (size_t i = 0; i < sources.size(); ++i) {
    v8::Local<v8::Value> evaluation_result =
        ExecuteScriptAndReturnValue(context, sources[i]);
    if (evaluation_result.IsEmpty())
      evaluation_result =
          v8::Local<v8::Value>::New(GetIsolate(), v8::Undefined(GetIsolate()));
    bool did_create;
    if (!result_array->CreateDataProperty(context, i, evaluation_result)
             .To(&did_create) ||
        !did_create)
      return;
  }

  if (results) {
    for (size_t i = 0; i < result_array->Length(); ++i) {
      v8::Local<v8::Value> value;
      if (!result_array->Get(context, i).ToLocal(&value))
        return;
      results->push_back(value);
    }
  }
}

RefPtr<DOMWrapperWorld> ScriptController::CreateNewInspectorIsolatedWorld(
    const String& world_name) {
  RefPtr<DOMWrapperWorld> world = DOMWrapperWorld::Create(
      GetIsolate(), DOMWrapperWorld::WorldType::kInspectorIsolated);
  // Bail out if we could not create an isolated world.
  if (!world)
    return nullptr;
  if (!world_name.IsEmpty()) {
    DOMWrapperWorld::SetNonMainWorldHumanReadableName(world->GetWorldId(),
                                                      world_name);
  }
  // Make sure the execution context exists.
  WindowProxy(*world);
  return world;
}

STATIC_ASSERT_ENUM(WebSettings::V8CacheStrategiesForCacheStorage::kDefault,
                   V8CacheStrategiesForCacheStorage::kDefault);
STATIC_ASSERT_ENUM(WebSettings::V8CacheStrategiesForCacheStorage::kNone,
                   V8CacheStrategiesForCacheStorage::kNone);
STATIC_ASSERT_ENUM(WebSettings::V8CacheStrategiesForCacheStorage::kNormal,
                   V8CacheStrategiesForCacheStorage::kNormal);
STATIC_ASSERT_ENUM(WebSettings::V8CacheStrategiesForCacheStorage::kAggressive,
                   V8CacheStrategiesForCacheStorage::kAggressive);

}  // namespace blink
