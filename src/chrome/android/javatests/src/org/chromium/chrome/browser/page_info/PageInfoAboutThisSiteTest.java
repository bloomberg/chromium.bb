// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.annotation.SuppressLint;

import androidx.annotation.NonNull;
import androidx.test.espresso.ViewAssertion;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.page_info.PageInfoAction;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.Hyperlink;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.SiteDescription;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.SiteInfo;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests for PageInfoAboutThisSite.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures(ChromeFeatureList.PAGE_INFO_ABOUT_THIS_SITE)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.DISABLE_STARTUP_PROMOS,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
@Batch(Batch.PER_CLASS)
@SuppressLint("VisibleForTests")
public class PageInfoAboutThisSiteTest {
    private static final String sSimpleHtml = "/chrome/test/data/android/simple.html";

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @ClassRule
    public static DisableAnimationsTestRule sDisableAnimationsTestRule =
            new DisableAnimationsTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public JniMocker mMocker = new JniMocker();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Mock
    private PageInfoAboutThisSiteController.Natives mMockAboutThisSiteJni;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(PageInfoAboutThisSiteControllerJni.TEST_HOOKS, mMockAboutThisSiteJni);
        mTestServerRule.setServerUsesHttps(true);
        sActivityTestRule.loadUrl(mTestServerRule.getServer().getURL(sSimpleHtml));

        RecordHistogram.forgetHistogramForTesting("Security.PageInfo.TimeOpen.AboutThisSiteShown");
        RecordHistogram.forgetHistogramForTesting(
                "Security.PageInfo.TimeOpen.AboutThisSiteNotShown");
        RecordHistogram.forgetHistogramForTesting("WebsiteSettings.Action");
    }

    private void openPageInfo() {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            new ChromePageInfo(activity.getModalDialogManagerSupplier(), null,
                    PageInfoController.OpenedFromSource.TOOLBAR, null)
                    .show(tab, ChromePageInfoHighlight.noHighlight());
        });
        onViewWaiting(allOf(withId(org.chromium.chrome.R.id.page_info_url_wrapper), isDisplayed()));
    }

    private void dismissPageInfo() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PageInfoController.getLastPageInfoControllerForTesting().runAfterDismiss(
                    helper::notifyCalled);
        });
        helper.waitForCallback(0);
    }

    @NonNull
    private ViewAssertion renderView(String renderId) {
        return (v, noMatchException) -> {
            if (noMatchException != null) throw noMatchException;
            // Allow disk writes and slow calls to render from UI thread.
            try (StrictModeContext ignored = StrictModeContext.allowAllThreadPolicies()) {
                mRenderTestRule.render(v, renderId);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        };
    }

    private void mockResponse(byte[] bytes) {
        doReturn(bytes)
                .when(mMockAboutThisSiteJni)
                .getSiteInfo(
                        any(BrowserContextHandle.class), any(GURL.class), any(WebContents.class));
    }

    private byte[] createDescription() {
        String url = mTestServerRule.getServer().getURL(sSimpleHtml);
        SiteDescription.Builder description =
                SiteDescription.newBuilder()
                        .setDescription("Some description about example.com for testing purposes")
                        .setSource(Hyperlink.newBuilder().setUrl(url).setLabel("Example Source"));
        return SiteInfo.newBuilder().setDescription(description).build().toByteArray();
    }

    @Test
    @MediumTest
    public void testAboutThisSiteRowWithData() throws TimeoutException {
        mockResponse(createDescription());
        openPageInfo();
        onView(withId(PageInfoAboutThisSiteController.ROW_ID)).check(matches(isDisplayed()));
        onView(withText(containsString("Some description"))).check(matches(isDisplayed()));

        dismissPageInfo();
        assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Security.PageInfo.TimeOpen.AboutThisSiteShown"));
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Security.PageInfo.TimeOpen.AboutThisSiteNotShown"));
    }

    @Test
    @MediumTest
    public void testAboutThisSiteRowWithDataOnInsecureSite() {
        sActivityTestRule.loadUrl(
                mTestServerRule.getServer().getURLWithHostName("invalidcert.com", sSimpleHtml));
        mockResponse(createDescription());
        openPageInfo();
        onView(withId(PageInfoAboutThisSiteController.ROW_ID)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testAboutThisSiteRowWithoutData() throws TimeoutException {
        mockResponse(null);
        openPageInfo();
        onView(withId(PageInfoAboutThisSiteController.ROW_ID)).check(matches(not(isDisplayed())));

        dismissPageInfo();
        assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Security.PageInfo.TimeOpen.AboutThisSiteShown"));
        assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Security.PageInfo.TimeOpen.AboutThisSiteNotShown"));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testAboutThisSiteRowRendering() {
        mockResponse(createDescription());
        openPageInfo();
        onView(withId(PageInfoAboutThisSiteController.ROW_ID))
                .check(renderView("page_info_about_this_site_row"));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testAboutThisSiteSubPageRendering() {
        mockResponse(createDescription());
        openPageInfo();
        onView(withId(PageInfoAboutThisSiteController.ROW_ID)).perform(click());
        onView(withId(R.id.page_info_wrapper))
                .check(renderView("page_info_about_this_site_subpage"));
    }

    @Test
    @MediumTest
    public void testAboutThisSiteSubPageSourceClicked()
            throws ExecutionException, TimeoutException {
        assertEquals(0, RecordHistogram.getHistogramTotalCountForTesting("WebsiteSettings.Action"));
        mockResponse(createDescription());
        openPageInfo();
        assertEquals(1, RecordHistogram.getHistogramTotalCountForTesting("WebsiteSettings.Action"));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "WebsiteSettings.Action", PageInfoAction.PAGE_INFO_OPENED));

        onView(withId(PageInfoAboutThisSiteController.ROW_ID)).perform(click());
        assertEquals(2, RecordHistogram.getHistogramTotalCountForTesting("WebsiteSettings.Action"));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting("WebsiteSettings.Action",
                        PageInfoAction.PAGE_INFO_ABOUT_THIS_SITE_PAGE_OPENED));
        final CallbackHelper onTabAdded = new CallbackHelper();
        final TabModelObserver observer = new TabModelObserver() {
            @Override
            public void willAddTab(Tab tab, @TabLaunchType int type) {
                onTabAdded.notifyCalled();
            }
        };
        final TabModel tabModel = sActivityTestRule.getActivity().getCurrentTabModel();
        TestThreadUtils.runOnUiThreadBlocking(() -> tabModel.addObserver(observer));

        final int callCount = onTabAdded.getCallCount();
        onView(withText(containsString("Example Source"))).perform(click());
        onTabAdded.waitForCallback(callCount);
        TestThreadUtils.runOnUiThreadBlocking(() -> tabModel.removeObserver(observer));
        assertEquals(3, RecordHistogram.getHistogramTotalCountForTesting("WebsiteSettings.Action"));
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting("WebsiteSettings.Action",
                        PageInfoAction.PAGE_INFO_ABOUT_THIS_SITE_SOURCE_LINK_CLICKED));
    }
}