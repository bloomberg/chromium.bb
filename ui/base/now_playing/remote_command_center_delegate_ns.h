// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_NOW_PLAYING_REMOTE_COMMAND_CENTER_DELEGATE_NS_H_
#define UI_BASE_NOW_PLAYING_REMOTE_COMMAND_CENTER_DELEGATE_NS_H_

#import <Cocoa/Cocoa.h>
#import <MediaPlayer/MediaPlayer.h>

namespace now_playing {
class RemoteCommandCenterDelegateImpl;
}  // namespace now_playing

API_AVAILABLE(macos(10.12.2))
@interface RemoteCommandCenterDelegateNS : NSObject {
 @private
  now_playing::RemoteCommandCenterDelegateImpl* delegate_;
}

- (instancetype)initWithDelegate:
    (now_playing::RemoteCommandCenterDelegateImpl*)delegate;

// Called by the OS via the MPRemoteCommandCenter.
- (MPRemoteCommandHandlerStatus)onCommand:(MPRemoteCommandEvent*)event;

// Called by the RemoteCommandCenterDelegateImpl to enable/disable different
// commands.
- (void)setCanPlay:(bool)can_play;
- (void)setCanPause:(bool)can_pause;
- (void)setCanStop:(bool)can_stop;
- (void)setCanPlayPause:(bool)can_playpause;
- (void)setCanGoNextTrack:(bool)can_go_next_track;
- (void)setCanGoPreviousTrack:(bool)can_go_prev_track;

@end

#endif  // UI_BASE_NOW_PLAYING_REMOTE_COMMAND_CENTER_DELEGATE_NS_H_
