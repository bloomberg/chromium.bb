// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.contrib.RecyclerViewActions.actionOnItem;
import static android.support.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static android.support.test.espresso.matcher.ViewMatchers.withChild;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.waitToBeHidden;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabTestHelper.isKeyboardAccessoryTabLayout;

import android.app.Activity;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.lang.ref.WeakReference;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for autofill keyboard accessory.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@EnableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY,
        ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillKeyboardAccessoryIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    /**
     * This FakeKeyboard triggers as a regular keyboard but has no measurable height. This simulates
     * being the upper half in multi-window mode.
     */
    private static class MultiWindowKeyboard extends FakeKeyboard {
        public MultiWindowKeyboard(WeakReference<Activity> activity) {
            super(activity);
        }

        @Override
        protected int getStaticKeyboardHeight() {
            return 0;
        }
    }

    private void loadTestPage(ChromeWindow.KeyboardVisibilityDelegateFactory keyboardDelegate)
            throws InterruptedException, ExecutionException, TimeoutException {
        mHelper.loadTestPage("/chrome/test/data/autofill/autofill_test_form.html", false, false,
                keyboardDelegate);
        ManualFillingTestHelper.createAutofillTestProfiles();
        DOMUtils.waitForNonZeroNodeBounds(mHelper.getWebContents(), "NAME_FIRST");
    }

    /**
     * Autofocused fields should not show a keyboard accessory.
     */
    @Test
    @MediumTest
    public void testAutofocusedFieldDoesNotShowKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage(FakeKeyboard::new);
        CriteriaHelper.pollUiThread(() -> {
            View accessory = mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory);
            return accessory == null || !accessory.isShown();
        });
    }

    /**
     * Tapping on an input field should show a keyboard and its keyboard accessory.
     */
    @Test
    @MediumTest
    public void testTapInputFieldShowsKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage(FakeKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST");
        mHelper.waitForKeyboardAccessoryToBeShown();
    }

    /**
     * Switching fields should re-scroll the keyboard accessory to the left.
     */
    @Test
    @MediumTest
    public void testSwitchFieldsRescrollsKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage(FakeKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("EMAIL_ADDRESS");
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Scroll to the second position and check it actually happened.
        TestThreadUtils.runOnUiThreadBlocking(() -> getSuggestionsComponent().scrollToPosition(2));
        CriteriaHelper.pollUiThread(() -> {
            return getSuggestionsComponent().computeHorizontalScrollOffset() > 0;
        }, "Should keep the manual scroll position.");

        // Clicking any other node should now scroll the items back to the initial position.
        mHelper.clickNodeAndShowKeyboard("NAME_LAST");
        CriteriaHelper.pollUiThread(() -> {
            return getSuggestionsComponent().computeHorizontalScrollOffset() == 0;
        }, "Should be scrolled back to position 0.");
    }

    /**
     * Selecting a keyboard accessory suggestion should hide the keyboard and its keyboard
     * accessory.
     */
    @Test
    @MediumTest
    public void testSelectSuggestionHidesKeyboardAccessory()
            throws ExecutionException, InterruptedException, TimeoutException {
        loadTestPage(FakeKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST");
        mHelper.waitForKeyboardAccessoryToBeShown();

        CriteriaHelper.pollUiThread(() -> getFirstSuggestion() != null);
        TestThreadUtils.runOnUiThreadBlocking(() -> getFirstSuggestion().performClick());
        mHelper.waitForKeyboardAccessoryToDisappear();
    }

    @Test
    @MediumTest
    public void testSuggestionsCloseAccessoryWhenClicked()
            throws ExecutionException, InterruptedException, TimeoutException {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        loadTestPage(MultiWindowKeyboard::new);
        mHelper.clickNode("NAME_FIRST");
        mHelper.waitForKeyboardAccessoryToBeShown();

        CriteriaHelper.pollUiThread(() -> getFirstSuggestion() != null);
        TestThreadUtils.runOnUiThreadBlocking(() -> getFirstSuggestion().performClick());
        mHelper.waitForKeyboardAccessoryToDisappear();
    }

    @Test
    @SmallTest
    public void testPressingBackButtonHidesAccessoryWithAutofillSuggestions()
            throws InterruptedException, TimeoutException, ExecutionException {
        loadTestPage(MultiWindowKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST");
        mHelper.waitForKeyboardAccessoryToBeShown();

        whenDisplayed(withId(R.id.bar_items_view))
                .perform(scrollTo(isKeyboardAccessoryTabLayout()))
                .perform(actionOnItem(isKeyboardAccessoryTabLayout(), selectTabAtPosition(0)));

        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));

        assertTrue(TestThreadUtils.runOnUiThreadBlocking(
                () -> mHelper.getManualFillingCoordinator().handleBackPress()));

        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
    }

    @Test
    @MediumTest
    public void testSheetHasMinimumSizeWhenTriggeredBySuggestion()
            throws ExecutionException, InterruptedException, TimeoutException {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        loadTestPage(MultiWindowKeyboard::new);
        mHelper.clickNode("NAME_FIRST");
        mHelper.waitForKeyboardAccessoryToBeShown();

        whenDisplayed(withId(R.id.bar_items_view))
                .perform(scrollTo(isKeyboardAccessoryTabLayout()),
                        actionOnItem(isKeyboardAccessoryTabLayout(), selectTabAtPosition(0)));

        CriteriaHelper.pollUiThread(() -> {
            View sheetView =
                    mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory_sheet);
            return sheetView.isShown() && sheetView.getHeight() > 0;
        });

        // Click the back arrow.
        whenDisplayed(withId(R.id.show_keyboard)).perform(click());
        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));

        CriteriaHelper.pollUiThread(() -> {
            View sheetView =
                    mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory_sheet);
            return sheetView.getHeight() == 0 || !sheetView.isShown();
        });
    }

    private RecyclerView getSuggestionsComponent() {
        final ViewGroup keyboardAccessory = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory));
        assert keyboardAccessory != null;
        return (RecyclerView) keyboardAccessory.findViewById(R.id.bar_items_view);
    }

    private View getFirstSuggestion() {
        ViewGroup recyclerView = getSuggestionsComponent();
        assert recyclerView != null;
        return recyclerView.getChildAt(0);
    }
}
