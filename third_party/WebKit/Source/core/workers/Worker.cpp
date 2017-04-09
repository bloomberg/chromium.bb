// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/Worker.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/UseCounter.h"
#include "core/workers/DedicatedWorkerMessagingProxyProvider.h"
#include "core/workers/InProcessWorkerMessagingProxy.h"

namespace blink {

Worker::Worker(ExecutionContext* context) : InProcessWorkerBase(context) {}

Worker* Worker::Create(ExecutionContext* context,
                       const String& url,
                       ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  Document* document = ToDocument(context);
  UseCounter::Count(context, UseCounter::kWorkerStart);
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
  DedicatedWorkerMessagingProxyProvider* proxy_provider =
      DedicatedWorkerMessagingProxyProvider::From(*document->GetPage());
  DCHECK(proxy_provider);
  return proxy_provider->CreateWorkerMessagingProxy(this);
}

}  // namespace blink
