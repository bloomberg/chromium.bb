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
#include "bindings/core/v8/ScriptState.h"
#include "platform/heap/Handle.h"
#include "v8/include/v8.h"
#include "wtf/RefPtr.h"

namespace blink {

class Frame;
class ScriptController;

// WindowProxy represents all the per-global object state for a Frame that
// persist between navigations.
class WindowProxy : public GarbageCollectedFinalized<WindowProxy> {
 public:
  virtual ~WindowProxy();

  DECLARE_TRACE();

  v8::Local<v8::Context> contextIfInitialized() const {
    return m_scriptState ? m_scriptState->context() : v8::Local<v8::Context>();
  }
  void initializeIfNeeded();

  void clearForClose();
  void clearForNavigation();

  v8::Local<v8::Object> globalIfNotDetached();
  v8::Local<v8::Object> releaseGlobal();
  void setGlobal(v8::Local<v8::Object>);

  // TODO(dcheng): Temporarily exposed to avoid include cycles. Remove the need
  // for this and remove this getter.
  DOMWrapperWorld& world() { return *m_world; }

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
  virtual void disposeContext(GlobalDetachmentBehavior);

  // Associates the window wrapper and its prototype chain with the native
  // DOMWindow object.  Also does some more Window-specific initialization.
  void setupWindowPrototypeChain();

  v8::Isolate* isolate() const { return m_isolate; }
  Frame* frame() const { return m_frame.get(); }
  ScriptState* getScriptState() const { return m_scriptState.get(); }

 private:
  v8::Isolate* const m_isolate;
  const Member<Frame> m_frame;

 protected:
  // TODO(dcheng): Move this to LocalWindowProxy once RemoteWindowProxy uses
  // remote contexts.
  RefPtr<ScriptState> m_scriptState;
  // TODO(dcheng): Consider making these private and using getters.
  const RefPtr<DOMWrapperWorld> m_world;
  ScopedPersistent<v8::Object> m_globalProxy;
  Lifecycle m_lifecycle;
};

}  // namespace blink

#endif  // WindowProxy_h
