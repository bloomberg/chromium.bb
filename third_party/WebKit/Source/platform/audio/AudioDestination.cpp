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
#include "public/platform/Platform.h"
#include "public/platform/WebAudioLatencyHint.h"
#include "public/platform/WebSecurityOrigin.h"
#include "wtf/PtrUtil.h"

namespace blink {

// FIFO Size.
//
// TODO(hongchan): This was estimated based on the largest callback buffer size
// that we would ever need. The current UMA stats indicates that this is, in
// fact, probably too small. There are Android devices out there with a size of
// 8000 or so.  We might need to make this larger. See: crbug.com/670747
const size_t kFIFOSize = 8192;

std::unique_ptr<AudioDestination> AudioDestination::create(
    AudioIOCallback& callback,
    unsigned numberOfOutputChannels,
    const WebAudioLatencyHint& latencyHint,
    PassRefPtr<SecurityOrigin> securityOrigin) {
  return WTF::wrapUnique(new AudioDestination(callback, numberOfOutputChannels,
                                              latencyHint,
                                              std::move(securityOrigin)));
}

AudioDestination::AudioDestination(AudioIOCallback& callback,
                                   unsigned numberOfOutputChannels,
                                   const WebAudioLatencyHint& latencyHint,
                                   PassRefPtr<SecurityOrigin> securityOrigin)
    : m_numberOfOutputChannels(numberOfOutputChannels),
      m_isPlaying(false),
      m_callback(callback),
      m_outputBus(AudioBus::create(numberOfOutputChannels,
                                   AudioUtilities::kRenderQuantumFrames,
                                   false)),
      m_renderBus(AudioBus::create(numberOfOutputChannels,
                                   AudioUtilities::kRenderQuantumFrames)),
      m_fifo(
          WTF::wrapUnique(new PushPullFIFO(numberOfOutputChannels, kFIFOSize))),
      m_framesElapsed(0) {
  // Create WebAudioDevice. blink::WebAudioDevice is designed to support the
  // local input (e.g. loopback from OS audio system), but Chromium's media
  // renderer does not support it currently. Thus, we use zero for the number
  // of input channels.
  m_webAudioDevice = WTF::wrapUnique(Platform::current()->createAudioDevice(
      0, numberOfOutputChannels, latencyHint, this, String(),
      std::move(securityOrigin)));
  DCHECK(m_webAudioDevice);

  m_callbackBufferSize = m_webAudioDevice->framesPerBuffer();
  if (!checkBufferSize()) {
    NOTREACHED();
  }
}

AudioDestination::~AudioDestination() {
  stop();
}

void AudioDestination::render(const WebVector<float*>& destinationData,
                              size_t numberOfFrames,
                              double delay,
                              double delayTimestamp,
                              size_t priorFramesSkipped) {
  CHECK_EQ(destinationData.size(), m_numberOfOutputChannels);
  CHECK_EQ(numberOfFrames, m_callbackBufferSize);

  // Note that this method is called by AudioDeviceThread. If FIFO is not ready,
  // or the requested render size is greater than FIFO size return here.
  // (crbug.com/692423)
  if (!m_fifo || m_fifo->length() < numberOfFrames)
    return;

  m_framesElapsed -= std::min(m_framesElapsed, priorFramesSkipped);
  double outputPosition =
      m_framesElapsed / static_cast<double>(m_webAudioDevice->sampleRate()) -
      delay;
  m_outputPosition.position = outputPosition;
  m_outputPosition.timestamp = delayTimestamp;
  m_outputPositionReceivedTimestamp = base::TimeTicks::Now();

  // Associate the destination data array with the output bus then fill the
  // FIFO.
  for (unsigned i = 0; i < m_numberOfOutputChannels; ++i)
    m_outputBus->setChannelMemory(i, destinationData[i], numberOfFrames);

  // Number of frames to render via WebAudio graph. |framesToRender > 0| means
  // the frames in FIFO is not enough to fulfill the requested frames from the
  // audio device.
  size_t framesToRender = numberOfFrames > m_fifo->framesAvailable()
                              ? numberOfFrames - m_fifo->framesAvailable()
                              : 0;

  for (size_t pushedFrames = 0; pushedFrames < framesToRender;
       pushedFrames += AudioUtilities::kRenderQuantumFrames) {
    // If platform buffer is more than two times longer than |framesToProcess|
    // we do not want output position to get stuck so we promote it
    // using the elapsed time from the moment it was initially obtained.
    if (m_callbackBufferSize > AudioUtilities::kRenderQuantumFrames * 2) {
      double delta =
          (base::TimeTicks::Now() - m_outputPositionReceivedTimestamp)
              .InSecondsF();
      m_outputPosition.position += delta;
      m_outputPosition.timestamp += delta;
    }

    // Some implementations give only rough estimation of |delay| so
    // we might have negative estimation |outputPosition| value.
    if (m_outputPosition.position < 0.0)
      m_outputPosition.position = 0.0;

    // Process WebAudio graph and push the rendered output to FIFO.
    m_callback.render(nullptr, m_renderBus.get(),
                      AudioUtilities::kRenderQuantumFrames, m_outputPosition);
    m_fifo->push(m_renderBus.get());
  }

  m_fifo->pull(m_outputBus.get(), numberOfFrames);

  m_framesElapsed += numberOfFrames;
}

void AudioDestination::start() {
  if (m_webAudioDevice && !m_isPlaying) {
    m_webAudioDevice->start();
    m_isPlaying = true;
  }
}

void AudioDestination::stop() {
  if (m_webAudioDevice && m_isPlaying) {
    m_webAudioDevice->stop();
    m_isPlaying = false;
  }
}

size_t AudioDestination::hardwareBufferSize() {
  return Platform::current()->audioHardwareBufferSize();
}

float AudioDestination::hardwareSampleRate() {
  return static_cast<float>(Platform::current()->audioHardwareSampleRate());
}

unsigned long AudioDestination::maxChannelCount() {
  return static_cast<unsigned long>(
      Platform::current()->audioHardwareOutputChannels());
}

bool AudioDestination::checkBufferSize() {
  // Histogram for audioHardwareBufferSize
  DEFINE_STATIC_LOCAL(SparseHistogram, hardwareBufferSizeHistogram,
                      ("WebAudio.AudioDestination.HardwareBufferSize"));

  // Histogram for the actual callback size used.  Typically, this is the same
  // as audioHardwareBufferSize, but can be adjusted depending on some
  // heuristics below.
  DEFINE_STATIC_LOCAL(SparseHistogram, callbackBufferSizeHistogram,
                      ("WebAudio.AudioDestination.CallbackBufferSize"));

  // Record the sizes if we successfully created an output device.
  hardwareBufferSizeHistogram.sample(hardwareBufferSize());
  callbackBufferSizeHistogram.sample(m_callbackBufferSize);

  // Check if the requested buffer size is too large.
  bool isBufferSizeValid =
      m_callbackBufferSize + AudioUtilities::kRenderQuantumFrames <= kFIFOSize;
  DCHECK(isBufferSizeValid);
  return isBufferSizeValid;
}

}  // namespace blink
