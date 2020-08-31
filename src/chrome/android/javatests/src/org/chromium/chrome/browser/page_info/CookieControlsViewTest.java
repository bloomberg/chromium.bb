// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isChecked;
import static android.support.test.espresso.matcher.ViewMatchers.isNotChecked;
import static android.support.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.matcher.ViewMatchers;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.content_settings.ContentSettingsFeatureList;
import org.chromium.components.page_info.PageInfoAction;
import org.chromium.components.page_info.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/**
 * Tests for CookieControlsView.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
@Features.EnableFeatures(
        ContentSettingsFeatureList.IMPROVED_COOKIE_CONTROLS_FOR_THIRD_PARTY_COOKIE_BLOCKING)
public class CookieControlsViewTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @ClassRule
    public static DisableAnimationsTestRule disableAnimationsRule = new DisableAnimationsTestRule();

    private final String mPath = "/chrome/test/data/android/simple.html";
    private EmbeddedTestServer mTestServer;

    private void loadUrlAndOpenPageInfo(String url) {
        mActivityTestRule.loadUrlInNewTab(url);
        onView(withId(org.chromium.chrome.R.id.location_bar_status_icon)).perform(click());
    }

    private void setThirdPartyCookieBlocking(boolean value) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge.getInstance().setBoolean(Pref.BLOCK_THIRD_PARTY_COOKIES, value);
        });
    }

    private int getTotalPageActionHistogramCount() {
        return RecordHistogram.getHistogramTotalCountForTesting("WebsiteSettings.Action");
    }

    private int getPageActionHistogramCount(@PageInfoAction int action) {
        return RecordHistogram.getHistogramValueCountForTesting("WebsiteSettings.Action", action);
    }

    @Before
    public void setUp() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        mTestServer.destroy();
    }

    /**
     * Tests that CookieControlsView is hidden on a blank page.
     */
    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/1062645")
    public void testHiddenOnBlankPage() {
        setThirdPartyCookieBlocking(true);
        onView(withId(org.chromium.chrome.R.id.location_bar_status_icon)).perform(click());
        onView(withId(R.id.page_info_cookie_controls_view))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
    }

    /**
     * Tests that CookieControlsView is hidden if third-party cookie blocking is off.
     */
    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/1062645")
    public void testHiddenWhenDisabled() {
        setThirdPartyCookieBlocking(false);
        loadUrlAndOpenPageInfo(mTestServer.getURLWithHostName("foo.com", mPath));
        onView(withId(R.id.page_info_cookie_controls_view))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
    }

    /**
     * Tests that CookieControlsView can be instantiated and shown.
     */
    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/1062645")
    public void testShow() {
        setThirdPartyCookieBlocking(true);
        loadUrlAndOpenPageInfo(mTestServer.getURLWithHostName("foo.com", mPath));
        onView(withId(R.id.page_info_cookie_controls_view))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)));
    }

    /**
     * Tests that CookieControlsView updates on navigations.
     */
    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/1062645")
    public void testUpdate() {
        Assert.assertEquals(0, getTotalPageActionHistogramCount());

        setThirdPartyCookieBlocking(true);
        loadUrlAndOpenPageInfo(mTestServer.getURLWithHostName("foo.com", mPath));
        Assert.assertEquals(1, getTotalPageActionHistogramCount());
        Assert.assertEquals(1, getPageActionHistogramCount(PageInfoAction.PAGE_INFO_OPENED));
        int switch_id = R.id.cookie_controls_block_cookies_switch;
        onView(withId(switch_id)).check(matches(isChecked()));
        onView(withId(switch_id)).perform(click());
        onView(withId(switch_id)).check(matches(isNotChecked()));
        Assert.assertEquals(2, getTotalPageActionHistogramCount());
        Assert.assertEquals(
                1, getPageActionHistogramCount(PageInfoAction.PAGE_INFO_COOKIE_ALLOWED_FOR_SITE));

        // Load a different page.
        loadUrlAndOpenPageInfo(mTestServer.getURLWithHostName("bar.com", mPath));
        onView(withId(switch_id)).check(matches(isChecked()));

        // Go back to foo.com.
        loadUrlAndOpenPageInfo(mTestServer.getURLWithHostName("foo.com", mPath));
        onView(withId(switch_id)).check(matches(isNotChecked()));
        onView(withId(switch_id)).perform(click());
        onView(withId(switch_id)).check(matches(isChecked()));
        Assert.assertEquals(5, getTotalPageActionHistogramCount());

        Assert.assertEquals(
                1, getPageActionHistogramCount(PageInfoAction.PAGE_INFO_COOKIE_BLOCKED_FOR_SITE));
        Assert.assertEquals(3, getPageActionHistogramCount(PageInfoAction.PAGE_INFO_OPENED));
    }
}
