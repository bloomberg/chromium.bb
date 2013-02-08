// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/android/webmediaplayer_manager_android.h"

#include "webkit/media/android/webmediaplayer_android.h"

namespace webkit_media {

WebMediaPlayerManagerAndroid::WebMediaPlayerManagerAndroid()
    : next_media_player_id_(0),
      fullscreen_frame_(NULL) {
}

WebMediaPlayerManagerAndroid::~WebMediaPlayerManagerAndroid() {
  std::map<int32, WebMediaPlayerAndroid*>::iterator player_it;
  for (player_it = media_players_.begin();
      player_it != media_players_.end(); ++player_it) {
    WebMediaPlayerAndroid* player = player_it->second;
    player->Detach();
  }
}

int WebMediaPlayerManagerAndroid::RegisterMediaPlayer(
    WebMediaPlayerAndroid* player) {
  media_players_[next_media_player_id_] = player;
  return next_media_player_id_++;
}

void WebMediaPlayerManagerAndroid::UnregisterMediaPlayer(int player_id) {
  media_players_.erase(player_id);
}

void WebMediaPlayerManagerAndroid::ReleaseMediaResources() {
  std::map<int32, WebMediaPlayerAndroid*>::iterator player_it;
  for (player_it = media_players_.begin();
      player_it != media_players_.end(); ++player_it) {
    WebMediaPlayerAndroid* player = player_it->second;

    // Do not release if an audio track is still playing
    if (player && (player->paused() || player->hasVideo()))
      player->ReleaseMediaResources();
  }
}

WebMediaPlayerAndroid* WebMediaPlayerManagerAndroid::GetMediaPlayer(
    int player_id) {
  std::map<int32, WebMediaPlayerAndroid*>::iterator iter =
      media_players_.find(player_id);
  if (iter != media_players_.end())
    return iter->second;
  return NULL;
}

bool WebMediaPlayerManagerAndroid::CanEnterFullscreen(WebKit::WebFrame* frame) {
  return !fullscreen_frame_ || IsInFullscreen(frame);
}

void WebMediaPlayerManagerAndroid::DidEnterFullscreen(WebKit::WebFrame* frame) {
  fullscreen_frame_ = frame;
}

void WebMediaPlayerManagerAndroid::DidExitFullscreen() {
  fullscreen_frame_ = NULL;
}

bool WebMediaPlayerManagerAndroid::IsInFullscreen(WebKit::WebFrame* frame) {
  return fullscreen_frame_ == frame;
}

}  // namespace webkit_media
