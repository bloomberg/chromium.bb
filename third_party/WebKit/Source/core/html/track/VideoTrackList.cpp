// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/track/VideoTrackList.h"

#include "core/html/media/HTMLMediaElement.h"
#include "core/html/track/VideoTrack.h"

namespace blink {

VideoTrackList* VideoTrackList::Create(HTMLMediaElement& media_element) {
  return new VideoTrackList(media_element);
}

VideoTrackList::~VideoTrackList() = default;

VideoTrackList::VideoTrackList(HTMLMediaElement& media_element)
    : TrackListBase<VideoTrack>(&media_element) {}

const AtomicString& VideoTrackList::InterfaceName() const {
  return EventTargetNames::VideoTrackList;
}

int VideoTrackList::selectedIndex() const {
  for (unsigned i = 0; i < length(); ++i) {
    VideoTrack* track = AnonymousIndexedGetter(i);

    if (track->selected())
      return i;
  }

  return -1;
}

void VideoTrackList::TrackSelected(WebMediaPlayer::TrackId selected_track_id) {
  // Clear the selected flag on the previously selected track, if any.
  for (unsigned i = 0; i < length(); ++i) {
    VideoTrack* track = AnonymousIndexedGetter(i);

    if (track->id() != selected_track_id)
      track->ClearSelected();
    else
      DCHECK(track->selected());
  }
}

}  // namespace blink
