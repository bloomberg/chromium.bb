// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorkletMessagingProxy_h
#define AnimationWorkletMessagingProxy_h

#include <memory>
#include "core/dom/AnimationWorkletProxyClient.h"
#include "core/workers/ThreadedWorkletMessagingProxy.h"

namespace blink {

class ExecutionContext;
class WorkerClients;
class WorkerThread;

class AnimationWorkletMessagingProxy final
    : public ThreadedWorkletMessagingProxy {
 public:
  AnimationWorkletMessagingProxy(ExecutionContext*, WorkerClients*);

  DECLARE_VIRTUAL_TRACE();

 private:
  ~AnimationWorkletMessagingProxy() override;

  std::unique_ptr<WorkerThread> CreateWorkerThread() override;
};

}  // namespace blink

#endif  // AnimationWorkletMessagingProxy_h
