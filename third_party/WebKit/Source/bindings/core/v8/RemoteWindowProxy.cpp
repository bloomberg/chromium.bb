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

#include "bindings/core/v8/RemoteWindowProxy.h"

#include <algorithm>
#include <utility>

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8GCForContextDispose.h"
#include "bindings/core/v8/V8Initializer.h"
#include "bindings/core/v8/V8Window.h"
#include "platform/Histogram.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "v8/include/v8.h"
#include "wtf/Assertions.h"

namespace blink {

RemoteWindowProxy::RemoteWindowProxy(v8::Isolate* isolate,
                                     RemoteFrame& frame,
                                     RefPtr<DOMWrapperWorld> world)
    : WindowProxy(isolate, frame, std::move(world)) {}

void RemoteWindowProxy::disposeContext(GlobalDetachmentBehavior behavior) {
  if (m_lifecycle != Lifecycle::ContextInitialized)
    return;

  if (behavior == DetachGlobal) {
    v8::Local<v8::Context> context = m_scriptState->context();
    // Clean up state on the global proxy, which will be reused.
    if (!m_globalProxy.isEmpty()) {
      CHECK(m_globalProxy == context->Global());
      CHECK_EQ(toScriptWrappable(context->Global()),
               toScriptWrappable(
                   context->Global()->GetPrototype().As<v8::Object>()));
      m_globalProxy.get().SetWrapperClassId(0);
    }
    V8DOMWrapper::clearNativeInfo(isolate(), context->Global());
    m_scriptState->detachGlobalObject();

#if DCHECK_IS_ON()
    didDetachGlobalObject();
#endif
  }

  m_scriptState->disposePerContextData();

  // It's likely that disposing the context has created a lot of
  // garbage. Notify V8 about this so it'll have a chance of cleaning
  // it up when idle.
  V8GCForContextDispose::instance().notifyContextDisposed(
      frame()->isMainFrame());

  DCHECK(m_lifecycle == Lifecycle::ContextInitialized);
  m_lifecycle = Lifecycle::ContextDetached;
}

void RemoteWindowProxy::initialize() {
  TRACE_EVENT1("v8", "RemoteWindowProxy::initialize", "isMainWindow",
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

  // Remote frames always require a full canAccess() check.
  context->UseDefaultSecurityToken();
}

void RemoteWindowProxy::createContext() {
  // Create a new v8::Context with the window object as the global object
  // (aka the inner global). Reuse the outer global proxy if it already exists.
  v8::Local<v8::ObjectTemplate> globalTemplate =
      V8Window::domTemplate(isolate(), *m_world)->InstanceTemplate();
  CHECK(!globalTemplate.IsEmpty());

  v8::Local<v8::Context> context;
  {
    V8PerIsolateData::UseCounterDisabledScope useCounterDisabled(
        V8PerIsolateData::from(isolate()));
    context = v8::Context::New(isolate(), nullptr, globalTemplate,
                               m_globalProxy.newLocal(isolate()));
  }
  CHECK(!context.IsEmpty());

#if DCHECK_IS_ON()
  didAttachGlobalObject();
#endif

  m_scriptState = ScriptState::create(context, m_world);

  // TODO(haraken): Currently we cannot enable the following DCHECK because
  // an already detached window proxy can be re-initialized. This is wrong.
  // DCHECK(m_lifecycle == Lifecycle::ContextUninitialized);
  m_lifecycle = Lifecycle::ContextInitialized;
  DCHECK(m_scriptState->contextIsValid());
}

void RemoteWindowProxy::setupWindowPrototypeChain() {
  // Associate the window wrapper object and its prototype chain with the
  // corresponding native DOMWindow object.
  DOMWindow* window = frame()->domWindow();
  const WrapperTypeInfo* wrapperTypeInfo = window->wrapperTypeInfo();
  v8::Local<v8::Context> context = m_scriptState->context();

  // The global proxy object.  Note this is not the global object.
  v8::Local<v8::Object> globalProxy = context->Global();
  CHECK(m_globalProxy == globalProxy);
  V8DOMWrapper::setNativeInfo(isolate(), globalProxy, wrapperTypeInfo, window);
  // Mark the handle to be traced by Oilpan, since the global proxy has a
  // reference to the DOMWindow.
  m_globalProxy.get().SetWrapperClassId(wrapperTypeInfo->wrapperClassId);

  // The global object, aka window wrapper object.
  v8::Local<v8::Object> windowWrapper =
      globalProxy->GetPrototype().As<v8::Object>();
  windowWrapper = V8DOMWrapper::associateObjectWithWrapper(
      isolate(), window, wrapperTypeInfo, windowWrapper);

  // The prototype object of Window interface.
  v8::Local<v8::Object> windowPrototype =
      windowWrapper->GetPrototype().As<v8::Object>();
  CHECK(!windowPrototype.IsEmpty());
  V8DOMWrapper::setNativeInfo(isolate(), windowPrototype, wrapperTypeInfo,
                              window);

  // The named properties object of Window interface.
  v8::Local<v8::Object> windowProperties =
      windowPrototype->GetPrototype().As<v8::Object>();
  CHECK(!windowProperties.IsEmpty());
  V8DOMWrapper::setNativeInfo(isolate(), windowProperties, wrapperTypeInfo,
                              window);
}

}  // namespace blink
