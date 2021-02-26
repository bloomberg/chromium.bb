// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.policy.EnterpriseInfo;
import org.chromium.chrome.browser.policy.EnterpriseInfo.OwnedState;

/**
 * Unit tests for {@link SkipTosDialogPolicyListener}.
 *
 * For simplicity, this test will not cover cases that already tests in base class unit test
 * {@link PolicyLoadListenerUnitTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {SkipTosDialogPolicyListenerUnitTest.ShadowFirstRunUtils.class,
                ShadowRecordHistogram.class})
public class SkipTosDialogPolicyListenerUnitTest {
    private static final String HIST_IS_DEVICE_OWNED_DETECTED =
            "histogramRecorded.OnIsDeviceOwnedDetected";
    private static final String HIST_POLICY_LOAD_LISTENER_AVAILABLE =
            "histogramRecorded.OnPolicyLoadListenerAvailable";

    @Implements(FirstRunUtils.class)
    static class ShadowFirstRunUtils {
        static boolean sIsCctTosDialogEnabled;

        @Implementation
        public static boolean isCctTosDialogEnabled() {
            return sIsCctTosDialogEnabled;
        }
    }

    static class TestHistNameProvider implements SkipTosDialogPolicyListener.HistogramNameProvider {
        String mHistogramForEnterpriseInfo;
        String mHistogramForPolicyLoadListener;

        public TestHistNameProvider() {
            mHistogramForEnterpriseInfo = HIST_IS_DEVICE_OWNED_DETECTED;
            mHistogramForPolicyLoadListener = HIST_POLICY_LOAD_LISTENER_AVAILABLE;
        }

        @Override
        public String getOnDeviceOwnedDetectedTimeHistogramName() {
            return mHistogramForEnterpriseInfo;
        }

        @Override
        public String getOnPolicyAvailableTimeHistogramName() {
            return mHistogramForPolicyLoadListener;
        }
    }

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Spy
    public Callback<Boolean> mTosDialogCallback;
    @Spy
    public TestHistNameProvider mHistogramNameProvider;
    @Mock
    public PolicyLoadListener mMockPolicyLoadListener;
    @Mock
    public EnterpriseInfo mMockEnterpriseInfo;

    private SkipTosDialogPolicyListener mSkipTosDialogPolicyListener;
    private Callback<OwnedState> mEnterpriseInfoCallback;
    private Callback<Boolean> mPolicyLoadListenerCallback;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        Mockito.doAnswer(invocation -> {
                   mEnterpriseInfoCallback = invocation.getArgument(0);
                   return null;
               })
                .when(mMockEnterpriseInfo)
                .getDeviceEnterpriseInfo(any());

        Mockito.doAnswer(invocation -> {
                   mPolicyLoadListenerCallback = invocation.getArgument(0);
                   return null;
               })
                .when(mMockPolicyLoadListener)
                .onAvailable(any());

        // Set ToS to enabled by default.
        ShadowFirstRunUtils.sIsCctTosDialogEnabled = true;

        buildNewSkipTosDialogPolicyListener();

        assertPolicyCheckNotComplete();
        Mockito.verify(mMockEnterpriseInfo).getDeviceEnterpriseInfo(mEnterpriseInfoCallback);
        Mockito.verify(mMockPolicyLoadListener).onAvailable(mPolicyLoadListenerCallback);
    }

    @After
    public void tearDown() {
        ShadowRecordHistogram.reset();
    }

    @Test
    public void testDeviceNotOwned() {
        setDeviceFullyManaged(false);
        assertTosDialogEnabled();
        assertHistogramsRecorded(true, false);
    }

    @Test
    public void testNoPolicyLoaded() {
        mPolicyLoadListenerCallback.onResult(false);
        assertTosDialogEnabled();
        assertHistogramsRecorded(false, false);
    }

    @Test
    public void testPolicyLoadedWithNoEffect() {
        ShadowFirstRunUtils.sIsCctTosDialogEnabled = true;
        mPolicyLoadListenerCallback.onResult(true);
        assertTosDialogEnabled();
        assertHistogramsRecorded(false, true);
    }

    @Test
    public void testDeviceOwnedWithoutPolicySet() {
        setDeviceFullyManaged(true);
        assertPolicyCheckNotComplete();
        assertHistogramsRecorded(true, false);

        mPolicyLoadListenerCallback.onResult(false);
        assertTosDialogEnabled();
        assertHistogramsRecorded(true, false);
    }

    @Test
    public void testDeviceOwnedWithPolicySetToNoEffect() {
        setDeviceFullyManaged(true);
        assertPolicyCheckNotComplete();
        assertHistogramsRecorded(true, false);

        mPolicyLoadListenerCallback.onResult(true);
        assertTosDialogEnabled();
        assertHistogramsRecorded(true, true);
    }

    @Test
    public void testDeviceOwnedWithPolicySetToSkip() {
        setDeviceFullyManaged(true);
        assertPolicyCheckNotComplete();
        assertHistogramsRecorded(true, false);

        ShadowFirstRunUtils.sIsCctTosDialogEnabled = false;
        mPolicyLoadListenerCallback.onResult(true);
        assertTosDialogSkipped();
        assertHistogramsRecorded(true, true);
    }

    @Test
    public void testPolicySetToSkipWithDeviceNotOwned() {
        ShadowFirstRunUtils.sIsCctTosDialogEnabled = false;
        mPolicyLoadListenerCallback.onResult(true);
        assertPolicyCheckNotComplete();
        assertHistogramsRecorded(false, true);

        setDeviceFullyManaged(false);
        assertTosDialogEnabled();
        assertHistogramsRecorded(true, true);
    }

    @Test
    public void testPolicySetToSkipWithDeviceOwned() {
        ShadowFirstRunUtils.sIsCctTosDialogEnabled = false;
        mPolicyLoadListenerCallback.onResult(true);
        assertPolicyCheckNotComplete();
        assertHistogramsRecorded(false, true);

        setDeviceFullyManaged(true);
        assertTosDialogSkipped();
        assertHistogramsRecorded(true, true);
    }

    @Test
    public void testDestroy_PolicyNotFound() {
        mSkipTosDialogPolicyListener.destroy();
        Mockito.verify(mMockPolicyLoadListener).destroy();

        // Policy signal should be ignored after destroy.
        mPolicyLoadListenerCallback.onResult(false);
        assertPolicyCheckNotComplete();
        assertHistogramsRecorded(false, false);
    }

    @Test
    public void testDestroy_DeviceNotOwned() {
        mSkipTosDialogPolicyListener.destroy();
        Mockito.verify(mMockPolicyLoadListener).destroy();

        // Device owned signal should be ignored after destroy.
        setDeviceFullyManaged(false);
        assertPolicyCheckNotComplete();
        assertHistogramsRecorded(false, false);
    }

    @Test
    public void testDestroy_SkipTosDialog() {
        mSkipTosDialogPolicyListener.destroy();
        Mockito.verify(mMockPolicyLoadListener).destroy();

        setDeviceFullyManaged(true);
        assertPolicyCheckNotComplete();
        assertHistogramsRecorded(false, false);

        ShadowFirstRunUtils.sIsCctTosDialogEnabled = false;
        mPolicyLoadListenerCallback.onResult(true);
        // Signals should be ignore since #destroy happened.
        assertPolicyCheckNotComplete();
        assertHistogramsRecorded(false, false);
    }

    @Test
    public void testBuildListenerAfterPolicyLoadedAsNotNeeded() {
        setupMockPolicyLoadListenerInitialized(false);

        buildNewSkipTosDialogPolicyListener();
        assertTosDialogEnabled();
        assertHistogramsRecorded(false, false);
    }

    @Test
    public void testBuildListenerAfterPolicyLoadedAsNeeded_TosSkipped() {
        setupMockPolicyLoadListenerInitialized(true);
        ShadowFirstRunUtils.sIsCctTosDialogEnabled = false;

        buildNewSkipTosDialogPolicyListener();
        assertPolicyCheckNotComplete();
        assertHistogramsRecorded(false, true);

        setDeviceFullyManaged(true);
        assertTosDialogSkipped();
        assertHistogramsRecorded(true, true);
    }

    @Test
    public void testBuildListenerAfterPolicyLoadedAsNeeded_TosEnabled() {
        setupMockPolicyLoadListenerInitialized(true);

        buildNewSkipTosDialogPolicyListener();
        assertTosDialogEnabled();
        assertHistogramsRecorded(false, true);
    }

    @Test
    public void testBuildListenerAfterDetectedDeviceNotManaged() {
        setupMockEnterpriseInitializedWithDeviceManaged(false);

        buildNewSkipTosDialogPolicyListener();
        assertTosDialogEnabled();
        assertHistogramsRecorded(true, false);
    }

    @Test
    public void testBuildListenerAfterDetectedDeviceManaged() {
        setupMockEnterpriseInitializedWithDeviceManaged(true);

        buildNewSkipTosDialogPolicyListener();
        assertPolicyCheckNotComplete();
        assertHistogramsRecorded(true, false);

        ShadowFirstRunUtils.sIsCctTosDialogEnabled = false;
        mPolicyLoadListenerCallback.onResult(true);
        assertTosDialogSkipped();
        assertHistogramsRecorded(true, true);
    }

    @Test
    public void testUpdateHistogramNameProvider() {
        // Update the names for mHistogramNameProvider and test if the old hists are not recorded.
        String newHistogramForEnterprise = "another.histogram.enterprise";
        String newHistogramForPolicy = "another.histogram.policy";

        mHistogramNameProvider.mHistogramForEnterpriseInfo = newHistogramForEnterprise;
        mHistogramNameProvider.mHistogramForPolicyLoadListener = newHistogramForPolicy;

        setDeviceFullyManaged(true);
        Mockito.verify(mHistogramNameProvider).getOnDeviceOwnedDetectedTimeHistogramName();
        Assert.assertEquals("Old histogram for EnterpriseInfo should not be recorded.", 0,
                RecordHistogram.getHistogramTotalCountForTesting(HIST_IS_DEVICE_OWNED_DETECTED));
        Assert.assertEquals("New Histogram for EnterpriseInfo should be recorded.", 1,
                RecordHistogram.getHistogramTotalCountForTesting(newHistogramForEnterprise));

        mPolicyLoadListenerCallback.onResult(true);
        Mockito.verify(mHistogramNameProvider).getOnPolicyAvailableTimeHistogramName();
        Assert.assertEquals("Old histogram for Policy should not be recorded.", 0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        HIST_POLICY_LOAD_LISTENER_AVAILABLE));
        Assert.assertEquals("New Histogram for Policy should be recorded.", 1,
                RecordHistogram.getHistogramTotalCountForTesting(newHistogramForPolicy));
    }

    private void assertTosDialogEnabled() {
        Assert.assertFalse("ToS dialog should be enabled.", mSkipTosDialogPolicyListener.get());
        Mockito.verify(mTosDialogCallback).onResult(false);
    }

    private void assertTosDialogSkipped() {
        Assert.assertTrue(
                "ToS dialog should be skipped according to device and enterprise setting.",
                mSkipTosDialogPolicyListener.get());
        Mockito.verify(mTosDialogCallback).onResult(true);
    }

    private void assertPolicyCheckNotComplete() {
        Assert.assertNull("Whether ToS policy might take effect should not be decided yet.",
                mSkipTosDialogPolicyListener.get());
        Mockito.verify(mTosDialogCallback, never()).onResult(anyBoolean());
    }

    private void assertHistogramsRecorded(boolean isDeviceOwned, boolean isPolicyAvailable) {
        assertOnDeviceOwnedDetectedTimeHistogramRecorded(isDeviceOwned);
        assertOnPolicyAvailableTimeHistogramRecorded(isPolicyAvailable);
    }

    private void assertOnDeviceOwnedDetectedTimeHistogramRecorded(boolean isRecorded) {
        int timesRecorded = isRecorded ? 1 : 0;
        Mockito.verify(mHistogramNameProvider, times(timesRecorded))
                .getOnDeviceOwnedDetectedTimeHistogramName();
        Assert.assertEquals("Histogram for EnterpriseInfo is not recorded correctly.",
                timesRecorded,
                RecordHistogram.getHistogramTotalCountForTesting(HIST_IS_DEVICE_OWNED_DETECTED));
    }

    private void assertOnPolicyAvailableTimeHistogramRecorded(boolean isRecorded) {
        int timesRecorded = isRecorded ? 1 : 0;
        Mockito.verify(mHistogramNameProvider, times(timesRecorded))
                .getOnPolicyAvailableTimeHistogramName();
        Assert.assertEquals("Histogram for PolicyLoadListener is not recorded.", timesRecorded,
                RecordHistogram.getHistogramTotalCountForTesting(
                        HIST_POLICY_LOAD_LISTENER_AVAILABLE));
    }

    private void buildNewSkipTosDialogPolicyListener() {
        mSkipTosDialogPolicyListener = new SkipTosDialogPolicyListener(
                mMockPolicyLoadListener, mMockEnterpriseInfo, mHistogramNameProvider);
        mSkipTosDialogPolicyListener.onAvailable(mTosDialogCallback);
    }

    private void setupMockPolicyLoadListenerInitialized(boolean hasPolicy) {
        Mockito.reset(mMockPolicyLoadListener);
        mPolicyLoadListenerCallback = null;

        Mockito.doAnswer(invocation -> {
                   Callback<Boolean> callback = invocation.getArgument(0);
                   mPolicyLoadListenerCallback = callback;
                   callback.onResult(hasPolicy);
                   return hasPolicy;
               })
                .when(mMockPolicyLoadListener)
                .onAvailable(any());
    }

    private void setupMockEnterpriseInitializedWithDeviceManaged(boolean isDeviceOwned) {
        Mockito.reset(mMockEnterpriseInfo);
        mEnterpriseInfoCallback = null;

        Mockito.doAnswer(invocation -> {
                   Callback<OwnedState> callback = invocation.getArgument(0);
                   mEnterpriseInfoCallback = callback;
                   OwnedState state = new OwnedState(isDeviceOwned, false);
                   callback.onResult(state);
                   return state;
               })
                .when(mMockEnterpriseInfo)
                .getDeviceEnterpriseInfo(any());
    }

    private void setDeviceFullyManaged(boolean isDeviceOwned) {
        mEnterpriseInfoCallback.onResult(new OwnedState(isDeviceOwned, false));
    }
}
