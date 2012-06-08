// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/android/webmediaplayer_manager_android.h"

#include "webkit/media/android/webmediaplayer_android.h"

// Threshold on the number of media players per renderer before we start
// attempting to release inactive media players.
static const int kMediaPlayerThreshold = 2;

namespace webkit_media {

WebMediaPlayerManagerAndroid::WebMediaPlayerManagerAndroid()
    : next_media_player_id_(0) {
}

WebMediaPlayerManagerAndroid::~WebMediaPlayerManagerAndroid() {
  ReleaseMediaResources();
}

int WebMediaPlayerManagerAndroid::RegisterMediaPlayer(
    WebMediaPlayerAndroid* player) {
  media_players_[next_media_player_id_] = player;
  return next_media_player_id_++;
}

void WebMediaPlayerManagerAndroid::UnregisterMediaPlayer(int player_id) {
  std::map<int32, WebMediaPlayerAndroid*>::iterator iter =
      media_players_.find(player_id);
  DCHECK(iter != media_players_.end());

  media_players_.erase(player_id);
}

void WebMediaPlayerManagerAndroid::RequestMediaResources(int player_id) {
  std::map<int32, WebMediaPlayerAndroid*>::iterator iter =
      media_players_.find(player_id);
  DCHECK(iter != media_players_.end());

  if ((iter->second)->IsInitialized())
    return;

  // Release active players that are paused. Because we only release paused
  // players, the number of running players could go beyond the limit.
  // TODO(qinmin): we should use LRU to release the oldest player if we are
  // reaching hardware limit.
  if (GetActivePlayerCount() < kMediaPlayerThreshold)
    return;

  std::map<int32, WebMediaPlayerAndroid*>::iterator player_it;
  for (player_it = media_players_.begin();
       player_it != media_players_.end(); ++player_it) {
    WebMediaPlayerAndroid* player = player_it->second;
    if (player->IsInitialized() && player->paused())
      player->ReleaseMediaResources();
  }
}

void WebMediaPlayerManagerAndroid::ReleaseMediaResources() {
  std::map<int32, WebMediaPlayerAndroid*>::iterator player_it;
  for (player_it = media_players_.begin();
      player_it != media_players_.end(); ++player_it) {
    (player_it->second)->ReleaseMediaResources();
  }
}

int32 WebMediaPlayerManagerAndroid::GetActivePlayerCount() {
  int32 count = 0;
  std::map<int32, WebMediaPlayerAndroid*>::iterator iter;
  for (iter = media_players_.begin(); iter != media_players_.end(); ++iter) {
    if ((iter->second)->IsInitialized())
      count++;
  }
  return count;
}

WebMediaPlayerAndroid* WebMediaPlayerManagerAndroid::GetMediaPlayer(
    int player_id) {
  std::map<int32, WebMediaPlayerAndroid*>::iterator iter =
      media_players_.find(player_id);
  if (iter != media_players_.end())
    return iter->second;
  return NULL;
}

}  // namespace webkit_media
