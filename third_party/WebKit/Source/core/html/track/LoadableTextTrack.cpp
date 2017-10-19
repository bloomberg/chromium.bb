/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#include "core/html/track/LoadableTextTrack.h"

#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLTrackElement.h"

namespace blink {

LoadableTextTrack::LoadableTextTrack(HTMLTrackElement* track)
    : TextTrack(SubtitlesKeyword(),
                g_empty_atom,
                g_empty_atom,
                g_empty_atom,
                kTrackElement),
      track_element_(track) {
  DCHECK(track_element_);
}

LoadableTextTrack::~LoadableTextTrack() {}

bool LoadableTextTrack::IsDefault() const {
  return track_element_->FastHasAttribute(HTMLNames::defaultAttr);
}

void LoadableTextTrack::setMode(const AtomicString& mode) {
  TextTrack::setMode(mode);
  if (track_element_->getReadyState() == HTMLTrackElement::kNone)
    track_element_->ScheduleLoad();
}

size_t LoadableTextTrack::TrackElementIndex() const {
  // Count the number of preceding <track> elements (== the index.)
  size_t index = 0;
  for (const HTMLTrackElement* track =
           Traversal<HTMLTrackElement>::PreviousSibling(*track_element_);
       track; track = Traversal<HTMLTrackElement>::PreviousSibling(*track))
    ++index;

  return index;
}

void LoadableTextTrack::Trace(blink::Visitor* visitor) {
  visitor->Trace(track_element_);
  TextTrack::Trace(visitor);
}

}  // namespace blink
