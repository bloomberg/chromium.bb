// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/android/media_metadata_android.h"

namespace webkit_media {

MediaMetadataAndroid::MediaMetadataAndroid()
    : width(0),
      height(0),
      paused(false),
      can_pause(false),
      can_seek_forward(false),
      can_seek_backward(false) {
}

MediaMetadataAndroid::MediaMetadataAndroid(int w,
                                           int h,
                                           base::TimeDelta d,
                                           base::TimeDelta ct,
                                           bool p,
                                           bool cp,
                                           bool csf,
                                           bool csb)
    : width(w),
      height(h),
      duration(d),
      current_time(ct),
      paused(p),
      can_pause(cp),
      can_seek_forward(csf),
      can_seek_backward(csb) {
}

MediaMetadataAndroid::~MediaMetadataAndroid() {
}

}  // namespace webkit_media
