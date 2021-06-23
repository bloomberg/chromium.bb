// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

/** Unit tests for {@link VoiceToolbarButtonController}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class VoiceToolbarButtonControllerUnitTest {
    private static final int WIDTH_DELTA = 50;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private Context mContext;
    @Mock
    private Resources mResources;
    @Mock
    private Tab mTab;
    @Mock
    private GURL mUrl;
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    private ModalDialogManager mModalDialogManager;
    @Mock
    private VoiceToolbarButtonController.VoiceSearchDelegate mVoiceSearchDelegate;
    @Mock
    private Drawable mDrawable;
    @Mock
    private Tracker mTracker;

    private Configuration mConfiguration = new Configuration();
    private VoiceToolbarButtonController mVoiceToolbarButtonController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mConfiguration.screenWidthDp =
                VoiceToolbarButtonController.DEFAULT_MIN_WIDTH_DP + WIDTH_DELTA;
        doReturn(mConfiguration).when(mResources).getConfiguration();
        doReturn(mResources).when(mContext).getResources();

        doReturn(true).when(mVoiceSearchDelegate).isVoiceSearchEnabled();

        doReturn(false).when(mTab).isIncognito();

        doReturn("https").when(mUrl).getScheme();
        doReturn(mUrl).when(mTab).getUrl();

        doReturn(mContext).when(mTab).getContext();
        AdaptiveToolbarFeatures.clearParsedParamsForTesting();
        // clang-format off
        mVoiceToolbarButtonController = new VoiceToolbarButtonController(mContext, mDrawable,
                () -> mTab, () -> mTracker, mActivityLifecycleDispatcher, mModalDialogManager,
                mVoiceSearchDelegate);
        // clang-format on

        TrackerFactory.setTrackerForTests(mTracker);
    }

    @EnableFeatures({ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR})
    @DisableFeatures({ChromeFeatureList.TOOLBAR_MIC_IPH_ANDROID,
            ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    @Test
    public void
    onConfigurationChanged_screenWidthChanged() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR, false);
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_NONE);

        assertTrue(mVoiceToolbarButtonController.get(mTab).canShow());

        // Screen width shrinks below the threshold (e.g. screen rotated).
        mConfiguration.screenWidthDp =
                VoiceToolbarButtonController.DEFAULT_MIN_WIDTH_DP - WIDTH_DELTA;
        mVoiceToolbarButtonController.onConfigurationChanged(mConfiguration);

        assertFalse(mVoiceToolbarButtonController.get(mTab).canShow());

        // Make sure the opposite works as well.
        mConfiguration.screenWidthDp =
                VoiceToolbarButtonController.DEFAULT_MIN_WIDTH_DP + WIDTH_DELTA;
        mVoiceToolbarButtonController.onConfigurationChanged(mConfiguration);

        assertTrue(mVoiceToolbarButtonController.get(mTab).canShow());
    }

    @EnableFeatures({ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR,
            ChromeFeatureList.TOOLBAR_MIC_IPH_ANDROID})
    @Test
    public void
    testIPHCommandHelper() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR, false);
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_NONE);
        assertNull(mVoiceToolbarButtonController.get(/*tab*/ null)
                           .getButtonSpec()
                           .getIPHCommandBuilder());

        // Verify that IPHCommandBuilder is set just once;
        IPHCommandBuilder builder =
                mVoiceToolbarButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder();

        assertNotNull(
                mVoiceToolbarButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder());
        assertEquals(builder,
                mVoiceToolbarButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION})
    @DisableFeatures({ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR,
            ChromeFeatureList.TOOLBAR_MIC_IPH_ANDROID})
    public void
    testIPHEvent() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR, false);
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_NONE);
        doReturn(true).when(mTracker).shouldTriggerHelpUI(
                FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_VOICE_SEARCH_FEATURE);

        View view = Mockito.mock(View.class);
        mVoiceToolbarButtonController.get(mTab).getButtonSpec().getOnClickListener().onClick(view);

        verify(mTracker, times(1))
                .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_VOICE_SEARCH_OPENED);
    }

    @Test
    public void isToolbarMicEnabled_adaptiveButtons_nonVoice() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR, true);
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_SHARE);

        assertFalse(VoiceToolbarButtonController.isToolbarMicEnabled());
    }

    @Test
    public void isToolbarMicEnabled_adaptiveButtons_voice() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR, true);
        AdaptiveToolbarFeatures.MODE_PARAM.setForTesting(AdaptiveToolbarFeatures.ALWAYS_VOICE);

        assertTrue(VoiceToolbarButtonController.isToolbarMicEnabled());
    }

    @EnableFeatures({ChromeFeatureList.VOICE_BUTTON_IN_TOP_TOOLBAR})
    @Test
    public void isToolbarMicEnabled_toolbarMic() {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR, false);

        assertTrue(VoiceToolbarButtonController.isToolbarMicEnabled());
    }
}
