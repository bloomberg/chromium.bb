// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_PROXY_ANDROID_H_
#define WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_PROXY_ANDROID_H_

#include <string>

#include "base/time.h"
#include "googleurl/src/gurl.h"

namespace webkit_media {

// An interface to facilitate the IPC between the browser side
// android::MediaPlayer and the WebMediaPlayerImplAndroid in the renderer
// process.
// Implementation is provided by content::WebMediaPlayerProxyImplAndroid.
class WebMediaPlayerProxyAndroid {
 public:
  virtual ~WebMediaPlayerProxyAndroid();

  // Initialize a MediaPlayerBridge object in browser process
  virtual void Initialize(int player_id, const GURL& url,
                          const GURL& first_party_for_cookies) = 0;

  // Start the player.
  virtual void Start(int player_id) = 0;

  // Pause the player.
  virtual void Pause(int player_id) = 0;

  // Perform seek on the player.
  virtual void Seek(int player_id, base::TimeDelta time) = 0;

  // Release resources for the player.
  virtual void ReleaseResources(int player_id) = 0;

  // Destroy the player in the browser process
  virtual void DestroyPlayer(int player_id) = 0;

  // Request the player to enter fullscreen.
  virtual void EnterFullscreen(int player_id) = 0;

  // Request the player to exit fullscreen.
  virtual void ExitFullscreen(int player_id) = 0;

  // Request an external surface for out-of-band compositing.
  virtual void RequestExternalSurface(int player_id) = 0;
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_PROXY_ANDROID_H_
