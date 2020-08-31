// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.components.optimization_guide.proto.ModelsProto.OptimizationTarget;
import org.chromium.content_public.browser.NavigationHandle;

import java.util.Arrays;

/**
 * Unit tests for OptimizationGuideBridge that call into native.
 */
public class OptimizationGuideBridgeNativeUnitTest {
    private static final String TEST_URL = "https://example.com/";

    private static class OptimizationGuideCallback
            implements OptimizationGuideBridge.OptimizationGuideCallback {
        private boolean mWasCalled;
        private @OptimizationGuideDecision int mDecision;
        private OptimizationMetadata mMetadata;

        @Override
        public void onOptimizationGuideDecision(
                @OptimizationGuideDecision int decision, OptimizationMetadata metadata) {
            mWasCalled = true;
            mDecision = decision;
            mMetadata = metadata;
        }

        public boolean wasCalled() {
            return mWasCalled;
        }

        public @OptimizationGuideDecision int getDecision() {
            return mDecision;
        }

        public OptimizationMetadata getMetadata() {
            return mMetadata;
        }
    }

    @CalledByNative
    private OptimizationGuideBridgeNativeUnitTest() {}

    @CalledByNative
    public void testRegisterOptimizationTypesAndTargets() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge();
        bridge.registerOptimizationTypesAndTargets(
                Arrays.asList(new OptimizationType[] {
                        OptimizationType.PERFORMANCE_HINTS, OptimizationType.DEFER_ALL_SCRIPT}),
                Arrays.asList(new OptimizationTarget[] {
                        OptimizationTarget.OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD}));
    }

    @CalledByNative
    public void testCanApplyOptimizationPreInit() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge();

        NavigationHandle navHandle = new NavigationHandle(0, TEST_URL, true, false, false);
        OptimizationGuideCallback callback = new OptimizationGuideCallback();
        bridge.canApplyOptimization(navHandle, OptimizationType.PERFORMANCE_HINTS, callback);

        assertTrue(callback.wasCalled());
        assertEquals(OptimizationGuideDecision.UNKNOWN, callback.getDecision());
        assertNull(callback.getMetadata());
    }

    @CalledByNative
    public void testCanApplyOptimizationHasHint() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge();

        NavigationHandle navHandle = new NavigationHandle(0, TEST_URL, true, false, false);
        OptimizationGuideCallback callback = new OptimizationGuideCallback();
        bridge.canApplyOptimization(navHandle, OptimizationType.PERFORMANCE_HINTS, callback);

        assertTrue(callback.wasCalled());
        assertEquals(OptimizationGuideDecision.TRUE, callback.getDecision());
        assertNotNull(callback.getMetadata());
        assertNotNull(callback.getMetadata().getPerformanceHintsMetadata());
        assertEquals(
                1, callback.getMetadata().getPerformanceHintsMetadata().getPerformanceHintsCount());
    }
}
