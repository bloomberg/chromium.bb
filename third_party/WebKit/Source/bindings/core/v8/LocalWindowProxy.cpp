/*
 * Copyright (C) 2008, 2009, 2011 Google Inc. All rights reserved.
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

#include "bindings/core/v8/LocalWindowProxy.h"

#include "bindings/core/v8/ConditionalFeaturesForCore.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "bindings/core/v8/V8HTMLDocument.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/V8Initializer.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "bindings/core/v8/V8Window.h"
#include "core/dom/Modulator.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/DocumentNameCollection.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/loader/FrameLoader.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "v8/include/v8.h"
#include "wtf/Assertions.h"

namespace blink {

void LocalWindowProxy::disposeContext(GlobalDetachmentBehavior behavior) {
  if (m_lifecycle != Lifecycle::ContextInitialized)
    return;

  ScriptState::Scope scope(m_scriptState.get());
  v8::Local<v8::Context> context = m_scriptState->context();
  // The embedder could run arbitrary code in response to the
  // willReleaseScriptContext callback, so all disposing should happen after
  // it returns.
  frame()->loader().client()->willReleaseScriptContext(context,
                                                       m_world->worldId());
  MainThreadDebugger::instance()->contextWillBeDestroyed(m_scriptState.get());

  WindowProxy::disposeContext(behavior);
}

void LocalWindowProxy::initialize() {
  TRACE_EVENT1("v8", "LocalWindowProxy::initialize", "isMainWindow",
               frame()->isMainFrame());
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER(
      frame()->isMainFrame() ? "Blink.Binding.InitializeMainWindowProxy"
                             : "Blink.Binding.InitializeNonMainWindowProxy");

  ScriptForbiddenScope::AllowUserAgentScript allowScript;

  v8::HandleScope handleScope(isolate());

  createContext();

  ScriptState::Scope scope(m_scriptState.get());
  v8::Local<v8::Context> context = m_scriptState->context();
  if (m_globalProxy.isEmpty()) {
    m_globalProxy.set(isolate(), context->Global());
    CHECK(!m_globalProxy.isEmpty());
  }

  setupWindowPrototypeChain();

  SecurityOrigin* origin = 0;
  if (m_world->isMainWorld()) {
    // ActivityLogger for main world is updated within updateDocumentInternal().
    updateDocumentInternal();
    origin = frame()->document()->getSecurityOrigin();
    // FIXME: Can this be removed when CSP moves to browser?
    ContentSecurityPolicy* csp = frame()->document()->contentSecurityPolicy();
    context->AllowCodeGenerationFromStrings(
        csp->allowEval(0, SecurityViolationReportingPolicy::SuppressReporting));
    context->SetErrorMessageForCodeGenerationFromStrings(
        v8String(isolate(), csp->evalDisabledErrorMessage()));
  } else {
    updateActivityLogger();
    origin = m_world->isolatedWorldSecurityOrigin();
    setSecurityToken(origin);
  }

  MainThreadDebugger::instance()->contextCreated(m_scriptState.get(), frame(),
                                                 origin);
  frame()->loader().client()->didCreateScriptContext(context,
                                                     m_world->worldId());
  // If conditional features for window have been queued before the V8 context
  // was ready, then inject them into the context now
  if (m_world->isMainWorld()) {
    installConditionalFeaturesOnWindow(m_scriptState.get());
  }

  if (m_world->isMainWorld())
    frame()->loader().dispatchDidClearWindowObjectInMainWorld();
}

void LocalWindowProxy::createContext() {
  // Create a new v8::Context with the window object as the global object
  // (aka the inner global).  Reuse the global proxy object (aka the outer
  // global) if it already exists.  See the comments in
  // setupWindowPrototypeChain for the structure of the prototype chain of
  // the global object.
  v8::Local<v8::ObjectTemplate> globalTemplate =
      V8Window::domTemplate(isolate(), *m_world)->InstanceTemplate();
  CHECK(!globalTemplate.IsEmpty());

  Vector<const char*> extensionNames;
  // Dynamically tell v8 about our extensions now.
  if (frame()->loader().client()->allowScriptExtensions()) {
    const V8Extensions& extensions = ScriptController::registeredExtensions();
    extensionNames.reserveInitialCapacity(extensions.size());
    for (const auto* extension : extensions)
      extensionNames.push_back(extension->name());
  }
  v8::ExtensionConfiguration extensionConfiguration(extensionNames.size(),
                                                    extensionNames.data());

  v8::Local<v8::Context> context;
  {
    V8PerIsolateData::UseCounterDisabledScope useCounterDisabled(
        V8PerIsolateData::from(isolate()));
    context =
        v8::Context::New(isolate(), &extensionConfiguration, globalTemplate,
                         m_globalProxy.newLocal(isolate()));
  }
  CHECK(!context.IsEmpty());

  m_scriptState = ScriptState::create(context, m_world);

  // TODO(haraken): Currently we cannot enable the following DCHECK because
  // an already detached window proxy can be re-initialized. This is wrong.
  // DCHECK(m_lifecycle == Lifecycle::ContextUninitialized);
  m_lifecycle = Lifecycle::ContextInitialized;
  DCHECK(m_scriptState->contextIsValid());
}

void LocalWindowProxy::updateDocumentProperty() {
  DCHECK(m_world->isMainWorld());

  ScriptState::Scope scope(m_scriptState.get());
  v8::Local<v8::Context> context = m_scriptState->context();
  v8::Local<v8::Value> documentWrapper =
      ToV8(frame()->document(), context->Global(), isolate());
  DCHECK(documentWrapper->IsObject());
  // Update the cached accessor for window.document.
  CHECK(V8PrivateProperty::getWindowDocumentCachedAccessor(isolate()).set(
      context, context->Global(), documentWrapper));
}

void LocalWindowProxy::updateActivityLogger() {
  m_scriptState->perContextData()->setActivityLogger(
      V8DOMActivityLogger::activityLogger(
          m_world->worldId(),
          frame()->document() ? frame()->document()->baseURI() : KURL()));
}

void LocalWindowProxy::setSecurityToken(SecurityOrigin* origin) {
  // If two tokens are equal, then the SecurityOrigins canAccess each other.
  // If two tokens are not equal, then we have to call canAccess.
  // Note: we can't use the HTTPOrigin if it was set from the DOM.
  String token;
  // If document.domain is modified, v8 needs to do a full canAccess check,
  // so always use an empty security token in that case.
  bool delaySet = m_world->isMainWorld() && origin->domainWasSetInDOM();
  if (origin && !delaySet)
    token = origin->toString();

  // An empty or "null" token means we always have to call
  // canAccess. The toString method on securityOrigins returns the
  // string "null" for empty security origins and for security
  // origins that should only allow access to themselves. In this
  // case, we use the global object as the security token to avoid
  // calling canAccess when a script accesses its own objects.
  v8::HandleScope handleScope(isolate());
  v8::Local<v8::Context> context = m_scriptState->context();
  if (token.isEmpty() || token == "null") {
    context->UseDefaultSecurityToken();
    return;
  }

  if (m_world->isIsolatedWorld()) {
    SecurityOrigin* frameSecurityOrigin =
        frame()->document()->getSecurityOrigin();
    String frameSecurityToken = frameSecurityOrigin->toString();
    // We need to check the return value of domainWasSetInDOM() on the
    // frame's SecurityOrigin because, if that's the case, only
    // SecurityOrigin::m_domain would have been modified.
    // m_domain is not used by SecurityOrigin::toString(), so we would end
    // up generating the same token that was already set.
    if (frameSecurityOrigin->domainWasSetInDOM() ||
        frameSecurityToken.isEmpty() || frameSecurityToken == "null") {
      context->UseDefaultSecurityToken();
      return;
    }
    token = frameSecurityToken + token;
  }

  // NOTE: V8 does identity comparison in fast path, must use a symbol
  // as the security token.
  context->SetSecurityToken(v8AtomicString(isolate(), token));
}

void LocalWindowProxy::updateDocument() {
  DCHECK(m_world->isMainWorld());
  // For an uninitialized main window proxy, there's nothing we need
  // to update. The update is done when the window proxy gets initialized later.
  if (m_lifecycle == Lifecycle::ContextUninitialized)
    return;

  // If this WindowProxy was previously initialized, reinitialize it now to
  // preserve JS object identity. Otherwise, extant references to the
  // WindowProxy will be broken.
  if (m_lifecycle == Lifecycle::ContextDetached) {
    initialize();
    DCHECK_EQ(Lifecycle::ContextInitialized, m_lifecycle);
    // Initialization internally updates the document properties, so just
    // return afterwards.
    return;
  }

  updateDocumentInternal();
}

void LocalWindowProxy::updateDocumentInternal() {
  updateActivityLogger();
  updateDocumentProperty();
  updateSecurityOrigin(frame()->document()->getSecurityOrigin());
}

static v8::Local<v8::Value> getNamedProperty(
    HTMLDocument* htmlDocument,
    const AtomicString& key,
    v8::Local<v8::Object> creationContext,
    v8::Isolate* isolate) {
  if (!htmlDocument->hasNamedItem(key) && !htmlDocument->hasExtraNamedItem(key))
    return v8Undefined();

  DocumentNameCollection* items = htmlDocument->documentNamedItems(key);
  if (items->isEmpty())
    return v8Undefined();

  if (items->hasExactlyOneItem()) {
    HTMLElement* element = items->item(0);
    DCHECK(element);
    Frame* frame = isHTMLIFrameElement(*element)
                       ? toHTMLIFrameElement(*element).contentFrame()
                       : 0;
    if (frame)
      return ToV8(frame->domWindow(), creationContext, isolate);
    return ToV8(element, creationContext, isolate);
  }
  return ToV8(items, creationContext, isolate);
}

static void getter(v8::Local<v8::Name> property,
                   const v8::PropertyCallbackInfo<v8::Value>& info) {
  if (!property->IsString())
    return;
  // FIXME: Consider passing StringImpl directly.
  AtomicString name = toCoreAtomicString(property.As<v8::String>());
  HTMLDocument* htmlDocument = V8HTMLDocument::toImpl(info.Holder());
  DCHECK(htmlDocument);
  v8::Local<v8::Value> result =
      getNamedProperty(htmlDocument, name, info.Holder(), info.GetIsolate());
  if (!result.IsEmpty()) {
    v8SetReturnValue(info, result);
    return;
  }
  v8::Local<v8::Value> value;
  if (info.Holder()
          ->GetRealNamedPropertyInPrototypeChain(
              info.GetIsolate()->GetCurrentContext(), property.As<v8::String>())
          .ToLocal(&value))
    v8SetReturnValue(info, value);
}

void LocalWindowProxy::namedItemAdded(HTMLDocument* document,
                                      const AtomicString& name) {
  DCHECK(m_world->isMainWorld());

  // Context must be initialized before this point.
  DCHECK(m_lifecycle >= Lifecycle::ContextInitialized);
  // TODO(yukishiino): Is it okay to not update named properties
  // after the context gets detached?
  if (m_lifecycle == Lifecycle::ContextDetached)
    return;

  ScriptState::Scope scope(m_scriptState.get());
  v8::Local<v8::Object> documentWrapper =
      m_world->domDataStore().get(document, isolate());
  // TODO(yukishiino,peria): We should check if the own property with the same
  // name already exists or not, and if it exists, we shouldn't define a new
  // accessor property (it fails).
  documentWrapper->SetAccessor(isolate()->GetCurrentContext(),
                               v8String(isolate(), name), getter);
}

void LocalWindowProxy::namedItemRemoved(HTMLDocument* document,
                                        const AtomicString& name) {
  DCHECK(m_world->isMainWorld());

  // Context must be initialized before this point.
  DCHECK(m_lifecycle >= Lifecycle::ContextInitialized);
  // TODO(yukishiino): Is it okay to not update named properties
  // after the context gets detached?
  if (m_lifecycle == Lifecycle::ContextDetached)
    return;

  if (document->hasNamedItem(name) || document->hasExtraNamedItem(name))
    return;
  ScriptState::Scope scope(m_scriptState.get());
  v8::Local<v8::Object> documentWrapper =
      m_world->domDataStore().get(document, isolate());
  documentWrapper
      ->Delete(isolate()->GetCurrentContext(), v8String(isolate(), name))
      .ToChecked();
}

void LocalWindowProxy::updateSecurityOrigin(SecurityOrigin* origin) {
  // For an uninitialized main window proxy, there's nothing we need
  // to update. The update is done when the window proxy gets initialized later.
  if (m_lifecycle == Lifecycle::ContextUninitialized)
    return;
  // TODO(yukishiino): Is it okay to not update security origin when the context
  // is detached?
  if (m_lifecycle == Lifecycle::ContextDetached)
    return;

  setSecurityToken(origin);
}

LocalWindowProxy::LocalWindowProxy(v8::Isolate* isolate,
                                   LocalFrame& frame,
                                   RefPtr<DOMWrapperWorld> world)
    : WindowProxy(isolate, frame, std::move(world)) {}

}  // namespace blink
