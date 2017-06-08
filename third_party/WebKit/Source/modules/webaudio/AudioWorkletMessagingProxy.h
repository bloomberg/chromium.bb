// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletMessagingProxy_h
#define AudioWorkletMessagingProxy_h

#include <memory>
#include "core/workers/ThreadedWorkletMessagingProxy.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ExecutionContext;
class WorkerThread;

class AudioWorkletMessagingProxy final : public ThreadedWorkletMessagingProxy {
  USING_FAST_MALLOC(AudioWorkletMessagingProxy);

 public:
  AudioWorkletMessagingProxy(ExecutionContext*, WorkerClients*);

 private:
  ~AudioWorkletMessagingProxy() override;

  std::unique_ptr<WorkerThread> CreateWorkerThread(double origin_time) override;
};

}  // namespace blink

#endif  // AudioWorkletMessagingProxy_h
