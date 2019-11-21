// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.testing;

import com.google.android.libraries.feed.common.functional.Consumer;

/** Test helper class that ensures that the asynchronous consumer is called. */
public class RequiredConsumer<T> implements Consumer<T> {

  private final Consumer<T> consumer;
  private boolean isCalled = false;

  public RequiredConsumer(Consumer<T> consumer) {
    this.consumer = consumer;
  }

  @Override
  public void accept(T input) {
    isCalled = true;
    consumer.accept(input);
  }

  public boolean isCalled() {
    return isCalled;
  }
}
