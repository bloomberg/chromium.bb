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

#include <utility>

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8GCForContextDispose.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/Frame.h"
#include "v8/include/v8.h"
#include "wtf/Assertions.h"

namespace blink {

WindowProxy::~WindowProxy() {
  // clearForClose() or clearForNavigation() must be invoked before destruction
  // starts.
  DCHECK(m_lifecycle != Lifecycle::ContextInitialized);
}

DEFINE_TRACE(WindowProxy) {
  visitor->trace(m_frame);
}

WindowProxy::WindowProxy(v8::Isolate* isolate,
                         Frame& frame,
                         RefPtr<DOMWrapperWorld> world)
    : m_isolate(isolate),
      m_frame(frame),

      m_world(std::move(world)),
      m_lifecycle(Lifecycle::ContextUninitialized) {}

void WindowProxy::clearForClose() {
  disposeContext(DoNotDetachGlobal);
}

void WindowProxy::clearForNavigation() {
  disposeContext(DetachGlobal);
}

v8::Local<v8::Object> WindowProxy::globalIfNotDetached() {
  if (m_lifecycle == Lifecycle::ContextInitialized) {
    DLOG_IF(FATAL, !m_isGlobalObjectAttached)
        << "Context is initialized but global object is detached!";
    return m_globalProxy.newLocal(m_isolate);
  }
  return v8::Local<v8::Object>();
}

v8::Local<v8::Object> WindowProxy::releaseGlobal() {
  DCHECK(m_lifecycle != Lifecycle::ContextInitialized);

  // Make sure the global object was detached from the proxy by calling
  // clearForNavigation().
  DLOG_IF(FATAL, m_isGlobalObjectAttached)
      << "Context not detached by calling clearForNavigation()";

  v8::Local<v8::Object> global = m_globalProxy.newLocal(m_isolate);
  m_globalProxy.clear();
  return global;
}

void WindowProxy::setGlobal(v8::Local<v8::Object> global) {
  CHECK(m_globalProxy.isEmpty());
  m_globalProxy.set(m_isolate, global);

  // Initialize the window proxy now, to re-establish the connection between
  // the global object and the v8::Context. This is really only needed for a
  // RemoteDOMWindow, since it has no scripting environment of its own.
  // Without this, existing script references to a swapped in RemoteDOMWindow
  // would be broken until that RemoteDOMWindow was vended again through an
  // interface like window.frames.
  initializeIfNeeded();
}

// Create a new environment and setup the global object.
//
// The global object corresponds to a DOMWindow instance. However, to
// allow properties of the JS DOMWindow instance to be shadowed, we
// use a shadow object as the global object and use the JS DOMWindow
// instance as the prototype for that shadow object. The JS DOMWindow
// instance is undetectable from JavaScript code because the __proto__
// accessors skip that object.
//
// The shadow object and the DOMWindow instance are seen as one object
// from JavaScript. The JavaScript object that corresponds to a
// DOMWindow instance is the shadow object. When mapping a DOMWindow
// instance to a V8 object, we return the shadow object.
//
// To implement split-window, see
//   1) https://bugs.webkit.org/show_bug.cgi?id=17249
//   2) https://wiki.mozilla.org/Gecko:SplitWindow
//   3) https://bugzilla.mozilla.org/show_bug.cgi?id=296639
// we need to split the shadow object further into two objects:
// an outer window and an inner window. The inner window is the hidden
// prototype of the outer window. The inner window is the default
// global object of the context. A variable declared in the global
// scope is a property of the inner window.
//
// The outer window sticks to a LocalFrame, it is exposed to JavaScript
// via window.window, window.self, window.parent, etc. The outer window
// has a security token which is the domain. The outer window cannot
// have its own properties. window.foo = 'x' is delegated to the
// inner window.
//
// When a frame navigates to a new page, the inner window is cut off
// the outer window, and the outer window identify is preserved for
// the frame. However, a new inner window is created for the new page.
// If there are JS code holds a closure to the old inner window,
// it won't be able to reach the outer window via its global object.
void WindowProxy::initializeIfNeeded() {
  // TODO(haraken): It is wrong to re-initialize an already detached window
  // proxy. This must be 'if(m_lifecycle == Lifecycle::ContextUninitialized)'.
  if (m_lifecycle != Lifecycle::ContextInitialized) {
    initialize();
  }
}

v8::Local<v8::Object> WindowProxy::associateWithWrapper(
    DOMWindow* window,
    const WrapperTypeInfo* wrapperTypeInfo,
    v8::Local<v8::Object> wrapper) {
  if (m_world->domDataStore().set(m_isolate, window, wrapperTypeInfo,
                                  wrapper)) {
    wrapperTypeInfo->wrapperCreated();
    V8DOMWrapper::setNativeInfo(m_isolate, wrapper, wrapperTypeInfo, window);
    DCHECK(V8DOMWrapper::hasInternalFieldsSet(wrapper));
  }
  SECURITY_CHECK(toScriptWrappable(wrapper) == window);
  return wrapper;
}

}  // namespace blink
