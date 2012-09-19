// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_ANDROID_MEDIA_PLAYER_BRIDGE_MANAGER_IMPL_H_
#define WEBKIT_MEDIA_ANDROID_MEDIA_PLAYER_BRIDGE_MANAGER_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "media/base/android/media_player_bridge_manager.h"

namespace webkit_media {

// Class for managing active MediaPlayerBridge objects if they are created
// in render process.
class MediaPlayerBridgeManagerImpl : public media::MediaPlayerBridgeManager {
 public:
  // The manager will start to reclaim unused resources when the number of
  // active MediaPlayerBridge objects reaches |threshold|.
  explicit MediaPlayerBridgeManagerImpl(int threshold);
  virtual ~MediaPlayerBridgeManagerImpl();

  // media::MediaPlayerBridgeManager implementations.
  virtual void RequestMediaResources(media::MediaPlayerBridge* player) OVERRIDE;
  virtual void ReleaseMediaResources(media::MediaPlayerBridge* player) OVERRIDE;

 private:
  // An array of active MediaPlayerBridge objects.
  std::vector<media::MediaPlayerBridge*> players_;

  // Threshold before we start to reclaim unused media resources.
  int active_player_threshold_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlayerBridgeManagerImpl);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_ANDROID_MEDIA_PLAYER_BRIDGE_MANAGER_IMPL_H_
