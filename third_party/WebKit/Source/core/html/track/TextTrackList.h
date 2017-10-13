/*
 * Copyright (C) 2011, 2012 Apple Inc.  All rights reserved.
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

#ifndef TextTrackList_h
#define TextTrackList_h

#include "core/dom/events/EventListener.h"
#include "core/dom/events/EventTarget.h"
#include "core/html/media/HTMLMediaElement.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

namespace blink {

class MediaElementEventQueue;
class TextTrack;

class CORE_EXPORT TextTrackList final : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TextTrackList* Create(HTMLMediaElement* owner) {
    return new TextTrackList(owner);
  }
  ~TextTrackList() override;

  unsigned length() const;
  int GetTrackIndex(TextTrack*);
  int GetTrackIndexRelativeToRenderedTracks(TextTrack*);
  bool Contains(TextTrack*) const;

  TextTrack* AnonymousIndexedGetter(unsigned index);
  TextTrack* getTrackById(const AtomicString& id);
  void Append(TextTrack*);
  void Remove(TextTrack*);

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(addtrack);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(change);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(removetrack);

  HTMLMediaElement* Owner() const;

  void ScheduleChangeEvent();
  void RemoveAllInbandTracks();

  bool HasShowingTracks();

  DECLARE_VIRTUAL_TRACE();

  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  explicit TextTrackList(HTMLMediaElement*);

  void ScheduleTrackEvent(const AtomicString& event_name, TextTrack*);

  void ScheduleAddTrackEvent(TextTrack*);
  void ScheduleRemoveTrackEvent(TextTrack*);

  void InvalidateTrackIndexesAfterTrack(TextTrack*);

  Member<HTMLMediaElement> owner_;

  Member<MediaElementEventQueue> async_event_queue_;

  HeapVector<TraceWrapperMember<TextTrack>> add_track_tracks_;
  HeapVector<TraceWrapperMember<TextTrack>> element_tracks_;
  HeapVector<TraceWrapperMember<TextTrack>> inband_tracks_;
};

}  // namespace blink

#endif  // TextTrackList_h
