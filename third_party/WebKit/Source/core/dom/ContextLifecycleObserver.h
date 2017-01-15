/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef ContextLifecycleObserver_h
#define ContextLifecycleObserver_h

#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "platform/LifecycleObserver.h"

namespace blink {

class LocalDOMWindow;
class LocalFrame;

// ContextClient and ContextLifecycleObserver are helpers to associate an
// object with an ExecutionContext.
//
// - getExecutionContext() returns null after the context is detached.
// - frame() is a syntax sugar for getExecutionContext()->frame(). It returns
//   null after the context is detached or the context is not a Document.
class CORE_EXPORT ContextClient : public GarbageCollectedMixin {
 public:
  ExecutionContext* getExecutionContext() const;
  LocalFrame* frame() const;

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit ContextClient(ExecutionContext*);
  explicit ContextClient(LocalFrame*);

 private:
  WeakMember<ExecutionContext> m_executionContext;
};

// ContextLifecycleObserver provides an additional contextDestroyed() hook
// to execute cleanup code when a context is destroyed. Prefer the simpler
// ContextClient when possible.
class CORE_EXPORT ContextLifecycleObserver
    : public LifecycleObserver<ExecutionContext, ContextLifecycleObserver> {
 public:
  virtual void contextDestroyed(ExecutionContext*) {}

  ExecutionContext* getExecutionContext() const { return lifecycleContext(); }
  LocalFrame* frame() const;

  enum Type {
    GenericType,
    SuspendableObjectType,
  };

  Type observerType() const { return m_observerType; }

 protected:
  explicit ContextLifecycleObserver(ExecutionContext* executionContext,
                                    Type type = GenericType)
      : LifecycleObserver(executionContext), m_observerType(type) {}

 private:
  Type m_observerType;
};

// DOMWindowClient is a helper to associate an object with a LocalDOMWindow.
//
// - domWindow() returns null after the window is detached.
// - frame() is a syntax sugar for domWindow()->frame(). It returns
//   null after the window is detached.
//
// If the object is a per-ExecutionContext thing, use ContextClient/
// ContextLifecycleObserver. If the object is a per-DOMWindow thing, use
// DOMWindowClient. Basically, DOMWindowClient is expected to be used (only)
// for objects directly held by LocalDOMWindow. Other objects should use
// ContextClient/ContextLifecycleObserver.
//
// There is a subtle difference between the timing when the context gets
// detached and the timing when the window gets detached. In common cases,
// these two happen at the same timing. The only exception is a case where
// a frame navigates from an initial empty document to another same-origin
// document. In this case, a Document is recreated but a DOMWindow is reused.
// Hence, in the navigated document ContextClient::getExecutionContext()
// returns null while DOMWindowClient::domWindow() keeps returning the window.
class CORE_EXPORT DOMWindowClient : public GarbageCollectedMixin {
 public:
  LocalDOMWindow* domWindow() const;
  LocalFrame* frame() const;

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit DOMWindowClient(LocalDOMWindow*);
  explicit DOMWindowClient(LocalFrame*);

 private:
  WeakMember<LocalDOMWindow> m_domWindow;
};

}  // namespace blink

#endif  // ContextLifecycleObserver_h
