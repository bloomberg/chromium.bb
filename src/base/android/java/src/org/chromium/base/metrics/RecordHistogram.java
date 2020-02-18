// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

/** Java API for recording UMA histograms. */
@JNINamespace("base::android")
@MainDex
public class RecordHistogram {
    /** Underlying {@link UmaRecorder} used by methods in this class. */
    private static UmaRecorder sRecorder = new NativeUmaRecorder();

    /**
     * Null, unless recording of histograms is currently disabled for testing. Exposed for use in
     * peer classes {e.g. AnimationFrameTimeHistogram}.
     * <p>
     * Use {@link #setDisabledForTests(boolean)} to set this value.
     */
    @VisibleForTesting
    public static Throwable sDisabledBy;

    /**
     * Tests may not have native initialized, so they may need to disable metrics. The value should
     * be reset after the test done, to avoid carrying over state to unrelated tests.
     * <p>
     * In JUnit tests this can be done automatically using
     * {@link org.chromium.chrome.browser.DisableHistogramsRule}.
     */
    @VisibleForTesting
    public static void setDisabledForTests(boolean disabled) {
        if (disabled) {
            if (sDisabledBy != null) {
                throw new IllegalStateException("Histograms are already disabled.", sDisabledBy);
            }
            sDisabledBy = new Throwable();
            sRecorder = new NoopUmaRecorder();
        } else {
            sDisabledBy = null;
            sRecorder = new NativeUmaRecorder();
        }
    }

    /**
     * Records a sample in a boolean UMA histogram of the given name. Boolean histogram has two
     * buckets, corresponding to success (true) and failure (false). This is the Java equivalent of
     * the UMA_HISTOGRAM_BOOLEAN C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, either true or false
     */
    public static void recordBooleanHistogram(String name, boolean sample) {
        sRecorder.recordBooleanHistogram(name, sample);
    }

