// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedWorkletMessagingProxy_h
#define ThreadedWorkletMessagingProxy_h

#include "core/CoreExport.h"
#include "core/workers/ThreadedMessagingProxyBase.h"
#include "core/workers/WorkletGlobalScopeProxy.h"
#include "platform/wtf/WeakPtr.h"

namespace blink {

class ThreadedWorkletObjectProxy;

class CORE_EXPORT ThreadedWorkletMessagingProxy
    : public ThreadedMessagingProxyBase,
      public WorkletGlobalScopeProxy {
 public:
  // WorkletGlobalScopeProxy implementation.
  void EvaluateScript(const ScriptSourceCode&) final;
  void TerminateWorkletGlobalScope() final;

  void Initialize();

 protected:
  explicit ThreadedWorkletMessagingProxy(ExecutionContext*);

  ThreadedWorkletObjectProxy& WorkletObjectProxy() {
    return *worklet_object_proxy_;
  }

 private:
  friend class ThreadedWorkletMessagingProxyForTest;

  std::unique_ptr<ThreadedWorkletObjectProxy> worklet_object_proxy_;

  WeakPtrFactory<ThreadedWorkletMessagingProxy> weak_ptr_factory_;
};

}  // namespace blink

#endif  // ThreadedWorkletMessagingProxy_h
