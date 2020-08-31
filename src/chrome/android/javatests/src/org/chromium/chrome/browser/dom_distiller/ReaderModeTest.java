// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.actionWithAssertions;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.notNullValue;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.dom_distiller.ReaderModeManager.DOM_DISTILLER_SCHEME;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.action.GeneralClickAction;
import android.support.test.espresso.action.GeneralLocation;
import android.support.test.espresso.action.Press;
import android.support.test.espresso.action.Tap;
import android.support.test.filters.MediumTest;

import androidx.annotation.NonNull;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.download.DownloadTestRule;
import org.chromium.chrome.browser.download.DownloadTestRule.CustomMainActivityStart;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.ReaderModeInfoBar;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.infobar.InfoBar;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import java.util.Objects;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * End-to-end tests for Reader Mode (Simplified view).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ReaderModeTest implements CustomMainActivityStart {
    @Rule
    public DownloadTestRule mDownloadTestRule = new DownloadTestRule(this);

    private static final String TEST_PAGE = "/chrome/test/data/dom_distiller/simple_article.html";
    private static final String TITLE = "Test Page Title";
    private static final String CONTENT = "Lorem ipsum";

    @SuppressWarnings("FieldCanBeLocal")
    private EmbeddedTestServer mTestServer;
    private String mURL;

    @Mock
    DistilledPagePrefs.Observer mTestObserver;

    @Override
    public void customMainActivityStart() {
        MockitoAnnotations.initMocks(this);
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mURL = mTestServer.getURL(TEST_PAGE);
        mDownloadTestRule.startMainActivityWithURL(mURL);
    }

    @After
    public void tearDown() {
        if (mTestServer != null) mTestServer.stopAndDestroyServer();
    }

    @Test
    @MediumTest
    public void testReaderModeInfobarShown() {
        waitForReaderModeInfobar();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.READER_MODE_IN_CCT)
    public void testReaderModeInCCT() throws TimeoutException {
        Tab originalTab = mDownloadTestRule.getActivity().getActivityTab();
        String innerHtml = getInnerHtml(originalTab);
        assertThat(innerHtml).doesNotContain("article-header");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            originalTab.getUserDataHost()
                    .getUserData(ReaderModeManager.USER_DATA_KEY)
                    .activateReaderMode();
        });
        CustomTabActivity customTabActivity = waitForCustomTabActivity();
        CriteriaHelper.pollUiThread(
                () -> Assert.assertThat(customTabActivity.getActivityTab(), notNullValue()));
        @NonNull
        Tab distillerViewerTab = Objects.requireNonNull(customTabActivity.getActivityTab());
        waitForDistillation(TITLE, distillerViewerTab);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.READER_MODE_IN_CCT})
    public void testReaderModeInCCT_Downloaded() throws TimeoutException {
        Tab originalTab = mDownloadTestRule.getActivity().getActivityTab();
        String innerHtml = getInnerHtml(originalTab);
        assertThat(innerHtml).doesNotContain("article-header");

        downloadAndOpenOfflinePage();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            originalTab.getUserDataHost()
                    .getUserData(ReaderModeManager.USER_DATA_KEY)
                    .activateReaderMode();
        });
        CustomTabActivity customTabActivity = waitForCustomTabActivity();
        CriteriaHelper.pollUiThread(
                () -> Assert.assertThat(customTabActivity.getActivityTab(), notNullValue()));
        @NonNull
        Tab distillerViewerTab = Objects.requireNonNull(customTabActivity.getActivityTab());
        waitForDistillation(TITLE, distillerViewerTab);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.READER_MODE_IN_CCT, ChromeFeatureList.CCT_INCOGNITO})
    public void testReaderModeInCCT_Incognito() throws TimeoutException {
        mDownloadTestRule.startMainActivityWithURL(mURL);
        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                (ChromeTabbedActivity) mDownloadTestRule.getActivity(), mURL, true);

        Tab originalTab = mDownloadTestRule.getActivity().getActivityTab();
        assertTrue(originalTab.isIncognito());
        String innerHtml = getInnerHtml(originalTab);
        assertThat(innerHtml).doesNotContain("article-header");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            originalTab.getUserDataHost()
                    .getUserData(ReaderModeManager.USER_DATA_KEY)
                    .activateReaderMode();
        });
        CustomTabActivity customTabActivity = waitForCustomTabActivity();
        CriteriaHelper.pollUiThread(
                () -> Assert.assertThat(customTabActivity.getActivityTab(), notNullValue()));
        @NonNull
        Tab distillerViewerTab = Objects.requireNonNull(customTabActivity.getActivityTab());
        waitForDistillation(TITLE, distillerViewerTab);
        assertTrue(distillerViewerTab.isIncognito());
    }

    private void downloadAndOpenOfflinePage() {
        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(), org.chromium.chrome.R.id.offline_page_id);
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(callCount));

        // Stop the server and also disconnect the network.
        mTestServer.stopAndDestroyServer();
        mTestServer = null;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { NetworkChangeNotifier.forceConnectivityState(false); });

        // Load the page that has an offline copy. The offline page should be shown.
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        Assert.assertFalse(isOfflinePage(tab));
        mDownloadTestRule.loadUrl(tab.getUrl().getSpec());
        Assert.assertTrue(isOfflinePage(tab));
    }

    private static boolean isOfflinePage(final Tab tab) {
        AtomicBoolean isOffline = new AtomicBoolean();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> isOffline.set(OfflinePageUtils.isOfflinePage(tab)));
        return isOffline.get();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.READER_MODE_IN_CCT)
    public void testReaderModeInTab() throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        String innerHtml = getInnerHtml(tab);
        assertThat(innerHtml).doesNotContain("article-header");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.getUserDataHost().getUserData(ReaderModeManager.USER_DATA_KEY).activateReaderMode();
        });
        waitForDistillation(TITLE, mDownloadTestRule.getActivity().getActivityTab());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.READER_MODE_IN_CCT)
    public void testPreferenceInCCT() throws TimeoutException {
        Tab originalTab = mDownloadTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            originalTab.getUserDataHost()
                    .getUserData(ReaderModeManager.USER_DATA_KEY)
                    .activateReaderMode();
        });
        CustomTabActivity customTabActivity = waitForCustomTabActivity();
        CriteriaHelper.pollUiThread(() -> customTabActivity.getActivityTab() != null);
        @NonNull
        Tab distillerViewerTab = Objects.requireNonNull(customTabActivity.getActivityTab());
        waitForDistillation(TITLE, distillerViewerTab);

        testPreference(customTabActivity, distillerViewerTab);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.READER_MODE_IN_CCT)
    public void testPreferenceInTab() throws TimeoutException {
        mDownloadTestRule.loadUrl(
                DomDistillerUrlUtils.getDistillerViewUrlFromUrl(DOM_DISTILLER_SCHEME, mURL, TITLE));

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        waitForDistillation(TITLE, tab);

        testPreference(mDownloadTestRule.getActivity(), tab);
    }

    /**
     * Wait until a {@link CustomTabActivity} shows up, and return it.
     * @return a {@link CustomTabActivity}
     */
    private CustomTabActivity waitForCustomTabActivity() {
        AtomicReference<CustomTabActivity> activity = new AtomicReference<>();
        CriteriaHelper.pollUiThread(() -> {
            for (Activity runningActivity : ApplicationStatus.getRunningActivities()) {
                if (runningActivity instanceof CustomTabActivity) {
                    activity.set((CustomTabActivity) runningActivity);
                    return true;
                }
            }
            return false;
        });
        return activity.get();
    }

    private DistilledPagePrefs getDistilledPagePrefs() {
        AtomicReference<DistilledPagePrefs> prefs = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            DomDistillerService domDistillerService =
                    DomDistillerServiceFactory.getForProfile(Profile.getLastUsedRegularProfile());
            prefs.set(domDistillerService.getDistilledPagePrefs());
        });
        return prefs.get();
    }

    private void testThemeColor(ChromeActivity activity, Tab tab) {
        waitForBackgroundColor(tab, "\"rgb(255, 255, 255)\"");

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), activity,
                org.chromium.chrome.R.id.reader_mode_prefs_id);
        onView(isRoot()).check(ViewUtils.waitForView(allOf(withText("Dark"), isDisplayed())));
        onView(withText("Dark")).perform(click());
        Espresso.pressBack();
        waitForBackgroundColor(tab, "\"rgb(32, 33, 36)\"");

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), activity,
                org.chromium.chrome.R.id.reader_mode_prefs_id);
        onView(isRoot()).check(ViewUtils.waitForView(allOf(withText("Sepia"), isDisplayed())));
        onView(withText("Sepia")).perform(click());
        Espresso.pressBack();
        waitForBackgroundColor(tab, "\"rgb(254, 247, 224)\"");

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), activity,
                org.chromium.chrome.R.id.reader_mode_prefs_id);
        onView(isRoot()).check(ViewUtils.waitForView(allOf(withText("Light"), isDisplayed())));
        onView(withText("Light")).perform(click());
        Espresso.pressBack();
        waitForBackgroundColor(tab, "\"rgb(255, 255, 255)\"");

        verify(mTestObserver, times(3)).onChangeTheme(anyInt());
    }

    private void testFontSize(ChromeActivity activity, Tab tab) {
        waitForFontSize(tab, "\"14px\"");

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), activity,
                org.chromium.chrome.R.id.reader_mode_prefs_id);
        onView(isRoot()).check(ViewUtils.waitForView(allOf(withId(R.id.font_size), isDisplayed())));
        // Max is 200% font size.
        onView(withId(R.id.font_size))
                .perform(actionWithAssertions(new GeneralClickAction(
                        Tap.SINGLE, GeneralLocation.CENTER_RIGHT, Press.FINGER)));
        Espresso.pressBack();
        waitForFontSize(tab, "\"28px\"");

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), activity,
                org.chromium.chrome.R.id.reader_mode_prefs_id);
        onView(isRoot()).check(ViewUtils.waitForView(allOf(withId(R.id.font_size), isDisplayed())));
        // Min is 50% font size.
        onView(withId(R.id.font_size))
                .perform(actionWithAssertions(new GeneralClickAction(
                        Tap.SINGLE, GeneralLocation.CENTER_LEFT, Press.FINGER)));
        Espresso.pressBack();
        waitForFontSize(tab, "\"7px\"");

        verify(mTestObserver, times(2)).onChangeFontScaling(anyFloat());
    }

    private void testPreference(ChromeActivity activity, Tab tab) {
        DistilledPagePrefs prefs = getDistilledPagePrefs();
        prefs.addObserver(mTestObserver);

        testThemeColor(activity, tab);
        testFontSize(activity, tab);
        // TODO(crbug/1069520): change font family as well.
    }

    /**
     * Run JavaScript on a certain {@link Tab}.
     * @param tab The tab to be injected to.
     * @param javaScript The JavaScript code to be injected.
     * @return The result of the code.
     */
    private String runJavaScript(Tab tab, String javaScript) throws TimeoutException {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(tab.getWebContents(), javaScript);
        javascriptHelper.waitUntilHasValue();
        return javascriptHelper.getJsonResultAndClear();
    }

    /**
     * @param tab The tab to be inspected.
     * @return The inner HTML of a certain {@link Tab}.
     */
    private String getInnerHtml(Tab tab) throws TimeoutException {
        return runJavaScript(tab, "document.body.innerHTML");
    }

    /**
     * Wait until the background color of a certain {@link Tab} to be a given value.
     * @param tab The tab to be inspected.
     * @param expectedColor The expected background color
     */
    private void waitForBackgroundColor(Tab tab, String expectedColor) {
        String query = "window.getComputedStyle(document.body)['backgroundColor']";
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(expectedColor, () -> runJavaScript(tab, query)));
    }

    /**
     * Wait until the font size of a certain {@link Tab} to be a given value.
     * @param tab The tab to be inspected.
     * @param expectedSize The expected font size
     */
    private void waitForFontSize(Tab tab, String expectedSize) {
        String query = "window.getComputedStyle(document.body)['fontSize']";
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(expectedSize, () -> runJavaScript(tab, query)));
    }

    /**
     * Wait until a {@link ReaderModeInfoBar} shows up.
     */
    private void waitForReaderModeInfobar() {
        CriteriaHelper.pollUiThread(() -> {
            for (InfoBar infobar : mDownloadTestRule.getInfoBars()) {
                if (infobar instanceof ReaderModeInfoBar) return true;
            }
            return false;
        });
    }

    /**
     * Wait until the distilled content is shown on the {@link Tab}.
     * @param expectedTitle The expected title of the distilled content
     * @param tab the tab to wait
     */
    private void waitForDistillation(@SuppressWarnings("SameParameterValue") String expectedTitle,
            Tab tab) throws TimeoutException {
        CriteriaHelper.pollUiThread(
                Criteria.equals("chrome-distiller", () -> tab.getUrl().getScheme()));
        ChromeTabUtils.waitForTabPageLoaded(tab, null);
        // Distiller Viewer load the content dynamically, so waitForTabPageLoaded() is not enough.
        CriteriaHelper.pollUiThread(Criteria.equals(expectedTitle, tab::getTitle));

        String innerHtml = getInnerHtml(tab);
        assertThat(innerHtml).contains("article-header");
        assertThat(innerHtml).contains(CONTENT);
    }
}
