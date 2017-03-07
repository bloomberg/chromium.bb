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
#include "bindings/core/v8/V8PagePopupControllerBinding.h"
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

void WindowProxy::disposeContext(GlobalDetachmentBehavior behavior) {
  DCHECK(m_lifecycle == Lifecycle::ContextInitialized);

  if (behavior == DetachGlobal) {
    v8::Local<v8::Context> context = m_scriptState->context();
    // Clean up state on the global proxy, which will be reused.
    if (!m_globalProxy.isEmpty()) {
      // TODO(yukishiino): This DCHECK failed on Canary (M57) and Dev (M56).
      // We need to figure out why m_globalProxy != context->Global().
      DCHECK(m_globalProxy == context->Global());
      DCHECK_EQ(toScriptWrappable(context->Global()),
                toScriptWrappable(
                    context->Global()->GetPrototype().As<v8::Object>()));
      m_globalProxy.get().SetWrapperClassId(0);
    }
    V8DOMWrapper::clearNativeInfo(m_isolate, context->Global());
    m_scriptState->detachGlobalObject();
  }

  m_scriptState->disposePerContextData();

  // It's likely that disposing the context has created a lot of
  // garbage. Notify V8 about this so it'll have a chance of cleaning
  // it up when idle.
  V8GCForContextDispose::instance().notifyContextDisposed(
      m_frame->isMainFrame());

  DCHECK(m_lifecycle == Lifecycle::ContextInitialized);
  m_lifecycle = Lifecycle::ContextDetached;
}

void WindowProxy::clearForClose() {
  disposeContext(DoNotDetachGlobal);
}

void WindowProxy::clearForNavigation() {
  disposeContext(DetachGlobal);
}

v8::Local<v8::Object> WindowProxy::globalIfNotDetached() {
  if (m_lifecycle == Lifecycle::ContextInitialized) {
    DCHECK(m_scriptState->contextIsValid());
    DCHECK(m_globalProxy == m_scriptState->context()->Global());
    return m_globalProxy.newLocal(m_isolate);
  }
  return v8::Local<v8::Object>();
}

v8::Local<v8::Object> WindowProxy::releaseGlobal() {
  DCHECK(m_lifecycle != Lifecycle::ContextInitialized);
  // Make sure the global object was detached from the proxy by calling
  // clearForNavigation().
  if (m_lifecycle == Lifecycle::ContextDetached)
    ASSERT(m_scriptState->isGlobalObjectDetached());

  v8::Local<v8::Object> global = m_globalProxy.newLocal(m_isolate);
  m_globalProxy.clear();
  return global;
}

void WindowProxy::setGlobal(v8::Local<v8::Object> global) {
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

void WindowProxy::setupWindowPrototypeChain() {
  // Associate the window wrapper object and its prototype chain with the
  // corresponding native DOMWindow object.
  // The full structure of the global object's prototype chain is as follows:
  //
  // global proxy object [1]
  //   -- has prototype --> global object (window wrapper object) [2]
  //   -- has prototype --> Window.prototype
  //   -- has prototype --> WindowProperties [3]
  //   -- has prototype --> EventTarget.prototype
  //   -- has prototype --> Object.prototype
  //   -- has prototype --> null
  //
  // [1] Global proxy object is as known as "outer global object".  It's an
  //   empty object and remains after navigation.  When navigated, points to
  //   a different global object as the prototype object.
  // [2] Global object is as known as "inner global object" or "window wrapper
  //   object".  The prototype chain between global proxy object and global
  //   object is NOT observable from user JavaScript code.  All other
  //   prototype chains are observable.  Global proxy object and global object
  //   together appear to be the same single JavaScript object.  See also:
  //     https://wiki.mozilla.org/Gecko:SplitWindow
  //   global object (= window wrapper object) provides most of Window's DOM
  //   attributes and operations.  Also global variables defined by user
  //   JavaScript are placed on this object.  When navigated, a new global
  //   object is created together with a new v8::Context, but the global proxy
  //   object doesn't change.
  // [3] WindowProperties is a named properties object of Window interface.

  DOMWindow* window = m_frame->domWindow();
  const WrapperTypeInfo* wrapperTypeInfo = window->wrapperTypeInfo();
  v8::Local<v8::Context> context = m_scriptState->context();

  // The global proxy object.  Note this is not the global object.
  v8::Local<v8::Object> globalProxy = context->Global();
  CHECK(m_globalProxy == globalProxy);
  V8DOMWrapper::setNativeInfo(m_isolate, globalProxy, wrapperTypeInfo, window);
  // Mark the handle to be traced by Oilpan, since the global proxy has a
  // reference to the DOMWindow.
  m_globalProxy.get().SetWrapperClassId(wrapperTypeInfo->wrapperClassId);

  // The global object, aka window wrapper object.
  v8::Local<v8::Object> windowWrapper =
      globalProxy->GetPrototype().As<v8::Object>();
  windowWrapper = V8DOMWrapper::associateObjectWithWrapper(
      m_isolate, window, wrapperTypeInfo, windowWrapper);

  // The prototype object of Window interface.
  v8::Local<v8::Object> windowPrototype =
      windowWrapper->GetPrototype().As<v8::Object>();
  CHECK(!windowPrototype.IsEmpty());
  V8DOMWrapper::setNativeInfo(m_isolate, windowPrototype, wrapperTypeInfo,
                              window);

  // The named properties object of Window interface.
  v8::Local<v8::Object> windowProperties =
      windowPrototype->GetPrototype().As<v8::Object>();
  CHECK(!windowProperties.IsEmpty());
  V8DOMWrapper::setNativeInfo(m_isolate, windowProperties, wrapperTypeInfo,
                              window);

  // TODO(keishi): Remove installPagePopupController and implement
  // PagePopupController in another way.
  V8PagePopupControllerBinding::installPagePopupController(context,
                                                           windowWrapper);
}

}  // namespace blink
