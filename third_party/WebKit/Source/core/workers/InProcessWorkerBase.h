// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InProcessWorkerBase_h
#define InProcessWorkerBase_h

#include "core/CoreExport.h"
#include "core/dom/MessagePort.h"
#include "core/dom/SuspendableObject.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "core/workers/AbstractWorker.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class InProcessWorkerMessagingProxy;
class ScriptState;
class WorkerScriptLoader;

// Base class for workers that operate in the same process as the document that
// creates them.
class CORE_EXPORT InProcessWorkerBase
    : public AbstractWorker,
      public ActiveScriptWrappable<InProcessWorkerBase> {
  // Eager finalization is needed to notify the parent object destruction of the
  // GC-managed messaging proxy and to initiate worker termination.
  EAGERLY_FINALIZE();

 public:
  ~InProcessWorkerBase() override;

  void postMessage(ScriptState*,
                   PassRefPtr<SerializedScriptValue> message,
                   const MessagePortArray&,
                   ExceptionState&);
  static bool CanTransferArrayBuffersAndImageBitmaps() { return true; }
  void terminate();

  // SuspendableObject
  void ContextDestroyed(ExecutionContext*) override;

  // ScriptWrappable
  bool HasPendingActivity() const final;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(message);

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit InProcessWorkerBase(ExecutionContext*);
  bool Initialize(ExecutionContext*, const String&, ExceptionState&);

  // Creates a proxy to allow communicating with the worker's global scope.
  // InProcessWorkerBase does not take ownership of the created proxy. The proxy
  // is expected to manage its own lifetime, and delete itself in response to
  // terminateWorkerGlobalScope().
  virtual InProcessWorkerMessagingProxy* CreateInProcessWorkerMessagingProxy(
      ExecutionContext*) = 0;

 private:
  // Callbacks for m_scriptLoader.
  void OnResponse();
  void OnFinished();

  RefPtr<WorkerScriptLoader> script_loader_;

  Member<InProcessWorkerMessagingProxy> context_proxy_;
};

}  // namespace blink

#endif  // InProcessWorkerBase_h
