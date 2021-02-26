// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.support.test.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.components.browser_ui.modaldialog.ModalDialogTestUtils;
import org.chromium.components.browser_ui.modaldialog.R;
import org.chromium.components.browser_ui.modaldialog.TabModalPresenter;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/**
 * TestRule for permissions UI testing on Android.
 *
 * This class allows for easy testing of permissions infobar and dialog prompts. Writing a test
 * simply requires a HTML file containing JavaScript methods which trigger a permission prompt. The
 * methods should update the page's title with <prefix>: <count>, where <count> is the number of
 * updates expected (usually 1, although some APIs like Geolocation's watchPosition may trigger
 * callbacks repeatedly).
 *
 * Subclasses may then call runAllowTest to start a test server, navigate to the provided HTML page,
 * and run the JavaScript method. The permission will be granted, and the test will verify that the
 * page title is updated as expected.
 *
 * runAllowTest has several parameters to specify the conditions of the test, including whether
 * a persistence toggle is expected, whether it should be explicitly toggled, whether to trigger the
 * JS call with a gesture, and whether an infobar or a dialog is expected.
 */
public class PermissionTestRule extends ChromeTabbedActivityTestRule {
    private InfoBarTestAnimationListener mListener;
    private EmbeddedTestServer mTestServer;
    private boolean mUseHttpsServer;

    /**
     * Waits till a JavaScript callback which updates the page title is called the specified number
     * of times. The page title is expected to be of the form <prefix>: <count>.
     */
    public static class PermissionUpdateWaiter extends EmptyTabObserver {
        private CallbackHelper mCallbackHelper;
        private String mPrefix;
        private String mExpectedTitle;
        private final ChromeActivity mActivity;

        public PermissionUpdateWaiter(String prefix, ChromeActivity activity) {
            mCallbackHelper = new CallbackHelper();
            mPrefix = prefix;
            mActivity = activity;
        }

        @Override
        public void onTitleUpdated(Tab tab) {
            if (ChromeTabUtils.getTitleOnUiThread(mActivity.getActivityTab())
                            .equals(mExpectedTitle)) {
                mCallbackHelper.notifyCalled();
            }
        }

        /**
         * Wait for the page title to reach the expected number of updates. The page is expected to
         * update the title like so: `prefix` + numUpdates. In essence this waits for the page to
         * update the title to match, and does not actually count page title updates.
         * @param numUpdates The number that should be after the prefix for the wait to be over. `0`
         *         to only wait for the prefix.
         * @throws Exception
         */
        public void waitForNumUpdates(int numUpdates) throws Exception {
            // Update might have already happened, check before waiting for title udpdates.
            mExpectedTitle = mPrefix;
            if (numUpdates != 0) mExpectedTitle += numUpdates;
            if (ChromeTabUtils.getTitleOnUiThread(mActivity.getActivityTab())
                            .equals(mExpectedTitle)) {
                return;
            }

            mCallbackHelper.waitForCallback(0);
        }
    }

