// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.chrome.browser.modaldialog.ModalDialogTestUtils.checkCurrentPresenter;
import static org.chromium.chrome.browser.modaldialog.ModalDialogTestUtils.checkDialogDismissalCause;
import static org.chromium.chrome.browser.modaldialog.ModalDialogTestUtils.checkPendingSize;
import static org.chromium.chrome.browser.modaldialog.ModalDialogTestUtils.createDialog;
import static org.chromium.chrome.browser.modaldialog.ModalDialogTestUtils.showDialog;

import android.support.test.espresso.Espresso;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link AppModalPresenter}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AppModalPresenterTest {
    private class TestObserver implements ModalDialogTestUtils.TestDialogDismissedObserver {
        public final CallbackHelper onDialogDismissedCallback = new CallbackHelper();

        @Override
        public void onDialogDismissed(int dismissalCause) {
            onDialogDismissedCallback.notifyCalled();
            checkDialogDismissalCause(mExpectedDismissalCause, dismissalCause);
        }
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ChromeTabbedActivity mActivity;
    private ModalDialogManager mManager;
    private TestObserver mTestObserver;
    private Integer mExpectedDismissalCause;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mManager = mActivity.getModalDialogManager();
        mTestObserver = new TestObserver();
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDismiss_BackPressed() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, "1", null);
        PropertyModel dialog2 = createDialog(mActivity, "2", null);

        // Initially there are no dialogs in the pending list. Browser controls are not restricted.
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        checkCurrentPresenter(mManager, null);

        // Add three dialogs available for showing. The app modal dialog should be shown first.
        showDialog(mManager, dialog1, ModalDialogType.APP);
        showDialog(mManager, dialog2, ModalDialogType.APP);
        checkPendingSize(mManager, ModalDialogType.APP, 1);
        onView(withText("1")).check(matches(isDisplayed()));
        checkCurrentPresenter(mManager, ModalDialogType.APP);

        // Perform back press. The first app modal dialog should be dismissed, and the second one
        // should be shown.
        Espresso.pressBack();
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        onView(withText("1")).check(doesNotExist());
        onView(withText("2")).check(matches(isDisplayed()));
        checkCurrentPresenter(mManager, ModalDialogType.APP);

        // Perform a second back press. The second app modal dialog should be dismissed.
        Espresso.pressBack();
        checkPendingSize(mManager, ModalDialogType.APP, 0);
        onView(withText("2")).check(doesNotExist());
        checkCurrentPresenter(mManager, null);
    }

    @Test
    @SmallTest
    @Feature({"ModalDialog"})
    public void testDismiss_DismissalCause_BackPressed() throws Exception {
        PropertyModel dialog1 = createDialog(mActivity, "1", mTestObserver);
        mExpectedDismissalCause = DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE;

        showDialog(mManager, dialog1, ModalDialogType.APP);

        // Dismiss the tab modal dialog and verify dismissal cause.
        int callCount = mTestObserver.onDialogDismissedCallback.getCallCount();
        Espresso.pressBack();
        mTestObserver.onDialogDismissedCallback.waitForCallback(callCount);

        mExpectedDismissalCause = null;
    }
}
