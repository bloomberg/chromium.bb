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

#include "platform/audio/AudioDestination.h"

#include <memory>
#include "platform/Histogram.h"
#include "platform/audio/AudioUtilities.h"
#include "platform/audio/PushPullFIFO.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAudioLatencyHint.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

// FIFO Size.
//
// TODO(hongchan): This was estimated based on the largest callback buffer size
// that we would ever need. The current UMA stats indicates that this is, in
// fact, probably too small. There are Android devices out there with a size of
// 8000 or so.  We might need to make this larger. See: crbug.com/670747
const size_t kFIFOSize = 8192;

std::unique_ptr<AudioDestination> AudioDestination::Create(
    AudioIOCallback& callback,
    unsigned number_of_output_channels,
    const WebAudioLatencyHint& latency_hint,
    PassRefPtr<SecurityOrigin> security_origin) {
  return WTF::WrapUnique(
      new AudioDestination(callback, number_of_output_channels, latency_hint,
                           std::move(security_origin)));
}

AudioDestination::AudioDestination(AudioIOCallback& callback,
                                   unsigned number_of_output_channels,
                                   const WebAudioLatencyHint& latency_hint,
                                   PassRefPtr<SecurityOrigin> security_origin)
    : number_of_output_channels_(number_of_output_channels),
      is_playing_(false),
      callback_(callback),
      output_bus_(AudioBus::Create(number_of_output_channels,
                                   AudioUtilities::kRenderQuantumFrames,
                                   false)),
      render_bus_(AudioBus::Create(number_of_output_channels,
                                   AudioUtilities::kRenderQuantumFrames)),
      fifo_(WTF::WrapUnique(
          new PushPullFIFO(number_of_output_channels, kFIFOSize))),
      frames_elapsed_(0) {
  // Create WebAudioDevice. blink::WebAudioDevice is designed to support the
  // local input (e.g. loopback from OS audio system), but Chromium's media
  // renderer does not support it currently. Thus, we use zero for the number
  // of input channels.
  web_audio_device_ = WTF::WrapUnique(Platform::Current()->CreateAudioDevice(
      0, number_of_output_channels, latency_hint, this, String(),
      std::move(security_origin)));
  DCHECK(web_audio_device_);

  callback_buffer_size_ = web_audio_device_->FramesPerBuffer();
  if (!CheckBufferSize()) {
    NOTREACHED();
  }
}

AudioDestination::~AudioDestination() {
  Stop();
}

void AudioDestination::Render(const WebVector<float*>& destination_data,
                              size_t number_of_frames,
                              double delay,
                              double delay_timestamp,
                              size_t prior_frames_skipped) {
  CHECK_EQ(destination_data.size(), number_of_output_channels_);
  CHECK_EQ(number_of_frames, callback_buffer_size_);

  // Note that this method is called by AudioDeviceThread. If FIFO is not ready,
  // or the requested render size is greater than FIFO size return here.
  // (crbug.com/692423)
  if (!fifo_ || fifo_->length() < number_of_frames)
    return;

  frames_elapsed_ -= std::min(frames_elapsed_, prior_frames_skipped);
  double output_position =
      frames_elapsed_ / static_cast<double>(web_audio_device_->SampleRate()) -
      delay;
  output_position_.position = output_position;
  output_position_.timestamp = delay_timestamp;
  output_position_received_timestamp_ = base::TimeTicks::Now();

  // Associate the destination data array with the output bus then fill the
  // FIFO.
  for (unsigned i = 0; i < number_of_output_channels_; ++i)
    output_bus_->SetChannelMemory(i, destination_data[i], number_of_frames);

  // Number of frames to render via WebAudio graph. |framesToRender > 0| means
  // the frames in FIFO is not enough to fulfill the requested frames from the
  // audio device.
  size_t frames_to_render = number_of_frames > fifo_->FramesAvailable()
                                ? number_of_frames - fifo_->FramesAvailable()
                                : 0;

  for (size_t pushed_frames = 0; pushed_frames < frames_to_render;
       pushed_frames += AudioUtilities::kRenderQuantumFrames) {
    // If platform buffer is more than two times longer than |framesToProcess|
    // we do not want output position to get stuck so we promote it
    // using the elapsed time from the moment it was initially obtained.
    if (callback_buffer_size_ > AudioUtilities::kRenderQuantumFrames * 2) {
      double delta =
          (base::TimeTicks::Now() - output_position_received_timestamp_)
              .InSecondsF();
      output_position_.position += delta;
      output_position_.timestamp += delta;
    }

    // Some implementations give only rough estimation of |delay| so
    // we might have negative estimation |outputPosition| value.
    if (output_position_.position < 0.0)
      output_position_.position = 0.0;

    // Process WebAudio graph and push the rendered output to FIFO.
    callback_.Render(nullptr, render_bus_.Get(),
                     AudioUtilities::kRenderQuantumFrames, output_position_);
    fifo_->Push(render_bus_.Get());
  }

  fifo_->Pull(output_bus_.Get(), number_of_frames);

  frames_elapsed_ += number_of_frames;
}

void AudioDestination::Start() {
  if (web_audio_device_ && !is_playing_) {
    web_audio_device_->Start();
    is_playing_ = true;
  }
}

void AudioDestination::Stop() {
  if (web_audio_device_ && is_playing_) {
    web_audio_device_->Stop();
    is_playing_ = false;
  }
}

size_t AudioDestination::HardwareBufferSize() {
  return Platform::Current()->AudioHardwareBufferSize();
}

float AudioDestination::HardwareSampleRate() {
  return static_cast<float>(Platform::Current()->AudioHardwareSampleRate());
}

unsigned long AudioDestination::MaxChannelCount() {
  return static_cast<unsigned long>(
      Platform::Current()->AudioHardwareOutputChannels());
}

bool AudioDestination::CheckBufferSize() {
  // Histogram for audioHardwareBufferSize
  DEFINE_STATIC_LOCAL(SparseHistogram, hardware_buffer_size_histogram,
                      ("WebAudio.AudioDestination.HardwareBufferSize"));

  // Histogram for the actual callback size used.  Typically, this is the same
  // as audioHardwareBufferSize, but can be adjusted depending on some
  // heuristics below.
  DEFINE_STATIC_LOCAL(SparseHistogram, callback_buffer_size_histogram,
                      ("WebAudio.AudioDestination.CallbackBufferSize"));

  // Record the sizes if we successfully created an output device.
  hardware_buffer_size_histogram.Sample(HardwareBufferSize());
  callback_buffer_size_histogram.Sample(callback_buffer_size_);

  // Check if the requested buffer size is too large.
  bool is_buffer_size_valid =
      callback_buffer_size_ + AudioUtilities::kRenderQuantumFrames <= kFIFOSize;
  DCHECK(is_buffer_size_valid);
  return is_buffer_size_valid;
}

}  // namespace blink
