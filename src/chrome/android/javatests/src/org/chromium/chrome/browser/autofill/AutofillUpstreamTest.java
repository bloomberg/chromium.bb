// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.AutofillSaveCardInfoBar;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the Autofill Upstream and Expiration Date Fix Flow.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=AutofillUpstream"})
// Message UI is an alternative to infobar UI. Infobar UI is disabled when message UI is enabled.
@DisableFeatures({ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE})
public class AutofillUpstreamTest {
    private static final String TEST_SERVER_DIR = "components/test/data/autofill";
    private static final String TEST_FORM_URL = "/credit_card_upload_form_address_and_cc.html";
    private static final String SAVE_BUTTON_LABEL = "Save";
    private static final String CONTINUE_BUTTON_LABEL = "Continue";

    @Rule
    public SyncTestRule mActivityTestRule = new SyncTestRule();

    private EmbeddedTestServer mServer;

    @Before
    public void setUp() {
        mActivityTestRule.setUpAccountAndEnableSyncForTesting();
        mServer = new EmbeddedTestServer();
        mServer.initializeNative(InstrumentationRegistry.getContext(),
                EmbeddedTestServer.ServerHTTPSSetting.USE_HTTP);
        mServer.addDefaultHandlers(TEST_SERVER_DIR);
        mServer.start();
    }

    @After
    public void tearDown() {
        mServer.stopAndDestroyServer();
    }

    private void assertPrimaryButtonLabel(String buttonLabel) {
        InfoBarLayout view = (InfoBarLayout) getAutofillSaveCardInfoBar().getView();
        ButtonCompat primaryButton = view.getPrimaryButton();
        Assert.assertEquals(buttonLabel, primaryButton.getText().toString());
    }

    private void waitForSaveCardInfoBar() {
        CriteriaHelper.pollUiThread(
                ()
                        -> hasAutofillSaveCardInfobar(mActivityTestRule.getInfoBars()),
                "Autofill Save Card Infobar view was never added.");
    }

    private boolean hasAutofillSaveCardInfobar(List<InfoBar> infobars) {
        return (infobars != null && infobars.size() == 1
                && infobars.get(0) instanceof AutofillSaveCardInfoBar);
    }

    private AutofillSaveCardInfoBar getAutofillSaveCardInfoBar() {
        List<InfoBar> infobars = mActivityTestRule.getInfoBars();
        if (hasAutofillSaveCardInfobar(infobars)) {
            return (AutofillSaveCardInfoBar) infobars.get(0);
        }
        Assert.fail("Save card infobar not found");
        return null;
    }

    private PropertyModel getPropertyModelForDialog() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> mActivityTestRule.getActivity()
                                   .getModalDialogManager()
                                   .getCurrentDialogForTest());
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testSaveCardInfoBarWithAllFieldsFilled() throws TimeoutException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardInfoBar();

        assertPrimaryButtonLabel(SAVE_BUTTON_LABEL);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testSaveCardInfoBarWithEmptyMonth() throws TimeoutException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the month field
        DOMUtils.clickNode(webContents, "clear_month");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardInfoBar();

        assertPrimaryButtonLabel(CONTINUE_BUTTON_LABEL);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testSaveCardInfoBarWithEmptyYear() throws TimeoutException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the year field
        DOMUtils.clickNode(webContents, "clear_year");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardInfoBar();

        assertPrimaryButtonLabel(CONTINUE_BUTTON_LABEL);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testSaveCardInfoBarWithEmptyMonthAndYear() throws TimeoutException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the month and year field
        DOMUtils.clickNode(webContents, "clear_expiration_date");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardInfoBar();

        assertPrimaryButtonLabel(CONTINUE_BUTTON_LABEL);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testSaveCardInfoBarContinueButton_EmptyExpDate_launchesExpDateFixFlow()
            throws TimeoutException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the month and year field
        DOMUtils.clickNode(webContents, "clear_expiration_date");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardInfoBar();
        // Click on the continue button
        TestThreadUtils.runOnUiThreadBlocking(
                () -> getAutofillSaveCardInfoBar().onButtonClicked(true));
        PropertyModel fixflowPromptPropertyModel = getPropertyModelForDialog();

        // Verify that dialog is not null
        Assert.assertNotNull(fixflowPromptPropertyModel);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testSaveCardInfoBarWithEmptyName() throws TimeoutException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the name field
        DOMUtils.clickNode(webContents, "clear_name");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardInfoBar();

        assertPrimaryButtonLabel(CONTINUE_BUTTON_LABEL);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testSaveCardInfoBarContinueButton_EmptyName_launchesNameFixFlow()
            throws TimeoutException {
        mActivityTestRule.loadUrl(mServer.getURL(TEST_FORM_URL));
        final WebContents webContents = mActivityTestRule.getActivity().getCurrentWebContents();

        DOMUtils.clickNode(webContents, "fill_form");
        // Clear the name field
        DOMUtils.clickNode(webContents, "clear_name");
        DOMUtils.clickNode(webContents, "submit");
        waitForSaveCardInfoBar();
        // Click on the continue button
        TestThreadUtils.runOnUiThreadBlocking(
                () -> getAutofillSaveCardInfoBar().onButtonClicked(true));
        PropertyModel fixflowPromptPropertyModel = getPropertyModelForDialog();

        // Verify that dialog is not null
        Assert.assertNotNull(fixflowPromptPropertyModel);
    }
}
