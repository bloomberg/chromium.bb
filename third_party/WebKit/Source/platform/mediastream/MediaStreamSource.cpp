/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/mediastream/MediaStreamSource.h"

namespace blink {

MediaStreamSource* MediaStreamSource::Create(const String& id,
                                             StreamType type,
                                             const String& name,
                                             bool remote,
                                             ReadyState ready_state,
                                             bool requires_consumer) {
  return new MediaStreamSource(id, type, name, remote, ready_state,
                               requires_consumer);
}

MediaStreamSource::MediaStreamSource(const String& id,
                                     StreamType type,
                                     const String& name,
                                     bool remote,
                                     ReadyState ready_state,
                                     bool requires_consumer)
    : id_(id),
      type_(type),
      name_(name),
      remote_(remote),
      ready_state_(ready_state),
      requires_consumer_(requires_consumer) {}

void MediaStreamSource::SetReadyState(ReadyState ready_state) {
  if (ready_state_ != kReadyStateEnded && ready_state_ != ready_state) {
    ready_state_ = ready_state;

    // Observers may dispatch events which create and add new Observers;
    // take a snapshot so as to safely iterate.
    HeapVector<Member<Observer>> observers;
    CopyToVector(observers_, observers);
    for (auto observer : observers)
      observer->SourceChangedState();

    // setReadyState() will be invoked via the MediaStreamComponent::dispose()
    // prefinalizer, allocating |observers|. Which means that |observers| will
    // live until the next GC (but be unreferenced by other heap objects),
    // _but_ it will potentially contain references to Observers that were
    // GCed after the MediaStreamComponent prefinalizer had completed.
    //
    // So, if the next GC is a conservative one _and_ it happens to find
    // a reference to |observers| when scanning the stack, we're in trouble
    // as it contains references to now-dead objects.
    //
    // Work around this by explicitly clearing the vector backing store.
    //
    // TODO(sof): consider adding run-time checks that disallows this kind
    // of dead object revivification by default.
    for (size_t i = 0; i < observers.size(); ++i)
      observers[i] = nullptr;
  }
}

void MediaStreamSource::AddObserver(MediaStreamSource::Observer* observer) {
  observers_.insert(observer);
}

void MediaStreamSource::AddAudioConsumer(AudioDestinationConsumer* consumer) {
  DCHECK(requires_consumer_);
  MutexLocker locker(audio_consumers_lock_);
  audio_consumers_.insert(consumer);
}

bool MediaStreamSource::RemoveAudioConsumer(
    AudioDestinationConsumer* consumer) {
  DCHECK(requires_consumer_);
  MutexLocker locker(audio_consumers_lock_);
  auto it = audio_consumers_.find(consumer);
  if (it == audio_consumers_.end())
    return false;
  audio_consumers_.erase(it);
  return true;
}

void MediaStreamSource::GetSettings(WebMediaStreamTrack::Settings& settings) {
  settings.device_id = Id();

  if (echo_cancellation_.has_value())
    settings.echo_cancellation = *echo_cancellation_;
}

void MediaStreamSource::SetAudioFormat(size_t number_of_channels,
                                       float sample_rate) {
  DCHECK(requires_consumer_);
  MutexLocker locker(audio_consumers_lock_);
  for (AudioDestinationConsumer* consumer : audio_consumers_)
    consumer->SetFormat(number_of_channels, sample_rate);
}

void MediaStreamSource::ConsumeAudio(AudioBus* bus, size_t number_of_frames) {
  DCHECK(requires_consumer_);
  MutexLocker locker(audio_consumers_lock_);
  for (AudioDestinationConsumer* consumer : audio_consumers_)
    consumer->ConsumeAudio(bus, number_of_frames);
}

DEFINE_TRACE(MediaStreamSource) {
  visitor->Trace(observers_);
}

}  // namespace blink
