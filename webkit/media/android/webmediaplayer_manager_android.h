// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_MANAGER_ANDROID_H_
#define WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_MANAGER_ANDROID_H_

#include <map>

#include "base/basictypes.h"

namespace WebKit {
class WebFrame;
}

namespace webkit_media {

class WebMediaPlayerAndroid;

// Class for managing all the WebMediaPlayerAndroid objects in the same
// RenderView.
class WebMediaPlayerManagerAndroid {
 public:
  WebMediaPlayerManagerAndroid();
  virtual ~WebMediaPlayerManagerAndroid();

  // Register and unregister a WebMediaPlayerAndroid object.
  int RegisterMediaPlayer(WebMediaPlayerAndroid* player);
  void UnregisterMediaPlayer(int player_id);

  // Release all the media resources managed by this object unless
  // an audio play is in progress.
  void ReleaseMediaResources();

  // Check whether a player can enter fullscreen.
  bool CanEnterFullscreen(WebKit::WebFrame* frame);

  // Called when a player entered or exited fullscreen.
  void DidEnterFullscreen(WebKit::WebFrame* frame);
  void DidExitFullscreen();

  // Check whether the Webframe is in fullscreen.
  bool IsInFullscreen(WebKit::WebFrame* frame);

  // Get the pointer to WebMediaPlayerAndroid given the |player_id|.
  WebMediaPlayerAndroid* GetMediaPlayer(int player_id);

 private:
  // Info for all available WebMediaPlayerAndroid on a page; kept so that
  // we can enumerate them to send updates about tab focus and visibily.
  std::map<int32, WebMediaPlayerAndroid*> media_players_;

  int32 next_media_player_id_;

  // WebFrame of the fullscreen video.
  WebKit::WebFrame* fullscreen_frame_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerManagerAndroid);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_MANAGER_ANDROID_H_
