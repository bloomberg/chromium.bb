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

#include "bindings/core/v8/WindowProxy.h"

#include <algorithm>
#include <utility>

#include "bindings/core/v8/ConditionalFeatures.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "bindings/core/v8/V8Document.h"
#include "bindings/core/v8/V8GCForContextDispose.h"
#include "bindings/core/v8/V8HTMLCollection.h"
#include "bindings/core/v8/V8HTMLDocument.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/V8Initializer.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "bindings/core/v8/V8PagePopupControllerBinding.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "bindings/core/v8/V8Window.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/DocumentNameCollection.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "v8/include/v8-debug.h"
#include "v8/include/v8.h"
#include "wtf/Assertions.h"
#include "wtf/StringExtras.h"
#include "wtf/text/CString.h"

namespace blink {

RemoteWindowProxy::RemoteWindowProxy(v8::Isolate* isolate,
                                     RemoteFrame& frame,
                                     RefPtr<DOMWrapperWorld> world)
    : WindowProxy(isolate, frame, std::move(world)) {}

void RemoteWindowProxy::disposeContext(GlobalDetachmentBehavior behavior) {
  if (m_lifecycle != Lifecycle::ContextInitialized)
    return;

  WindowProxy::disposeContext(behavior);
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
  // (aka the inner global).  Reuse the global proxy object (aka the outer
  // global) if it already exists.  See the comments in
  // setupWindowPrototypeChain for the structure of the prototype chain of
  // the global object.
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

  m_scriptState = ScriptState::create(context, m_world);

  // TODO(haraken): Currently we cannot enable the following DCHECK because
  // an already detached window proxy can be re-initialized. This is wrong.
  // DCHECK(m_lifecycle == Lifecycle::ContextUninitialized);
  m_lifecycle = Lifecycle::ContextInitialized;
  DCHECK(m_scriptState->contextIsValid());
}

}  // namespace blink
