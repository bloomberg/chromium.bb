/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef OfflineAudioDestinationNode_h
#define OfflineAudioDestinationNode_h

#include <memory>
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioDestinationNode.h"
#include "modules/webaudio/OfflineAudioContext.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebThread.h"

namespace blink {

class BaseAudioContext;
class AudioBus;
class OfflineAudioContext;

class OfflineAudioDestinationHandler final : public AudioDestinationHandler {
 public:
  static PassRefPtr<OfflineAudioDestinationHandler> Create(
      AudioNode&,
      unsigned number_of_channels,
      size_t frames_to_process,
      float sample_rate);
  ~OfflineAudioDestinationHandler() override;

  // AudioHandler
  void Dispose() override;
  void Initialize() override;
  void Uninitialize() override;

  // AudioNode
  double TailTime() const override { return 0; }
  double LatencyTime() const override { return 0; }

  OfflineAudioContext* Context() const final;

  // AudioDestinationHandler
  void StartRendering() override;
  void StopRendering() override;
  unsigned long MaxChannelCount() const override;

  // Returns the rendering callback buffer size.  This should never be
  // called.
  size_t CallbackBufferSize() const override;

  double SampleRate() const override { return sample_rate_; }
  int FramesPerBuffer() const override {
    NOTREACHED();
    return 0;
  }

  size_t RenderQuantumFrames() const {
    return AudioUtilities::kRenderQuantumFrames;
  }

  // This is called when rendering of the offline context is started
  // which will save the rendered audio data in |render_target|.  This
  // allows creation of the AudioBuffer when startRendering is called
  // instead of when the OfflineAudioContext is created.
  void InitializeOfflineRenderThread(AudioBuffer* render_target);
  AudioBuffer* RenderTarget() const { return render_target_.Get(); }

  unsigned NumberOfChannels() const { return number_of_channels_; }

 private:
  OfflineAudioDestinationHandler(AudioNode&,
                                 unsigned number_of_channels,
                                 size_t frames_to_process,
                                 float sample_rate);

  // Set up the rendering and start. After setting the context up, it will
  // eventually call |doOfflineRendering|.
  void StartOfflineRendering();

  // Suspend the rendering loop and notify the main thread to resolve the
  // associated promise.
  void SuspendOfflineRendering();

  // Start the rendering loop.
  void DoOfflineRendering();

  // Finish the rendering loop and notify the main thread to resolve the
  // promise with the rendered buffer.
  void FinishOfflineRendering();

  // Suspend/completion callbacks for the main thread.
  void NotifySuspend(size_t);
  void NotifyComplete();

  // The offline version of render() method. If the rendering needs to be
  // suspended after checking, this stops the rendering and returns true.
  // Otherwise, it returns false after rendering one quantum.
  bool RenderIfNotSuspended(AudioBus* source_bus,
                            AudioBus* destination_bus,
                            size_t number_of_frames);

  // The context can run on two types of threads: when the AudioWorklet is
  // enabled, the context runs on AudioWorkletThread whereas it runs on the
  // normal WebThread owned by AudioDestination without AudioWorklet feature.
  // This method returns the current thread regardless of the thread type.
  WebThread* GetRenderingThread();

  // This AudioHandler renders into this AudioBuffer.
  // This Persistent doesn't make a reference cycle including the owner
  // OfflineAudioDestinationNode. It is accessed by both audio and main thread.
  CrossThreadPersistent<AudioBuffer> render_target_;
  // Temporary AudioBus for each render quantum.
  RefPtr<AudioBus> render_bus_;

  // Rendering thread.
  std::unique_ptr<WebThread> render_thread_;

  // The experimental worklet rendering thread. Points the thread borrowed from
  // AudioWorkletThread.
  WebThread* worklet_backing_thread_ = nullptr;

  // These variables are for counting the number of frames for the current
  // progress and the remaining frames to be processed.
  size_t frames_processed_;
  size_t frames_to_process_;

  // This flag is necessary to distinguish the state of the context between
  // 'created' and 'suspended'. If this flag is false and the current state
  // is 'suspended', it means the context is created and have not started yet.
  bool is_rendering_started_;

  unsigned number_of_channels_;
  float sample_rate_;
};

class OfflineAudioDestinationNode final : public AudioDestinationNode {
 public:
  static OfflineAudioDestinationNode* Create(BaseAudioContext*,
                                             unsigned number_of_channels,
                                             size_t frames_to_process,
                                             float sample_rate);

 private:
  OfflineAudioDestinationNode(BaseAudioContext&,
                              unsigned number_of_channels,
                              size_t frames_to_process,
                              float sample_rate);
};

}  // namespace blink

#endif  // OfflineAudioDestinationNode_h
