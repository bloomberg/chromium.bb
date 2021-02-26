// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sms;

import android.widget.EditText;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.components.browser_ui.sms.WebOTPServiceInfoBar;
import org.chromium.components.browser_ui.sms.WebOTPServiceUma;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;

/**
 * Tests for the WebOTPServiceInfoBar class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebOTPServiceInfoBarTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ChromeActivity mActivity;
    private static final String INFOBAR_HISTOGRAM = "Blink.Sms.Receive.Infobar";
    private static final String TIME_CANCEL_ON_KEYBOARD_DISMISSAL_HISTOGRAM =
            "Blink.Sms.Receive.TimeCancelOnKeyboardDismissal";

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
    }

    private WebOTPServiceInfoBar createInfoBar() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Tab tab = mActivity.getActivityTab();
            WebOTPServiceInfoBar infoBar = WebOTPServiceInfoBar.create(
                    mActivity.getWindowAndroid(), /*enumeratedIconId=*/0, "title", "message", "ok");
            InfoBarContainer.get(tab).addInfoBarForTesting(infoBar);
            return infoBar;
        });
    }

    private void assertHistogramRecordedCount(String name, int expectedCount) {
        Assert.assertEquals(expectedCount, RecordHistogram.getHistogramTotalCountForTesting(name));
    }

    private void assertHistogramRecordedCount(String name, int sample, int expectedCount) {
        Assert.assertEquals(
                expectedCount, RecordHistogram.getHistogramValueCountForTesting(name, sample));
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "UiCatalogue"})
    public void testSmsInfoBarOk() {
        WebOTPServiceInfoBar infoBar = createInfoBar();

        Assert.assertFalse(InfoBarUtil.hasSecondaryButton(infoBar));

        // Click primary button.
        Assert.assertTrue(InfoBarUtil.clickPrimaryButton(infoBar));

        assertHistogramRecordedCount(INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.SHOWN, 1);
        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.KEYBOARD_DISMISSED, 0);
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "UiCatalogue"})
    public void testSmsInfoBarClose() {
        WebOTPServiceInfoBar infoBar = createInfoBar();

        Assert.assertFalse(InfoBarUtil.hasSecondaryButton(infoBar));

        // Close infobar.
        Assert.assertTrue(InfoBarUtil.clickCloseButton(infoBar));

        assertHistogramRecordedCount(INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.SHOWN, 1);
        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.KEYBOARD_DISMISSED, 0);
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "UiCatalogue"})
    public void testHideKeyboardWhenInfoBarIsShown() {
        KeyboardVisibilityDelegate keyboardVisibilityDelegate =
                mActivityTestRule.getKeyboardDelegate();
        EditText editText = new EditText(mActivity);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.setContentView(editText);
            editText.requestFocus();
            keyboardVisibilityDelegate.showKeyboard(editText);
        });

        // Wait until the keyboard is showing.
        CriteriaHelper.pollUiThread(
                () -> keyboardVisibilityDelegate.isKeyboardShowing(mActivity, editText));

        WebOTPServiceInfoBar infoBar = createInfoBar();

        // Keyboard is hidden after info bar is created and shown.
        CriteriaHelper.pollUiThread(
                () -> !keyboardVisibilityDelegate.isKeyboardShowing(mActivity, editText));

        assertHistogramRecordedCount(INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.SHOWN, 1);
        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.KEYBOARD_DISMISSED, 1);
        assertHistogramRecordedCount(TIME_CANCEL_ON_KEYBOARD_DISMISSAL_HISTOGRAM, 0);
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "UiCatalogue"})
    public void testUMARecordedWhenInfobarDismissedAfterHidingKeyboard() {
        KeyboardVisibilityDelegate keyboardVisibilityDelegate =
                mActivityTestRule.getKeyboardDelegate();
        EditText editText = new EditText(mActivity);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.setContentView(editText);
            editText.requestFocus();
            keyboardVisibilityDelegate.showKeyboard(editText);
        });

        // Wait until the keyboard is showing.
        CriteriaHelper.pollUiThread(
                () -> keyboardVisibilityDelegate.isKeyboardShowing(mActivity, editText));

        WebOTPServiceInfoBar infoBar = createInfoBar();

        // Keyboard is hidden after info bar is created and shown.
        CriteriaHelper.pollUiThread(
                () -> !keyboardVisibilityDelegate.isKeyboardShowing(mActivity, editText));

        // Close info bar.
        InfoBarUtil.clickCloseButton(infoBar);

        assertHistogramRecordedCount(INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.SHOWN, 1);
        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.KEYBOARD_DISMISSED, 1);
        assertHistogramRecordedCount(TIME_CANCEL_ON_KEYBOARD_DISMISSAL_HISTOGRAM, 1);
    }
}
