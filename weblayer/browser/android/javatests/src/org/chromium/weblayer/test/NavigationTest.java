// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.weblayer.LoadError;
import org.chromium.weblayer.NavigateParams;
import org.chromium.weblayer.Navigation;
import org.chromium.weblayer.NavigationCallback;
import org.chromium.weblayer.NavigationController;
import org.chromium.weblayer.NavigationState;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Example test that just starts the weblayer shell.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class NavigationTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    // URLs used for base tests.
    private static final String URL1 = "data:text,foo";
    private static final String URL2 = "data:text,bar";
    private static final String URL3 = "data:text,baz";
    private static final String URL4 = "data:text,bat";

    private static class Callback extends NavigationCallback {
        public static class NavigationCallbackHelper extends CallbackHelper {
            private Uri mUri;
            private boolean mIsSameDocument;
            private int mHttpStatusCode;
            private List<Uri> mRedirectChain;
            private @LoadError int mLoadError;
            private @NavigationState int mNavigationState;

            public void notifyCalled(Navigation navigation) {
                mUri = navigation.getUri();
                mIsSameDocument = navigation.isSameDocument();
                mHttpStatusCode = navigation.getHttpStatusCode();
                mRedirectChain = navigation.getRedirectChain();
                mLoadError = navigation.getLoadError();
                mNavigationState = navigation.getState();
                notifyCalled();
            }

            public void assertCalledWith(int currentCallCount, String uri) throws TimeoutException {
                waitForCallback(currentCallCount);
                assertEquals(mUri.toString(), uri);
            }

            public void assertCalledWith(int currentCallCount, String uri, boolean isSameDocument)
                    throws TimeoutException {
                waitForCallback(currentCallCount);
                assertEquals(mUri.toString(), uri);
                assertEquals(mIsSameDocument, isSameDocument);
            }

            public void assertCalledWith(int currentCallCount, List<Uri> redirectChain)
                    throws TimeoutException {
                waitForCallback(currentCallCount);
                assertEquals(mRedirectChain, redirectChain);
            }

            public void assertCalledWith(int currentCallCount, String uri, @LoadError int loadError)
                    throws TimeoutException {
                waitForCallback(currentCallCount);
                assertEquals(mUri.toString(), uri);
                assertEquals(mLoadError, loadError);
            }

            public int getHttpStatusCode() {
                return mHttpStatusCode;
            }

            @NavigationState
            public int getNavigationState() {
                return mNavigationState;
            }
        }

        public static class NavigationCallbackValueRecorder {
            private List<String> mObservedValues =
                    Collections.synchronizedList(new ArrayList<String>());

            public void recordValue(String parameter) {
                mObservedValues.add(parameter);
            }

            public List<String> getObservedValues() {
                return mObservedValues;
            }

            public void waitUntilValueObserved(String expectation) {
                CriteriaHelper.pollInstrumentationThread(
                        () -> Assert.assertThat(expectation, Matchers.isIn(mObservedValues)));
            }
        }

        public NavigationCallbackHelper onStartedCallback = new NavigationCallbackHelper();
        public NavigationCallbackHelper onRedirectedCallback = new NavigationCallbackHelper();
        public NavigationCallbackHelper onReadyToCommitCallback = new NavigationCallbackHelper();
        public NavigationCallbackHelper onCompletedCallback = new NavigationCallbackHelper();
        public NavigationCallbackHelper onFailedCallback = new NavigationCallbackHelper();
        public NavigationCallbackValueRecorder loadStateChangedCallback =
                new NavigationCallbackValueRecorder();
        public NavigationCallbackValueRecorder loadProgressChangedCallback =
                new NavigationCallbackValueRecorder();
        public CallbackHelper onFirstContentfulPaintCallback = new CallbackHelper();

        @Override
        public void onNavigationStarted(Navigation navigation) {
            onStartedCallback.notifyCalled(navigation);
        }

        @Override
        public void onNavigationRedirected(Navigation navigation) {
            onRedirectedCallback.notifyCalled(navigation);
        }

        @Override
        public void onReadyToCommitNavigation(Navigation navigation) {
            onReadyToCommitCallback.notifyCalled(navigation);
        }

        @Override
        public void onNavigationCompleted(Navigation navigation) {
            onCompletedCallback.notifyCalled(navigation);
        }

        @Override
        public void onNavigationFailed(Navigation navigation) {
            onFailedCallback.notifyCalled(navigation);
        }

        @Override
        public void onFirstContentfulPaint() {
            onFirstContentfulPaintCallback.notifyCalled();
        }

        @Override
        public void onLoadStateChanged(boolean isLoading, boolean toDifferentDocument) {
            loadStateChangedCallback.recordValue(
                    Boolean.toString(isLoading) + " " + Boolean.toString(toDifferentDocument));
        }

        @Override
        public void onLoadProgressChanged(double progress) {
            loadProgressChangedCallback.recordValue(
                    progress == 1 ? "load complete" : "load started");
        }
    }

    private final Callback mCallback = new Callback();

    @Test
    @SmallTest
    public void testNavigationEvents() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);

        setNavigationCallback(activity);
        int curStartedCount = mCallback.onStartedCallback.getCallCount();
        int curCommittedCount = mCallback.onReadyToCommitCallback.getCallCount();
        int curCompletedCount = mCallback.onCompletedCallback.getCallCount();
        int curOnFirstContentfulPaintCount =
                mCallback.onFirstContentfulPaintCallback.getCallCount();

        mActivityTestRule.navigateAndWait(URL2);

        mCallback.onStartedCallback.assertCalledWith(curStartedCount, URL2);
        mCallback.onReadyToCommitCallback.assertCalledWith(curCommittedCount, URL2);
        mCallback.onCompletedCallback.assertCalledWith(curCompletedCount, URL2);
        mCallback.onFirstContentfulPaintCallback.waitForCallback(curOnFirstContentfulPaintCount);
        assertEquals(mCallback.onCompletedCallback.getHttpStatusCode(), 200);
    }

    @Test
    @SmallTest
    public void testLoadStateUpdates() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        setNavigationCallback(activity);
        mActivityTestRule.navigateAndWait(URL1);

        /* Wait until the NavigationCallback is notified of load completion. */
        mCallback.loadStateChangedCallback.waitUntilValueObserved("false false");
        mCallback.loadProgressChangedCallback.waitUntilValueObserved("load complete");

        /* Verify that the NavigationCallback was notified of load progress /before/ load
         * completion.
         */
        int finishStateIndex =
                mCallback.loadStateChangedCallback.getObservedValues().indexOf("false false");
        int finishProgressIndex =
                mCallback.loadProgressChangedCallback.getObservedValues().indexOf("load complete");
        int startStateIndex =
                mCallback.loadStateChangedCallback.getObservedValues().lastIndexOf("true true");
        int startProgressIndex =
                mCallback.loadProgressChangedCallback.getObservedValues().lastIndexOf(
                        "load started");

        assertNotEquals(startStateIndex, -1);
        assertNotEquals(startProgressIndex, -1);
        assertNotEquals(finishStateIndex, -1);
        assertNotEquals(finishProgressIndex, -1);

        assertTrue(startStateIndex < finishStateIndex);
        assertTrue(startProgressIndex < finishProgressIndex);
    }

    @Test
    @SmallTest
    public void testReplace() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        final NavigateParams params =
                new NavigateParams.Builder().setShouldReplaceCurrentEntry(true).build();
        navigateAndWaitForCompletion(URL2,
                ()
                        -> activity.getTab().getNavigationController().navigate(
                                Uri.parse(URL2), params));
        runOnUiThreadBlocking(() -> {
            NavigationController navigationController = activity.getTab().getNavigationController();
            assertFalse(navigationController.canGoForward());
            assertFalse(navigationController.canGoBack());
            assertEquals(1, navigationController.getNavigationListSize());
        });

        // Verify that a default NavigateParams does not replace.
        final NavigateParams params2 = new NavigateParams();
        navigateAndWaitForCompletion(URL3,
                ()
                        -> activity.getTab().getNavigationController().navigate(
                                Uri.parse(URL3), params2));
        runOnUiThreadBlocking(() -> {
            NavigationController navigationController = activity.getTab().getNavigationController();
            assertFalse(navigationController.canGoForward());
            assertTrue(navigationController.canGoBack());
            assertEquals(2, navigationController.getNavigationListSize());
        });
    }

    @Test
    @SmallTest
    public void testGoBackAndForward() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        mActivityTestRule.navigateAndWait(URL2);
        mActivityTestRule.navigateAndWait(URL3);

        NavigationController navigationController =
                runOnUiThreadBlocking(() -> activity.getTab().getNavigationController());

        navigateAndWaitForCompletion(URL2, () -> {
            assertTrue(navigationController.canGoBack());
            navigationController.goBack();
        });

        navigateAndWaitForCompletion(URL1, () -> {
            assertTrue(navigationController.canGoBack());
            navigationController.goBack();
        });

        navigateAndWaitForCompletion(URL2, () -> {
            assertFalse(navigationController.canGoBack());
            assertTrue(navigationController.canGoForward());
            navigationController.goForward();
        });

        navigateAndWaitForCompletion(URL3, () -> {
            assertTrue(navigationController.canGoForward());
            navigationController.goForward();
        });

        runOnUiThreadBlocking(() -> { assertFalse(navigationController.canGoForward()); });
    }

    @Test
    @SmallTest
    public void testGoToIndex() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        mActivityTestRule.navigateAndWait(URL2);
        mActivityTestRule.navigateAndWait(URL3);
        mActivityTestRule.navigateAndWait(URL4);

        // Navigate back to the 2nd url.
        assertEquals(URL2, goToIndexAndReturnUrl(activity.getTab(), 1));

        // Navigate forwards to the 4th url.
        assertEquals(URL4, goToIndexAndReturnUrl(activity.getTab(), 3));
    }

    @Test
    @SmallTest
    public void testGetNavigationEntryTitle() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(
                "data:text/html,<head><title>Page A</title></head>");
        setNavigationCallback(activity);

        mActivityTestRule.navigateAndWait("data:text/html,<head><title>Page B</title></head>");
        mActivityTestRule.navigateAndWait("data:text/html,<head><title>Page C</title></head>");

        runOnUiThreadBlocking(() -> {
            NavigationController navigationController = activity.getTab().getNavigationController();
            assertEquals("Page A", navigationController.getNavigationEntryTitle(0));
            assertEquals("Page B", navigationController.getNavigationEntryTitle(1));
            assertEquals("Page C", navigationController.getNavigationEntryTitle(2));
        });
    }

    @Test
    @SmallTest
    public void testSameDocument() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        int curCompletedCount = mCallback.onCompletedCallback.getCallCount();

        mActivityTestRule.executeScriptSync(
                "history.pushState(null, '', '#bar');", true /* useSeparateIsolate */);

        mCallback.onCompletedCallback.assertCalledWith(
                curCompletedCount, "data:text,foo#bar", true);
    }

    @Test
    @SmallTest
    public void testReload() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        navigateAndWaitForCompletion(
                URL1, () -> { activity.getTab().getNavigationController().reload(); });
    }

    @Test
    @SmallTest
    public void testStop() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        int curFailedCount = mCallback.onFailedCallback.getCallCount();

        runOnUiThreadBlocking(() -> {
            NavigationController navigationController = activity.getTab().getNavigationController();
            navigationController.registerNavigationCallback(new NavigationCallback() {
                @Override
                public void onNavigationStarted(Navigation navigation) {
                    navigationController.stop();
                }
            });
            navigationController.navigate(Uri.parse(URL2));
        });

        mCallback.onFailedCallback.assertCalledWith(curFailedCount, URL2);
    }

    @Test
    @SmallTest
    public void testRedirect() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        int curRedirectedCount = mCallback.onRedirectedCallback.getCallCount();

        String finalUrl = mActivityTestRule.getTestServer().getURL("/echo");
        String url = mActivityTestRule.getTestServer().getURL("/server-redirect?" + finalUrl);
        navigateAndWaitForCompletion(finalUrl,
                () -> { activity.getTab().getNavigationController().navigate(Uri.parse(url)); });

        mCallback.onRedirectedCallback.assertCalledWith(
                curRedirectedCount, Arrays.asList(Uri.parse(url), Uri.parse(finalUrl)));
    }

    @Test
    @SmallTest
    public void testNavigationList() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        mActivityTestRule.navigateAndWait(URL2);
        mActivityTestRule.navigateAndWait(URL3);

        NavigationController navigationController =
                runOnUiThreadBlocking(() -> activity.getTab().getNavigationController());

        runOnUiThreadBlocking(() -> {
            assertEquals(3, navigationController.getNavigationListSize());
            assertEquals(2, navigationController.getNavigationListCurrentIndex());
            assertEquals(URL1, navigationController.getNavigationEntryDisplayUri(0).toString());
            assertEquals(URL2, navigationController.getNavigationEntryDisplayUri(1).toString());
            assertEquals(URL3, navigationController.getNavigationEntryDisplayUri(2).toString());
        });

        navigateAndWaitForCompletion(URL2, () -> { navigationController.goBack(); });

        runOnUiThreadBlocking(() -> {
            assertEquals(3, navigationController.getNavigationListSize());
            assertEquals(1, navigationController.getNavigationListCurrentIndex());
        });
    }

    @Test
    @SmallTest
    public void testLoadError() throws Exception {
        String url = mActivityTestRule.getTestDataURL("non_existent.html");

        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        setNavigationCallback(activity);

        int curCompletedCount = mCallback.onCompletedCallback.getCallCount();

        // navigateAndWait() expects a success code, so it won't work here.
        runOnUiThreadBlocking(
                () -> { activity.getTab().getNavigationController().navigate(Uri.parse(url)); });

        mCallback.onCompletedCallback.assertCalledWith(
                curCompletedCount, url, LoadError.HTTP_CLIENT_ERROR);
        assertEquals(mCallback.onCompletedCallback.getHttpStatusCode(), 404);
        assertEquals(mCallback.onCompletedCallback.getNavigationState(), NavigationState.COMPLETE);
    }

    @Test
    @SmallTest
    public void testRepostConfirmation() throws Exception {
        // Load a page with a form.
        InstrumentationActivity activity =
                mActivityTestRule.launchShellWithUrl(mActivityTestRule.getTestDataURL("form.html"));
        assertNotNull(activity);
        setNavigationCallback(activity);

        // Touch the page; this should submit the form.
        int currentCallCount = mCallback.onCompletedCallback.getCallCount();
        EventUtils.simulateTouchCenterOfView(activity.getWindow().getDecorView());
        String targetUrl = mActivityTestRule.getTestDataURL("simple_page.html");
        mCallback.onCompletedCallback.assertCalledWith(currentCallCount, targetUrl);

        // Make sure a tab modal shows after we attempt a reload.
        Boolean isTabModalShowingResult[] = new Boolean[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void onTabModalStateChanged(boolean isTabModalShowing) {
                    isTabModalShowingResult[0] = isTabModalShowing;
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);
            tab.getNavigationController().reload();
        });

        callbackHelper.waitForFirst();
        assertTrue(isTabModalShowingResult[0]);
    }

    private void setNavigationCallback(InstrumentationActivity activity) {
        runOnUiThreadBlocking(
                ()
                        -> activity.getTab().getNavigationController().registerNavigationCallback(
                                mCallback));
    }

    private void registerNavigationCallback(NavigationCallback callback) {
        runOnUiThreadBlocking(()
                                      -> mActivityTestRule.getActivity()
                                                 .getTab()
                                                 .getNavigationController()
                                                 .registerNavigationCallback(callback));
    }

    private void navigateAndWaitForCompletion(String expectedUrl, Runnable navigateRunnable)
            throws Exception {
        int currentCallCount = mCallback.onCompletedCallback.getCallCount();
        runOnUiThreadBlocking(navigateRunnable);
        mCallback.onCompletedCallback.assertCalledWith(currentCallCount, expectedUrl);
    }

    private String goToIndexAndReturnUrl(Tab tab, int index) throws Exception {
        NavigationController navigationController =
                runOnUiThreadBlocking(() -> tab.getNavigationController());

        final BoundedCountDownLatch navigationComplete = new BoundedCountDownLatch(1);
        final AtomicReference<String> navigationUrl = new AtomicReference<String>();
        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationCompleted(Navigation navigation) {
                navigationComplete.countDown();
                navigationUrl.set(navigation.getUri().toString());
            }
        };

        runOnUiThreadBlocking(() -> {
            navigationController.registerNavigationCallback(navigationCallback);
            navigationController.goToIndex(index);
        });

        navigationComplete.timedAwait();

        runOnUiThreadBlocking(
                () -> { navigationController.unregisterNavigationCallback(navigationCallback); });

        return navigationUrl.get();
    }

    @Test
    @SmallTest
    public void testStopFromOnNavigationStarted() throws Exception {
        final InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        final BoundedCountDownLatch doneLatch = new BoundedCountDownLatch(1);
        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationStarted(Navigation navigation) {
                activity.getTab().getNavigationController().stop();
                doneLatch.countDown();
            }
        };
        runOnUiThreadBlocking(() -> {
            NavigationController controller = activity.getTab().getNavigationController();
            controller.registerNavigationCallback(navigationCallback);
            controller.navigate(Uri.parse(URL1));
        });
        doneLatch.timedAwait();
    }

    // NavigationCallback implementation that sets a header in either start or redirect.
    private static final class HeaderSetter extends NavigationCallback {
        private final String mName;
        private final String mValue;
        private final boolean mInStart;
        public boolean mGotIllegalArgumentException;

        HeaderSetter(String name, String value, boolean inStart) {
            mName = name;
            mValue = value;
            mInStart = inStart;
        }

        @Override
        public void onNavigationStarted(Navigation navigation) {
            if (mInStart) applyHeader(navigation);
        }

        @Override
        public void onNavigationRedirected(Navigation navigation) {
            if (!mInStart) applyHeader(navigation);
        }

        private void applyHeader(Navigation navigation) {
            try {
                navigation.setRequestHeader(mName, mValue);
            } catch (IllegalArgumentException e) {
                mGotIllegalArgumentException = true;
            }
        }
    }

    @Test
    @SmallTest
    public void testSetRequestHeaderInStart() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        String headerName = "header";
        String headerValue = "value";
        HeaderSetter setter = new HeaderSetter(headerName, headerValue, true);
        registerNavigationCallback(setter);
        String url = testServer.setResponse("/ok.html", "<html>ok</html>", null);
        mActivityTestRule.navigateAndWait(url);
        assertFalse(setter.mGotIllegalArgumentException);
        assertEquals(headerValue, testServer.getLastRequest("/ok.html").headerValue(headerName));
    }

    @Test
    @SmallTest
    public void testSetRequestHeaderInRedirect() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        String headerName = "header";
        String headerValue = "value";
        HeaderSetter setter = new HeaderSetter(headerName, headerValue, false);
        registerNavigationCallback(setter);
        // The destination of the redirect.
        String finalUrl = testServer.setResponse("/ok.html", "<html>ok</html>", null);
        // The url that redirects to |finalUrl|.
        String redirectingUrl = testServer.setRedirect("/redirect.html", finalUrl);
        Tab tab = mActivityTestRule.getActivity().getTab();
        NavigationWaiter waiter = new NavigationWaiter(finalUrl, tab, false, false);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getNavigationController().navigate(Uri.parse(redirectingUrl)); });
        waiter.waitForNavigation();
        assertFalse(setter.mGotIllegalArgumentException);
        assertEquals(headerValue, testServer.getLastRequest("/ok.html").headerValue(headerName));
    }

    @Test
    @SmallTest
    public void testSetRequestHeaderThrowsExceptionInCompleted() throws Exception {
        mActivityTestRule.launchShellWithUrl(null);
        boolean gotCompleted[] = new boolean[1];
        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationCompleted(Navigation navigation) {
                gotCompleted[0] = true;
                boolean gotException = false;
                try {
                    navigation.setRequestHeader("name", "value");
                } catch (IllegalStateException e) {
                    gotException = true;
                }
                assertTrue(gotException);
            }
        };
        registerNavigationCallback(navigationCallback);
        mActivityTestRule.navigateAndWait(URL1);
        assertTrue(gotCompleted[0]);
    }

    @Test
    @SmallTest
    public void testSetRequestHeaderThrowsExceptionWithInvalidValue() throws Exception {
        mActivityTestRule.launchShellWithUrl(null);
        HeaderSetter setter = new HeaderSetter("name", "\0", true);
        registerNavigationCallback(setter);
        mActivityTestRule.navigateAndWait(URL1);
        assertTrue(setter.mGotIllegalArgumentException);
    }

    // NavigationCallback implementation that sets the user-agent string in onNavigationStarted().
    private static final class UserAgentSetter extends NavigationCallback {
        private final String mValue;

        UserAgentSetter(String value) {
            mValue = value;
        }

        @Override
        public void onNavigationStarted(Navigation navigation) {
            navigation.setUserAgentString(mValue);
        }
    }

    @Test
    @SmallTest
    public void testSetUserAgentString() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        String customUserAgent = "custom-ua";
        UserAgentSetter setter = new UserAgentSetter(customUserAgent);
        registerNavigationCallback(setter);
        String url = testServer.setResponse("/ok.html", "<html>ok</html>", null);
        mActivityTestRule.navigateAndWait(url);
        String actualUserAgent = testServer.getLastRequest("/ok.html").headerValue("User-Agent");
        assertEquals(customUserAgent, actualUserAgent);
    }
}
