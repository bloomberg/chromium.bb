// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/android/media_player_bridge_manager_impl.h"

#include "media/base/android/media_player_bridge.h"

namespace webkit_media {

MediaPlayerBridgeManagerImpl::MediaPlayerBridgeManagerImpl(
    int threshold)
    : active_player_threshold_(threshold) {
}

MediaPlayerBridgeManagerImpl::~MediaPlayerBridgeManagerImpl() {}

void MediaPlayerBridgeManagerImpl::RequestMediaResources(
    media::MediaPlayerBridge* player) {
  if (!player)
    return;

  int num_active_player = 0;
  std::vector<media::MediaPlayerBridge*>::iterator it;
  for (it = players_.begin(); it != players_.end(); ++it) {
    // The player is already active, ignore it.
    if ((*it) == player)
      return;

    num_active_player++;
  }

  if (num_active_player < active_player_threshold_) {
    players_.push_back(player);
    return;
  }

  // Get a list of the players to free.
  std::vector<media::MediaPlayerBridge*> players_to_release;
  for (it = players_.begin(); it != players_.end(); ++it) {
    if ((*it)->prepared() && !(*it)->IsPlaying())
      players_to_release.push_back(*it);
  }

  // Calling release() will result in a call to ReleaseMediaResources(), so
  // the player will be removed from |players_|.
  for (it = players_to_release.begin(); it != players_to_release.end(); ++it)
    (*it)->Release();

  players_.push_back(player);
}

void MediaPlayerBridgeManagerImpl::ReleaseMediaResources(
    media::MediaPlayerBridge* player) {
  for (std::vector<media::MediaPlayerBridge*>::iterator it = players_.begin();
      it != players_.end(); ++it) {
    if ((*it) == player) {
      players_.erase(it);
      return;
    }
  }
}

}  // namespace webkit_media
