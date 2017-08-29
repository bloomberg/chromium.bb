/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TrackEvent_h
#define TrackEvent_h

#include "core/dom/events/Event.h"
#include "core/html/track/TrackBase.h"
#include "core/html/track/TrackEventInit.h"

namespace blink {

class VideoTrackOrAudioTrackOrTextTrack;

class CORE_EXPORT TrackEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~TrackEvent() override;

  static TrackEvent* Create() { return new TrackEvent; }

  static TrackEvent* Create(const AtomicString& type,
                            const TrackEventInit& initializer) {
    return new TrackEvent(type, initializer);
  }

  template <typename T>
  static TrackEvent* Create(const AtomicString& type, T* track) {
    return new TrackEvent(type, track);
  }

  const AtomicString& InterfaceName() const override;

  void track(VideoTrackOrAudioTrackOrTextTrack&);

  DECLARE_VIRTUAL_TRACE();

 private:
  TrackEvent();
  TrackEvent(const AtomicString& type, const TrackEventInit& initializer);
  template <typename T>
  TrackEvent(const AtomicString& type, T* track)
      : Event(type, false, false), track_(track) {}

  Member<TrackBase> track_;
};

}  // namespace blink

#endif  // TrackEvent_h
