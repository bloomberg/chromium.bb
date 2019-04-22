// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/media_controls_shared_helper.h"

#include <cmath>
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/time_ranges.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace {

const double kCurrentTimeBufferedDelta = 1.0;

}

namespace blink {

base::Optional<unsigned>
MediaControlsSharedHelpers::GetCurrentBufferedTimeRange(
    HTMLMediaElement& media_element) {
  double current_time = media_element.currentTime();
  double duration = media_element.duration();
  TimeRanges* buffered_time_ranges = media_element.buffered();

  DCHECK(buffered_time_ranges);

  if (std::isnan(duration) || std::isinf(duration) || !duration ||
      std::isnan(current_time)) {
    return base::nullopt;
  }

  // Calculate the size of the after segment (i.e. what has been buffered).
  for (unsigned i = 0; i < buffered_time_ranges->length(); ++i) {
    float start = buffered_time_ranges->start(i, ASSERT_NO_EXCEPTION);
    float end = buffered_time_ranges->end(i, ASSERT_NO_EXCEPTION);
    // The delta is there to avoid corner cases when buffered
    // ranges is out of sync with current time because of
    // asynchronous media pipeline and current time caching in
    // HTMLMediaElement.
    // This is related to https://www.w3.org/Bugs/Public/show_bug.cgi?id=28125
    // FIXME: Remove this workaround when WebMediaPlayer
    // has an asynchronous pause interface.
    if (!std::isnan(start) && !std::isnan(end) &&
        start <= current_time + kCurrentTimeBufferedDelta &&
        end > current_time) {
      return i;
    }
  }

  return base::nullopt;
}

String MediaControlsSharedHelpers::FormatTime(double time) {
  if (!std::isfinite(time))
    time = 0;

  int seconds = static_cast<int>(fabs(time));
  int minutes = seconds / 60;
  int hours = minutes / 60;

  seconds %= 60;
  minutes %= 60;

  const char* negative_sign = (time < 0 ? "-" : "");

  // [0-10) minutes duration is m:ss
  // [10-60) minutes duration is mm:ss
  // [1-10) hours duration is h:mm:ss
  // [10-100) hours duration is hh:mm:ss
  // [100-1000) hours duration is hhh:mm:ss
  // etc.

  if (hours > 0) {
    return String::Format("%s%d:%02d:%02d", negative_sign, hours, minutes,
                          seconds);
  }

  return String::Format("%s%d:%02d", negative_sign, minutes, seconds);
}

}  // namespace blink
