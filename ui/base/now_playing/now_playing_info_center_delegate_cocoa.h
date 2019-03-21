// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_NOW_PLAYING_NOW_PLAYING_INFO_CENTER_DELEGATE_COCOA_H_
#define UI_BASE_NOW_PLAYING_NOW_PLAYING_INFO_CENTER_DELEGATE_COCOA_H_

#import <Cocoa/Cocoa.h>
#import <MediaPlayer/MediaPlayer.h>

API_AVAILABLE(macos(10.12.2))
@interface NowPlayingInfoCenterDelegateCocoa : NSObject

- (instancetype)init;

// Clears all "Now Playing" information.
- (void)resetNowPlayingInfo;

// Called by the NowPlayingInfoCenterDelegateImpl to set metadata.
- (void)setPlaybackState:(MPNowPlayingPlaybackState)state;

@end

#endif  // UI_BASE_NOW_PLAYING_NOW_PLAYING_INFO_CENTER_DELEGATE_COCOA_H_
