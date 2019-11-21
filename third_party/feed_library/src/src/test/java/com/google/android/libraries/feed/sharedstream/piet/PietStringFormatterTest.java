// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.piet;

import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.common.time.testing.FakeClock;
import java.util.concurrent.TimeUnit;
import org.junit.Test;

/** Tests the {@link PietStringFormatter} */
public class PietStringFormatterTest {

  @Test
  public void testGetRelativeElapsedString() {
    FakeClock clock = new FakeClock();
    clock.set(TimeUnit.MINUTES.toMillis(1));

    PietStringFormatter pietStringFormatter = new PietStringFormatter(clock);
    assertThat(pietStringFormatter.getRelativeElapsedString(TimeUnit.MINUTES.toMillis(1)))
        .isEqualTo("1 minute ago");
  }
}
