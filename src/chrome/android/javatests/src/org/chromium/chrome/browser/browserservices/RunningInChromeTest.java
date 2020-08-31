// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createSession;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createTrustedWebActivityIntent;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.isTrustedWebActivity;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.spoofVerification;
import static org.chromium.chrome.browser.notifications.NotificationConstants.NOTIFICATION_ID_TWA_DISCLOSURE_INITIAL;
import static org.chromium.chrome.browser.notifications.NotificationConstants.NOTIFICATION_ID_TWA_DISCLOSURE_SUBSEQUENT;

import android.content.Intent;
import android.support.test.espresso.Espresso;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.dependency_injection.ModuleOverridesRule;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy.NotificationEntry;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests to make sure the Running in Chrome prompt is shown.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_NEW_DISCLOSURE)
public class RunningInChromeTest {
    private static final String TAG = "RunningInChrome";

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    private static final Set<Integer> TEST_SNACKBARS = new HashSet<>(Arrays.asList(
            Snackbar.UMA_TWA_PRIVACY_DISCLOSURE, Snackbar.UMA_TWA_PRIVACY_DISCLOSURE_V2));

    private final CustomTabActivityTestRule mCustomTabActivityTestRule =
            new CustomTabActivityTestRule();
    private final EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();
    private final TestRule mModuleOverridesRule = new ModuleOverridesRule()
            .setOverride(ChromeActivityCommonsModule.Factory.class,
                    ChromeActivityCommonsModuleForTest::new);

    @Rule
    public RuleChain mRuleChain = RuleChain.emptyRuleChain()
            .around(mCustomTabActivityTestRule)
            .around(mEmbeddedTestServerRule)
            .around(mModuleOverridesRule);

    /**
     * This class causes Dagger to provide our MockNotificationManagerProxy instead of the real one.
     */
    class ChromeActivityCommonsModuleForTest extends ChromeActivityCommonsModule {
        public ChromeActivityCommonsModuleForTest(ChromeActivity<?> activity,
                ActivityLifecycleDispatcher lifecycleDispatcher) {
            super(activity, lifecycleDispatcher);
        }

        @Override
        public NotificationManagerProxy provideNotificationManagerProxy() {
            return mMockNotificationManager;
        }
    }


    private String mTestPage;
    private BrowserServicesStore mStore;
    private final MockNotificationManagerProxy mMockNotificationManager =
            new MockNotificationManagerProxy();

    @Before
    public void setUp() {
        // Native needs to be initialized to start the test server.
        LibraryLoader.getInstance().ensureInitialized();

        mEmbeddedTestServerRule.setServerUsesHttps(true); // TWAs only work with HTTPS.
        mTestPage = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);

        mMockNotificationManager.setNotificationsEnabled(false);

        mStore = new BrowserServicesStore(
                ChromeApplication.getComponent().resolveSharedPreferencesManager());
        mStore.removeTwaDisclosureAcceptanceForPackage(PACKAGE_NAME);
    }

    @Test
    @MediumTest
    public void showsNewRunningInChrome() throws TimeoutException {
        launch(createTrustedWebActivityIntent(mTestPage));

        clearOtherSnackbars();

        assertTrue(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));
        Espresso.onView(withText(containsString(getString())))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void showsNotification() throws TimeoutException {
        mMockNotificationManager.setNotificationsEnabled(true);

        launch(createTrustedWebActivityIntent(mTestPage));

        String scope = Origin.createOrThrow(mTestPage).toString();
        CriteriaHelper.pollUiThread(() -> assertTrue(showingNotification(scope)));
    }

    @Test
    @MediumTest
    public void dismissesNotification_onNavigation() throws TimeoutException {
        mMockNotificationManager.setNotificationsEnabled(true);

        launch(createTrustedWebActivityIntent(mTestPage));

        String scope = Origin.createOrThrow(mTestPage).toString();
        CriteriaHelper.pollUiThread(() ->
                assertTrue(showingNotification(scope)));

        mCustomTabActivityTestRule.loadUrl("https://www.example.com/");

        CriteriaHelper.pollUiThread(() ->
                assertFalse(showingNotification(scope)));
    }

    @Test
    @MediumTest
    public void dismissesNotification_onActivityClose() throws TimeoutException {
        mMockNotificationManager.setNotificationsEnabled(true);

        launch(createTrustedWebActivityIntent(mTestPage));

        String scope = Origin.createOrThrow(mTestPage).toString();
        CriteriaHelper.pollUiThread(() -> assertTrue(showingNotification(scope)));

        mCustomTabActivityTestRule.getActivity().finish();

        CriteriaHelper.pollUiThread(() -> assertFalse(showingNotification(scope)));
    }

    private boolean showingNotification(String tag) {
        for (NotificationEntry entry : mMockNotificationManager.getNotifications()) {
            if (!entry.tag.equals(tag)) continue;

            if (entry.id == NOTIFICATION_ID_TWA_DISCLOSURE_INITIAL) return true;
            if (entry.id == NOTIFICATION_ID_TWA_DISCLOSURE_SUBSEQUENT) return true;
        }

        return false;
    }

    public void launch(Intent intent) throws TimeoutException {
        String url = intent.getData().toString();
        spoofVerification(PACKAGE_NAME, url);
        createSession(intent, PACKAGE_NAME);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    private void clearOtherSnackbars() {
        // On the trybots a Downloads Snackbar is shown over the one we want to test.
        SnackbarManager manager = mCustomTabActivityTestRule.getActivity().getSnackbarManager();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            while (true) {
                Snackbar snackbar = manager.getCurrentSnackbarForTesting();

                if (snackbar == null) break;
                if (TEST_SNACKBARS.contains(snackbar.getIdentifierForTesting())) break;

                manager.dismissSnackbars(snackbar.getController());
            }
        });
    }

    private String getString() {
        return mCustomTabActivityTestRule.getActivity().getResources()
                .getString(R.string.twa_running_in_chrome);
    }
}
