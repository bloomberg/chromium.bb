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

#include "third_party/blink/renderer/platform/audio/audio_destination.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/push_pull_fifo.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"

namespace blink {

// FIFO Size.
//
// TODO(hongchan): This was estimated based on the largest callback buffer size
// that we would ever need. The current UMA stats indicates that this is, in
// fact, probably too small. There are Android devices out there with a size of
// 8000 or so.  We might need to make this larger. See: crbug.com/670747
const size_t kFIFOSize = 96 * 128;

scoped_refptr<AudioDestination> AudioDestination::Create(
    AudioIOCallback& callback,
    unsigned number_of_output_channels,
    const WebAudioLatencyHint& latency_hint) {
  return base::AdoptRef(
      new AudioDestination(callback, number_of_output_channels, latency_hint));
}

AudioDestination::AudioDestination(AudioIOCallback& callback,
                                   unsigned number_of_output_channels,
                                   const WebAudioLatencyHint& latency_hint)
    : number_of_output_channels_(number_of_output_channels),
      play_state_(PlayState::kStopped),
      fifo_(
          std::make_unique<PushPullFIFO>(number_of_output_channels, kFIFOSize)),
      output_bus_(AudioBus::Create(number_of_output_channels,
                                   audio_utilities::kRenderQuantumFrames,
                                   false)),
      render_bus_(AudioBus::Create(number_of_output_channels,
                                   audio_utilities::kRenderQuantumFrames)),
      callback_(callback),
      frames_elapsed_(0) {
  // Create WebAudioDevice. blink::WebAudioDevice is designed to support the
  // local input (e.g. loopback from OS audio system), but Chromium's media
  // renderer does not support it currently. Thus, we use zero for the number
  // of input channels.
  web_audio_device_ = Platform::Current()->CreateAudioDevice(
      0, number_of_output_channels, latency_hint, this, String());
  DCHECK(web_audio_device_);

  callback_buffer_size_ = web_audio_device_->FramesPerBuffer();

  // Primes the FIFO for the given callback buffer size. This is to prevent
  // first FIFO pulls from causing "underflow" errors.
  const unsigned priming_render_quanta =
      ceil(callback_buffer_size_ /
           static_cast<float>(audio_utilities::kRenderQuantumFrames));
  for (unsigned i = 0; i < priming_render_quanta; ++i) {
    fifo_->Push(render_bus_.get());
  }

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
  TRACE_EVENT_BEGIN2("webaudio", "AudioDestination::Render",
                     "callback_buffer_size", number_of_frames, "frames skipped",
                     prior_frames_skipped);
  CHECK_EQ(destination_data.size(), number_of_output_channels_);
  CHECK_EQ(number_of_frames, callback_buffer_size_);

  // Note that this method is called by AudioDeviceThread. If FIFO is not ready,
  // or the requested render size is greater than FIFO size return here.
  // (crbug.com/692423)
  if (!fifo_ || fifo_->length() < number_of_frames) {
    TRACE_EVENT_INSTANT1(
        "webaudio",
        "AudioDestination::Render - FIFO not ready or the size is too small",
        TRACE_EVENT_SCOPE_THREAD, "fifo length", fifo_ ? fifo_->length() : 0);
    TRACE_EVENT_END2("webaudio", "AudioDestination::Render", "timestamp (s)",
                     delay_timestamp, "delay (s)", delay);
    return;
  }

  // Associate the destination data array with the output bus then fill the
  // FIFO.
  for (unsigned i = 0; i < number_of_output_channels_; ++i)
    output_bus_->SetChannelMemory(i, destination_data[i], number_of_frames);

  size_t frames_to_render = fifo_->Pull(output_bus_.get(), number_of_frames);

  // Use the dual-thread rendering model if the AudioWorklet is activated.
  if (worklet_task_runner_) {
    PostCrossThreadTask(
        *worklet_task_runner_,
        FROM_HERE,
        CrossThreadBind(&AudioDestination::RequestRender, WrapRefCounted(this),
                        number_of_frames, frames_to_render, delay,
                        delay_timestamp, prior_frames_skipped));
  } else {
    // Otherwise use the single-thread rendering with AudioDeviceThread.
    RequestRender(number_of_frames, frames_to_render, delay,
                  delay_timestamp, prior_frames_skipped);
  }
  TRACE_EVENT_END2("webaudio", "AudioDestination::Render", "timestamp (s)",
                   delay_timestamp, "delay (s)", delay);
}


void AudioDestination::RequestRender(size_t frames_requested,
                                     size_t frames_to_render,
                                     double delay,
                                     double delay_timestamp,
                                     size_t prior_frames_skipped) {
  TRACE_EVENT2("webaudio", "AudioDestination::RequestRender",
               "frames_to_render", frames_to_render, "timestamp (s)",
               delay_timestamp);

  frames_elapsed_ -= std::min(frames_elapsed_, prior_frames_skipped);
  AudioIOPosition output_position;
  output_position.position =
      frames_elapsed_ / static_cast<double>(web_audio_device_->SampleRate()) -
      delay;
  output_position.timestamp = delay_timestamp;
  base::TimeTicks received_timestamp = base::TimeTicks::Now();

  for (size_t pushed_frames = 0; pushed_frames < frames_to_render;
       pushed_frames += audio_utilities::kRenderQuantumFrames) {
    // If platform buffer is more than two times longer than |framesToProcess|
    // we do not want output position to get stuck so we promote it
    // using the elapsed time from the moment it was initially obtained.
    if (callback_buffer_size_ > audio_utilities::kRenderQuantumFrames * 2) {
      double delta = (base::TimeTicks::Now() - received_timestamp).InSecondsF();
      output_position.position += delta;
      output_position.timestamp += delta;
    }

    // Some implementations give only rough estimation of |delay| so
    // we might have negative estimation |outputPosition| value.
    if (output_position.position < 0.0)
      output_position.position = 0.0;

    // Process WebAudio graph and push the rendered output to FIFO.
    callback_.Render(render_bus_.get(), audio_utilities::kRenderQuantumFrames,
                     output_position);

    fifo_->Push(render_bus_.get());
  }

  frames_elapsed_ += frames_requested;
}

void AudioDestination::Start() {
  DCHECK(IsMainThread());

  // Start the "audio device" after the rendering thread is ready.
  if (web_audio_device_ && play_state_ == PlayState::kStopped) {
    TRACE_EVENT0("webaudio", "AudioDestination::Start");
    web_audio_device_->Start();
    play_state_ = PlayState::kPlaying;
  }
}

void AudioDestination::StartWithWorkletTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> worklet_task_runner) {
  DCHECK(IsMainThread());

  if (web_audio_device_ && play_state_ == PlayState::kStopped) {
    TRACE_EVENT0("webaudio", "AudioDestination::Start");
    worklet_task_runner_ = std::move(worklet_task_runner);
    web_audio_device_->Start();
    play_state_ = PlayState::kPlaying;
  }
}