    /**
     * Records a sample in an enumerated histogram of the given name and boundary. Note that
     * {@code max} identifies the histogram - it should be the same at every invocation. This is the
     * Java equivalent of the UMA_HISTOGRAM_ENUMERATION C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 0 and at most {@code max-1}
     * @param max upper bound for legal sample values - all sample values have to be strictly
     *            lower than {@code max}
     */
    public static void recordEnumeratedHistogram(String name, int sample, int max) {
        recordExactLinearHistogram(name, sample, max);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_COUNTS_1M C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 1 and at most 999999
     */
    public static void recordCountHistogram(String name, int sample) {
        sRecorder.recordExponentialHistogram(name, sample, 1, 1_000_000, 50);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_COUNTS_100 C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 1 and at most 99
     */
    public static void recordCount100Histogram(String name, int sample) {
        sRecorder.recordExponentialHistogram(name, sample, 1, 100, 50);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_COUNTS_1000 C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 1 and at most 999
     */
    public static void recordCount1000Histogram(String name, int sample) {
        sRecorder.recordExponentialHistogram(name, sample, 1, 1_000, 50);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_COUNTS_100000 C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 1 and at most 99999
     */
    public static void recordCount100000Histogram(String name, int sample) {
        sRecorder.recordExponentialHistogram(name, sample, 1, 100_000, 50);
    }

    /**
     * Records a sample in a count histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_CUSTOM_COUNTS C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, expected to fall in range {@code [min, max)}
     * @param min the smallest expected sample value; at least 1
     * @param max the smallest sample value that will be recorded in overflow bucket
     * @param numBuckets the number of buckets including underflow ({@code [0, min)}) and overflow
     *                   ({@code [max, inf)}) buckets; at most 100
     */
    public static void recordCustomCountHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        sRecorder.recordExponentialHistogram(name, sample, min, max, numBuckets);
    }

    /**
     * Records a sample in a linear histogram. This is the Java equivalent for using
     * {@code base::LinearHistogram}.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, expected to fall in range {@code [min, max)}
     * @param min the smallest expected sample value; at least 1
     * @param max the smallest sample value that will be recorded in overflow bucket
     * @param numBuckets the number of buckets including underflow ({@code [0, min)}) and overflow
     *                   ({@code [max, inf)}) buckets; at most 100
     */
    public static void recordLinearCountHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        sRecorder.recordLinearHistogram(name, sample, min, max, numBuckets);
    }

    /**
     * Records a sample in a percentage histogram. This is the Java equivalent of the
     * UMA_HISTOGRAM_PERCENTAGE C++ macro.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, at least 0 and at most 100
     */
    public static void recordPercentageHistogram(String name, int sample) {
        recordExactLinearHistogram(name, sample, 101);
    }

    /**
     * Records a sparse histogram. This is the Java equivalent of {@code base::UmaHistogramSparse}.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded: All values of {@code sample} are valid, including
     *               negative values. Keep the number of distinct values across all users
     *               {@code <= 100} ideally, definitely {@code <= 1000}.
     */
    public static void recordSparseHistogram(String name, int sample) {
        sRecorder.recordSparseHistogram(name, sample);
    }

    /**
     * Records a sample in a histogram of times. Useful for recording short durations. This is the
     * Java equivalent of the UMA_HISTOGRAM_TIMES C++ macro.
     * <p>
     * Note that histogram samples will always be converted to milliseconds when logged.
     *
     * @param name name of the histogram
     * @param durationMs duration to be recorded in milliseconds
     */
    public static void recordTimesHistogram(String name, long durationMs) {
        recordCustomTimesHistogramMilliseconds(
                name, durationMs, 1, DateUtils.SECOND_IN_MILLIS * 10, 50);
    }

    /**
     * Records a sample in a histogram of times. Useful for recording medium durations. This is the
     * Java equivalent of the UMA_HISTOGRAM_MEDIUM_TIMES C++ macro.
     * <p>
     * Note that histogram samples will always be converted to milliseconds when logged.
     *
     * @param name name of the histogram
     * @param durationMs duration to be recorded in milliseconds
     */
    public static void recordMediumTimesHistogram(String name, long durationMs) {
        recordCustomTimesHistogramMilliseconds(
                name, durationMs, 10, DateUtils.MINUTE_IN_MILLIS * 3, 50);
    }

    /**
     * Records a sample in a histogram of times. Useful for recording long durations. This is the
     * Java equivalent of the UMA_HISTOGRAM_LONG_TIMES C++ macro.
     * <p>
     * Note that histogram samples will always be converted to milliseconds when logged.
     *
     * @param name name of the histogram
     * @param durationMs duration to be recorded in milliseconds
     */
    public static void recordLongTimesHistogram(String name, long durationMs) {
        recordCustomTimesHistogramMilliseconds(name, durationMs, 1, DateUtils.HOUR_IN_MILLIS, 50);
    }

    /**
     * Records a sample in a histogram of times. Useful for recording long durations. This is the
     * Java equivalent of the UMA_HISTOGRAM_LONG_TIMES_100 C++ macro.
     * <p>
     * Note that histogram samples will always be converted to milliseconds when logged.
     *
     * @param name name of the histogram
     * @param durationMs duration to be recorded in milliseconds
     */
    public static void recordLongTimesHistogram100(String name, long durationMs) {
        recordCustomTimesHistogramMilliseconds(name, durationMs, 1, DateUtils.HOUR_IN_MILLIS, 100);
    }

    /**
     * Records a sample in a histogram of times with custom buckets. This is the Java equivalent of
     * the UMA_HISTOGRAM_CUSTOM_TIMES C++ macro.
     * <p>
     * Note that histogram samples will always be converted to milliseconds when logged.
     *
     * @param name name of the histogram
     * @param durationMs duration to be recorded in milliseconds; expected to fall in range
     *                   {@code [min, max)}
     * @param min the smallest expected sample value; at least 1
     * @param max the smallest sample value that will be recorded in overflow bucket
     * @param numBuckets the number of buckets including underflow ({@code [0, min)}) and overflow
     *                   ({@code [max, inf)}) buckets; at most 100
     */
    public static void recordCustomTimesHistogram(
            String name, long durationMs, long min, long max, int numBuckets) {
        recordCustomTimesHistogramMilliseconds(name, durationMs, min, max, numBuckets);
    }

    /**
     * Records a sample in a histogram of sizes in KB. This is the Java equivalent of the
     * UMA_HISTOGRAM_MEMORY_KB C++ macro.
     * <p>
     * Good for sizes up to about 500MB.
     *
     * @param name name of the histogram
     * @param sizeInkB Sample to record in KB
     */
    public static void recordMemoryKBHistogram(String name, int sizeInKB) {
        sRecorder.recordExponentialHistogram(name, sizeInKB, 1000, 500000, 50);
    }

    /**
     * Records a sample in a linear histogram where each bucket in range {@code [0, max)} counts
     * exactly a single value.
     *
     * @param name name of the histogram
     * @param sample sample to be recorded, expected to fall in range {@code [0, max)}
     * @param max the smallest value counted in the overflow bucket, shouldn't be larger than 100
     */
    private static void recordExactLinearHistogram(String name, int sample, int max) {
        // Range [0, 1) is counted in the underflow bucket. The first "real" bucket starts at 1.
        final int min = 1;
        // One extra is added for the overflow bucket.
        final int numBuckets = max + 1;
        sRecorder.recordLinearHistogram(name, sample, min, max, numBuckets);
    }

    private static int clampToInt(long value) {
        if (value > Integer.MAX_VALUE) return Integer.MAX_VALUE;
        // Note: Clamping to MIN_VALUE rather than 0, to let base/ histograms code do its own
        // handling of negative values in the future.
        if (value < Integer.MIN_VALUE) return Integer.MIN_VALUE;
        return (int) value;
    }

    private static void recordCustomTimesHistogramMilliseconds(
            String name, long duration, long min, long max, int numBuckets) {
        sRecorder.recordExponentialHistogram(
                name, clampToInt(duration), clampToInt(min), clampToInt(max), numBuckets);
    }

    /**
     * Returns the number of samples recorded in the given bucket of the given histogram.
     *
     * @param name name of the histogram to look up
     * @param sample the bucket containing this sample value will be looked up
     */
    @VisibleForTesting
    public static int getHistogramValueCountForTesting(String name, int sample) {
        return RecordHistogramJni.get().getHistogramValueCountForTesting(name, sample);
    }

    /**
     * Returns the number of samples recorded for the given histogram.
     *
     * @param name name of the histogram to look up
     */
    @VisibleForTesting
    public static int getHistogramTotalCountForTesting(String name) {
        return RecordHistogramJni.get().getHistogramTotalCountForTesting(name);
    }

    /**
     * Natives API to read metrics reported when testing.
     */
    @NativeMethods
    public interface Natives {
        int getHistogramValueCountForTesting(String name, int sample);
        int getHistogramTotalCountForTesting(String name);
    }
}
