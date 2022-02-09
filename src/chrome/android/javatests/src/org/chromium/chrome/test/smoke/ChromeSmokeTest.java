// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.smoke;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiApplicationTestRule;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.pagecontroller.utils.UiLocatorHelper;

import java.util.Arrays;
import java.util.concurrent.Callable;

/**
 * Smoke Test for Chrome Android.
 */
@LargeTest
@RunWith(BaseJUnit4ClassRunner.class)
public class ChromeSmokeTest {
    private static final String DATA_URL = "data:,Hello";
    private static final String ACTIVITY_NAME = "org.chromium.chrome.browser.ChromeTabbedActivity";

    public static final long TIMEOUT_MS = 20000L;
    public static final long UI_CHECK_INTERVAL = 1000L;
    private String mPackageName;

    private static Runnable toNotSatisfiedRunnable(
            Callable<Boolean> criteria, String failureReason) {
        return () -> {
            try {
                boolean isSatisfied = criteria.call();
                Criteria.checkThat(failureReason, isSatisfied, Matchers.is(true));
            } catch (RuntimeException e) {
                throw e;
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        };
    }

    private void waitUntilAnyVisible(IUi2Locator... locators) {
        CriteriaHelper.pollInstrumentationThread(
                toNotSatisfiedRunnable(
                        ()
                                -> {
                            for (IUi2Locator locator : locators) {
                                if (UiAutomatorUtils.getInstance().getLocatorHelper().isOnScreen(
                                            locator)) {
                                    return true;
                                }
                            }
                            return false;
                        },
                        "One of " + Arrays.toString(locators) + " should have been visible."),
                TIMEOUT_MS, UI_CHECK_INTERVAL);
    }

    private void navigateThroughFRE() {
        // Used in ToSAndUMAFirstRunFragment FRE page.
        IUi2Locator termsAcceptButton = Ui2Locators.withAnyResEntry(R.id.terms_accept);

        // Used in SyncConsentFirstRunFragment FRE page.
        IUi2Locator noAddAccountButton = Ui2Locators.withAnyResEntry(R.id.negative_button);

        // Used in SigninFirstRunFragment FRE page.
        IUi2Locator signinSkipButton = Ui2Locators.withAnyResEntry(R.id.signin_fre_dismiss_button);
        IUi2Locator signinContinueButton =
                Ui2Locators.withAnyResEntry(R.id.signin_fre_continue_button);

        // Used in DataReductionProxyFirstRunFragment FRE page.
        IUi2Locator dataSaverPromoNextButton = Ui2Locators.withAnyResEntry(R.id.next_button);

        // Used in DefaultSearchEngineFirstRunFragment FRE page.
        IUi2Locator defaultSearchEngineNextButton =
                Ui2Locators.withAnyResEntry(R.id.button_primary);

        // Url bar shown after the FRE is over.
        IUi2Locator urlBar = Ui2Locators.withAnyResEntry(R.id.url_bar);

        // When Play services is too old, android shows an alert.
        IUi2Locator updatePlayServicesPanel = Ui2Locators.withResName("android:id/parentPanel");
        IUi2Locator playServicesUpdateText =
                Ui2Locators.withTextContaining("update Google Play services");

        UiLocatorHelper uiLocatorHelper = UiAutomatorUtils.getInstance().getLocatorHelper();

        // These locators show up in one FRE page or another
        IUi2Locator[] frePageDetectors = new IUi2Locator[] {
                playServicesUpdateText,
                termsAcceptButton,
                signinSkipButton,
                signinContinueButton,
                noAddAccountButton,
                dataSaverPromoNextButton,
                defaultSearchEngineNextButton,
                urlBar,
        };

        // Manually go through FRE.
        while (true) {
            // Wait for an FRE page to show up.
            waitUntilAnyVisible(frePageDetectors);
            // If the update play services alert is visible, dismiss it.
            if (uiLocatorHelper.isOnScreen(playServicesUpdateText)) {
                UiAutomatorUtils.getInstance().clickOutsideOf(updatePlayServicesPanel);
                // Different FRE versions show up randomly and in different order,
                // figure out which one we are on and proceed.
            } else if (uiLocatorHelper.isOnScreen(termsAcceptButton)) {
                // Click on the accept terms in FRE.
                UiAutomatorUtils.getInstance().click(termsAcceptButton);
            } else if (uiLocatorHelper.isOnScreen(noAddAccountButton)) {
                // Do not add an account.
                UiAutomatorUtils.getInstance().click(noAddAccountButton);
            } else if (uiLocatorHelper.isOnScreen(signinSkipButton)) {
                // Do not sign in with an account.
                UiAutomatorUtils.getInstance().click(signinSkipButton);
            } else if (uiLocatorHelper.isOnScreen(signinContinueButton)) {
                // Sometimes there is only the continue button (eg: when signin is
                // disabled.)
                UiAutomatorUtils.getInstance().click(signinContinueButton);
            } else if (uiLocatorHelper.isOnScreen(dataSaverPromoNextButton)) {
                // Just press next on Data saver promo.
                UiAutomatorUtils.getInstance().click(dataSaverPromoNextButton);
            } else if (uiLocatorHelper.isOnScreen(defaultSearchEngineNextButton)) {
                // Just press next on choosing the default SE.
                UiAutomatorUtils.getInstance().click(defaultSearchEngineNextButton);
            } else if (uiLocatorHelper.isOnScreen(urlBar)) {
                // FRE is over.
                break;
            } else {
                throw new RuntimeException("Unexpected FRE or Start page detected.");
            }
        }
    }

    @Before
    public void setUp() throws Exception {
        mPackageName = InstrumentationRegistry.getArguments().getString(
                ChromeUiApplicationTestRule.PACKAGE_NAME_ARG, "org.chromium.chrome");
    }

    @Test
    @DisabledTest(message = "https://crbug.com/1289733")
    public void testHello() {
        Context context = InstrumentationRegistry.getContext();
        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(DATA_URL));
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setComponent(new ComponentName(mPackageName, ACTIVITY_NAME));
        context.startActivity(intent);

        // Looks for the any view/layout with the chrome package name.
        IUi2Locator locatorChrome = Ui2Locators.withPackageName(mPackageName);
        // Wait until chrome shows up
        waitUntilAnyVisible(locatorChrome);

        // Go through the FRE until you see ChromeTabbedActivity urlbar.
        navigateThroughFRE();

        // FRE should be over and we should be shown the url we requested.
        IUi2Locator dataUrlText = Ui2Locators.withText(DATA_URL);
        UiAutomatorUtils.getInstance().getLocatorHelper().verifyOnScreen(dataUrlText);
    }
}
