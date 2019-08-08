// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.base.annotations.CalledByNative;

/**
 * Android wrapper of the native history::DeletionInfo class. Any class that uses this needs to
 * register a {@link HistoryDeletionBridge.Observer} on {@Link HistoryDeletionBridge} to listen for
 * the native signals that produce this signal.
 */
public class HistoryDeletionInfo {
    private final long mHistoryDeletionInfoPtr;

    @CalledByNative
    private static HistoryDeletionInfo create(long historyDeletionInfoPtr) {
        return new HistoryDeletionInfo(historyDeletionInfoPtr);
    }

    HistoryDeletionInfo(long historyDeletionInfoPtr) {
        mHistoryDeletionInfoPtr = historyDeletionInfoPtr;
    }

    /**
     * @return An array of URLs that were deleted.
     */
    public String[] getDeletedURLs() {
        return nativeGetDeletedURLs(mHistoryDeletionInfoPtr);
    }

    /**
     * @return True if the time range is valid.
     */
    public boolean isTimeRangeValid() {
        return nativeIsTimeRangeValid(mHistoryDeletionInfoPtr);
    }

    /**
     * @return True if the time range is for all time.
     */
    public boolean isTimeRangeForAllTime() {
        return nativeIsTimeRangeForAllTime(mHistoryDeletionInfoPtr);
    }

    /**
     * @return The beginning of the time range if the time range is valid.
     */
    public long getTimeRangeBegin() {
        return nativeGetTimeRangeBegin(mHistoryDeletionInfoPtr);
    }

    /**
     * @return The end of the time range if the time range is valid.
     */
    public long getTimeRangeEnd() {
        return nativeGetTimeRangeBegin(mHistoryDeletionInfoPtr);
    }

    private static native String[] nativeGetDeletedURLs(long historyDeletionInfoPtr);
    private static native boolean nativeIsTimeRangeValid(long historyDeletionInfoPtr);
    private static native boolean nativeIsTimeRangeForAllTime(long historyDeletionInfoPtr);
    private static native long nativeGetTimeRangeBegin(long historyDeletionInfoPtr);
    private static native long nativeGetTimeRangeEnd(long historyDeletionInfoPtr);
}
