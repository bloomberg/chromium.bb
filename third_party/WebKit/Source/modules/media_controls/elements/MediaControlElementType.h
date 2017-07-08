// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlElementType_h
#define MediaControlElementType_h

// Classes sub-classing MediaControlElementBase will all have a defined type
// from this list. It is used by code that need to know what type of media
// control element it is interacting with.
enum MediaControlElementType {
  kMediaEnterFullscreenButton = 0,
  kMediaMuteButton,
  kMediaPlayButton,
  kMediaSlider,
  kMediaSliderThumb,
  kMediaShowClosedCaptionsButton,
  kMediaHideClosedCaptionsButton,
  kMediaTextTrackList,
  kMediaUnMuteButton,
  kMediaPauseButton,
  kMediaTimelineContainer,
  kMediaCurrentTimeDisplay,
  kMediaTimeRemainingDisplay,
  kMediaTrackSelectionCheckmark,
  kMediaControlsPanel,
  kMediaVolumeSliderContainer,
  kMediaVolumeSlider,
  kMediaVolumeSliderThumb,
  kMediaExitFullscreenButton,
  kMediaOverlayPlayButton,
  kMediaCastOffButton,
  kMediaCastOnButton,
  kMediaOverlayCastOffButton,
  kMediaOverlayCastOnButton,
  kMediaOverflowButton,
  kMediaOverflowList,
  kMediaDownloadButton,
};

#endif  // MediaControlElementType_h
