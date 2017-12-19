// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DedicatedWorker_h
#define DedicatedWorker_h

#include "base/memory/scoped_refptr.h"
#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/PausableObject.h"
#include "core/dom/events/EventListener.h"
#include "core/dom/events/EventTarget.h"
#include "core/messaging/MessagePort.h"
#include "core/workers/AbstractWorker.h"
#include "core/workers/WorkerOptions.h"
#include "platform/wtf/Forward.h"

namespace v8_inspector {
struct V8StackTraceId;
}  // namespace v8_inspector

namespace blink {

class DedicatedWorkerMessagingProxy;
class ExceptionState;
class ExecutionContext;
class ScriptState;
class WorkerScriptLoader;
struct GlobalScopeCreationParams;

// Implementation of the Worker interface defined in the WebWorker HTML spec:
// https://html.spec.whatwg.org/multipage/workers.html#worker
//
// Confusingly, the Worker interface is for dedicated workers, so this class is
// called DedicatedWorker. This lives on the main thread.
class CORE_EXPORT DedicatedWorker final
    : public AbstractWorker,
      public ActiveScriptWrappable<DedicatedWorker> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DedicatedWorker);
  // Eager finalization is needed to notify the parent object destruction of the
  // GC-managed messaging proxy and to initiate worker termination.
  EAGERLY_FINALIZE();

 public:
  static DedicatedWorker* Create(ExecutionContext*,
                                 const String& url,
                                 ExceptionState&);

  ~DedicatedWorker() override;

  void postMessage(ScriptState*,
                   scoped_refptr<SerializedScriptValue> message,
                   const MessagePortArray&,
                   ExceptionState&);
  static bool CanTransferArrayBuffersAndImageBitmaps() { return true; }
  void terminate();

  // Implements ContextLifecycleObserver (via AbstractWorker).
  void ContextDestroyed(ExecutionContext*) override;

  // Implements ScriptWrappable
  // (via AbstractWorker -> EventTargetWithInlineData -> EventTarget).
  bool HasPendingActivity() const final;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(message);

  void Trace(blink::Visitor*) override;

 private:
  DedicatedWorker(ExecutionContext*,
                  const KURL& script_url,
                  const WorkerOptions&);

  // Starts the worker.
  void Start();

  std::unique_ptr<GlobalScopeCreationParams> CreateGlobalScopeCreationParams();

  // Creates a proxy to allow communicating with the worker's global scope.
  // DedicatedWorker does not take ownership of the created proxy. The proxy
  // is expected to manage its own lifetime, and delete itself in response to
  // terminateWorkerGlobalScope().
  DedicatedWorkerMessagingProxy* CreateMessagingProxy(ExecutionContext*);

  // [classic script only]
  // Callbacks for |script_loader_|.
  void OnResponse();
  void OnFinished(const v8_inspector::V8StackTraceId&);

  // Implements EventTarget (via AbstractWorker -> EventTargetWithInlineData).
  const AtomicString& InterfaceName() const final;

  const KURL script_url_;
  const WorkerOptions options_;
  const Member<DedicatedWorkerMessagingProxy> context_proxy_;

  scoped_refptr<WorkerScriptLoader> script_loader_;
};

}  // namespace blink

#endif  // DedicatedWorker_h