void AudioDestination::Stop() {
  DCHECK(IsMainThread());

  // This assumes stopping the "audio device" is synchronous and dumping the
  // rendering thread is safe after that.
  if (web_audio_device_ && play_state_ != PlayState::kStopped) {
    TRACE_EVENT0("webaudio", "AudioDestination::Stop");
    web_audio_device_->Stop();
    worklet_task_runner_ = nullptr;
    play_state_ = PlayState::kStopped;
  }
}

void AudioDestination::Pause() {
  DCHECK(IsMainThread());
  if (web_audio_device_ && play_state_ == PlayState::kPlaying) {
    web_audio_device_->Pause();
    play_state_ = PlayState::kPaused;
  }
}

void AudioDestination::Resume() {
  DCHECK(IsMainThread());
  if (web_audio_device_ && play_state_ == PlayState::kPaused) {
    web_audio_device_->Resume();
    play_state_ = PlayState::kPlaying;
  }
}

size_t AudioDestination::CallbackBufferSize() const {
  DCHECK(IsMainThread());
  return callback_buffer_size_;
}

bool AudioDestination::IsPlaying() {
  DCHECK(IsMainThread());
  return play_state_ == PlayState::kPlaying;
}

int AudioDestination::FramesPerBuffer() const {
  DCHECK(IsMainThread());
  return web_audio_device_->FramesPerBuffer();
}

size_t AudioDestination::HardwareBufferSize() {
  return Platform::Current()->AudioHardwareBufferSize();
}

float AudioDestination::HardwareSampleRate() {
  return static_cast<float>(Platform::Current()->AudioHardwareSampleRate());
}

uint32_t AudioDestination::MaxChannelCount() {
  return Platform::Current()->AudioHardwareOutputChannels();
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
      callback_buffer_size_ + audio_utilities::kRenderQuantumFrames <=
      kFIFOSize;
  DCHECK_LE(callback_buffer_size_ + audio_utilities::kRenderQuantumFrames,
            kFIFOSize);
  return is_buffer_size_valid;
}
}  // namespace blink
