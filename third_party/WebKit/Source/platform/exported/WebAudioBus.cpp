/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/platform/WebAudioBus.h"

#include "platform/audio/AudioBus.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class WebAudioBusPrivate : public AudioBus {};

void WebAudioBus::Initialize(unsigned number_of_channels,
                             size_t length,
                             double sample_rate) {
  RefPtr<AudioBus> audio_bus = AudioBus::Create(number_of_channels, length);
  audio_bus->SetSampleRate(sample_rate);

  if (private_)
    (static_cast<AudioBus*>(private_))->Deref();

  audio_bus->Ref();
  private_ = static_cast<WebAudioBusPrivate*>(audio_bus.Get());
}

void WebAudioBus::ResizeSmaller(size_t new_length) {
  DCHECK(private_);
  if (private_) {
    DCHECK_LE(new_length, length());
    private_->ResizeSmaller(new_length);
  }
}

void WebAudioBus::Reset() {
  if (private_) {
    (static_cast<AudioBus*>(private_))->Deref();
    private_ = 0;
  }
}

unsigned WebAudioBus::NumberOfChannels() const {
  if (!private_)
    return 0;
  return private_->NumberOfChannels();
}

size_t WebAudioBus::length() const {
  if (!private_)
    return 0;
  return private_->length();
}

double WebAudioBus::SampleRate() const {
  if (!private_)
    return 0;
  return private_->SampleRate();
}

float* WebAudioBus::ChannelData(unsigned channel_index) {
  if (!private_)
    return 0;
  DCHECK_LT(channel_index, NumberOfChannels());
  return private_->Channel(channel_index)->MutableData();
}

PassRefPtr<AudioBus> WebAudioBus::Release() {
  RefPtr<AudioBus> audio_bus(AdoptRef(static_cast<AudioBus*>(private_)));
  private_ = 0;
  return audio_bus;
}

}  // namespace blink
