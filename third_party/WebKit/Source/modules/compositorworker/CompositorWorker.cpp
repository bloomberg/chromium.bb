// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorker.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/CompositorWorkerProxyClient.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "core/page/ChromeClient.h"
#include "core/workers/WorkerClients.h"
#include "modules/EventTargetModules.h"
#include "modules/compositorworker/CompositorWorkerMessagingProxy.h"
#include "modules/compositorworker/CompositorWorkerProxyClientImpl.h"
#include "modules/compositorworker/CompositorWorkerThread.h"

namespace blink {

inline CompositorWorker::CompositorWorker(ExecutionContext* context)
    : InProcessWorkerBase(context) {}

CompositorWorker* CompositorWorker::Create(ExecutionContext* context,
                                           const String& url,
                                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  Document* document = ToDocument(context);
  if (!document->GetPage()) {
    exception_state.ThrowDOMException(kInvalidAccessError,
                                      "The context provided is invalid.");
    return nullptr;
  }
  CompositorWorker* worker = new CompositorWorker(context);

  // Ensure the compositor worker backing thread is ready before we try to
  // initialize the CompositorWorker so that we can construct oilpan
  // objects on the compositor thread referenced from the worker clients.
  CompositorWorkerThread::EnsureSharedBackingThread();

  if (worker->Initialize(context, url, exception_state))
    return worker;
  return nullptr;
}

CompositorWorker::~CompositorWorker() {
  DCHECK(IsMainThread());
}

const AtomicString& CompositorWorker::InterfaceName() const {
  return EventTargetNames::CompositorWorker;
}

InProcessWorkerMessagingProxy*
CompositorWorker::CreateInProcessWorkerMessagingProxy(
    ExecutionContext* context) {
  Document* document = ToDocument(context);
  WorkerClients* worker_clients = WorkerClients::Create();
  CompositorWorkerProxyClient* client =
      CompositorWorkerProxyClientImpl::FromDocument(document);
  ProvideCompositorWorkerProxyClientTo(worker_clients, client);
  return new CompositorWorkerMessagingProxy(this, worker_clients);
}

}  // namespace blink
