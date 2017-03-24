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

MediaStreamSource* MediaStreamSource::create(const String& id,
                                             StreamType type,
                                             const String& name,
                                             bool remote,
                                             ReadyState readyState,
                                             bool requiresConsumer) {
  return new MediaStreamSource(id, type, name, remote, readyState,
                               requiresConsumer);
}

MediaStreamSource::MediaStreamSource(const String& id,
                                     StreamType type,
                                     const String& name,
                                     bool remote,
                                     ReadyState readyState,
                                     bool requiresConsumer)
    : m_id(id),
      m_type(type),
      m_name(name),
      m_remote(remote),
      m_readyState(readyState),
      m_requiresConsumer(requiresConsumer) {}

void MediaStreamSource::setReadyState(ReadyState readyState) {
  if (m_readyState != ReadyStateEnded && m_readyState != readyState) {
    m_readyState = readyState;

    // Observers may dispatch events which create and add new Observers;
    // take a snapshot so as to safely iterate.
    HeapVector<Member<Observer>> observers;
    copyToVector(m_observers, observers);
    for (auto observer : observers)
      observer->sourceChangedState();

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

void MediaStreamSource::addObserver(MediaStreamSource::Observer* observer) {
  m_observers.insert(observer);
}

void MediaStreamSource::addAudioConsumer(AudioDestinationConsumer* consumer) {
  ASSERT(m_requiresConsumer);
  MutexLocker locker(m_audioConsumersLock);
  m_audioConsumers.insert(consumer);
}

bool MediaStreamSource::removeAudioConsumer(
    AudioDestinationConsumer* consumer) {
  ASSERT(m_requiresConsumer);
  MutexLocker locker(m_audioConsumersLock);
  auto it = m_audioConsumers.find(consumer);
  if (it == m_audioConsumers.end())
    return false;
  m_audioConsumers.erase(it);
  return true;
}

void MediaStreamSource::getSettings(WebMediaStreamTrack::Settings& settings) {
  settings.deviceId = id();
}

void MediaStreamSource::setAudioFormat(size_t numberOfChannels,
                                       float sampleRate) {
  ASSERT(m_requiresConsumer);
  MutexLocker locker(m_audioConsumersLock);
  for (AudioDestinationConsumer* consumer : m_audioConsumers)
    consumer->setFormat(numberOfChannels, sampleRate);
}

void MediaStreamSource::consumeAudio(AudioBus* bus, size_t numberOfFrames) {
  ASSERT(m_requiresConsumer);
  MutexLocker locker(m_audioConsumersLock);
  for (AudioDestinationConsumer* consumer : m_audioConsumers)
    consumer->consumeAudio(bus, numberOfFrames);
}

DEFINE_TRACE(MediaStreamSource) {
  visitor->trace(m_observers);
}

}  // namespace blink
