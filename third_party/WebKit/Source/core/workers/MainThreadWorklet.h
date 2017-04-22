// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MainThreadWorklet_h
#define MainThreadWorklet_h

#include "core/workers/Worklet.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/CoreExport.h"
#include "core/workers/WorkletObjectProxy.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;

// A MainThreadWorklet is a worklet that runs only on the main thread.
// TODO(nhiroki): This is a temporary class to support module loading for main
// thread worklets. This and ThreadedWorklet will be merged into the base
// Worklet class once threaded worklets are ready to use module loading.
class CORE_EXPORT MainThreadWorklet : public Worklet,
                                      public WorkletObjectProxy {
  USING_GARBAGE_COLLECTED_MIXIN(MainThreadWorklet);
  WTF_MAKE_NONCOPYABLE(MainThreadWorklet);

 public:
  virtual ~MainThreadWorklet() = default;

  // Worklet
  ScriptPromise addModule(ScriptState*, const String& url) final;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) final;

  // WorkletObjectProxy
  void DidFetchAndInvokeScript(int32_t request_id, bool success) final;

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit MainThreadWorklet(LocalFrame*);

 private:
  HeapHashMap<int32_t /* request_id */, Member<ScriptPromiseResolver>>
      resolver_map_;
};

}  // namespace blink

#endif  // MainThreadWorklet_h
