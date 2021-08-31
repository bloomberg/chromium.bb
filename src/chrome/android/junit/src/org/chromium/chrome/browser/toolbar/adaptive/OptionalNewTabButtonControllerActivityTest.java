// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.content.res.Configuration;
import android.content.res.Resources;

import androidx.test.core.app.ActivityScenario;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.CommandLine;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider.ButtonDataObserver;
import org.chromium.chrome.browser.toolbar.top.OptionalBrowsingModeButtonController;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabCreatorManager;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.display.DisplayAndroidManager;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.NoSuchElementException;

/**
 * Robolectric tests running {@link OptionalNewTabButtonController} in a {@link
 * ChromeTabbedActivity}.
 */
@Config(shadows = {OptionalNewTabButtonControllerActivityTest.ShadowDelegate.class,
                OptionalNewTabButtonControllerActivityTest.ShadowChromeFeatureList.class})
@RunWith(BaseRobolectricTestRunner.class)
public class OptionalNewTabButtonControllerActivityTest {
    /**
     * Shadow of {@link OptionalNewTabButtonController.Delegate}. Injects testing values into every
     * instance of {@link OptionalNewTabButtonController}.
     */
    @Implements(OptionalNewTabButtonController.Delegate.class)
    public static class ShadowDelegate {
        private static MockTabCreatorManager sTabCreatorManager;
        private static MockTabModelSelector sTabModelSelector;
        private static boolean sIsNTP;

        protected static void reset() {
            sTabModelSelector = null;
            sTabCreatorManager = null;
            sIsNTP = false;
        }

        @Implementation
        protected TabCreatorManager getTabCreatorManager() {
            return sTabCreatorManager;
        }

        @Implementation
        protected TabModelSelector getTabModelSelector() {
            return sTabModelSelector;
        }

        @Implementation
        protected boolean isNTPTab(Tab tab) {
            return sIsNTP;
        }
    }

    // TODO(crbug.com/1199025): Remove this shadow.
    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        private static final Map<String, String> sParamValues = new HashMap<>();

        @Implementation
        public static String getFieldTrialParamByFeature(String feature, String paramKey) {
            Assert.assertTrue(ChromeFeatureList.isEnabled(feature));
            return sParamValues.getOrDefault(paramKey, "");
        }

        @Implementation
        public static boolean isEnabled(String featureName) {
            return featureName.equals(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR);
        }