    @Override
    public Statement apply(final Statement base, Description desc) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                ruleSetUp();
                base.evaluate();
                ruleTearDown();
            }
        }, desc);
    }

    public PermissionTestRule() {
        this(false);
    }

    public PermissionTestRule(boolean useHttpsServer) {
        mUseHttpsServer = useHttpsServer;
    }

    private void ruleSetUp() {
        // TODO(https://crbug.com/867446): Refactor to use EmbeddedTestServerRule.
        mTestServer = mUseHttpsServer
                ? EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getContext(), ServerCertificate.CERT_OK)
                : EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    /**
     * Starts an activity and listens for info-bars appearing/disappearing.
     */
    void setUpActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
        mListener = new InfoBarTestAnimationListener();
        getInfoBarContainer().addAnimationListener(mListener);
    }

    private void ruleTearDown() {
        mTestServer.stopAndDestroyServer();
    }

    public void setUpUrl(final String url) {
        loadUrl(getURL(url));
    }

    public String getURL(String url) {
        return mTestServer.getURL(url);
    }

    public String getOrigin() {
        return mTestServer.getURL("/");
    }

    public String getURLWithHostName(String hostName, String url) {
        return mTestServer.getURLWithHostName(hostName, url);
    }

    /**
     * Runs a permission prompt test that grants the permission and expects the page title to be
     * updated in response.
     * @param updateWaiter  The update waiter to wait for callbacks. Should be added as an observer
     *                      to the current tab prior to calling this method.
     * @param javascript    The JS function to run in the current tab to execute the test and update
     *                      the page title.
     * @param nUpdates      How many updates of the page title to wait for.
     * @param withGeature   True if we require a user gesture to trigger the prompt.
     * @param isDialog      True if we are expecting a permission dialog, false for an infobar.
     * @throws Exception
     */
    public void runAllowTest(PermissionUpdateWaiter updateWaiter, final String url,
            String javascript, int nUpdates, boolean withGesture, boolean isDialog)
            throws Exception {
        setUpUrl(url);
        if (withGesture) {
            runJavaScriptCodeInCurrentTabWithGesture(javascript);
        } else {
            runJavaScriptCodeInCurrentTab(javascript);
        }
        replyToPromptAndWaitForUpdates(updateWaiter, true, nUpdates, isDialog);
    }

    /**
     * Runs a permission prompt test that does not grant the permission and expects the page title
     * to be updated in response.
     * @param updateWaiter  The update waiter to wait for callbacks. Should be added as an observer
     *                      to the current tab prior to calling this method.
     * @param javascript    The JS function to run in the current tab to execute the test and update
     *                      the page title.
     * @param nUpdates      How many updates of the page title to wait for.
     * @param withGesture   True if we require a user gesture to trigger the prompt.
     * @param isDialog      True if we are expecting a permission dialog, false for an infobar.
     * @throws Exception
     */
    public void runDenyTest(PermissionUpdateWaiter updateWaiter, final String url,
            String javascript, int nUpdates, boolean withGesture, boolean isDialog)
            throws Exception {
        setUpUrl(url);
        if (withGesture) {
            runJavaScriptCodeInCurrentTabWithGesture(javascript);
        } else {
            runJavaScriptCodeInCurrentTab(javascript);
        }
        replyToPromptAndWaitForUpdates(updateWaiter, false, nUpdates, isDialog);
    }

    /**
     * Runs a permission prompt test that expects no prompt to be displayed because the permission
     * is already granted/blocked and expects the page title to be updated.
     * @param updateWaiter  The update waiter to wait for callbacks. Should be added as an observer
     *                      to the current tab prior to calling this method.
     * @param javascript    The JS function to run in the current tab to execute the test and update
     *                      the page title.
     * @param nUpdates      How many updates of the page title to wait for.
     * @param withGesture   True if we require a user gesture.
     * @param isDialog      True if we are testing a permission dialog, false for an infobar.
     * @throws Exception
     */
    public void runNoPromptTest(PermissionUpdateWaiter updateWaiter, final String url,
            String javascript, int nUpdates, boolean withGesture, boolean isDialog)
            throws Exception {
        setUpUrl(url);
        if (withGesture) {
            runJavaScriptCodeInCurrentTabWithGesture(javascript);
        } else {
            runJavaScriptCodeInCurrentTab(javascript);
        }
        waitForUpdatesAndAssertNoPrompt(updateWaiter, nUpdates, isDialog);
    }

    private void replyToPromptAndWaitForUpdates(PermissionUpdateWaiter updateWaiter, boolean allow,
            int nUpdates, boolean isDialog) throws Exception {
        if (isDialog) {
            waitForDialogShownState(true);
            replyToDialogAndWaitForUpdates(updateWaiter, nUpdates, allow);
        } else {
            replyToInfoBarAndWaitForUpdates(updateWaiter, nUpdates, allow);
        }
    }

    private void waitForUpdatesAndAssertNoPrompt(
            PermissionUpdateWaiter updateWaiter, int nUpdates, boolean isDialog) throws Exception {
        updateWaiter.waitForNumUpdates(nUpdates);

        if (isDialog) {
            Assert.assertFalse("Modal permission prompt shown when none expected",
                    PermissionDialogController.getInstance().isDialogShownForTest());
        } else {
            Assert.assertEquals(
                    "Permission infobar shown when none expected", getInfoBars().size(), 0);
        }
    }

    private void runJavaScriptCodeInCurrentTabWithGesture(String javascript)
            throws java.util.concurrent.TimeoutException {
        runJavaScriptCodeInCurrentTab("functionToRun = '" + javascript + "'");
        TouchCommon.singleClickView(getActivity().getActivityTab().getView());
    }

    /**
     * Replies to an infobar permission prompt and waits for a provided number
     * of updates to the page title in response.
     */
    private void replyToInfoBarAndWaitForUpdates(
            PermissionUpdateWaiter updateWaiter, int nUpdates, boolean allow) throws Exception {
        mListener.addInfoBarAnimationFinished("InfoBar not added.");
        InfoBar infobar = getInfoBars().get(0);
        Assert.assertNotNull(infobar);

        if (allow) {
            Assert.assertTrue("Allow button wasn't found", InfoBarUtil.clickPrimaryButton(infobar));
        } else {
            Assert.assertTrue(
                    "Block button wasn't found", InfoBarUtil.clickSecondaryButton(infobar));
        }
        updateWaiter.waitForNumUpdates(nUpdates);
    }

    /**
     * Replies to a dialog permission prompt and waits for a provided number of
     * updates to the page title in response.
     */
    private void replyToDialogAndWaitForUpdates(
            PermissionUpdateWaiter updateWaiter, int nUpdates, boolean allow) throws Exception {
        replyToDialog(allow, getActivity());
        updateWaiter.waitForNumUpdates(nUpdates);
    }

    /**
     * Verify the shown state of the dialog.
     */
    protected void waitForDialogShownState(boolean expectedShowState) {
        waitForDialogShownState(getActivity(), expectedShowState);
    }

    /**
     * Utility functions to support permissions testing in other contexts.
     */
    public static void replyToDialog(boolean allow, ChromeActivity activity) {
        // Wait a tiny bit before clicking the dialog. Sometimes the click happens too quick and the
        // dialog is not ready. See crbug.com/1098806 for example flaky tests.
        try {
            Thread.sleep(300);
        } catch (Exception ex) {
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabModalPresenter presenter = (TabModalPresenter) activity.getModalDialogManager()
                                                  .getCurrentPresenterForTest();
            TouchCommon.singleClickView(presenter.getDialogContainerForTest().findViewById(
                    allow ? R.id.positive_button : R.id.negative_button));
        });
    }

    /**
     * Wait for the permission dialog to be in the expected shown state.
     */
    public static void waitForDialogShownState(ChromeActivity activity, boolean expectedShowState) {
        ModalDialogManager dialogManager =
                TestThreadUtils.runOnUiThreadBlockingNoException(activity::getModalDialogManager);
        CriteriaHelper.pollUiThread(() -> {
            boolean isDialogShownForTest =
                    PermissionDialogController.getInstance().isDialogShownForTest();
            if (isDialogShownForTest) {
                ModalDialogTestUtils.checkCurrentPresenter(dialogManager, ModalDialogType.TAB);
            }
            Criteria.checkThat(isDialogShownForTest, Matchers.is(expectedShowState));
        });
    }

    /**
     * Wait for the permission dialog to be shown.
     */
    public static void waitForDialog(ChromeActivity activity) {
        waitForDialogShownState(activity, true);
    }
}
