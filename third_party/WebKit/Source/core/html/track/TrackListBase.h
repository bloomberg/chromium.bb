// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TrackListBase_h
#define TrackListBase_h

#include "core/events/EventTarget.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/track/TrackEvent.h"
#include "core/html/track/TrackEventInit.h"
#include "platform/bindings/TraceWrapperMember.h"

namespace blink {

template <class T>
class TrackListBase : public EventTargetWithInlineData {
 public:
  explicit TrackListBase(HTMLMediaElement* media_element)
      : media_element_(media_element) {}

  ~TrackListBase() override {}

  unsigned length() const { return tracks_.size(); }
  T* AnonymousIndexedGetter(unsigned index) const {
    if (index >= tracks_.size())
      return nullptr;
    return tracks_[index].Get();
  }

  T* getTrackById(const String& id) const {
    for (const auto& track : tracks_) {
      if (String(track->id()) == id)
        return track.Get();
    }

    return nullptr;
  }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(change);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(addtrack);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(removetrack);

  // EventTarget interface
  ExecutionContext* GetExecutionContext() const override {
    if (media_element_)
      return media_element_->GetExecutionContext();
    return nullptr;
  }

  void Add(T* track) {
    track->SetMediaElement(media_element_);
    tracks_.push_back(track);
    ScheduleEvent(TrackEvent::Create(EventTypeNames::addtrack, track));
  }

  void Remove(WebMediaPlayer::TrackId track_id) {
    for (unsigned i = 0; i < tracks_.size(); ++i) {
      if (tracks_[i]->id() != track_id)
        continue;

      tracks_[i]->SetMediaElement(0);
      ScheduleEvent(
          TrackEvent::Create(EventTypeNames::removetrack, tracks_[i].Get()));
      tracks_.erase(i);
      return;
    }
    NOTREACHED();
  }

  void RemoveAll() {
    for (const auto& track : tracks_)
      track->SetMediaElement(0);

    tracks_.clear();
  }

  void ScheduleChangeEvent() {
    ScheduleEvent(Event::Create(EventTypeNames::change));
  }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(tracks_);
    visitor->Trace(media_element_);
    EventTargetWithInlineData::Trace(visitor);
  }

  DECLARE_VIRTUAL_TRACE_WRAPPERS() {
    for (auto track : tracks_) {
      visitor->TraceWrappers(track);
    }
    EventTargetWithInlineData::TraceWrappers(visitor);
  }

 private:
  void ScheduleEvent(Event* event) {
    event->SetTarget(this);
    media_element_->ScheduleEvent(event);
  }

  HeapVector<TraceWrapperMember<T>> tracks_;
  Member<HTMLMediaElement> media_element_;
};

}  // namespace blink

#endif
