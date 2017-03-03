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
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Event.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8HTMLElement.h"
#include "bindings/core/v8/V8PerContextData.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8Window.h"
#include "bindings/core/v8/WindowProxy.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/events/Event.h"
#include "core/events/EventListener.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/NavigationScheduler.h"
#include "core/loader/ProgressTracker.h"
#include "core/plugins/PluginView.h"
#include "platform/FrameViewBase.h"
#include "platform/Histogram.h"
#include "platform/UserGestureIndicator.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"
#include "wtf/StdLibExtras.h"
#include "wtf/StringExtras.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextPosition.h"

namespace blink {

ScriptController::ScriptController(LocalFrame* frame)
    : m_windowProxyManager(LocalWindowProxyManager::create(*frame)) {}

DEFINE_TRACE(ScriptController) {
  visitor->trace(m_windowProxyManager);
}

void ScriptController::clearForClose() {
  m_windowProxyManager->clearForClose();
  MainThreadDebugger::instance()->didClearContextsForFrame(frame());
}

void ScriptController::updateSecurityOrigin(SecurityOrigin* securityOrigin) {
  m_windowProxyManager->updateSecurityOrigin(securityOrigin);
}

namespace {

V8CacheOptions cacheOptions(const ScriptResource* resource,
                            const Settings* settings) {
  V8CacheOptions v8CacheOptions(V8CacheOptionsDefault);
  if (settings)
    v8CacheOptions = settings->getV8CacheOptions();
  if (resource && !resource->response().cacheStorageCacheName().isNull()) {
    switch (settings->getV8CacheStrategiesForCacheStorage()) {
      case V8CacheStrategiesForCacheStorage::None:
        v8CacheOptions = V8CacheOptionsNone;
        break;
      case V8CacheStrategiesForCacheStorage::Normal:
        v8CacheOptions = V8CacheOptionsCode;
        break;
      case V8CacheStrategiesForCacheStorage::Default:
      case V8CacheStrategiesForCacheStorage::Aggressive:
        v8CacheOptions = V8CacheOptionsAlways;
        break;
    }
  }
  return v8CacheOptions;
}

}  // namespace

v8::Local<v8::Value> ScriptController::executeScriptAndReturnValue(
    v8::Local<v8::Context> context,
    const ScriptSourceCode& source,
    AccessControlStatus accessControlStatus) {
  TRACE_EVENT1("devtools.timeline", "EvaluateScript", "data",
               InspectorEvaluateScriptEvent::data(
                   frame(), source.url().getString(), source.startPosition()));
  probe::NativeBreakpoint nativeBreakpoint(frame()->document(),
                                           "scriptFirstStatement");

  v8::Local<v8::Value> result;
  {
    V8CacheOptions v8CacheOptions =
        cacheOptions(source.resource(), frame()->settings());

    // Isolate exceptions that occur when compiling and executing
    // the code. These exceptions should not interfere with
    // javascript code we might evaluate from C++ when returning
    // from here.
    v8::TryCatch tryCatch(isolate());
    tryCatch.SetVerbose(true);

    v8::Local<v8::Script> script;
    if (!v8Call(V8ScriptRunner::compileScript(
                    source, isolate(), accessControlStatus, v8CacheOptions),
                script, tryCatch))
      return result;

    if (!v8Call(V8ScriptRunner::runCompiledScript(isolate(), script,
                                                  frame()->document()),
                result, tryCatch))
      return result;
  }

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorUpdateCountersEvent::data());

  return result;
}

LocalWindowProxy* ScriptController::windowProxy(DOMWrapperWorld& world) {
  LocalWindowProxy* windowProxy = m_windowProxyManager->windowProxy(world);
  windowProxy->initializeIfNeeded();
  return windowProxy;
}

bool ScriptController::shouldBypassMainWorldCSP() {
  v8::HandleScope handleScope(isolate());
  v8::Local<v8::Context> context = isolate()->GetCurrentContext();
  if (context.IsEmpty() || !toDOMWindow(context))
    return false;
  DOMWrapperWorld& world = DOMWrapperWorld::current(isolate());
  return world.isIsolatedWorld() ? world.isolatedWorldHasContentSecurityPolicy()
                                 : false;
}

