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
#include "bindings/core/v8/V8Window.h"
#include "platform/Histogram.h"
#include "platform/ScriptForbiddenScope.h"
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

  if (behavior == DetachGlobal && !m_globalProxy.isEmpty()) {
    m_globalProxy.get().SetWrapperClassId(0);
    V8DOMWrapper::clearNativeInfo(isolate(), m_globalProxy.newLocal(isolate()));
#if DCHECK_IS_ON()
    didDetachGlobalObject();
#endif
  }

  DCHECK(m_lifecycle == Lifecycle::ContextInitialized);
  m_lifecycle = Lifecycle::ContextDetached;
}

void RemoteWindowProxy::initialize() {
  TRACE_EVENT1("v8", "RemoteWindowProxy::initialize", "isMainWindow",
               frame()->isMainFrame());
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER(
      frame()->isMainFrame()
          ? "Blink.Binding.InitializeMainRemoteWindowProxy"
          : "Blink.Binding.InitializeNonMainRemoteWindowProxy");

  ScriptForbiddenScope::AllowUserAgentScript allowScript;

  v8::HandleScope handleScope(isolate());
  createContext();
  setupWindowPrototypeChain();
}

void RemoteWindowProxy::createContext() {
  // Create a new v8::Context with the window object as the global object
  // (aka the inner global). Reuse the outer global proxy if it already exists.
  v8::Local<v8::ObjectTemplate> globalTemplate =
      V8Window::domTemplate(isolate(), *m_world)->InstanceTemplate();
  CHECK(!globalTemplate.IsEmpty());

  v8::Local<v8::Object> globalProxy =
      v8::Context::NewRemoteContext(isolate(), globalTemplate,
                                    m_globalProxy.newLocal(isolate()))
          .ToLocalChecked();
  if (m_globalProxy.isEmpty())
    m_globalProxy.set(isolate(), globalProxy);
  else
    DCHECK(m_globalProxy.get() == globalProxy);
  CHECK(!m_globalProxy.isEmpty());

#if DCHECK_IS_ON()
  didAttachGlobalObject();
#endif

  // TODO(haraken): Currently we cannot enable the following DCHECK because
  // an already detached window proxy can be re-initialized. This is wrong.
  // DCHECK(m_lifecycle == Lifecycle::ContextUninitialized);
  m_lifecycle = Lifecycle::ContextInitialized;
}

void RemoteWindowProxy::setupWindowPrototypeChain() {
  // Associate the window wrapper object and its prototype chain with the
  // corresponding native DOMWindow object.
  DOMWindow* window = frame()->domWindow();
  const WrapperTypeInfo* wrapperTypeInfo = window->wrapperTypeInfo();

  // The global proxy object.  Note this is not the global object.
  v8::Local<v8::Object> globalProxy = m_globalProxy.newLocal(isolate());
  CHECK(m_globalProxy == globalProxy);
  V8DOMWrapper::setNativeInfo(isolate(), globalProxy, wrapperTypeInfo, window);
  // Mark the handle to be traced by Oilpan, since the global proxy has a
  // reference to the DOMWindow.
  m_globalProxy.get().SetWrapperClassId(wrapperTypeInfo->wrapperClassId);

  // The global object, aka window wrapper object.
  v8::Local<v8::Object> windowWrapper =
      globalProxy->GetPrototype().As<v8::Object>();
  v8::Local<v8::Object> associatedWrapper =
      associateWithWrapper(window, wrapperTypeInfo, windowWrapper);
  DCHECK(associatedWrapper == windowWrapper);
}

}  // namespace blink
