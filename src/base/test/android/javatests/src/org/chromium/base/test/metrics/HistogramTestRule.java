// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.metrics;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.metrics.RecordHistogramJni;

/**
 * A {@link TestRule} to test histograms. Returns the amount of recorded samples during a test run.
 * Similar to HistogramTester, it takes a snapshot of existing histogram rules before a test runs
 * and only returns the difference to the this snapshot.
 *
 * Example usage:
 *
 *    @Rule
 *    public HistogramTestRule mHistogramTester = new HistogramTestRule();
 *
 *    @Test
 *    public void testFoo() {
 *        ...
 *        assertEquals(1, mHistogramTester.getHistogramTotalCount("Hist.Name"));
 *    }
 */
public class HistogramTestRule implements TestRule {
    // Pointer to a native HistogramSnapshotPtr instance.
    // This Java class owns the pointer and is responsible for destroying it.
    private long mSnapShotPtr;

    @Override
    public Statement apply(Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                assert mSnapShotPtr == 0;
                mSnapShotPtr = RecordHistogramJni.get().createHistogramSnapshotForTesting();
                try {
                    base.evaluate();
                } finally {
                    RecordHistogramJni.get().destroyHistogramSnapshotForTesting(mSnapShotPtr);
                    mSnapShotPtr = 0;
                }
            }
        };
    }

    /**
     * Returns the number of samples recorded in the given bucket of the given histogram.
     *
     * @param name name of the histogram to look up
     * @param sample the bucket containing this sample value will be looked up
     */
    public int getHistogramValueCount(String name, int sample) {
        assert mSnapShotPtr != 0;
        return RecordHistogramJni.get().getHistogramValueCountForTesting(
                name, sample, mSnapShotPtr);
    }

    /**
     * Returns the number of samples recorded for the given histogram.
     *
     * @param name name of the histogram to look up
     */
    public int getHistogramTotalCount(String name) {
        assert mSnapShotPtr != 0;
        return RecordHistogramJni.get().getHistogramTotalCountForTesting(name, mSnapShotPtr);
    }
}
