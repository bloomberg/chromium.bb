// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorkerMessagingProxy_h
#define CompositorWorkerMessagingProxy_h

#include <memory>
#include "core/workers/InProcessWorkerMessagingProxy.h"

namespace blink {

class CompositorWorkerMessagingProxy final
    : public InProcessWorkerMessagingProxy {
 public:
  CompositorWorkerMessagingProxy(InProcessWorkerBase*, WorkerClients*);

 private:
  ~CompositorWorkerMessagingProxy() override;
  WTF::Optional<WorkerBackingThreadStartupData> CreateBackingThreadStartupData(
      v8::Isolate*) override;
  std::unique_ptr<WorkerThread> CreateWorkerThread() override;
};

}  // namespace blink

#endif  // CompositorWorkerMessagingProxy_h
