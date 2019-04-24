// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;
import android.util.AndroidRuntimeException;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContents.DependencyFactory;
import org.chromium.android_webview.AwContents.InternalAccessDelegate;
import org.chromium.android_webview.AwContents.NativeDrawFunctorFactory;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.GraphicsTestUtils;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.net.test.util.TestWebServer;

import java.lang.annotation.Annotation;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Custom ActivityTestRunner for WebView instrumentation tests */
public class AwActivityTestRule extends ActivityTestRule<AwTestRunnerActivity> {
    public static final long WAIT_TIMEOUT_MS = scaleTimeout(15000);

    public static final int CHECK_INTERVAL = 100;

    private static final String TAG = "AwActivityTestRule";

    private static final Pattern MAYBE_QUOTED_STRING = Pattern.compile("^(\"?)(.*)\\1$");

    /**
     * An interface to call onCreateWindow(AwContents).
     */
    public interface OnCreateWindowHandler {
        /** This will be called when a new window pops up from the current webview. */
        public boolean onCreateWindow(AwContents awContents);
    }

    private Description mCurrentTestDescription;

    // The browser context needs to be a process-wide singleton.
    private AwBrowserContext mBrowserContext;

    public AwActivityTestRule() {
        super(AwTestRunnerActivity.class, /* initialTouchMode */ false, /* launchActivity */ false);
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        mCurrentTestDescription = description;
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUp();
                base.evaluate();
            }
        }, description);
    }

    public void setUp() throws Exception {
        if (needsAwBrowserContextCreated()) {
            createAwBrowserContext();
        }
        if (needsBrowserProcessStarted()) {
            startBrowserProcess();
        }
    }

    public AwTestRunnerActivity launchActivity() {
        if (getActivity() == null) {
            return launchActivity(null);
        }
        return getActivity();
    }

    public AwBrowserContext createAwBrowserContextOnUiThread(
            InMemorySharedPreferences prefs, Context appContext) {
        return new AwBrowserContext(prefs, appContext);
    }

    public TestDependencyFactory createTestDependencyFactory() {
        return new TestDependencyFactory();
    }

    /**
     * Override this to return false if the test doesn't want to create an
     * AwBrowserContext automatically.
     */
    public boolean needsAwBrowserContextCreated() {
        return true;
    }

    /**
     * Override this to return false if the test doesn't want the browser
     * startup sequence to be run automatically.
     *
     * @return Whether the instrumentation test requires the browser process to
     *         already be started.
     */
    public boolean needsBrowserProcessStarted() {
        return true;
    }

    public void createAwBrowserContext() {
        if (mBrowserContext != null) {
            throw new AndroidRuntimeException("There should only be one browser context.");
        }
        launchActivity(); // The Activity must be launched in order to load native code
        final InMemorySharedPreferences prefs = new InMemorySharedPreferences();
        final Context appContext = InstrumentationRegistry.getInstrumentation()
                                           .getTargetContext()
                                           .getApplicationContext();
        ThreadUtils.runOnUiThreadBlockingNoException(
                () -> mBrowserContext = createAwBrowserContextOnUiThread(prefs, appContext));
    }

    public void startBrowserProcess() throws Exception {
        // The Activity must be launched in order for proper webview statics to be setup.
        launchActivity();
        ThreadUtils.runOnUiThreadBlocking(() -> AwBrowserProcess.start());
    }

    public static void enableJavaScriptOnUiThread(final AwContents awContents) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> awContents.getSettings().setJavaScriptEnabled(true));
    }

    public static void setNetworkAvailableOnUiThread(
            final AwContents awContents, final boolean networkUp) {
        ThreadUtils.runOnUiThreadBlocking(() -> awContents.setNetworkAvailable(networkUp));
    }

    /**
     * Loads url on the UI thread and blocks until onPageFinished is called.
     */
    public void loadUrlSync(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            final String url) throws Exception {
        loadUrlSync(awContents, onPageFinishedHelper, url, null);
    }

    public void loadUrlSync(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            final String url, final Map<String, String> extraHeaders) throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadUrlAsync(awContents, url, extraHeaders);
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    public void loadUrlSyncAndExpectError(final AwContents awContents,
            CallbackHelper onPageFinishedHelper, CallbackHelper onReceivedErrorHelper,
            final String url) throws Exception {
        int onErrorCallCount = onReceivedErrorHelper.getCallCount();
        int onFinishedCallCount = onPageFinishedHelper.getCallCount();
        loadUrlAsync(awContents, url);
        onReceivedErrorHelper.waitForCallback(
                onErrorCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        onPageFinishedHelper.waitForCallback(
                onFinishedCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    /**
     * Loads url on the UI thread but does not block.
     */
    public void loadUrlAsync(final AwContents awContents, final String url) throws Exception {
        loadUrlAsync(awContents, url, null);
    }

    public void loadUrlAsync(
            final AwContents awContents, final String url, final Map<String, String> extraHeaders) {
        ThreadUtils.runOnUiThreadBlocking(() -> awContents.loadUrl(url, extraHeaders));
    }

    /**
     * Posts url on the UI thread and blocks until onPageFinished is called.
     */
    public void postUrlSync(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            final String url, byte[] postData) throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        postUrlAsync(awContents, url, postData);
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    /**
     * Loads url on the UI thread but does not block.
     */
    public void postUrlAsync(final AwContents awContents, final String url, byte[] postData)
            throws Exception {
        class PostUrl implements Runnable {
            byte[] mPostData;
            public PostUrl(byte[] postData) {
                mPostData = postData;
            }
            @Override
            public void run() {
                awContents.postUrl(url, mPostData);
            }
        }
        ThreadUtils.runOnUiThreadBlocking(new PostUrl(postData));
    }

    /**
     * Loads data on the UI thread and blocks until onPageFinished is called.
     */
    public void loadDataSync(final AwContents awContents, CallbackHelper onPageFinishedHelper,
            final String data, final String mimeType, final boolean isBase64Encoded)
            throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadDataAsync(awContents, data, mimeType, isBase64Encoded);
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    public void loadDataSyncWithCharset(final AwContents awContents,
            CallbackHelper onPageFinishedHelper, final String data, final String mimeType,
            final boolean isBase64Encoded, final String charset) throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> awContents.loadUrl(LoadUrlParams.createLoadDataParams(
                                data, mimeType, isBase64Encoded, charset)));
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    /**
     * Loads data on the UI thread but does not block.
     */
    public void loadDataAsync(final AwContents awContents, final String data, final String mimeType,
            final boolean isBase64Encoded) throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> awContents.loadData(data, mimeType, isBase64Encoded ? "base64" : null));
    }

    public void loadDataWithBaseUrlSync(final AwContents awContents,
            CallbackHelper onPageFinishedHelper, final String data, final String mimeType,
            final boolean isBase64Encoded, final String baseUrl, final String historyUrl)
            throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadDataWithBaseUrlAsync(awContents, data, mimeType, isBase64Encoded, baseUrl, historyUrl);
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    public void loadDataWithBaseUrlAsync(final AwContents awContents, final String data,
            final String mimeType, final boolean isBase64Encoded, final String baseUrl,
            final String historyUrl) throws Throwable {
        runOnUiThread(() -> awContents.loadDataWithBaseURL(baseUrl, data, mimeType,
                                      isBase64Encoded ? "base64" : null, historyUrl));
    }

    /**
     * Reloads the current page synchronously.
     */
    public void reloadSync(final AwContents awContents, CallbackHelper onPageFinishedHelper)
            throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> awContents.getNavigationController().reload(true));
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    /**
     * Stops loading on the UI thread.
     */
    public void stopLoading(final AwContents awContents) {
        ThreadUtils.runOnUiThreadBlocking(() -> awContents.stopLoading());
    }

    public void waitForVisualStateCallback(final AwContents awContents) throws Exception {
        final CallbackHelper ch = new CallbackHelper();
        final int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            final long requestId = 666;
            awContents.insertVisualStateCallback(requestId, new AwContents.VisualStateCallback() {
                @Override
                public void onComplete(long id) {
                    Assert.assertEquals(requestId, id);
                    ch.notifyCalled();
                }
            });
        });
        ch.waitForCallback(chCount);
    }

    public void insertVisualStateCallbackOnUIThread(final AwContents awContents,
            final long requestId, final AwContents.VisualStateCallback callback) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> awContents.insertVisualStateCallback(requestId, callback));
    }

    // Waits for the pixel at the center of AwContents to color up into expectedColor.
    // Note that this is a stricter condition that waiting for a visual state callback,
    // as visual state callback only indicates that *something* has appeared in WebView.
    public void waitForPixelColorAtCenterOfView(final AwContents awContents,
            final AwTestContainerView testContainerView, final int expectedColor) throws Exception {
        pollUiThread(() -> GraphicsTestUtils.getPixelColorAtCenterOfView(
                    awContents, testContainerView) == expectedColor);
    }

    public AwTestContainerView createAwTestContainerView(final AwContentsClient awContentsClient) {
        return createAwTestContainerView(awContentsClient, false, null);
    }

    public AwTestContainerView createAwTestContainerView(final AwContentsClient awContentsClient,
            boolean supportsLegacyQuirks, final TestDependencyFactory testDependencyFactory) {
        AwTestContainerView testContainerView = createDetachedAwTestContainerView(
                awContentsClient, supportsLegacyQuirks, testDependencyFactory);
        getActivity().addView(testContainerView);
        testContainerView.requestFocus();
        return testContainerView;
    }

    public AwBrowserContext getAwBrowserContext() {
        return mBrowserContext;
    }

    public AwTestContainerView createDetachedAwTestContainerView(
            final AwContentsClient awContentsClient) {
        return createDetachedAwTestContainerView(awContentsClient, false, null);
    }

    public AwTestContainerView createDetachedAwTestContainerView(
            final AwContentsClient awContentsClient, boolean supportsLegacyQuirks,
            TestDependencyFactory testDependencyFactory) {
        if (testDependencyFactory == null) {
            testDependencyFactory = createTestDependencyFactory();
        }
        boolean allowHardwareAcceleration = isHardwareAcceleratedTest();
        final AwTestContainerView testContainerView =
                testDependencyFactory.createAwTestContainerView(
                        getActivity(), allowHardwareAcceleration);

        AwSettings awSettings =
                testDependencyFactory.createAwSettings(getActivity(), supportsLegacyQuirks);
        AwContents awContents = testDependencyFactory.createAwContents(mBrowserContext,
                testContainerView, testContainerView.getContext(),
                testContainerView.getInternalAccessDelegate(),
                testContainerView.getNativeDrawFunctorFactory(), awContentsClient, awSettings,
                testDependencyFactory);
        testContainerView.initialize(awContents);
        return testContainerView;
    }

    public boolean isHardwareAcceleratedTest() {
        return !testMethodHasAnnotation(DisableHardwareAccelerationForTest.class);
    }

    public AwTestContainerView createAwTestContainerViewOnMainSync(final AwContentsClient client)
            throws Exception {
        return createAwTestContainerViewOnMainSync(client, false, null);
    }

    public AwTestContainerView createAwTestContainerViewOnMainSync(
            final AwContentsClient client, final boolean supportsLegacyQuirks) {
        return createAwTestContainerViewOnMainSync(client, supportsLegacyQuirks, null);
    }

    public AwTestContainerView createAwTestContainerViewOnMainSync(final AwContentsClient client,
            final boolean supportsLegacyQuirks, final TestDependencyFactory testDependencyFactory) {
        return ThreadUtils.runOnUiThreadBlockingNoException(() -> createAwTestContainerView(
                    client, supportsLegacyQuirks, testDependencyFactory));
    }

    public void destroyAwContentsOnMainSync(final AwContents awContents) {
        if (awContents == null) return;
        ThreadUtils.runOnUiThreadBlocking(() -> awContents.destroy());
    }

    public String getTitleOnUiThread(final AwContents awContents) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> awContents.getTitle());
    }

    public AwSettings getAwSettingsOnUiThread(final AwContents awContents) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> awContents.getSettings());
    }

    /**
     * Verify double quotes in both sides of the raw string. Strip the double quotes and
     * returns rest of the string.
     */
    public String maybeStripDoubleQuotes(String raw) {
        Assert.assertNotNull(raw);
        Matcher m = MAYBE_QUOTED_STRING.matcher(raw);
        Assert.assertTrue(m.matches());
        return m.group(2);
    }

    /**
     * Executes the given snippet of JavaScript code within the given ContentView. Returns the
     * result of its execution in JSON format.
     */
    public String executeJavaScriptAndWaitForResult(final AwContents awContents,
            TestAwContentsClient viewClient, final String code) throws Exception {
        return JSUtils.executeJavaScriptAndWaitForResult(
                InstrumentationRegistry.getInstrumentation(), awContents,
                viewClient.getOnEvaluateJavaScriptResultHelper(), code);
    }

    /**
     * Executes JavaScript code within the given ContentView to get the text content in
     * document body. Returns the result string without double quotes.
     */
    public String getJavaScriptResultBodyTextContent(
            final AwContents awContents, final TestAwContentsClient viewClient) throws Exception {
        String raw = executeJavaScriptAndWaitForResult(
                awContents, viewClient, "document.body.textContent");
        return maybeStripDoubleQuotes(raw);
    }

    /**
     * Wrapper around CriteriaHelper.pollInstrumentationThread. This uses AwActivityTestRule-specifc
     * timeouts and treats timeouts and exceptions as test failures automatically.
     */
    public static void pollInstrumentationThread(final Callable<Boolean> callable)
            throws Exception {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return callable.call();
                } catch (Throwable e) {
                    Log.e(TAG, "Exception while polling.", e);
                    return false;
                }
            }
        }, WAIT_TIMEOUT_MS, CHECK_INTERVAL);
    }

    /**
     * Wrapper around {@link AwActivityTestRule#pollInstrumentationThread()} but runs the
     * callable on the UI thread.
     */
    public void pollUiThread(final Callable<Boolean> callable) throws Exception {
        pollInstrumentationThread(() -> ThreadUtils.runOnUiThreadBlocking(callable));
    }

    /**
     * Clears the resource cache. Note that the cache is per-application, so this will clear the
     * cache for all WebViews used.
     */
    public void clearCacheOnUiThread(final AwContents awContents, final boolean includeDiskFiles)
            throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> awContents.clearCache(includeDiskFiles));
    }

    /**
     * Returns pure page scale.
     */
    public float getScaleOnUiThread(final AwContents awContents) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> awContents.getPageScaleFactor());
    }

    /**
     * Returns page scale multiplied by the screen density.
     */
    public float getPixelScaleOnUiThread(final AwContents awContents) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> awContents.getScale());
    }

    /**
     * Returns whether a user can zoom the page in.
     */
    public boolean canZoomInOnUiThread(final AwContents awContents) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> awContents.canZoomIn());
    }

    /**
     * Returns whether a user can zoom the page out.
     */
    public boolean canZoomOutOnUiThread(final AwContents awContents) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> awContents.canZoomOut());
    }

    public void killRenderProcessOnUiThreadAsync(final AwContents awContents) throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> awContents.killRenderProcess());
    }

    /**
     * Loads the main html then triggers the popup window.
     */
    public void triggerPopup(final AwContents parentAwContents,
            TestAwContentsClient parentAwContentsClient, TestWebServer testWebServer,
            String mainHtml, String popupHtml, String popupPath, String triggerScript)
            throws Exception {
        enableJavaScriptOnUiThread(parentAwContents);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            parentAwContents.getSettings().setSupportMultipleWindows(true);
            parentAwContents.getSettings().setJavaScriptCanOpenWindowsAutomatically(true);
        });

        final String parentUrl = testWebServer.setResponse("/popupParent.html", mainHtml, null);
        if (popupHtml != null) {
            testWebServer.setResponse(popupPath, popupHtml, null);
        } else {
            testWebServer.setResponseWithNoContentStatus(popupPath);
        }

        parentAwContentsClient.getOnCreateWindowHelper().setReturnValue(true);
        loadUrlSync(parentAwContents, parentAwContentsClient.getOnPageFinishedHelper(), parentUrl);

        TestAwContentsClient.OnCreateWindowHelper onCreateWindowHelper =
                parentAwContentsClient.getOnCreateWindowHelper();
        int currentCallCount = onCreateWindowHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> parentAwContents.evaluateJavaScriptForTests(triggerScript, null));
        onCreateWindowHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    /**
     * Supplies the popup window with AwContents then waits for the popup window to finish loading.
     * @param parentAwContents Parent webview's AwContents.
     */
    public PopupInfo connectPendingPopup(AwContents parentAwContents) throws Exception {
        PopupInfo popupInfo = createPopupContents(parentAwContents);
        loadPopupContents(parentAwContents, popupInfo, null);
        return popupInfo;
    }

    /**
     * Creates a popup window with AwContents.
     */
    public PopupInfo createPopupContents(final AwContents parentAwContents) throws Exception {
        TestAwContentsClient popupContentsClient;
        AwTestContainerView popupContainerView;
        final AwContents popupContents;
        popupContentsClient = new TestAwContentsClient();
        popupContainerView = createAwTestContainerViewOnMainSync(popupContentsClient);
        popupContents = popupContainerView.getAwContents();
        enableJavaScriptOnUiThread(popupContents);
        return new PopupInfo(popupContentsClient, popupContainerView, popupContents);
    }

    /**
     * Waits for the popup window to finish loading.
     * @param parentAwContents Parent webview's AwContents.
     * @param info The PopupInfo.
     * @param onCreateWindowHandler An instance of OnCreateWindowHandler. null if there isn't.
     */
    public void loadPopupContents(final AwContents parentAwContents, PopupInfo info,
            OnCreateWindowHandler onCreateWindowHandler) throws Exception {
        TestAwContentsClient popupContentsClient = info.popupContentsClient;
        AwTestContainerView popupContainerView = info.popupContainerView;
        final AwContents popupContents = info.popupContents;

        if (onCreateWindowHandler != null) onCreateWindowHandler.onCreateWindow(popupContents);

        ThreadUtils.runOnUiThreadBlocking(
                () -> parentAwContents.supplyContentsForPopup(popupContents));

        OnPageFinishedHelper onPageFinishedHelper = popupContentsClient.getOnPageFinishedHelper();
        int finishCallCount = onPageFinishedHelper.getCallCount();
        TestAwContentsClient.OnReceivedTitleHelper onReceivedTitleHelper =
                popupContentsClient.getOnReceivedTitleHelper();
        int titleCallCount = onReceivedTitleHelper.getCallCount();

        onPageFinishedHelper.waitForCallback(
                finishCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        onReceivedTitleHelper.waitForCallback(
                titleCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    private boolean testMethodHasAnnotation(Class<? extends Annotation> clazz) {
        return mCurrentTestDescription.getAnnotation(clazz) != null ? true : false;
    }

    /**
     * Factory class used in creation of test AwContents instances. Test cases
     * can provide subclass instances to the createAwTest* methods in order to
     * create an AwContents instance with injected test dependencies.
     */
    public static class TestDependencyFactory extends AwContents.DependencyFactory {
        public AwTestContainerView createAwTestContainerView(
                AwTestRunnerActivity activity, boolean allowHardwareAcceleration) {
            return new AwTestContainerView(activity, allowHardwareAcceleration);
        }

        public AwSettings createAwSettings(Context context, boolean supportsLegacyQuirks) {
            return new AwSettings(context, false /* isAccessFromFileURLsGrantedByDefault */,
                    supportsLegacyQuirks, false /* allowEmptyDocumentPersistence */,
                    true /* allowGeolocationOnInsecureOrigins */,
                    false /* doNotUpdateSelectionOnMutatingSelectionRange */);
        }

        public AwContents createAwContents(AwBrowserContext browserContext, ViewGroup containerView,
                Context context, InternalAccessDelegate internalAccessAdapter,
                NativeDrawFunctorFactory nativeDrawFunctorFactory, AwContentsClient contentsClient,
                AwSettings settings, DependencyFactory dependencyFactory) {
            return new AwContents(browserContext, containerView, context, internalAccessAdapter,
                    nativeDrawFunctorFactory, contentsClient, settings, dependencyFactory);
        }
    }

    /**
     * POD object for holding references to helper objects of a popup window.
     */
    public static class PopupInfo {
        public final TestAwContentsClient popupContentsClient;
        public final AwTestContainerView popupContainerView;
        public final AwContents popupContents;

        public PopupInfo(TestAwContentsClient popupContentsClient,
                AwTestContainerView popupContainerView, AwContents popupContents) {
            this.popupContentsClient = popupContentsClient;
            this.popupContainerView = popupContainerView;
            this.popupContents = popupContents;
        }
    }
}
