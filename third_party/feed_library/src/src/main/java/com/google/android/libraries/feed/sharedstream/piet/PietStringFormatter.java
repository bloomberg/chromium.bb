// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.piet;

import android.text.format.DateUtils;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.android.libraries.feed.piet.host.StringFormatter;
import javax.inject.Inject;

/** Formats strings for Piet. */
public class PietStringFormatter implements StringFormatter {

  private final Clock clock;

  @Inject
  public PietStringFormatter(Clock clock) {
    this.clock = clock;
  }

  @Override
  public String getRelativeElapsedString(long elapsedTimeMillis) {
    return DateUtils.getRelativeTimeSpanString(
            clock.currentTimeMillis() - elapsedTimeMillis,
            clock.currentTimeMillis(),
            DateUtils.MINUTE_IN_MILLIS)
        .toString();
  }
}
