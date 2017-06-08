// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorkletMessagingProxy_h
#define AnimationWorkletMessagingProxy_h

#include <memory>
#include "core/dom/AnimationWorkletProxyClient.h"
#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ExecutionContext;
class WorkerClients;
class WorkerThread;

class AnimationWorkletMessagingProxy final
    : public ThreadedWorkletMessagingProxy {
  USING_FAST_MALLOC(AnimationWorkletMessagingProxy);

 public:
  AnimationWorkletMessagingProxy(ExecutionContext*, WorkerClients*);

 private:
  ~AnimationWorkletMessagingProxy() override;

  std::unique_ptr<WorkerThread> CreateWorkerThread(double origin_time) override;
};

}  // namespace blink

#endif  // AnimationWorkletMessagingProxy_h