TextPosition ScriptController::eventHandlerPosition() const {
  ScriptableDocumentParser* parser =
      frame()->document()->scriptableDocumentParser();
  if (parser)
    return parser->textPosition();
  return TextPosition::minimumPosition();
}

void ScriptController::enableEval() {
  v8::HandleScope handleScope(isolate());
  v8::Local<v8::Context> v8Context =
      m_windowProxyManager->mainWorldProxy()->contextIfInitialized();
  if (v8Context.IsEmpty())
    return;
  v8Context->AllowCodeGenerationFromStrings(true);
}

void ScriptController::disableEval(const String& errorMessage) {
  v8::HandleScope handleScope(isolate());
  v8::Local<v8::Context> v8Context =
      m_windowProxyManager->mainWorldProxy()->contextIfInitialized();
  if (v8Context.IsEmpty())
    return;
  v8Context->AllowCodeGenerationFromStrings(false);
  v8Context->SetErrorMessageForCodeGenerationFromStrings(
      v8String(isolate(), errorMessage));
}

PassRefPtr<SharedPersistent<v8::Object>> ScriptController::createPluginWrapper(
    FrameViewBase* frameViewBase) {
  DCHECK(frameViewBase);

  if (!frameViewBase->isPluginView())
    return nullptr;

  v8::HandleScope handleScope(isolate());
  v8::Local<v8::Object> scriptableObject =
      toPluginView(frameViewBase)->scriptableObject(isolate());

  if (scriptableObject.IsEmpty())
    return nullptr;

  return SharedPersistent<v8::Object>::create(scriptableObject, isolate());
}

V8Extensions& ScriptController::registeredExtensions() {
  DEFINE_STATIC_LOCAL(V8Extensions, extensions, ());
  return extensions;
}

void ScriptController::registerExtensionIfNeeded(v8::Extension* extension) {
  const V8Extensions& extensions = registeredExtensions();
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (extensions[i] == extension)
      return;
  }
  v8::RegisterExtension(extension);
  registeredExtensions().push_back(extension);
}

void ScriptController::clearWindowProxy() {
  // V8 binding expects ScriptController::clearWindowProxy only be called when a
  // frame is loading a new page. This creates a new context for the new page.
  m_windowProxyManager->clearForNavigation();
  MainThreadDebugger::instance()->didClearContextsForFrame(frame());
}

void ScriptController::updateDocument() {
  m_windowProxyManager->mainWorldProxy()->updateDocument();
}

bool ScriptController::executeScriptIfJavaScriptURL(const KURL& url,
                                                    Element* element) {
  if (!url.protocolIsJavaScript())
    return false;

  const int javascriptSchemeLength = sizeof("javascript:") - 1;
  String scriptSource = decodeURLEscapeSequences(url.getString())
                            .substring(javascriptSchemeLength);

  bool shouldBypassMainWorldContentSecurityPolicy =
      ContentSecurityPolicy::shouldBypassMainWorld(frame()->document());
  if (!frame()->page() ||
      (!shouldBypassMainWorldContentSecurityPolicy &&
       !frame()->document()->contentSecurityPolicy()->allowJavaScriptURLs(
           element, scriptSource, frame()->document()->url(),
           eventHandlerPosition().m_line))) {
    return true;
  }

  bool progressNotificationsNeeded =
      frame()->loader().stateMachine()->isDisplayingInitialEmptyDocument() &&
      !frame()->isLoading();
  if (progressNotificationsNeeded)
    frame()->loader().progress().progressStarted(FrameLoadTypeStandard);

  Document* ownerDocument = frame()->document();

  bool locationChangeBefore =
      frame()->navigationScheduler().locationChangePending();

  v8::HandleScope handleScope(isolate());
  v8::Local<v8::Value> result = evaluateScriptInMainWorld(
      ScriptSourceCode(scriptSource), NotSharableCrossOrigin,
      DoNotExecuteScriptWhenScriptsDisabled);

  // If executing script caused this frame to be removed from the page, we
  // don't want to try to replace its document!
  if (!frame()->page())
    return true;

  if (result.IsEmpty() || !result->IsString()) {
    if (progressNotificationsNeeded)
      frame()->loader().progress().progressCompleted();
    return true;
  }
  String scriptResult = toCoreString(v8::Local<v8::String>::Cast(result));

  // We're still in a frame, so there should be a DocumentLoader.
  DCHECK(frame()->document()->loader());
  if (!locationChangeBefore &&
      frame()->navigationScheduler().locationChangePending())
    return true;

  frame()->loader().replaceDocumentWhileExecutingJavaScriptURL(scriptResult,
                                                               ownerDocument);
  return true;
}

