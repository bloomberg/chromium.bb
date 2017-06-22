// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorkerProxyClient_h
#define CompositorWorkerProxyClient_h

#include "core/CoreExport.h"
#include "core/workers/WorkerClients.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class WorkerClients;
class WorkerGlobalScope;

class CORE_EXPORT CompositorWorkerProxyClient
    : public Supplement<WorkerClients> {
  WTF_MAKE_NONCOPYABLE(CompositorWorkerProxyClient);

 public:
  CompositorWorkerProxyClient() {}

  static CompositorWorkerProxyClient* From(WorkerClients*);
  static const char* SupplementName();

  virtual void Dispose() = 0;
  virtual void SetGlobalScope(WorkerGlobalScope*) = 0;
  virtual void RequestAnimationFrame() = 0;
};

CORE_EXPORT void ProvideCompositorWorkerProxyClientTo(
    WorkerClients*,
    CompositorWorkerProxyClient*);

}  // namespace blink

#endif  // CompositorWorkerProxyClient_h
