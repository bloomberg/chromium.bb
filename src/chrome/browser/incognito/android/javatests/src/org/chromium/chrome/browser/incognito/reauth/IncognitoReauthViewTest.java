// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.core.IsNot.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.R;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.io.IOException;

/**
 * Tests for Incognito reauth view layout.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoReauthViewTest extends BlankUiTestActivityTestCase {
    private View mView;
    private PropertyModel mPropertyModel;
    private PropertyModelChangeProcessor mModelChangeProcessor;
    private IncognitoReauthMenuDelegate mIncognitoReauthMenuDelegate;

    @Mock
    private Runnable mUnlockIncognitoRunnableMock;
    @Mock
    private Runnable mSeeOtherTabsRunnableMock;
    @Mock
    private TabModelSelector mTabModelSelectorMock;
    @Mock
    private TabModel mIncognitoTabModelMock;

    @Mock
    private SettingsLauncher mSettingsLauncherMock;

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.PRIVACY_INCOGNITO)
                    .build();

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);
        doReturn(mIncognitoTabModelMock).when(mTabModelSelectorMock).getModel(/*incognito=*/true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().setContentView(R.layout.incognito_reauth_view);
            mView = getActivity().findViewById(android.R.id.content);
            mIncognitoReauthMenuDelegate = new IncognitoReauthMenuDelegate(
                    getActivity(), mTabModelSelectorMock, mSettingsLauncherMock);
        });
    }

    @Test
    @MediumTest
    public void testIncognitoReauthView_FullScreen() {
        buildPropertyModelAndBindProcessor(/*isFullScreen=*/true);
        onView(withId(R.id.incognito_reauth_menu_button)).check(matches(isDisplayed()));
        onView(withId(R.id.incognito_reauth_unlock_incognito_button)).check(matches(isDisplayed()));
        onView(withText(R.string.incognito_reauth_page_unlock_incognito_button_label))
                .check(matches(isDisplayed()));

        onView(withId(R.id.incognito_reauth_see_other_tabs_label)).check(matches(isDisplayed()));
        onView(withText(R.string.incognito_reauth_page_see_other_tabs_label))
                .check(matches(isDisplayed()));

        onView(withId(R.id.incognito_reauth_unlock_incognito_button)).perform(click());
        verify(mUnlockIncognitoRunnableMock).run();

        onView(withId(R.id.incognito_reauth_see_other_tabs_label)).perform(click());
        verify(mSeeOtherTabsRunnableMock).run();
    }

    @Test
    @MediumTest
    public void testIncognitoReauthView_FullScreen_MenuButton_CloseIncognitoTabs_SubMenu() {
        buildPropertyModelAndBindProcessor(/*isFullScreen=*/true);
        onView(withId(R.id.incognito_reauth_menu_button)).perform(click());

        // Inside three dots menu.
        onView(withText(R.string.menu_close_all_incognito_tabs)).perform(click());
        verify(mIncognitoTabModelMock).closeAllTabs();
    }

    @Test
    @MediumTest
    public void testIncognitoReauthView_FullScreen_MenuButton_Settings_SubMenu() {
        buildPropertyModelAndBindProcessor(/*isFullScreen=*/true);
        onView(withId(R.id.incognito_reauth_menu_button)).perform(click());

        // Inside three dots menu.
        onView(withText(R.string.menu_settings)).perform(click());
        verify(mSettingsLauncherMock).launchSettingsActivity(any());
    }

    @Test
    @MediumTest
    public void testIncognitoReauthView_TabSwitcherView() {
        buildPropertyModelAndBindProcessor(/*isFullScreen=*/false);
        onView(withId(R.id.incognito_reauth_menu_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.incognito_reauth_unlock_incognito_button)).check(matches(isDisplayed()));
        onView(withText(R.string.incognito_reauth_page_unlock_incognito_button_label))
                .check(matches(isDisplayed()));

        onView(withId(R.id.incognito_reauth_see_other_tabs_label))
                .check(matches(not(isDisplayed())));
        onView(withText(R.string.incognito_reauth_page_see_other_tabs_label))
                .check(matches(not(isDisplayed())));

        onView(withId(R.id.incognito_reauth_unlock_incognito_button)).perform(click());
        verify(mUnlockIncognitoRunnableMock).run();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testIncongitoReauthViewRenderTest() throws IOException {
        buildPropertyModelAndBindProcessor(/*isFullScreen=*/true);
        mRenderTestRule.render(mView, "incognito_reauth_view");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testIncongitoReauthViewRenderTest_WithThreeDotsMenu() throws IOException {
        buildPropertyModelAndBindProcessor(/*isFullScreen=*/true);
        onView(withId(R.id.incognito_reauth_menu_button)).perform(click());
        onViewWaiting(allOf(withText(R.string.menu_settings), isDisplayed()));
        mRenderTestRule.render(mIncognitoReauthMenuDelegate.getBasicListMenu().getContentView(),
                "incognito_reauth_view_three_dots_menu");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testIncongitoReauthViewTabSwitcherRenderTest() throws IOException {
        buildPropertyModelAndBindProcessor(/*isFullScreen=*/false);
        mRenderTestRule.render(mView, "incognito_reauth_view_tab_switcher");
    }

    private void buildPropertyModelAndBindProcessor(boolean isFullScreen) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel = IncognitoReauthProperties.createPropertyModel(
                    mUnlockIncognitoRunnableMock, mSeeOtherTabsRunnableMock, isFullScreen,
                    (isFullScreen) ? () -> mIncognitoReauthMenuDelegate.getBasicListMenu() : null);
            mModelChangeProcessor = PropertyModelChangeProcessor.create(
                    mPropertyModel, mView, IncognitoReauthViewBinder::bind);
        });
    }
}
