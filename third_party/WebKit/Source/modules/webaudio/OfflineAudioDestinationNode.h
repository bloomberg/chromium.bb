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
      AudioBuffer* render_target);
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

  double SampleRate() const override { return render_target_->sampleRate(); }
  int FramesPerBuffer() const override {
    NOTREACHED();
    return 0;
  }

  size_t RenderQuantumFrames() const {
    return AudioUtilities::kRenderQuantumFrames;
  }

  void InitializeOfflineRenderThread();

 private:
  OfflineAudioDestinationHandler(AudioNode&, AudioBuffer* render_target);

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

  // This AudioHandler renders into this AudioBuffer.
  // This Persistent doesn't make a reference cycle including the owner
  // OfflineAudioDestinationNode. It is accessed by both audio and main thread.
  CrossThreadPersistent<AudioBuffer> render_target_;
  // Temporary AudioBus for each render quantum.
  RefPtr<AudioBus> render_bus_;

  // Rendering thread.
  std::unique_ptr<WebThread> render_thread_;

  // These variables are for counting the number of frames for the current
  // progress and the remaining frames to be processed.
  size_t frames_processed_;
  size_t frames_to_process_;

  // This flag is necessary to distinguish the state of the context between
  // 'created' and 'suspended'. If this flag is false and the current state
  // is 'suspended', it means the context is created and have not started yet.
  bool is_rendering_started_;
};

class OfflineAudioDestinationNode final : public AudioDestinationNode {
 public:
  static OfflineAudioDestinationNode* Create(BaseAudioContext*,
                                             AudioBuffer* render_target);

 private:
  OfflineAudioDestinationNode(BaseAudioContext&, AudioBuffer* render_target);
};

}  // namespace blink

#endif  // OfflineAudioDestinationNode_h
