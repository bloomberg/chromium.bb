// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorkerMessagingProxy_h
#define CompositorWorkerMessagingProxy_h

#include "core/workers/InProcessWorkerMessagingProxy.h"
#include "wtf/Allocator.h"

namespace blink {

class CompositorWorkerMessagingProxy final : public InProcessWorkerMessagingProxy {
    USING_FAST_MALLOC(CompositorWorkerMessagingProxy);
public:
    explicit CompositorWorkerMessagingProxy(InProcessWorkerBase*);

protected:
    ~CompositorWorkerMessagingProxy() override;

    PassOwnPtr<WorkerThread> createWorkerThread(double originTime) override;
};

} // namespace blink

#endif // CompositorWorkerMessagingProxy_h
