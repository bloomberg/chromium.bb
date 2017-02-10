// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorkerProxyClient_h
#define CompositorWorkerProxyClient_h

#include "core/CoreExport.h"
#include "core/dom/CompositorProxyClient.h"
#include "core/workers/WorkerClients.h"
#include "wtf/Noncopyable.h"

namespace blink {

class WorkerClients;
class WorkerGlobalScope;

class CORE_EXPORT CompositorWorkerProxyClient
    : public CompositorProxyClient,
      public Supplement<WorkerClients> {
  WTF_MAKE_NONCOPYABLE(CompositorWorkerProxyClient);
  USING_GARBAGE_COLLECTED_MIXIN(CompositorWorkerProxyClient);

 public:
  CompositorWorkerProxyClient() {}
  DEFINE_INLINE_VIRTUAL_TRACE() {
    CompositorProxyClient::trace(visitor);
    Supplement<WorkerClients>::trace(visitor);
  }

  static CompositorWorkerProxyClient* from(WorkerClients*);
  static const char* supplementName();

  virtual void dispose() = 0;
  virtual void setGlobalScope(WorkerGlobalScope*) = 0;
  virtual void requestAnimationFrame() = 0;
};

CORE_EXPORT void provideCompositorWorkerProxyClientTo(
    WorkerClients*,
    CompositorWorkerProxyClient*);

}  // namespace blink

#endif  // CompositorWorkerProxyClient_h
