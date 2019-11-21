// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.testing;

import static com.google.common.truth.Truth.assertThat;

import java.util.concurrent.CountDownLatch;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Test class for {@link RequiredConsumer} */
@RunWith(JUnit4.class)
public class RequiredConsumerTest {

  @Test
  public void testConsumer() throws Exception {
    CountDownLatch latch = new CountDownLatch(1);
    Boolean callValue = Boolean.TRUE;

    RequiredConsumer<Boolean> consumer =
        new RequiredConsumer<>(
            input -> {
              assertThat(input).isEqualTo(callValue);
              latch.countDown();
            });

    consumer.accept(callValue);

    assertThat(latch.getCount()).isEqualTo(0);
    assertThat(consumer.isCalled()).isTrue();
  }
}
