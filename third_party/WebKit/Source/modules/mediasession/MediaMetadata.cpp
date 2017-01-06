// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaMetadata.h"

#include "core/dom/ExecutionContext.h"
#include "modules/mediasession/MediaImage.h"
#include "modules/mediasession/MediaMetadataInit.h"
#include "modules/mediasession/MediaSession.h"

namespace blink {

// static
MediaMetadata* MediaMetadata::create(ExecutionContext* context,
                                     const MediaMetadataInit& metadata) {
  return new MediaMetadata(context, metadata);
}

MediaMetadata::MediaMetadata(ExecutionContext* context,
                             const MediaMetadataInit& metadata)
    : m_notifySessionTimer(this, &MediaMetadata::notifySessionTimerFired) {
  m_title = metadata.title();
  m_artist = metadata.artist();
  m_album = metadata.album();
  for (const auto& image : metadata.artwork())
    m_artwork.push_back(MediaImage::create(context, image));
}

String MediaMetadata::title() const {
  return m_title;
}

String MediaMetadata::artist() const {
  return m_artist;
}

String MediaMetadata::album() const {
  return m_album;
}

const HeapVector<Member<MediaImage>>& MediaMetadata::artwork() const {
  return m_artwork;
}

void MediaMetadata::setTitle(const String& title) {
  m_title = title;
  notifySessionAsync();
}

void MediaMetadata::setArtist(const String& artist) {
  m_artist = artist;
  notifySessionAsync();
}

void MediaMetadata::setAlbum(const String& album) {
  m_album = album;
  notifySessionAsync();
}

void MediaMetadata::setArtwork(const HeapVector<Member<MediaImage>>& artwork) {
  m_artwork = artwork;
  notifySessionAsync();
}

void MediaMetadata::setSession(MediaSession* session) {
  m_session = session;
}

void MediaMetadata::notifySessionAsync() {
  if (!m_session || m_notifySessionTimer.isActive())
    return;
  m_notifySessionTimer.startOneShot(0, BLINK_FROM_HERE);
}

void MediaMetadata::notifySessionTimerFired(TimerBase*) {
  if (!m_session)
    return;
  m_session->onMetadataChanged();
}

DEFINE_TRACE(MediaMetadata) {
  visitor->trace(m_artwork);
  visitor->trace(m_session);
}

}  // namespace blink
