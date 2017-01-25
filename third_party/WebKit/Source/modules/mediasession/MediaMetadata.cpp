// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaMetadata.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ToV8.h"
#include "core/dom/TaskRunnerHelper.h"
#include "modules/mediasession/MediaImage.h"
#include "modules/mediasession/MediaMetadataInit.h"
#include "modules/mediasession/MediaSession.h"

namespace blink {

// static
MediaMetadata* MediaMetadata::create(ScriptState* scriptState,
                                     const MediaMetadataInit& metadata,
                                     ExceptionState& exceptionState) {
  return new MediaMetadata(scriptState, metadata, exceptionState);
}

MediaMetadata::MediaMetadata(ScriptState* scriptState,
                             const MediaMetadataInit& metadata,
                             ExceptionState& exceptionState)
    : m_notifySessionTimer(
          TaskRunnerHelper::get(TaskType::MiscPlatformAPI, scriptState),
          this,
          &MediaMetadata::notifySessionTimerFired) {
  m_title = metadata.title();
  m_artist = metadata.artist();
  m_album = metadata.album();
  setArtworkInternal(scriptState, metadata.artwork(), exceptionState);
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

const HeapVector<MediaImage>& MediaMetadata::artwork() const {
  return m_artwork;
}

Vector<v8::Local<v8::Value>> MediaMetadata::artwork(
    ScriptState* scriptState) const {
  Vector<v8::Local<v8::Value>> result(m_artwork.size());

  for (size_t i = 0; i < m_artwork.size(); ++i) {
    result[i] =
        freezeV8Object(ToV8(m_artwork[i], scriptState), scriptState->isolate());
  }

  return result;
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

void MediaMetadata::setArtwork(ScriptState* scriptState,
                               const HeapVector<MediaImage>& artwork,
                               ExceptionState& exceptionState) {
  setArtworkInternal(scriptState, artwork, exceptionState);
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

void MediaMetadata::setArtworkInternal(ScriptState* scriptState,
                                       const HeapVector<MediaImage>& artwork,
                                       ExceptionState& exceptionState) {
  HeapVector<MediaImage> processedArtwork(artwork);

  for (MediaImage& image : processedArtwork) {
    KURL url = scriptState->getExecutionContext()->completeURL(image.src());
    if (!url.isValid()) {
      exceptionState.throwTypeError("'" + image.src() +
                                    "' can't be resolved to a valid URL.");
      return;
    }
    image.setSrc(url);
  }

  DCHECK(!exceptionState.hadException());
  m_artwork.swap(processedArtwork);
}

DEFINE_TRACE(MediaMetadata) {
  visitor->trace(m_artwork);
  visitor->trace(m_session);
}

}  // namespace blink
