// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/track/VideoTrack.h"

#include "core/html/media/HTMLMediaElement.h"

namespace blink {

VideoTrack::VideoTrack(const String& id,
                       const AtomicString& kind,
                       const AtomicString& label,
                       const AtomicString& language,
                       bool selected)
    : TrackBase(WebMediaPlayer::kVideoTrack, kind, label, language, id),
      selected_(selected) {}

VideoTrack::~VideoTrack() {}

void VideoTrack::Trace(blink::Visitor* visitor) {
  TrackBase::Trace(visitor);
}

void VideoTrack::setSelected(bool selected) {
  if (selected == selected_)
    return;

  selected_ = selected;

  if (MediaElement())
    MediaElement()->SelectedVideoTrackChanged(this);
}

const AtomicString& VideoTrack::AlternativeKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("alternative"));
  return keyword;
}

const AtomicString& VideoTrack::CaptionsKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("captions"));
  return keyword;
}

const AtomicString& VideoTrack::MainKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("main"));
  return keyword;
}

const AtomicString& VideoTrack::SignKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("sign"));
  return keyword;
}

const AtomicString& VideoTrack::SubtitlesKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("subtitles"));
  return keyword;
}

const AtomicString& VideoTrack::CommentaryKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("commentary"));
  return keyword;
}

bool VideoTrack::IsValidKindKeyword(const String& kind) {
  return kind == AlternativeKeyword() || kind == CaptionsKeyword() ||
         kind == MainKeyword() || kind == SignKeyword() ||
         kind == SubtitlesKeyword() || kind == CommentaryKeyword() ||
         kind == g_empty_atom;
}

}  // namespace blink
