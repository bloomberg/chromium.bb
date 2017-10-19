// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaMetadata.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "modules/mediasession/MediaImage.h"
#include "modules/mediasession/MediaMetadataInit.h"
#include "modules/mediasession/MediaSession.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

// static
MediaMetadata* MediaMetadata::Create(ScriptState* script_state,
                                     const MediaMetadataInit& metadata,
                                     ExceptionState& exception_state) {
  return new MediaMetadata(script_state, metadata, exception_state);
}

MediaMetadata::MediaMetadata(ScriptState* script_state,
                             const MediaMetadataInit& metadata,
                             ExceptionState& exception_state)
    : notify_session_timer_(
          TaskRunnerHelper::Get(TaskType::kMiscPlatformAPI, script_state),
          this,
          &MediaMetadata::NotifySessionTimerFired) {
  title_ = metadata.title();
  artist_ = metadata.artist();
  album_ = metadata.album();
  SetArtworkInternal(script_state, metadata.artwork(), exception_state);
}

String MediaMetadata::title() const {
  return title_;
}

String MediaMetadata::artist() const {
  return artist_;
}

String MediaMetadata::album() const {
  return album_;
}

const HeapVector<MediaImage>& MediaMetadata::artwork() const {
  return artwork_;
}

Vector<v8::Local<v8::Value>> MediaMetadata::artwork(
    ScriptState* script_state) const {
  Vector<v8::Local<v8::Value>> result(artwork_.size());

  for (size_t i = 0; i < artwork_.size(); ++i) {
    result[i] = FreezeV8Object(ToV8(artwork_[i], script_state),
                               script_state->GetIsolate());
  }

  return result;
}

void MediaMetadata::setTitle(const String& title) {
  title_ = title;
  NotifySessionAsync();
}

void MediaMetadata::setArtist(const String& artist) {
  artist_ = artist;
  NotifySessionAsync();
}

void MediaMetadata::setAlbum(const String& album) {
  album_ = album;
  NotifySessionAsync();
}

void MediaMetadata::setArtwork(ScriptState* script_state,
                               const HeapVector<MediaImage>& artwork,
                               ExceptionState& exception_state) {
  SetArtworkInternal(script_state, artwork, exception_state);
  NotifySessionAsync();
}

void MediaMetadata::SetSession(MediaSession* session) {
  session_ = session;
}

void MediaMetadata::NotifySessionAsync() {
  if (!session_ || notify_session_timer_.IsActive())
    return;
  notify_session_timer_.StartOneShot(0, BLINK_FROM_HERE);
}

void MediaMetadata::NotifySessionTimerFired(TimerBase*) {
  if (!session_)
    return;
  session_->OnMetadataChanged();
}

void MediaMetadata::SetArtworkInternal(ScriptState* script_state,
                                       const HeapVector<MediaImage>& artwork,
                                       ExceptionState& exception_state) {
  HeapVector<MediaImage> processed_artwork(artwork);

  for (MediaImage& image : processed_artwork) {
    KURL url = ExecutionContext::From(script_state)->CompleteURL(image.src());
    if (!url.IsValid()) {
      exception_state.ThrowTypeError("'" + image.src() +
                                     "' can't be resolved to a valid URL.");
      return;
    }
    image.setSrc(url);
  }

  DCHECK(!exception_state.HadException());
  artwork_.swap(processed_artwork);
}

void MediaMetadata::Trace(blink::Visitor* visitor) {
  visitor->Trace(artwork_);
  visitor->Trace(session_);
}

}  // namespace blink
