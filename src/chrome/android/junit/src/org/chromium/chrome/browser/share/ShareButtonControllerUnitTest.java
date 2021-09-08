// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
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
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.WebContents;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for {@link ShareButtonController}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class ShareButtonControllerUnitTest {
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
    private Drawable mDrawable;
    @Mock
    private ActivityTabProvider mTabProvider;
    @Mock
    private ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    @Mock
    private ShareDelegate mShareDelegate;
    @Mock
    private ShareUtils mShareUtils;
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    private ModalDialogManager mModalDialogManager;
    @Mock
    private Tracker mTracker;

    private Configuration mConfiguration = new Configuration();
    private ShareButtonController mShareButtonController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(mTab).when(mTabProvider).get();
        doReturn(mContext).when(mTab).getContext();
        doReturn(mResources).when(mContext).getResources();
        mConfiguration.screenWidthDp = ShareButtonController.MIN_WIDTH_DP + WIDTH_DELTA;
        doReturn(mConfiguration).when(mResources).getConfiguration();

        doReturn(mock(WebContents.class)).when(mTab).getWebContents();
        doReturn(true).when(mShareUtils).shouldEnableShare(mTab);

        doReturn(mShareDelegate).when(mShareDelegateSupplier).get();

        AdaptiveToolbarFeatures.clearParsedParamsForTesting();

        mShareButtonController =
                new ShareButtonController(mContext, mDrawable, mTabProvider, mShareDelegateSupplier,
                        ()
                                -> mTracker,
                        mShareUtils, mActivityLifecycleDispatcher, mModalDialogManager, () -> {});

        TrackerFactory.setTrackerForTests(mTracker);
    }

    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
    @Test
    public void testIPHCommandHelper() {
        assertNull(mShareButtonController.get(/*tab*/ null).getButtonSpec().getIPHCommandBuilder());

        // Verify that IPHCommandBuilder is set just once;
        IPHCommandBuilder builder =
                mShareButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder();

        assertNotNull(mShareButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder());

        // Verify that IPHCommandBuilder is same as before, get(Tab) did not create a new one.
        assertEquals(
                builder, mShareButtonController.get(mTab).getButtonSpec().getIPHCommandBuilder());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
    public void testIPHEvent() {
        doReturn(true).when(mTracker).shouldTriggerHelpUI(
                FeatureConstants.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_SHARE_FEATURE);

        View view = mock(View.class);
        mShareButtonController.get(mTab).getButtonSpec().getOnClickListener().onClick(view);

        verify(mTracker, times(1))
                .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_SHARE_OPENED);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
    public void testDoNotShowWhenTooNarrow() {
        mConfiguration.screenWidthDp = ShareButtonController.MIN_WIDTH_DP - 1;
        mShareButtonController.onConfigurationChanged(mConfiguration);

        ButtonData buttonData = mShareButtonController.get(mTab);

        assertFalse(buttonData.canShow());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
    public void testDoShowWhenWideEnough() {
        mConfiguration.screenWidthDp = ShareButtonController.MIN_WIDTH_DP;
        mShareButtonController.onConfigurationChanged(mConfiguration);

        ButtonData buttonData = mShareButtonController.get(mTab);

        assertTrue(buttonData.canShow());
    }
}
