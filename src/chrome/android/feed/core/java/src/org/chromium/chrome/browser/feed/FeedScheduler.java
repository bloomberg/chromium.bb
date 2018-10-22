// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.host.scheduler.SchedulerApi;

/**
 * An extension of the {@link SchedulerApi} with additional methods needed for scheduling logic
 * in Chromium.
 */
public interface FeedScheduler extends SchedulerApi {
    void destroy();

    void onForegrounded();

    void onFixedTimer(Runnable onCompletion);

    void onTaskReschedule();

    void onSuggestionConsumed();
}
