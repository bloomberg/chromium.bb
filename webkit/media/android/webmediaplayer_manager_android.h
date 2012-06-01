// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_MANAGER_ANDROID_H_
#define WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_MANAGER_ANDROID_H_
#pragma once

#include <jni.h>
#include <map>

#include "base/basictypes.h"

namespace webkit_media {

class WebMediaPlayerAndroid;

// Class for managing all the WebMediaPlayerAndroid objects in a renderer
// process.
class WebMediaPlayerManagerAndroid {
 public:
  WebMediaPlayerManagerAndroid();
  ~WebMediaPlayerManagerAndroid();

  // Register and unregister a WebMediaPlayerAndroid object.
  int RegisterMediaPlayer(WebMediaPlayerAndroid* player);
  void UnregisterMediaPlayer(int player_id);

  // Release all the media resources on the renderer process.
  void ReleaseMediaResources();

  // Get the pointer to WebMediaPlayerAndroid given the |player_id|.
  WebMediaPlayerAndroid* GetMediaPlayer(int player_id);

 private:
  // Information needed to manage WebMediaPlayerAndroid.
  // TODO(qinmin): more informations will be added here for resource management.
  struct MediaPlayerInfo {
    webkit_media::WebMediaPlayerAndroid* player;
  };

  // Info for all available WebMediaPlayerAndroid on a page; kept so that
  // we can enumerate them to send updates about tab focus and visibily.
  std::map<int32, MediaPlayerInfo> media_players_;

  int32 next_media_player_id_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerManagerAndroid);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_ANDROID_WEBMEDIAPLAYER_MANAGER_ANDROID_H_
