/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AudioDestination_h
#define AudioDestination_h

#include <memory>
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioIOCallback.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebAudioDevice.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebVector.h"

namespace blink {

class PushPullFIFO;
class SecurityOrigin;
class WebAudioLatencyHint;

// The AudioDestination class is an audio sink interface between the media
// renderer and the Blink's WebAudio module. It has a FIFO to adapt the
// different processing block sizes of WebAudio renderer and actual hardware
// audio callback.
class PLATFORM_EXPORT AudioDestination : public WebAudioDevice::RenderCallback {
  USING_FAST_MALLOC(AudioDestination);
  WTF_MAKE_NONCOPYABLE(AudioDestination);

 public:
  AudioDestination(AudioIOCallback&,
                   unsigned number_of_output_channels,
                   const WebAudioLatencyHint&,
                   RefPtr<SecurityOrigin>);
  ~AudioDestination() override;

  static std::unique_ptr<AudioDestination> Create(
      AudioIOCallback&,
      unsigned number_of_output_channels,
      const WebAudioLatencyHint&,
      RefPtr<SecurityOrigin>);

  // The actual render function (WebAudioDevice::RenderCallback) isochronously
  // invoked by the media renderer.
  void Render(const WebVector<float*>& destination_data,
              size_t number_of_frames,
              double delay,
              double delay_timestamp,
              size_t prior_frames_skipped) override;

  // The actual render request to the WebAudio destination node. This triggers
  // the WebAudio rendering pipe line on the web thread.
  void RequestRenderOnWebThread(size_t frames_requested,
                                size_t frames_to_render,
                                double delay,
                                double delay_timestamp,
                                size_t prior_frames_skipped);

  virtual void Start();
  virtual void Stop();

  // For AudioWorklet experimental threading.
  void StartWithWorkletThread(WebThread* worklet_backing_thread);

  // Getters must be accessed from the main thread.
  size_t CallbackBufferSize() const;
  bool IsPlaying();

  // TODO(hongchan): this should not be called by the rendering thread.
  double SampleRate() const { return web_audio_device_->SampleRate(); }

  // Returns the audio buffer size in frames used by the underlying audio
  // hardware.
  int FramesPerBuffer() const;

  // The information from the actual audio hardware. (via Platform::current)
  static float HardwareSampleRate();
  static unsigned long MaxChannelCount();

 private:
  // Check if the buffer size chosen by the WebAudioDevice is too large.
  bool CheckBufferSize();

  size_t HardwareBufferSize();

  bool IsRenderingThread();

  // Because the alternative thread can exist, this method returns what is
  // currently valid/available.
  WebThread* GetRenderingThread();

  void ClearRenderingThread();

  // Accessed by the main thread.
  std::unique_ptr<WebAudioDevice> web_audio_device_;
  const unsigned number_of_output_channels_;
  size_t callback_buffer_size_;
  bool is_playing_;

  // Accessed by the device thread. Rendering thread for WebAudio graph.
  std::unique_ptr<WebThread> rendering_thread_;

  // The experimental worklet rendering thread.
  WebThread* worklet_backing_thread_ = nullptr;

  // Accessed by both threads: resolves the buffer size mismatch between the
  // WebAudio engine and the callback function from the actual audio device.
  std::unique_ptr<PushPullFIFO> fifo_;

  // Accessed by device thread: to pass the data from FIFO to the device.
  RefPtr<AudioBus> output_bus_;

  // Accessed by rendering thread: to push the rendered result from WebAudio
  // graph into the FIFO.
  RefPtr<AudioBus> render_bus_;

  // Accessed by rendering thread: the render callback function of WebAudio
  // engine. (i.e. DestinationNode)
  AudioIOCallback& callback_;

  // Accessed by rendering thread.
  size_t frames_elapsed_;
};

}  // namespace blink

#endif  // AudioDestination_h
