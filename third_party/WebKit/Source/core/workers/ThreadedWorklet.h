// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThreadedWorklet_h
#define ThreadedWorklet_h

#include "core/workers/Worklet.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class WorkletGlobalScopeProxy;

// A ThreadedWorklet is a worklet that runs off the main thread.
// TODO(nhiroki): This is a temporary class to keep classic script loading for
// threaded worklets while module loading is being implemented for main thread
// worklets. This and MainThreadWorklet will be merged into the base Worklet
// class once threaded worklets are also ready to use module loading.
class CORE_EXPORT ThreadedWorklet : public Worklet {
  USING_GARBAGE_COLLECTED_MIXIN(ThreadedWorklet);
  WTF_MAKE_NONCOPYABLE(ThreadedWorklet);

 public:
  virtual ~ThreadedWorklet() = default;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) final;

  // Returns a proxy to WorkletGlobalScope on the context thread.
  virtual WorkletGlobalScopeProxy* GetWorkletGlobalScopeProxy() const = 0;

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit ThreadedWorklet(LocalFrame*);

 private:
  // Worklet
  void FetchAndInvokeScript(const KURL& module_url_record,
                            const WorkletOptions&,
                            ScriptPromiseResolver*) final;
  bool NeedsToCreateGlobalScope() final;
  WorkletGlobalScopeProxy* CreateGlobalScope() final;

  // Called when addModule() is called for the first time.
  virtual void Initialize() = 0;
  virtual bool IsInitialized() const = 0;
};

}  // namespace blink

#endif  // ThreadedWorklet_h
