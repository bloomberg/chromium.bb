// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withSpinnerText;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anything;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.lessThan;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.collection.IsMapContaining.hasEntry;

import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.withCount;

import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.view.MotionEvent;
import android.view.View;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.test.espresso.DataInteraction;
import androidx.test.filters.MediumTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.DeveloperModeUtils;
import org.chromium.android_webview.common.Flag;
import org.chromium.android_webview.devui.FlagsFragment;
import org.chromium.android_webview.devui.MainActivity;
import org.chromium.android_webview.devui.R;
import org.chromium.android_webview.services.DeveloperUiService;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Map;

/**
 * UI tests for {@link FlagsFragment}.
 * <p>
 * These tests should not be batched to make sure that the DeveloperUiService is killed
 * after each test, leaving a clean state.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class FlagsFragmentTest {
    @Rule
    public BaseActivityTestRule mRule = new BaseActivityTestRule<MainActivity>(MainActivity.class);

    @Before
    public void setUp() throws Exception {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), MainActivity.class);
        intent.putExtra(MainActivity.FRAGMENT_ID_INTENT_EXTRA, MainActivity.FRAGMENT_ID_FLAGS);
        mRule.launchActivity(intent);
    }

    @After
    public void tearDown() {
        // Make sure to clear shared preferences between tests to avoid any saved state.
        DeveloperUiService.clearSharedPrefsForTesting(InstrumentationRegistry.getTargetContext());
    }

    private CallbackHelper getFlagUiSearchBarListener() {
        final CallbackHelper helper = new CallbackHelper();
        FlagsFragment.setFilterListener(() -> { helper.notifyCalled(); });
        return helper;
    }

    private static Matcher<View> withHintText(final Matcher<String> stringMatcher) {
        return new TypeSafeMatcher<View>() {
            @Override
            public boolean matchesSafely(View view) {
                if (!(view instanceof EditText)) {
                    return false;
                }
                String hint = ((EditText) view).getHint().toString();
                return stringMatcher.matches(hint);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("with hint text: ");
                stringMatcher.describeTo(description);
            }
        };
    }

    private static Matcher<View> withHintText(final String expectedHint) {
        return withHintText(is(expectedHint));
    }

    @IntDef({CompoundDrawable.START, CompoundDrawable.TOP, CompoundDrawable.END,
            CompoundDrawable.BOTTOM})
    private @interface CompoundDrawable {
        int START = 0;
        int TOP = 1;
        int END = 2;
        int BOTTOM = 3;
    }

    private static Matcher<View> compoundDrawableVisible(@CompoundDrawable int position) {
        return new TypeSafeMatcher<View>() {
            @Override
            public boolean matchesSafely(View view) {
                if (!(view instanceof TextView)) {
                    return false;
                }
                Drawable[] compoundDrawables = ((TextView) view).getCompoundDrawablesRelative();
                Drawable endDrawable = compoundDrawables[position];
                return endDrawable != null;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("with drawable in position " + position);
            }
        };
    }

    // Click a TextView at the start/end/top/bottom. Does not check if any CompoundDrawable drawable
    // is in that position, it just sends a touch event for those coordinates.
    private static void tapCompoundDrawableOnUiThread(
            TextView view, @CompoundDrawable int position) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            long downTime = SystemClock.uptimeMillis();
            long eventTime = downTime + 50;
            float x = view.getWidth() / 2.0f;
            float y = view.getHeight() / 2.0f;
            if (position == CompoundDrawable.START) {
                x = 0.0f;
            } else if (position == CompoundDrawable.END) {
                x = view.getWidth();
            } else if (position == CompoundDrawable.TOP) {
                y = 0.0f;
            } else if (position == CompoundDrawable.BOTTOM) {
                y = view.getHeight();
            }

            int metaState = 0; // no modifier keys (ex. alt/control), this is just a touch event
            view.dispatchTouchEvent(MotionEvent.obtain(
                    downTime, eventTime, MotionEvent.ACTION_UP, x, y, metaState));
        });
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSearchEmptyByDefault() throws Throwable {
        onView(withId(R.id.flag_search_bar)).check(matches(withText("")));
        onView(withId(R.id.flag_search_bar)).check(matches(withHintText("Search flags")));

        // Magnifier should always be visible, "clear text" icon should be hidden
        onView(withId(R.id.flag_search_bar))
                .check(matches(compoundDrawableVisible(CompoundDrawable.START)));
        onView(withId(R.id.flag_search_bar))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.END))));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSearchByName() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("logging"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(allOf(withId(R.id.flag_name), withText(AwSwitches.WEBVIEW_VERBOSE_LOGGING)))
                .check(matches(isDisplayed()));
        onView(withId(R.id.flags_list)).check(matches(withCount(1)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSearchByDescription() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("highlight the contents"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(allOf(withId(R.id.flag_name), withText(AwSwitches.HIGHLIGHT_ALL_WEBVIEWS)))
                .check(matches(isDisplayed()));
        onView(withId(R.id.flags_list)).check(matches(withCount(1)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCaseInsensitive() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("LOGGING"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(allOf(withId(R.id.flag_name), withText(AwSwitches.WEBVIEW_VERBOSE_LOGGING)))
                .check(matches(isDisplayed()));
        onView(withId(R.id.flags_list)).check(matches(withCount(1)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMultipleResults() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);
        int totalNumFlags = flagsList.getCount();

        // This assumes:
        //  * There will always be > 1 flag which mentions WebView explicitly (ex.
        //    HIGHLIGHT_ALL_WEBVIEWS and WEBVIEW_VERBOSE_LOGGING)
        //  * There will always be >= 1 flag which does not mention WebView in its description (ex.
        //    --show-composited-layer-borders).
        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("webview"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(withId(R.id.flags_list))
                .check(matches(withCount(allOf(greaterThan(1), lessThan(totalNumFlags)))));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testClearingSearchShowsAllFlags() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);
        int totalNumFlags = flagsList.getCount();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("logging"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(withId(R.id.flags_list)).check(matches(withCount(1)));

        onView(withId(R.id.flag_search_bar)).perform(replaceText(""));
        helper.waitForCallback(searchBarChangeCount, 2);
        onView(withId(R.id.flags_list)).check(matches(withCount(totalNumFlags)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testTappingClearButtonClearsText() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("logging"));
        helper.waitForCallback(searchBarChangeCount, 1);

        // "x" icon should visible if there's some text
        onView(withId(R.id.flag_search_bar))
                .check(matches(compoundDrawableVisible(CompoundDrawable.END)));

        EditText searchBar = mRule.getActivity().findViewById(R.id.flag_search_bar);
        tapCompoundDrawableOnUiThread(searchBar, CompoundDrawable.END);

        // "x" icon disappears when text is cleared
        onView(withId(R.id.flag_search_bar)).check(matches(withText("")));
        onView(withId(R.id.flag_search_bar))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.END))));

        // Magnifier icon should still be visible
        onView(withId(R.id.flag_search_bar))
                .check(matches(compoundDrawableVisible(CompoundDrawable.START)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testDeletingTextHidesClearTextButton() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("logging"));
        helper.waitForCallback(searchBarChangeCount, 1);

        // "x" icon should visible if there's some text
        onView(withId(R.id.flag_search_bar))
                .check(matches(compoundDrawableVisible(CompoundDrawable.END)));

        onView(withId(R.id.flag_search_bar)).perform(replaceText(""));

        // "x" icon disappears when text is cleared
        onView(withId(R.id.flag_search_bar)).check(matches(withText("")));
        onView(withId(R.id.flag_search_bar))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.END))));

        // Magnifier icon should still be visible
        onView(withId(R.id.flag_search_bar))
                .check(matches(compoundDrawableVisible(CompoundDrawable.START)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testElsewhereOnSearchBarDoesNotClearText() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("logging"));
        helper.waitForCallback(searchBarChangeCount, 1);

        EditText searchBar = mRule.getActivity().findViewById(R.id.flag_search_bar);
        tapCompoundDrawableOnUiThread(searchBar, CompoundDrawable.TOP);

        // EditText should not be cleared
        onView(withId(R.id.flag_search_bar)).check(matches(withText("logging")));

        // "x" icon is still visible
        onView(withId(R.id.flag_search_bar))
                .check(matches(compoundDrawableVisible(CompoundDrawable.END)));
    }

    /**
     * Toggle a flag's spinner with the given state value, and check that text changes to the
     * correct value.
     *
     * @param flagInteraction the {@link DataInteraction} object representing a flag View item via
     *         {@code onData()}.
     * @param state {@code true} for "enabled", {@code false} for "disabled", {@code null} for
     *         Default.
     * @return the same {@code flagInteraction} passed param for the ease of chaining.
     */
    private DataInteraction toggleFlag(DataInteraction flagInteraction, Boolean state) {
        String stateText = state == null ? "Default" : state ? "Enabled" : "Disabled";
        flagInteraction.onChildView(withId(R.id.flag_toggle)).perform(click());
        onData(allOf(is(instanceOf(String.class)), is(stateText))).perform(click());
        flagInteraction.onChildView(withId(R.id.flag_toggle))
                .check(matches(withSpinnerText(containsString(stateText))));

        return flagInteraction;
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testTogglingFlagShowsBlueDot() throws Throwable {
        DataInteraction flagInteraction =
                onData(anything()).inAdapterView(withId(R.id.flags_list)).atPosition(1);

        // blue dot should be hidden by default
        flagInteraction.onChildView(withId(R.id.flag_name))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.START))));

        // Test enabling flags shows a bluedot next to flag name
        toggleFlag(flagInteraction, true);
        flagInteraction.onChildView(withId(R.id.flag_name))
                .check(matches(compoundDrawableVisible(CompoundDrawable.START)));

        // Test setting to default hide the blue dot
        toggleFlag(flagInteraction, null);
        flagInteraction.onChildView(withId(R.id.flag_name))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.START))));

        // Test disabling flags shows a bluedot next to flag name
        toggleFlag(flagInteraction, false);
        flagInteraction.onChildView(withId(R.id.flag_name))
                .check(matches(compoundDrawableVisible(CompoundDrawable.START)));

        // Test setting to default again hide the blue dot
        toggleFlag(flagInteraction, null);
        flagInteraction.onChildView(withId(R.id.flag_name))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.START))));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @DisabledTest(message = "https://crbug.com/1141442")
    public void testToggledFlagsFloatToTop() throws Throwable {
        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);
        int totalNumFlags = flagsList.getCount();
        String lastFlagName = ((Flag) flagsList.getAdapter().getItem(totalNumFlags - 1)).getName();

        // Toggle the last flag in the list.
        toggleFlag(onData(anything())
                           .inAdapterView(withId(R.id.flags_list))
                           .atPosition(totalNumFlags - 1),
                true);
        // Navigate from the flags UI then back to it to trigger list sorting.
        onView(withId(R.id.navigation_home)).perform(click());
        onView(withId(R.id.navigation_flags_ui)).perform(click());

        // Check that the toggled flag is now at the top of the list. This assumes that the flags
        // list has > 2 items.
        onData(anything())
                .inAdapterView(withId(R.id.flags_list))
                .atPosition(1)
                .onChildView(withId(R.id.flag_name))
                .check(matches(withText(lastFlagName)));

        // Reset to default.
        toggleFlag(onData(anything()).inAdapterView(withId(R.id.flags_list)).atPosition(1), null);
        // Navigate from the flags UI then back to it to trigger list sorting.
        onView(withId(R.id.navigation_home)).perform(click());
        onView(withId(R.id.navigation_flags_ui)).perform(click());
        // Check that flags goes back to the end of the list when untoggled.
        onData(anything())
                .inAdapterView(withId(R.id.flags_list))
                .atPosition(totalNumFlags - 1)
                .onChildView(withId(R.id.flag_name))
                .check(matches(withText(lastFlagName)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testResetFlags() throws Throwable {
        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);
        String firstFlagName = ((Flag) flagsList.getAdapter().getItem(1)).getName();

        toggleFlag(onData(anything()).inAdapterView(withId(R.id.flags_list)).atPosition(1), true);
        Map<String, Boolean> flagOverrides =
                DeveloperModeUtils.getFlagOverrides(DeveloperUiTest.TEST_WEBVIEW_PACKAGE_NAME);
        assertThat(
                "flagOverrides map should contain exactly one entry", flagOverrides.size(), is(1));
        assertThat(flagOverrides, hasEntry(firstFlagName, true));

        onView(withId(R.id.reset_flags_button)).perform(click());

        DataInteraction flagInteraction =
                onData(anything()).inAdapterView(withId(R.id.flags_list)).atPosition(1);
        flagInteraction.onChildView(withId(R.id.flag_name))
                .check(matches(not(compoundDrawableVisible(CompoundDrawable.START))));
        flagInteraction.onChildView(withId(R.id.flag_toggle))
                .check(matches(withSpinnerText(containsString("Default"))));
        Assert.assertTrue(
                DeveloperModeUtils.getFlagOverrides(DeveloperUiTest.TEST_WEBVIEW_PACKAGE_NAME)
                        .isEmpty());
    }
}
