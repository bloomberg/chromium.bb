// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/webmediaplayer_util.h"

#include <math.h>

namespace webkit_media {

base::TimeDelta ConvertSecondsToTimestamp(double seconds) {
  double microseconds = seconds * base::Time::kMicrosecondsPerSecond;
  return base::TimeDelta::FromMicroseconds(
      microseconds > 0 ? microseconds + 0.5 : ceil(microseconds - 0.5));
}

WebKit::WebTimeRanges ConvertToWebTimeRanges(
    const media::Ranges<base::TimeDelta>& ranges) {
  WebKit::WebTimeRanges result(ranges.size());
  for (size_t i = 0; i < ranges.size(); i++) {
    result[i].start = ranges.start(i).InSecondsF();
    result[i].end = ranges.end(i).InSecondsF();
  }
  return result;
}

}  // namespace webkit_media
