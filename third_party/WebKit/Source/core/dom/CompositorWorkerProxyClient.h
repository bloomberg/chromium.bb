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
    : public Supplement<WorkerClients> {
  WTF_MAKE_NONCOPYABLE(CompositorWorkerProxyClient);

 public:
  CompositorWorkerProxyClient() {}

  static CompositorWorkerProxyClient* from(WorkerClients*);
  static const char* supplementName();

  virtual void dispose() = 0;
  virtual void setGlobalScope(WorkerGlobalScope*) = 0;
  virtual void requestAnimationFrame() = 0;
  virtual CompositorProxyClient* compositorProxyClient() = 0;
};

CORE_EXPORT void provideCompositorWorkerProxyClientTo(
    WorkerClients*,
    CompositorWorkerProxyClient*);

}  // namespace blink

#endif  // CompositorWorkerProxyClient_h
