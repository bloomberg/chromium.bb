// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.Context;

import org.chromium.chrome.browser.omaha.OmahaBase.PostResult;

/** Delegates calls out from {@link OmahaBase}. */
public abstract class OmahaDelegate {
    private RequestGenerator mRequestGenerator;

    OmahaDelegate() {}

    /** @return Context that is used to interact with the system. */
    abstract Context getContext();

    /** @return Whether Chrome is installed as part of the system image. */
    abstract boolean isInSystemImage();

    /** @return The scheduler used to trigger jobs. */
    abstract ExponentialBackoffScheduler getScheduler();

    /** @return The {@link RequestGenerator} used to create Omaha XML. */
    final RequestGenerator getRequestGenerator() {
        if (mRequestGenerator == null) mRequestGenerator = createRequestGenerator(getContext());
        return mRequestGenerator;
    }

    /** @return A UUID that can be used to identify particular requests. */
    abstract String generateUUID();

    /** Determine whether or not Chrome is currently being used actively. */
    abstract boolean isChromeBeingUsed();

    /**
     * Schedules the Omaha client to run again.
     * @param currentTimestampMs Current time.
     * @param nextTimestampMs    When the service should be run again.
     */
    abstract void scheduleService(long currentTimestampMs, long nextTimestampMs);

    /** Creates a {@link RequestGenerator}. */
    abstract RequestGenerator createRequestGenerator(Context context);

    /**
     * Called when {@link OmahaBase#registerNewRequest} finishes.
     * @param timestampRequestMs When the next active user request should be generated.
     * @param timestampPostMs    Earliest time the next POST should be allowed.
     */
    void onRegisterNewRequestDone(long timestampRequestMs, long timestampPostMs) {}

    /**
     * Called when {@link OmahaBase#handlePostRequest} finishes.
     * @param result              See {@link PostResult}.
     * @param installEventWasSent Whether or not an install event was sent.
     */
    void onHandlePostRequestDone(@PostResult int result, boolean installEventWasSent) {}

    /**
     * Called when {@link OmahaBase#generateAndPostRequest} finishes.
     * @param succeeded Whether or not the post was successfully received by the server.
     */
    void onGenerateAndPostRequestDone(boolean succeeded) {}

    /**
     * Called when {@link OmahaBase#saveState} finishes.
     * @param timestampRequestMs When the next active user request should be generated.
     * @param timestampPostMs    Earliest time the next POST should be allowed.
     */
    void onSaveStateDone(long timestampRequestMs, long timestampPostMs) {}
}
