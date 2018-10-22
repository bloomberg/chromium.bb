// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.assertThat;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertNotNull;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;

import android.support.test.espresso.Espresso;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.ui.DropdownPopupWindowInterface;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests for password accessory views. This integration test currently stops testing at
 * the bridge - ideally, there should be an easy way to add a temporary account with temporary
 * passwords.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManualFillingIntegrationTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    @Test
    @SmallTest
    public void testAccessoryIsAvailable() throws InterruptedException {
        mHelper.loadTestPage(false);
        mHelper.createTestTab();

        assertNotNull("Controller for Manual filling should be available.",
                mActivityTestRule.getActivity().getManualFillingController());
        assertNotNull("Keyboard accessory should have an instance.",
                mActivityTestRule.getActivity()
                        .getManualFillingController()
                        .getKeyboardAccessory());
        assertNotNull("Accessory Sheet should have an instance.",
                mActivityTestRule.getActivity()
                        .getManualFillingController()
                        .getMediatorForTesting()
                        .getAccessorySheet());
    }

    @Test
    @SmallTest
    public void testKeyboardAccessoryHiddenUntilKeyboardShows()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.createTestTab();

        // Focus the field to bring up the accessory.
        onView(withId(R.id.keyboard_accessory)).check(doesNotExist());
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Check that ONLY the accessory is there but the sheet is still hidden.
        whenDisplayed(withId(R.id.keyboard_accessory));
        onView(withId(R.id.keyboard_accessory_sheet)).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testKeyboardAccessoryDisappearsWithKeyboard()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.createTestTab();

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        whenDisplayed(withId(R.id.keyboard_accessory));

        // Dismiss the keyboard to hide the accessory again.
        mHelper.clickSubmit();
        mHelper.waitForKeyboardToDisappear();
    }

    @Test
    @SmallTest
    public void testAccessorySheetHiddenUntilManuallyTriggered()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.createTestTab();

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Check that ONLY the accessory is there but the sheet is still hidden.
        whenDisplayed(withId(R.id.keyboard_accessory));
        onView(withId(R.id.keyboard_accessory_sheet)).check(doesNotExist());

        // Trigger the sheet and wait for it to open and the keyboard to disappear.
        onView(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));
    }

    @Test
    @SmallTest
    public void testAccessorySheetHiddenWhenRefocusingField()
            throws InterruptedException, TimeoutException {
        AtomicReference<ViewGroup.MarginLayoutParams> accessoryMargins = new AtomicReference<>();
        AtomicReference<View> accessorySheetView = new AtomicReference<>();
        mHelper.loadTestPage(false);
        mHelper.createTestTab();

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Check that ONLY the accessory is there but the sheet is still hidden.
        whenDisplayed(withId(R.id.keyboard_accessory)).check((view, e) -> {
            accessoryMargins.set((ViewGroup.MarginLayoutParams) view.getLayoutParams());
            assertThat(accessoryMargins.get().bottomMargin, is(0)); // Attached to keyboard.
        });
        onView(withId(R.id.keyboard_accessory_sheet)).check(doesNotExist());

        // Trigger the sheet and wait for it to open and the keyboard to disappear.
        onView(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet)).check((view, e) -> {
            accessorySheetView.set(view);
        });
        // The accessory bar is now pushed up by the accessory.
        CriteriaHelper.pollUiThread(() -> {
            return accessoryMargins.get().bottomMargin == accessorySheetView.get().getHeight();
        });

        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory_sheet));
        CriteriaHelper.pollUiThread(() -> accessoryMargins.get().bottomMargin == 0);
    }

    @Test
    @SmallTest
    public void testHidingSheetBringsBackKeyboard() throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.createTestTab();

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));

        // Click the tab again to hide the sheet and show the keyboard.
        onView(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboard();
        onView(withId(R.id.keyboard_accessory)).check(matches(isDisplayed()));
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory_sheet));
    }

    @Test
    @SmallTest
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
    public void testOpeningSheetDismissesAutofill()
            throws InterruptedException, TimeoutException, ExecutionException {
        mHelper.loadTestPage(false);
        new AutofillTestHelper().setProfile(new PersonalDataManager.AutofillProfile("",
                "https://www.example.com/", "Alan Turing", "", "Street Ave 4", "", "Capitaltown",
                "", "80666", "", "Disneyland", "1", "a.turing@enigma.com", "DE"));
        mHelper.createTestTab();

        // Focus the field to bring up the accessory.
        mHelper.clickEmailField();
        mHelper.waitForKeyboard();
        DropdownPopupWindowInterface popup = mHelper.waitForAutofillPopup();

        assertThat(popup.isShowing(), is(true));

        // Click the tab to show the sheet and hide keyboard and popup.
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));

        assertThat(popup.isShowing(), is(false));
    }

    @Test
    @SmallTest
    public void testInvokingTabSwitcherHidesAccessory()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.createTestTab();

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getLayoutManager().showOverview(false);
            mActivityTestRule.getActivity().getLayoutManager().hideOverview(false);
        });

        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory_sheet));
    }

    @Test
    @SmallTest
    public void testResumingTheAppDismissesAllInputMethods()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.createTestTab();

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));

        // Simulate backgrounding the main activity.
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().onPauseWithNative(); });

        // This should completely dismiss any input method.
        mHelper.waitForKeyboardToDisappear();
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory_sheet));
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory));

        // Simulate foregrounding the main activity.
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().onResumeWithNative(); });

        // Clicking the field should bring it back up
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));
    }

    @Test
    @SmallTest
    public void testPressingBackButtonHidesAccessorySheet()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);
        mHelper.createTestTab();

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));

        Espresso.pressBack();

        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory_sheet));
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory));
    }

    @Test
    @SmallTest
    public void testInfobarStaysHiddenWhenOpeningSheet()
            throws InterruptedException, TimeoutException {
        mHelper.loadTestPage(false);

        // TODO Create an infobar
        InfoBarContainer container =
                mActivityTestRule.getActivity().getActivityTab().getInfoBarContainer();
        InfoBarTestAnimationListener mListener = new InfoBarTestAnimationListener();
        container.addAnimationListener(mListener);
        final SimpleConfirmInfoBarBuilder.Listener testListener =
                new SimpleConfirmInfoBarBuilder.Listener() {
                    @Override
                    public void onInfoBarDismissed() {}
                    @Override
                    public boolean onInfoBarButtonClicked(boolean isPrimary) {
                        return false;
                    }
                };
        final String kInfoBarText = "SomeInfoBar";
        ThreadUtils.runOnUiThread(() -> {
            SimpleConfirmInfoBarBuilder.create(mActivityTestRule.getActivity().getActivityTab(),
                    testListener, InfoBarIdentifier.DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID, 0,
                    kInfoBarText, null, null, false);
        });
        mListener.addInfoBarAnimationFinished("InfoBar not added.");

        mHelper.createTestTab();
        whenDisplayed(withText(kInfoBarText));

        // Focus the field to bring up the accessory.
        mHelper.clickPasswordField();
        mHelper.waitForKeyboard();
        assertThat(mActivityTestRule.getActivity()
                           .getActivityTab()
                           .getInfoBarContainer()
                           .getVisibility(),
                is(not(View.VISIBLE)));

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.tabs)).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.keyboard_accessory_sheet));
        assertThat(mActivityTestRule.getActivity()
                           .getActivityTab()
                           .getInfoBarContainer()
                           .getVisibility(),
                is(not(View.VISIBLE)));
        Espresso.pressBack();

        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory_sheet));
        mHelper.waitToBeHidden(withId(R.id.keyboard_accessory));

        whenDisplayed(withText(kInfoBarText));
    }
}
