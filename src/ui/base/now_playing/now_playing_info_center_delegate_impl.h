// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_NOW_PLAYING_NOW_PLAYING_INFO_CENTER_DELEGATE_IMPL_H_
#define UI_BASE_NOW_PLAYING_NOW_PLAYING_INFO_CENTER_DELEGATE_IMPL_H_

#import <MediaPlayer/MediaPlayer.h>

#include "base/component_export.h"
#include "base/mac/scoped_nsobject.h"
#include "ui/base/now_playing/now_playing_info_center_delegate.h"

@class NowPlayingInfoCenterDelegateCocoa;

namespace now_playing {

// Implementation of NowPlayingInfoCenterDelegate that wraps an NSObject which
// interfaces with the MPNowPlayingInfoCenter.
class COMPONENT_EXPORT(NOW_PLAYING)
    API_AVAILABLE(macos(10.12.2)) NowPlayingInfoCenterDelegateImpl
    : public NowPlayingInfoCenterDelegate {
 public:
  NowPlayingInfoCenterDelegateImpl();
  ~NowPlayingInfoCenterDelegateImpl() override;

  // NowPlayingInfoCenterDelegate implementation.
  void SetPlaybackState(PlaybackState state) override;

 private:
  static NowPlayingInfoCenterDelegateImpl* instance_;

  MPNowPlayingPlaybackState PlaybackStateToMPNowPlayingPlaybackState(
      PlaybackState playback_state);

  base::scoped_nsobject<NowPlayingInfoCenterDelegateCocoa>
      now_playing_info_center_delegate_cocoa_;

  DISALLOW_COPY_AND_ASSIGN(NowPlayingInfoCenterDelegateImpl);
};

}  // namespace now_playing

#endif  // UI_BASE_NOW_PLAYING_NOW_PLAYING_INFO_CENTER_DELEGATE_IMPL_H_
