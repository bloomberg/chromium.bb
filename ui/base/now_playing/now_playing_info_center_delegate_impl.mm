// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/now_playing/now_playing_info_center_delegate_impl.h"

#include "ui/base/now_playing/now_playing_info_center_delegate_cocoa.h"

namespace now_playing {

// static
NowPlayingInfoCenterDelegateImpl* NowPlayingInfoCenterDelegateImpl::instance_ =
    nullptr;

// static
std::unique_ptr<NowPlayingInfoCenterDelegate>
NowPlayingInfoCenterDelegate::Create() {
  if (@available(macOS 10.12.2, *))
    return std::make_unique<NowPlayingInfoCenterDelegateImpl>();
  return nullptr;
}

NowPlayingInfoCenterDelegateImpl::NowPlayingInfoCenterDelegateImpl() {
  DCHECK_EQ(nullptr, instance_);
  instance_ = this;
  now_playing_info_center_delegate_cocoa_.reset(
      [[NowPlayingInfoCenterDelegateCocoa alloc] init]);
}

NowPlayingInfoCenterDelegateImpl::~NowPlayingInfoCenterDelegateImpl() {
  DCHECK_EQ(this, instance_);
  instance_ = nullptr;

  [now_playing_info_center_delegate_cocoa_ resetNowPlayingInfo];
}

void NowPlayingInfoCenterDelegateImpl::SetPlaybackState(
    PlaybackState playback_state) {
  MPNowPlayingPlaybackState state =
      PlaybackStateToMPNowPlayingPlaybackState(playback_state);
  [now_playing_info_center_delegate_cocoa_ setPlaybackState:state];
}

MPNowPlayingPlaybackState
NowPlayingInfoCenterDelegateImpl::PlaybackStateToMPNowPlayingPlaybackState(
    PlaybackState playback_state) {
  switch (playback_state) {
    case PlaybackState::kPlaying:
      return MPNowPlayingPlaybackStatePlaying;
    case PlaybackState::kPaused:
      return MPNowPlayingPlaybackStatePaused;
    case PlaybackState::kStopped:
      return MPNowPlayingPlaybackStateStopped;
    default:
      NOTREACHED();
  }
  return MPNowPlayingPlaybackStateUnknown;
}

}  // namespace now_playing
