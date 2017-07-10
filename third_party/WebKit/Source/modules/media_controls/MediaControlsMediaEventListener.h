// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlsMediaEventListener_h
#define MediaControlsMediaEventListener_h

#include "core/events/EventListener.h"
#include "platform/wtf/Optional.h"

namespace blink {

class HTMLMediaElement;
class MediaControlsImpl;
class RemotePlayback;

class MediaControlsMediaEventListener final : public EventListener {
 public:
  explicit MediaControlsMediaEventListener(MediaControlsImpl*);

  // Called by MediaControls when the HTMLMediaElement is added to a document
  // document. All event listeners should be added.
  void Attach();

  // Called by MediaControls when the HTMLMediaElement is no longer in the
  // document. All event listeners should be removed in order to prepare the
  // object to be garbage collected.
  void Detach();

  bool operator==(const EventListener&) const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  HTMLMediaElement& GetMediaElement();
  RemotePlayback* GetRemotePlayback();

  void handleEvent(ExecutionContext*, Event*) override;

  void OnRemotePlaybackAvailabilityChanged();

  Member<MediaControlsImpl> media_controls_;
  WTF::Optional<int> remote_playback_availability_callback_id_;
};

}  // namespace blink

#endif  // MediaControlsMediaEventListener_h
