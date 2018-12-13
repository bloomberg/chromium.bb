// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_REMOTE_PLAYBACK_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_REMOTE_PLAYBACK_OBSERVER_H_

namespace blink {

enum class WebRemotePlaybackAvailability;
enum class WebRemotePlaybackState;

// Interface to be implemented by objects that intend to be notified by remote
// playback status changes on an HTMLMediaElement. The object should self-add
// itself to the RemotePlaybackController using the add/remove observer methods.
class RemotePlaybackObserver : public GarbageCollectedMixin {
 public:
  // Called when the remote playback state is changed. The state is related to
  // the connection to a remote device.
  virtual void OnRemotePlaybackStateChanged(WebRemotePlaybackState) = 0;

  // Called when the remote playback availability is changed. The availabality
  // is realted to the presence of a compatible remote device on the network.
  virtual void OnRemotePlaybackAvailabilityChanged(
      WebRemotePlaybackAvailability) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_REMOTE_PLAYBACK_OBSERVER_H_
