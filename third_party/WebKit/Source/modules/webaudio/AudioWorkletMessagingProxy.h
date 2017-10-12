// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletMessagingProxy_h
#define AudioWorkletMessagingProxy_h

#include <memory>
#include "core/workers/ThreadedWorkletMessagingProxy.h"

namespace blink {

class AudioWorkletHandler;
class CrossThreadAudioParamInfo;
class CrossThreadAudioWorkletProcessorInfo;
class ExecutionContext;
class WebThread;
class WorkerThread;

// AudioWorkletMessagingProxy is a main thread interface for
// AudioWorkletGlobalScope. The proxy communicates with the associated global
// scope via AudioWorkletObjectProxy.
class AudioWorkletMessagingProxy final : public ThreadedWorkletMessagingProxy {
 public:
  AudioWorkletMessagingProxy(ExecutionContext*, WorkerClients*);

  // Since the creation of AudioWorkletProcessor needs to be done in the
  // different thread, this method is a wrapper for cross-thread task posting.
  void CreateProcessor(AudioWorkletHandler*);

  // Invokes AudioWorkletGlobalScope to create an instance of
  // AudioWorkletProcessor.
  void CreateProcessorOnRenderingThread(WorkerThread*,
                                        AudioWorkletHandler*,
                                        const String& name,
                                        float sample_rate);

  // Invoked by AudioWorkletObjectProxy on AudioWorkletThread to fetch the
  // information from AudioWorkletGlobalScope to AudioWorkletMessagingProxy
  // after the script code evaluation. It copies the information about newly
  // added AudioWorkletProcessor since the previous synchronization. (e.g.
  // processor name and AudioParam list)
  void SynchronizeWorkletProcessorInfoList(
      std::unique_ptr<Vector<CrossThreadAudioWorkletProcessorInfo>>);

  // Returns true if the processor with given name is registered in
  // AudioWorkletGlobalScope.
  bool IsProcessorRegistered(const String& name) const;

  const Vector<CrossThreadAudioParamInfo> GetParamInfoListForProcessor(
      const String& name) const;

  WebThread* GetWorkletBackingThread();

 private:
  ~AudioWorkletMessagingProxy() override;

  // Implements ThreadedWorkletMessagingProxy.
  std::unique_ptr<ThreadedWorkletObjectProxy> CreateObjectProxy(
      ThreadedWorkletMessagingProxy*,
      ParentFrameTaskRunners*) override;

  std::unique_ptr<WorkerThread> CreateWorkerThread() override;

  // Each entry consists of processor name and associated AudioParam list.
  HashMap<String, Vector<CrossThreadAudioParamInfo>> processor_info_map_;
};

}  // namespace blink

#endif  // AudioWorkletMessagingProxy_h
