// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/track/AudioTrack.h"

#include "core/html/media/HTMLMediaElement.h"

namespace blink {

AudioTrack::AudioTrack(const String& id,
                       const AtomicString& kind,
                       const AtomicString& label,
                       const AtomicString& language,
                       bool enabled)
    : TrackBase(WebMediaPlayer::kAudioTrack, kind, label, language, id),
      enabled_(enabled) {}

AudioTrack::~AudioTrack() {}

DEFINE_TRACE(AudioTrack) {
  TrackBase::Trace(visitor);
}

void AudioTrack::setEnabled(bool enabled) {
  if (enabled == enabled_)
    return;

  enabled_ = enabled;

  if (MediaElement())
    MediaElement()->AudioTrackChanged(this);
}

const AtomicString& AudioTrack::AlternativeKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("alternative"));
  return keyword;
}

const AtomicString& AudioTrack::DescriptionsKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("descriptions"));
  return keyword;
}

const AtomicString& AudioTrack::MainKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("main"));
  return keyword;
}

const AtomicString& AudioTrack::MainDescriptionsKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("main-desc"));
  return keyword;
}

const AtomicString& AudioTrack::TranslationKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("translation"));
  return keyword;
}

const AtomicString& AudioTrack::CommentaryKeyword() {
  DEFINE_STATIC_LOCAL(const AtomicString, keyword, ("commentary"));
  return keyword;
}

bool AudioTrack::IsValidKindKeyword(const String& kind) {
  return kind == AlternativeKeyword() || kind == DescriptionsKeyword() ||
         kind == MainKeyword() || kind == MainDescriptionsKeyword() ||
         kind == TranslationKeyword() || kind == CommentaryKeyword() ||
         kind == g_empty_atom;
}

}  // namespace blink
