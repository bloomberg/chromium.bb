// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.io.IOException;

/**
 * Tests for Incognito reauth view layout.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoReauthViewTest extends DummyUiActivityTestCase {
    private View mView;

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().setContentView(R.layout.incognito_reauth_view);
            mView = getActivity().findViewById(android.R.id.content);
        });
    }

    @Test
    @MediumTest
    public void testIncognitoReauthViewPageCorrectlyDisplayed() {
        onView(withId(R.id.incognito_reauth_unlock_incognito_button)).check(matches(isDisplayed()));
        onView(withText(R.string.incognito_reauth_page_unlock_incognito_button_label))
                .check(matches(isDisplayed()));

        onView(withId(R.id.incognito_reauth_see_other_tabs_label)).check(matches(isDisplayed()));
        onView(withText(R.string.incognito_reauth_page_see_other_tabs_label))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testIncongitoReauthViewRenderTest() throws IOException {
        mRenderTestRule.render(mView, "incognito_reauth_view");
    }
}
