// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/android/webmediaplayer_manager_android.h"

#include "webkit/media/android/webmediaplayer_android.h"

namespace webkit_media {

WebMediaPlayerManagerAndroid::WebMediaPlayerManagerAndroid()
    : next_media_player_id_(0) {
}

WebMediaPlayerManagerAndroid::~WebMediaPlayerManagerAndroid() {}

int WebMediaPlayerManagerAndroid::RegisterMediaPlayer(
    WebMediaPlayerAndroid* player) {
  MediaPlayerInfo info;
  info.player = player;
  media_players_[next_media_player_id_] = info;
  return next_media_player_id_++;
}

void WebMediaPlayerManagerAndroid::UnregisterMediaPlayer(int player_id) {
  std::map<int32, MediaPlayerInfo>::iterator iter =
      media_players_.find(player_id);
  DCHECK(iter != media_players_.end());

  media_players_.erase(player_id);
}

void WebMediaPlayerManagerAndroid::ReleaseMediaResources() {
  std::map<int32, MediaPlayerInfo>::iterator player_it;
  for (player_it = media_players_.begin();
      player_it != media_players_.end(); ++player_it) {
    (player_it->second).player->ReleaseMediaResources();
  }
}

WebMediaPlayerAndroid* WebMediaPlayerManagerAndroid::GetMediaPlayer(
    int player_id) {
  std::map<int32, MediaPlayerInfo>::iterator iter =
      media_players_.find(player_id);
  if (iter != media_players_.end())
    return (iter->second).player;
  return NULL;
}

}  // namespace webkit_media
