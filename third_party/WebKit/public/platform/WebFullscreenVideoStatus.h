// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFullscreenVideoStatus_h
#define WebFullscreenVideoStatus_h

namespace blink {

enum class WebFullscreenVideoStatus {
  // Video is not effectively fullscreen.
  kNotEffectivelyFullscreen = 0,
  // Video is fullscreen and allowed to enter Picture-in-Picture.
  kFullscreenAndPictureInPictureEnabled,
  // Video is fullscreen and is not allowed to enter Picture-in-Picture.
  kFullscreenAndPictureInPictureDisabled,
  // The maximum number of fullscreen status.
  kMax = kFullscreenAndPictureInPictureDisabled,
};

}  // namespace blink

#endif  // WebFullscreenVideoStatus_h
