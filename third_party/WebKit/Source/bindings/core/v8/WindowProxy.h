/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WindowProxy_h
#define WindowProxy_h

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScopedPersistent.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "v8/include/v8.h"
#include "wtf/RefPtr.h"

namespace blink {

class Frame;
class ScriptController;

// WindowProxy implements the split window model of a window for a frame. In the
// HTML standard, the split window model is composed of the Window interface
// (the inner global object) and the WindowProxy interface (the outer global
// proxy).
//
// The Window interface is backed by the Blink DOMWindow C++ implementation.
// In contrast, the WindowProxy interface does not have a corresponding
// C++ implementation in Blink: the WindowProxy class defined here only manages
// context initialization and detach. Instead, the behavior of the WindowProxy
// interface is defined by JSGlobalProxy in v8 and the prototype chain set up
// during context initialization.
//
// ====== Inner Global Object ======
// The inner global object is the global for the script environment of a Frame.
// Since Window and Document also have a 1:1 relationship, this means that each
// inner global object has an associated Document which does not change. On
// navigation, the new Document receives a new inner global object.
//
// However, there is one exception to the 1:1 DOMWindow:Document rule. If:
// - the previous Document is the initial empty document
// - the new Document is same-origin to the previous Document
// then the inner global object will be reused for the new Document. This is the
// only case where the associated Document of an inner global object can change.
//
// All methods and attributes defined on the Window interface are exposed via
// the inner global object. Global variables defined by script running in the
// Document also live on the inner global object.
//
// ====== Outer Global Proxy ====
// The outer global proxy is reused across navigations. It implements the
// security checks for same-origin/cross-origin access to the Window interface.
// When the check passes (i.e. the access is same-origin), the access is
// forwarded to the inner global object of the active Document in this
// WindowProxy's Frame).
//
// When the security check fails, the access is delegated to the outer global
// proxy's cross-origin interceptors. The cross-origin interceptors may choose
// to return a value (if the property is exposed cross-origin) or throw an
// exception otherwise.
//
// Note that the cross-origin interceptors are only used for cross-origin
// accesses: a same-origin access to a method that is available cross-origin,
// such as Window.postMessage, will be delegated to the inner global object.
//
// ====== LocalWindowProxy vs RemoteWindowProxy ======
// WindowProxy has two concrete subclasses:
// - LocalWindowProxy: implements the split window model for a frame in the same
//   process, i.e. a LocalFrame.
// - RemoteWindowProxy: implements the split window model for a frame in a
//   different process, i.e. a RemoteFrame.
//
// While having a RemoteFrame implies the frame must be cross-origin, the
// opposite is not true: a LocalFrame can be same-origin or cross-origin. One
// additional complexity (which slightly violates the HTML standard): it is
// possible to have SecurityOrigin::canAccess() return true for a RemoteFrame's
// security origin; however, it is important to still deny access as if the
// frame were cross-origin. This is due to complexities in the process
// allocation model for renderer processes. See https://crbug.com/601629.
//
// ====== LocalWindowProxy/RemoteWindowProxy ======
// Currently, the prototype chain for LocalWindowProxy and RemoteWindowProxy
// look the same:
//
//   outer global proxy
//     -- has prototype --> inner global object
//     -- has prototype --> Window.prototype
//     -- has prototype --> WindowProperties [1]
//     -- has prototype --> EventTarget.prototype
//     -- has prototype --> Object.prototype
//     -- has prototype --> null
//
// [1] WindowProperties is the named properties object of the Window interface.
//
// There is work in progress to refactor RemoteWindowProxy to use remote v8
// contexts, to reduce the overhead of remote frames.
//
// ====== References ======
// https://wiki.mozilla.org/Gecko:SplitWindow
// https://whatwg.org/C/browsers.html#the-windowproxy-exotic-object
class WindowProxy : public GarbageCollectedFinalized<WindowProxy> {
 public:
  virtual ~WindowProxy();

  DECLARE_TRACE();

  void initializeIfNeeded();

  void clearForClose();
  void clearForNavigation();

  CORE_EXPORT v8::Local<v8::Object> globalIfNotDetached();
  v8::Local<v8::Object> releaseGlobal();
  void setGlobal(v8::Local<v8::Object>);

  // TODO(dcheng): Temporarily exposed to avoid include cycles. Remove the need
  // for this and remove this getter.
  DOMWrapperWorld& world() { return *m_world; }

  virtual bool isLocal() const { return false; }

 protected:
  // TODO(dcheng): Remove this friend declaration once LocalWindowProxyManager
  // and ScriptController are merged.
  friend class ScriptController;

  // A valid transition is from ContextUninitialized to ContextInitialized,
  // and then ContextDetached. Other transitions are forbidden.
  enum class Lifecycle {
    ContextUninitialized,
    ContextInitialized,
    ContextDetached,
  };

  WindowProxy(v8::Isolate*, Frame&, RefPtr<DOMWrapperWorld>);

  virtual void initialize() = 0;

  enum GlobalDetachmentBehavior { DoNotDetachGlobal, DetachGlobal };
  virtual void disposeContext(GlobalDetachmentBehavior) = 0;

  v8::Isolate* isolate() const { return m_isolate; }
  Frame* frame() const { return m_frame.get(); }

#if DCHECK_IS_ON()
  void didAttachGlobalObject() { m_isGlobalObjectAttached = true; }
  void didDetachGlobalObject() { m_isGlobalObjectAttached = false; }
#endif

 private:
  v8::Isolate* const m_isolate;
  const Member<Frame> m_frame;
#if DCHECK_IS_ON()
  bool m_isGlobalObjectAttached = false;
#endif

 protected:
  // TODO(dcheng): Consider making these private and using getters.
  const RefPtr<DOMWrapperWorld> m_world;
  ScopedPersistent<v8::Object> m_globalProxy;
  Lifecycle m_lifecycle;
};

}  // namespace blink

#endif  // WindowProxy_h
