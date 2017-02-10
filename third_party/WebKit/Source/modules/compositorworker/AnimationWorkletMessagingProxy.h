// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorkletMessagingProxy_h
#define AnimationWorkletMessagingProxy_h

#include "core/dom/AnimationWorkletProxyClient.h"
#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "wtf/Allocator.h"
#include <memory>

namespace blink {

class ExecutionContext;
class WorkerThread;

class AnimationWorkletMessagingProxy final
    : public ThreadedWorkletMessagingProxy {
  USING_FAST_MALLOC(AnimationWorkletMessagingProxy);

 public:
  AnimationWorkletMessagingProxy(ExecutionContext*,
                                 AnimationWorkletProxyClient*);

 protected:
  ~AnimationWorkletMessagingProxy() override;

  std::unique_ptr<WorkerThread> createWorkerThread(double originTime) override;
  Persistent<AnimationWorkletProxyClient> m_proxyClient;
};

}  // namespace blink

#endif  // AnimationWorkletMessagingProxy_h