void ScriptController::executeScriptInMainWorld(const String& script,
                                                ExecuteScriptPolicy policy) {
  v8::HandleScope handleScope(isolate());
  evaluateScriptInMainWorld(ScriptSourceCode(script), NotSharableCrossOrigin,
                            policy);
}

void ScriptController::executeScriptInMainWorld(
    const ScriptSourceCode& sourceCode,
    AccessControlStatus accessControlStatus) {
  v8::HandleScope handleScope(isolate());
  evaluateScriptInMainWorld(sourceCode, accessControlStatus,
                            DoNotExecuteScriptWhenScriptsDisabled);
}

v8::Local<v8::Value> ScriptController::executeScriptInMainWorldAndReturnValue(
    const ScriptSourceCode& sourceCode,
    ExecuteScriptPolicy policy) {
  return evaluateScriptInMainWorld(sourceCode, NotSharableCrossOrigin, policy);
}

v8::Local<v8::Value> ScriptController::evaluateScriptInMainWorld(
    const ScriptSourceCode& sourceCode,
    AccessControlStatus accessControlStatus,
    ExecuteScriptPolicy policy) {
  if (policy == DoNotExecuteScriptWhenScriptsDisabled &&
      !frame()->document()->canExecuteScripts(AboutToExecuteScript))
    return v8::Local<v8::Value>();

  ScriptState* scriptState = ScriptState::forMainWorld(frame());
  if (!scriptState)
    return v8::Local<v8::Value>();
  v8::EscapableHandleScope handleScope(isolate());
  ScriptState::Scope scope(scriptState);

  if (frame()->loader().stateMachine()->isDisplayingInitialEmptyDocument())
    frame()->loader().didAccessInitialDocument();

  v8::Local<v8::Value> object = executeScriptAndReturnValue(
      scriptState->context(), sourceCode, accessControlStatus);

  if (object.IsEmpty())
    return v8::Local<v8::Value>();

  return handleScope.Escape(object);
}

void ScriptController::executeScriptInIsolatedWorld(
    int worldID,
    const HeapVector<ScriptSourceCode>& sources,
    Vector<v8::Local<v8::Value>>* results) {
  DCHECK_GT(worldID, 0);

  RefPtr<DOMWrapperWorld> world =
      DOMWrapperWorld::ensureIsolatedWorld(isolate(), worldID);
  LocalWindowProxy* isolatedWorldWindowProxy = windowProxy(*world);
  ScriptState* scriptState = isolatedWorldWindowProxy->getScriptState();
  if (!scriptState->contextIsValid())
    return;
  v8::Context::Scope scope(scriptState->context());
  v8::Local<v8::Array> resultArray = v8::Array::New(isolate(), sources.size());

  for (size_t i = 0; i < sources.size(); ++i) {
    v8::Local<v8::Value> evaluationResult =
        executeScriptAndReturnValue(scriptState->context(), sources[i]);
    if (evaluationResult.IsEmpty())
      evaluationResult =
          v8::Local<v8::Value>::New(isolate(), v8::Undefined(isolate()));
    if (!v8CallBoolean(resultArray->CreateDataProperty(scriptState->context(),
                                                       i, evaluationResult)))
      return;
  }

  if (results) {
    for (size_t i = 0; i < resultArray->Length(); ++i) {
      v8::Local<v8::Value> value;
      if (!resultArray->Get(scriptState->context(), i).ToLocal(&value))
        return;
      results->push_back(value);
    }
  }
}

}  // namespace blink
