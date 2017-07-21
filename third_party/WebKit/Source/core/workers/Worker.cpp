// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/Worker.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/CoreInitializer.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/UseCounter.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/workers/DedicatedWorkerMessagingProxy.h"
#include "core/workers/WorkerContentSettingsClient.h"
#include "public/platform/WebContentSettingsClient.h"
#include "public/web/WebFrameClient.h"

namespace blink {

Worker::Worker(ExecutionContext* context) : InProcessWorkerBase(context) {}

Worker* Worker::Create(ExecutionContext* context,
                       const String& url,
                       ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  Document* document = ToDocument(context);
  UseCounter::Count(context, WebFeature::kWorkerStart);
  if (!document->GetPage()) {
    exception_state.ThrowDOMException(kInvalidAccessError,
                                      "The context provided is invalid.");
    return nullptr;
  }
  Worker* worker = new Worker(context);
  if (worker->Initialize(context, url, exception_state))
    return worker;
  return nullptr;
}

Worker::~Worker() {
  DCHECK(IsMainThread());
}

const AtomicString& Worker::InterfaceName() const {
  return EventTargetNames::Worker;
}

InProcessWorkerMessagingProxy* Worker::CreateInProcessWorkerMessagingProxy(
    ExecutionContext* context) {
  Document* document = ToDocument(context);
  WebLocalFrameBase* web_frame =
      WebLocalFrameBase::FromFrame(document->GetFrame());

  WorkerClients* worker_clients = WorkerClients::Create();
  CoreInitializer::CallModulesProvideLocalFileSystem(*worker_clients);
  CoreInitializer::CallModulesProvideIndexedDB(*worker_clients);
  ProvideContentSettingsClientToWorker(
      worker_clients, web_frame->Client()->CreateWorkerContentSettingsClient());
  return new DedicatedWorkerMessagingProxy(this, worker_clients);
}

}  // namespace blink
