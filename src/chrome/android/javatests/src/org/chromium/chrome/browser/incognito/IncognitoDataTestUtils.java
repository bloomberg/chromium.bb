// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.ArrayList;
import java.util.List;

/**
 * This class provides helper methods for launching any Urls in CCT or Tabs.
 * This also provides parameters for tests. Parameters include pair of activity types.
 *
 */
public class IncognitoDataTestUtils {
    public enum ActivityType {
        INCOGNITO_TAB(true, false),
        INCOGNITO_CCT(true, true),
        REGULAR_TAB(false, false),
        REGULAR_CCT(false, true);

        public final boolean incognito;
        public final boolean cct;

        ActivityType(boolean incognito, boolean cct) {
            this.incognito = incognito;
            this.cct = cct;
        }

        public Tab launchUrl(ChromeActivityTestRule chromeActivityRule,
                CustomTabActivityTestRule customTabActivityTestRule, String url) {
            if (cct) {
                return launchUrlInCCT(customTabActivityTestRule, url, incognito);
            } else {
                return launchUrlInTab(chromeActivityRule, url, incognito);
            }
        }
    }

    public static class TestParams {
        private static List<ParameterSet> getParameters(
                boolean firstIncognito, boolean secondIncognito) {
            List<ParameterSet> tests = new ArrayList<>();

            for (ActivityType activity1 : ActivityType.values()) {
                for (ActivityType activity2 : ActivityType.values()) {
                    // We remove the test with two incognito tabs because they are known to share
                    // state via same session.
                    if ((activity1.incognito && !activity1.cct)
                            && (activity2.incognito && !activity2.cct)) {
                        continue;
                    }

                    if (activity1.incognito == firstIncognito
                            && activity2.incognito == secondIncognito) {
                        tests.add(new ParameterSet()
                                          .value(activity1.toString(), activity2.toString())
                                          .name(activity1.toString() + "_" + activity2.toString()));
                    }
                }
            }

            return tests;
        }

        public static class RegularToIncognito implements ParameterProvider {
            @Override
            public List<ParameterSet> getParameters() {
                return TestParams.getParameters(false, true);
            }
        }

        public static class IncognitoToRegular implements ParameterProvider {
            @Override
            public List<ParameterSet> getParameters() {
                return TestParams.getParameters(true, false);
            }
        }

        public static class IncognitoToIncognito implements ParameterProvider {
            @Override
            public List<ParameterSet> getParameters() {
                return TestParams.getParameters(true, true);
            }
        }

        public static class RegularToRegular implements ParameterProvider {
            @Override
            public List<ParameterSet> getParameters() {
                return TestParams.getParameters(false, false);
            }
        }

        public static class AllTypesToAllTypes implements ParameterProvider {
            @Override
            public List<ParameterSet> getParameters() {
                List<ParameterSet> result = new ArrayList<>();
                result.addAll(new TestParams.RegularToIncognito().getParameters());
                result.addAll(new TestParams.IncognitoToRegular().getParameters());
                result.addAll(new TestParams.IncognitoToIncognito().getParameters());
                result.addAll(new TestParams.RegularToRegular().getParameters());
                return result;
            }
        }
    }

    private static Tab launchUrlInTab(
            ChromeActivityTestRule testRule, String url, boolean incognito) {
        // This helps to bring back the "existing" chrome tabbed activity to foreground
        // in case the custom tab activity was launched before.
        testRule.startMainActivityOnBlankPage();

        Tab tab = testRule.loadUrlInNewTab(url, incognito);

        // Giving time to the WebContents to be ready.
        CriteriaHelper.pollUiThread(() -> { assertNotNull(tab.getWebContents()); });

        assertEquals(incognito, tab.getWebContents().isIncognito());
        return tab;
    }

    private static Tab launchUrlInCCT(
            CustomTabActivityTestRule testRule, String url, boolean incognito) {
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getContext(), url);

        if (incognito) {
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        }

        testRule.startCustomTabActivityWithIntent(intent);

        Tab tab = testRule.getActivity().getActivityTab();

        // Giving time to the WebContents to be ready.
        CriteriaHelper.pollUiThread(() -> { assertNotNull(tab.getWebContents()); });

        assertEquals(incognito, tab.getWebContents().isIncognito());
        return tab;
    }

    public static void closeTabs(ChromeActivityTestRule testRule) {
        ChromeActivity activity = testRule.getActivity();
        if (activity == null) return;
        activity.getTabModelSelector().getModel(false).closeAllTabs();
        activity.getTabModelSelector().getModel(true).closeAllTabs();
    }
}