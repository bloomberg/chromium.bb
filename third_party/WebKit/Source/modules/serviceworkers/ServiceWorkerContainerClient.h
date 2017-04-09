// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerContainerClient_h
#define ServiceWorkerContainerClient_h

#include "core/dom/Document.h"
#include "core/workers/WorkerClients.h"
#include "modules/ModulesExport.h"
#include "wtf/Forward.h"
#include <memory>

namespace blink {

class ExecutionContext;
class WebServiceWorkerProvider;

// This mainly exists to provide access to WebServiceWorkerProvider.
// Owned by Document (or WorkerClients).
class MODULES_EXPORT ServiceWorkerContainerClient final
    : public GarbageCollectedFinalized<ServiceWorkerContainerClient>,
      public Supplement<Document>,
      public Supplement<WorkerClients> {
  USING_GARBAGE_COLLECTED_MIXIN(ServiceWorkerContainerClient);
  WTF_MAKE_NONCOPYABLE(ServiceWorkerContainerClient);

 public:
  ServiceWorkerContainerClient(Document&,
                               std::unique_ptr<WebServiceWorkerProvider>);
  ServiceWorkerContainerClient(WorkerClients&,
                               std::unique_ptr<WebServiceWorkerProvider>);
  virtual ~ServiceWorkerContainerClient();

  WebServiceWorkerProvider* Provider() { return provider_.get(); }

  static const char* SupplementName();
  static ServiceWorkerContainerClient* From(ExecutionContext*);

  DEFINE_INLINE_VIRTUAL_TRACE() {
    Supplement<Document>::Trace(visitor);
    Supplement<WorkerClients>::Trace(visitor);
  }

 private:
  std::unique_ptr<WebServiceWorkerProvider> provider_;
};

MODULES_EXPORT void ProvideServiceWorkerContainerClientToWorker(
    WorkerClients*,
    std::unique_ptr<WebServiceWorkerProvider>);

}  // namespace blink

#endif  // ServiceWorkerContainerClient_h
