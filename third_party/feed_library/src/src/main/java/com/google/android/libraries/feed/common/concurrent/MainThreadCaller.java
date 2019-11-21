// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.common.concurrent;

import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.logging.Logger;

import java.util.concurrent.ExecutionException;

/** Abstract class which support calling Host methods on the {@link MainThreadRunner}. */
public abstract class MainThreadCaller {
    private static final String TAG = "MainThreadCaller";
    private final MainThreadRunner mainThreadRunner;

    protected MainThreadCaller(MainThreadRunner mainThreadRunner) {
        this.mainThreadRunner = mainThreadRunner;
    }

    /** Execute a task with a {@link Consumer}. */
    protected interface ConsumerTask<T> { void execute(Consumer<T> consumer); }

    /**
     * Run a {@link ConsumerTask} on the main thread, providing a Consumer to receive the results.
     */
    protected <T> T mainThreadCaller(String location, ConsumerTask<T> task, T failure) {
        SimpleSettableFuture<T> sharedStatesFuture = new SimpleSettableFuture<>();
        mainThreadRunner.execute(TAG + " " + location, () -> task.execute(sharedStatesFuture::put));
        try {
            return sharedStatesFuture.get();
        } catch (InterruptedException | ExecutionException e) {
            Logger.e(TAG, e, null);
            return failure;
        }
    }
}
