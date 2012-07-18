// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_ANDROID_MEDIA_METADATA_ANDROID_H_
#define WEBKIT_MEDIA_ANDROID_MEDIA_METADATA_ANDROID_H_

#include "base/basictypes.h"
#include "base/time.h"

namespace webkit_media {

// Provides the initial media metadata to ContentVideoView.
struct MediaMetadataAndroid {
  MediaMetadataAndroid();
  MediaMetadataAndroid(int w,
                       int h,
                       base::TimeDelta d,
                       base::TimeDelta ct,
                       bool p,
                       bool cp,
                       bool csf,
                       bool csb);
  ~MediaMetadataAndroid();

  int width;
  int height;
  base::TimeDelta duration;
  base::TimeDelta current_time;
  bool paused;
  bool can_pause;
  bool can_seek_forward;
  bool can_seek_backward;
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_ANDROID_MEDIA_METADATA_ANDROID_H_
