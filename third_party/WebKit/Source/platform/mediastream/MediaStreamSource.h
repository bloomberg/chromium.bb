/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
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
 * 3. Neither the name of Ericsson nor the names of its contributors
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

#ifndef MediaStreamSource_h
#define MediaStreamSource_h

#include <memory>
#include <utility>

#include "platform/PlatformExport.h"
#include "platform/audio/AudioDestinationConsumer.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/ThreadingPrimitives.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebMediaConstraints.h"
#include "public/platform/WebMediaStreamTrack.h"

namespace blink {

class PLATFORM_EXPORT MediaStreamSource final
    : public GarbageCollectedFinalized<MediaStreamSource> {
 public:
  class PLATFORM_EXPORT Observer : public GarbageCollectedMixin {
   public:
    virtual ~Observer() {}
    virtual void SourceChangedState() = 0;
  };

  class ExtraData {
    USING_FAST_MALLOC(ExtraData);

   public:
    virtual ~ExtraData() {}
  };

  enum StreamType { kTypeAudio, kTypeVideo };

  enum ReadyState {
    kReadyStateLive = 0,
    kReadyStateMuted = 1,
    kReadyStateEnded = 2
  };

  static MediaStreamSource* Create(const String& id,
                                   StreamType,
                                   const String& name,
                                   bool remote,
                                   ReadyState = kReadyStateLive,
                                   bool requires_consumer = false);

  const String& Id() const { return id_; }
  StreamType GetType() const { return type_; }
  const String& GetName() const { return name_; }
  bool Remote() const { return remote_; }

  void SetReadyState(ReadyState);
  ReadyState GetReadyState() const { return ready_state_; }

  void AddObserver(Observer*);

  ExtraData* GetExtraData() const { return extra_data_.get(); }
  void SetExtraData(std::unique_ptr<ExtraData> extra_data) {
    extra_data_ = std::move(extra_data);
  }

  void SetEchoCancellation(bool echo_cancellation) {
    echo_cancellation_ = WTF::make_optional(echo_cancellation);
  }

  void SetConstraints(WebMediaConstraints constraints) {
    constraints_ = constraints;
  }
  WebMediaConstraints Constraints() { return constraints_; }
  void GetSettings(WebMediaStreamTrack::Settings&);

  void SetAudioFormat(size_t number_of_channels, float sample_rate);
  void ConsumeAudio(AudioBus*, size_t number_of_frames);

  bool RequiresAudioConsumer() const { return requires_consumer_; }
  void AddAudioConsumer(AudioDestinationConsumer*);
  bool RemoveAudioConsumer(AudioDestinationConsumer*);
  const HashSet<AudioDestinationConsumer*>& AudioConsumers() {
    return audio_consumers_;
  }

  // |m_extraData| may hold pointers to GC objects, and it may touch them in
  // destruction.  So this class is eagerly finalized to finalize |m_extraData|
  // promptly.
  EAGERLY_FINALIZE();
  DECLARE_TRACE();

 private:
  MediaStreamSource(const String& id,
                    StreamType,
                    const String& name,
                    bool remote,
                    ReadyState,
                    bool requires_consumer);

  String id_;
  StreamType type_;
  String name_;
  bool remote_;
  ReadyState ready_state_;
  bool requires_consumer_;
  HeapHashSet<WeakMember<Observer>> observers_;
  Mutex audio_consumers_lock_;
  HashSet<AudioDestinationConsumer*> audio_consumers_;
  std::unique_ptr<ExtraData> extra_data_;
  WebMediaConstraints constraints_;
  WTF::Optional<bool> echo_cancellation_;
};

typedef HeapVector<Member<MediaStreamSource>> MediaStreamSourceVector;

}  // namespace blink

#endif  // MediaStreamSource_h
