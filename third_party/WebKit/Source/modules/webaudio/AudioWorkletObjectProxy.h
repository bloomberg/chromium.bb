// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletObjectProxy_h
#define AudioWorkletObjectProxy_h

#include "core/workers/ThreadedWorkletObjectProxy.h"

namespace blink {

class AudioWorkletGlobalScope;
class AudioWorkletMessagingProxy;

class AudioWorkletObjectProxy final
    : public ThreadedWorkletObjectProxy {
 public:
  AudioWorkletObjectProxy(AudioWorkletMessagingProxy*,
                          ParentFrameTaskRunners*);

  // Implements WorkerReportingProxy.
  void DidCreateWorkerGlobalScope(WorkerOrWorkletGlobalScope*) override;
  void DidEvaluateModuleScript(bool success) override;
  void WillDestroyWorkerGlobalScope() override;

 private:
  CrossThreadWeakPersistent<AudioWorkletMessagingProxy>
      GetAudioWorkletMessagingProxyWeakPtr();

  CrossThreadPersistent<AudioWorkletGlobalScope> global_scope_;
};

}  // namespace blink

#endif  // AudioWorkletObjectProxy_h
