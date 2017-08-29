/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "core/html/track/TextTrackCue.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/events/Event.h"
#include "core/html/track/TextTrack.h"
#include "core/html/track/TextTrackCueList.h"

namespace blink {

static const unsigned kInvalidCueIndex = UINT_MAX;

TextTrackCue::TextTrackCue(double start, double end)
    : start_time_(start),
      end_time_(end),
      track_(nullptr),
      cue_index_(kInvalidCueIndex),
      is_active_(false),
      pause_on_exit_(false) {}

void TextTrackCue::CueWillChange() {
  if (track_)
    track_->CueWillChange(this);
}

void TextTrackCue::CueDidChange() {
  if (track_)
    track_->CueDidChange(this);
}

TextTrack* TextTrackCue::track() const {
  return track_;
}

void TextTrackCue::SetTrack(TextTrack* track) {
  track_ = track;
}

Node* TextTrackCue::Owner() const {
  return track_ ? track_->Owner() : 0;
}

void TextTrackCue::setId(const AtomicString& id) {
  if (id_ == id)
    return;

  CueWillChange();
  id_ = id;
  CueDidChange();
}

void TextTrackCue::setStartTime(double value) {
  // TODO(93143): Add spec-compliant behavior for negative time values.
  if (start_time_ == value || value < 0)
    return;

  CueWillChange();
  start_time_ = value;
  CueDidChange();
}

void TextTrackCue::setEndTime(double value) {
  // TODO(93143): Add spec-compliant behavior for negative time values.
  if (end_time_ == value || value < 0)
    return;

  CueWillChange();
  end_time_ = value;
  CueDidChange();
}

void TextTrackCue::setPauseOnExit(bool value) {
  if (pause_on_exit_ == value)
    return;

  CueWillChange();
  pause_on_exit_ = value;
  CueDidChange();
}

void TextTrackCue::InvalidateCueIndex() {
  cue_index_ = kInvalidCueIndex;
}

unsigned TextTrackCue::CueIndex() {
  // This method can only be called on cues while they are associated with
  // a(n enabled) track (and hence that track's list of cues should exist.)
  DCHECK(track() && track()->cues());
  TextTrackCueList* cue_list = track()->cues();
  if (!cue_list->IsCueIndexValid(cue_index_))
    cue_list->ValidateCueIndexes();
  return cue_index_;
}

DispatchEventResult TextTrackCue::DispatchEventInternal(Event* event) {
  // When a TextTrack's mode is disabled: no cues are active, no events fired.
  if (!track() || track()->mode() == TextTrack::DisabledKeyword())
    return DispatchEventResult::kCanceledBeforeDispatch;

  return EventTarget::DispatchEventInternal(event);
}

const AtomicString& TextTrackCue::InterfaceName() const {
  return EventTargetNames::TextTrackCue;
}

DEFINE_TRACE(TextTrackCue) {
  visitor->Trace(track_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
