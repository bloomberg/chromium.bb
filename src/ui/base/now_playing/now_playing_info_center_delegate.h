// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_NOW_PLAYING_NOW_PLAYING_INFO_CENTER_DELEGATE_H_
#define UI_BASE_NOW_PLAYING_NOW_PLAYING_INFO_CENTER_DELEGATE_H_

#include <memory>

#include "base/component_export.h"

namespace now_playing {

class COMPONENT_EXPORT(NOW_PLAYING) NowPlayingInfoCenterDelegate {
 public:
  enum class PlaybackState {
    kPlaying,
    kPaused,
    kStopped,
  };

  virtual ~NowPlayingInfoCenterDelegate();

  // Creates and returns an instance of NowPlayingInfoCenterDelegate. Returns
  // null if the current version of Mac OS does not support
  // MPNowPlayingInfoCenter.
  static std::unique_ptr<NowPlayingInfoCenterDelegate> Create();

  // Used for setting metadata on the MPNowPlayingInfoCenter.
  // TODO(https://crbug.com/936513): Support all metadata attributes.
  virtual void SetPlaybackState(PlaybackState state) = 0;
};

}  // namespace now_playing

#endif  // UI_BASE_NOW_PLAYING_NOW_PLAYING_INFO_CENTER_DELEGATE_H_