        public static void reset() {
            sParamValues.clear();
        }
    }

    private ActivityScenario<ChromeTabbedActivity> mActivityScenario;
    private AdaptiveToolbarButtonController mAdaptiveButtonController;
    private Tab mTab;

    @Before
    public void setUp() {
        // Avoid leaking state from the previous test.
        resetStaticState();
        CommandLine.getInstance().appendSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE);
        CommandLine.getInstance().appendSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
        ShadowChromeFeatureList.sParamValues.put("mode", AdaptiveToolbarFeatures.ALWAYS_NEW_TAB);

        MockTabModelSelector tabModelSelector = new MockTabModelSelector(
                /*tabCount=*/1, /*incognitoTabCount=*/0, (id, incognito) -> {
                    Tab tab = spy(MockTab.createAndInitialize(id, incognito));
                    doReturn(Mockito.mock(WebContents.class)).when(tab).getWebContents();
                    return tab;
                });
        assertNull(ShadowDelegate.sTabModelSelector);
        assertNull(ShadowDelegate.sTabCreatorManager);
        ShadowDelegate.sTabModelSelector = tabModelSelector;
        ShadowDelegate.sTabCreatorManager = new MockTabCreatorManager(tabModelSelector);
        mTab = tabModelSelector.getCurrentTab();

        mActivityScenario = ActivityScenario.launch(ChromeTabbedActivity.class);
        mActivityScenario.onActivity(activity -> {
            mAdaptiveButtonController = getAdaptiveButton(getOptionalButtonController(activity));
            mAdaptiveButtonController.onFinishNativeInitialization();
        });
    }

    @After
    public void tearDown() {
        mActivityScenario.close();
        resetStaticState();
    }

    private static void resetStaticState() {
        ShadowDelegate.reset();
        ShadowChromeFeatureList.reset();
        // DisplayAndroidManager will reuse the Display between tests. This can cause
        // AsyncInitializationActivity#applyOverrides to set incorrect smallestWidth.
        DisplayAndroidManager.resetInstanceForTesting();
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
    }

    @Test
    @MediumTest
    @Config(qualifiers = "w390dp-h820dp")
    public void testRotateLandscape() {
        mActivityScenario.onActivity(activity -> {
            ButtonDataObserver observer = Mockito.mock(ButtonDataObserver.class);
            mAdaptiveButtonController.addObserver(observer);

            assertTrue(mAdaptiveButtonController.get(mTab).canShow());

            applyQualifiers(activity, "+land");

            verify(observer).buttonDataChanged(/*canShowHint=*/true);
            assertTrue(mAdaptiveButtonController.get(mTab).canShow());
        });
    }

    @Test
    @MediumTest
    @Config(qualifiers = "w359dp-h820dp")
    public void testRotateLandscape_narrow() {
        mActivityScenario.onActivity(activity -> {
            ButtonDataObserver observer = Mockito.mock(ButtonDataObserver.class);
            mAdaptiveButtonController.addObserver(observer);

            // The button needs width of at least 360dp to be visible.
            // See OptionalNewTabButtonController#MIN_WIDTH_DP
            assertFalse(mAdaptiveButtonController.get(mTab).canShow());

            applyQualifiers(activity, "+land");

            verify(observer).buttonDataChanged(/*canShowHint=*/true);
            assertTrue(mAdaptiveButtonController.get(mTab).canShow());
        });
    }

    @Test
    @MediumTest
    @Config(qualifiers = "w390dp-h820dp-land")
    public void testRotatePortrait() {
        mActivityScenario.onActivity(activity -> {
            ButtonDataObserver observer = Mockito.mock(ButtonDataObserver.class);
            mAdaptiveButtonController.addObserver(observer);

            assertTrue(mAdaptiveButtonController.get(mTab).canShow());

            applyQualifiers(activity, "+port");

            verify(observer).buttonDataChanged(/*canShowHint=*/true);
            assertTrue(mAdaptiveButtonController.get(mTab).canShow());
        });
    }

    @Test
    @MediumTest
    @Config(qualifiers = "w359dp-h820dp-land")
    public void testRotatePortrait_narrow() {
        mActivityScenario.onActivity(activity -> {
            ButtonDataObserver observer = Mockito.mock(ButtonDataObserver.class);
            mAdaptiveButtonController.addObserver(observer);

            assertTrue(mAdaptiveButtonController.get(mTab).canShow());

            applyQualifiers(activity, "+port");

            // The button needs width of at least 360dp to be visible.
            // See OptionalNewTabButtonController#MIN_WIDTH_DP
            verify(observer).buttonDataChanged(/*canShowHint=*/false);
            assertFalse(mAdaptiveButtonController.get(mTab).canShow());
        });
    }

    @Test
    @MediumTest
    @Config(qualifiers = "w600dp-h820dp")
    public void testRotateTablet() {
        mActivityScenario.onActivity(activity -> {
            assertFalse(mAdaptiveButtonController.get(mTab).canShow());

            // Rotating a tablet should not change canShow.
            applyQualifiers(activity, "+land");

            assertFalse(mAdaptiveButtonController.get(mTab).canShow());
        });
    }

    @Test
    @MediumTest
    @Config(qualifiers = "w400dp-h600dp")
    public void testNightMode() {
        mActivityScenario.onActivity(activity -> {
            assertTrue(mAdaptiveButtonController.get(mTab).canShow());

            // Unrelated qualifiers should not change canShow. This covers an early return from
            // onConfigurationChanged.
            applyQualifiers(activity, "+night");

            assertTrue(mAdaptiveButtonController.get(mTab).canShow());
        });
    }

    @Test
    @MediumTest
    @Config(qualifiers = "w400dp-h600dp")
    public void testNtp() {
        mActivityScenario.onActivity(activity -> {
            assertTrue(mAdaptiveButtonController.get(mTab).canShow());

            ShadowDelegate.sIsNTP = true;
            assertFalse(mAdaptiveButtonController.get(mTab).canShow());

            ShadowDelegate.sIsNTP = false;
            assertTrue(mAdaptiveButtonController.get(mTab).canShow());
        });
    }

    private static OptionalBrowsingModeButtonController getOptionalButtonController(
            ChromeTabbedActivity activity) {
        TopToolbarCoordinator toolbar =
                (TopToolbarCoordinator) activity.getToolbarManager().getToolbar();
        return toolbar.getOptionalButtonControllerForTesting();
    }

    private static AdaptiveToolbarButtonController getAdaptiveButton(
            OptionalBrowsingModeButtonController optionalButtonController) {
        List<ButtonDataProvider> buttonDataProviders =
                optionalButtonController.getButtonDataProvidersForTesting();
        for (ButtonDataProvider buttonDataProvider : buttonDataProviders) {
            if (!(buttonDataProvider instanceof AdaptiveToolbarButtonController)) {
                continue;
            }
            return (AdaptiveToolbarButtonController) buttonDataProvider;
        }
        throw new NoSuchElementException();
    }

    /** Sets device qualifiers and notifies the activity about configuration change. */
    private static void applyQualifiers(ChromeActivity activity, String qualifiers) {
        RuntimeEnvironment.setQualifiers(qualifiers);
        Configuration configuration = Resources.getSystem().getConfiguration();
        activity.onConfigurationChanged(configuration);
    }
}
