// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

/**
 * Utility class to provide engagement time helper methods.
 */
public class EngagementTimeUtil {
    /**
     * Provide the current time in milliseconds.
     *
     * @return long - the current time in milliseconds.
     */
    public long currentTime() {
        return System.currentTimeMillis();
    }

    /**
     * Given the last engagement timestamp, return the elapsed time in milliseconds since that time.
     * @param lastEngagementMs - time of the last engagement
     * @return time in milliseconds that have elapsed since lastEngagementMs
     */
    public long timeSinceLastEngagement(long lastEngagementMs) {
        return currentTime() - lastEngagementMs;
    }
}
