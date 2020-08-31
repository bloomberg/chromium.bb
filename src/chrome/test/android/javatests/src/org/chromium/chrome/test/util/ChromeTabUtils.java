// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.app.Instrumentation;
import android.support.test.InstrumentationRegistry;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;

import org.junit.Assert;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabWebContentsObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;

import java.util.List;
import java.util.Locale;
import java.util.concurrent.Callable;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * A utility class that contains methods generic to all Tabs tests.
 */
public class ChromeTabUtils {
    private static final String TAG = "ChromeTabUtils";
    public static final int TITLE_UPDATE_TIMEOUT_SECONDS = 3;

    /**
     * The required page load percentage for the page to be considered ready assuming the
     * TextureView is also ready.
     */
    private static final float CONSIDERED_READY_LOAD_PERCENTAGE = 1;

    /**
     * An observer that waits for a Tab to load a page.
     *
     * The observer can be configured to either wait for the Tab to load a specific page
     * (if expectedUrl is non-null) or any page (otherwise). On seeing the tab finish
     * a page load or crash, the observer will notify the provided callback and stop
     * watching the tab. On load stop, the observer will decrement the provided latch
     * and continue watching the page in case the tab subsequently crashes or finishes
     * a page load.
     *
     * This may seem complicated, but it's intended to handle three distinct cases:
     *  1) Successful page load + observer starts watching before onPageLoadFinished fires.
     *     This is the most normal case: onPageLoadFinished fires, then onLoadStopped fires,
     *     and we see both.
     *  2) Crash on page load. onLoadStopped fires, then onCrash fires, and we see both.
     *  3) Successful page load + observer starts watching after onPageLoadFinished fires.
     *     We miss the onPageLoadFinished and *only* see onLoadStopped.
     *
     * Receiving onPageLoadFinished is sufficient to know that we're dealing with scenario #1.
     * Receiving onCrash is sufficient to know that we're dealing with scenario #2.
     * Receiving onLoadStopped without a preceding onPageLoadFinished indicates that we're dealing
     * with either scenario #2 *or* #3, so we have to keep watching for a call to onCrash.
     */
    private static class TabPageLoadedObserver extends EmptyTabObserver {
        private CallbackHelper mCallback;
        private String mExpectedUrl;
        private CountDownLatch mLoadStoppedLatch;

        public TabPageLoadedObserver(CallbackHelper loadCompleteCallback, String expectedUrl,
                CountDownLatch loadStoppedLatch) {
            mCallback = loadCompleteCallback;
            mExpectedUrl = expectedUrl;
            mLoadStoppedLatch = loadStoppedLatch;
        }

        @Override
        public void onCrash(Tab tab) {
            mCallback.notifyFailed("Tab crashed :(");
            tab.removeObserver(this);
        }

        @Override
        public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
            mLoadStoppedLatch.countDown();
        }

        @Override
        public void onPageLoadFinished(Tab tab, String url) {
            if (mExpectedUrl == null || TextUtils.equals(url, mExpectedUrl)) {
                mCallback.notifyCalled();
                tab.removeObserver(this);
            }
        }
    }

    private static boolean loadComplete(Tab tab, String url) {
        return !tab.isLoading() && (url == null || TextUtils.equals(tab.getUrlString(), url))
                && !tab.getWebContents().isLoadingToDifferentDocument();
    }

    /**
     * Waits for the given tab to finish loading the given URL, or, if the given URL is
     * null, waits for the current page to load.
     *
     * @param tab The tab to wait for the page loading to be complete.
     * @param url The URL that will be waited to load for.  Pass in null if loading the
     *            current page is sufficient.
     */
    public static void waitForTabPageLoaded(final Tab tab, @Nullable final String url) {
        waitForTabPageLoaded(tab, url, null, 10L);
    }

    /**
     * Waits for the given tab to load the given URL, or, if the given URL is null, waits
     * for the triggered load to complete.
     *
     * @param tab The tab to wait for the page loading to be complete.
     * @param url The expected url of the loaded page.  Pass in null if loading the
     *            current page is sufficient.
     * @param loadTrigger The trigger action that will result in a page load finished event
     *                    to be fired (not run on the UI thread by default).
     */
    public static void waitForTabPageLoaded(
            final Tab tab, @Nullable final String url, @Nullable Runnable loadTrigger) {
        waitForTabPageLoaded(tab, url, loadTrigger, CallbackHelper.WAIT_TIMEOUT_SECONDS);
    }

    /**
     * Waits for the given tab to finish loading its current page.
     *
     * @param tab The tab to wait for the page loading to be complete.
     * @param loadTrigger The trigger action that will result in a page load finished event
     *                    to be fired (not run on the UI thread by default).
     * @param secondsToWait The number of seconds to wait for the page to be loaded.
     */
    public static void waitForTabPageLoaded(
            final Tab tab, Runnable loadTrigger, long secondsToWait) {
        waitForTabPageLoaded(tab, null, loadTrigger, secondsToWait);
    }

    /**
     * Waits for the given tab to load the given URL, or, if the given URL is null, waits
     * for the triggered load to complete.
     *
     * @param tab The tab to wait for the page loading to be complete.
     * @param url The expected url of the loaded page.  Pass in null if loading the
     *            current page is sufficient.
     * @param loadTrigger The trigger action that will result in a page load finished event
     *                    to be fired (not run on the UI thread by default).  Pass in null if the
     *                    load is triggered externally.
     * @param secondsToWait The number of seconds to wait for the page to be loaded.
     */
    public static void waitForTabPageLoaded(final Tab tab, @Nullable final String url,
            @Nullable Runnable loadTrigger, long secondsToWait) {
        Assert.assertFalse(ThreadUtils.runningOnUiThread());

        final CountDownLatch loadStoppedLatch = new CountDownLatch(1);
        final CallbackHelper loadedCallback = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Don't check for the load being already complete if there is a trigger to run.
            if (loadTrigger == null && loadComplete(tab, url)) {
                loadedCallback.notifyCalled();
                return;
            }
            tab.addObserver(new TabPageLoadedObserver(loadedCallback, url, loadStoppedLatch));
        });
        if (loadTrigger != null) {
            loadTrigger.run();
        }
        try {
            loadedCallback.waitForCallback(0, 1, secondsToWait, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            // In the event that:
            //  1) the tab is on the correct page
            //  2) we weren't notified that the page load finished
            //  3) we *were* notified that the tab stopped loading
            //  4) the tab didn't crash
            //
            // then it's likely the case that we started observing the tab after
            // onPageLoadFinished but before onLoadStopped. (The latter sets tab.mIsLoading to
            // false.) Try to carry on with the test.
            if (loadStoppedLatch.getCount() == 0 && loadComplete(tab, url)) {
                Log.w(TAG,
                        "onPageLoadFinished was never called, but loading stopped "
                                + "on the expected page. Tentatively continuing.");
            } else {
                WebContents webContents = tab.getWebContents();
                Assert.fail(String.format(Locale.ENGLISH,
                        "Page did not load.  Tab information at time of failure -- "
                                + "expected url: '%s', actual URL: '%s', load progress: %d, is "
                                + "loading: %b, web contents init: %b, web contents loading: %b",
                        url, tab.getUrlString(), Math.round(100 * tab.getProgress()),
                        tab.isLoading(), webContents != null,
                        webContents == null ? false : webContents.isLoadingToDifferentDocument()));
            }
        }
    }

    /**
     * Waits for the given tab to start loading its current page.
     *
     * @param tab The tab to wait for the page loading to be started.
     * @param expectedUrl The expected url of the started page load.  Pass in null if starting
     *                    any load is sufficient.
     * @param loadTrigger The trigger action that will result in a page load started event
     *                    to be fired (not run on the UI thread by default).
     */
    public static void waitForTabPageLoadStart(
            final Tab tab, @Nullable final String expectedUrl, Runnable loadTrigger) {
        waitForTabPageLoadStart(tab, expectedUrl, loadTrigger, CallbackHelper.WAIT_TIMEOUT_SECONDS);
    }

    /**
     * Waits for the given tab to start loading its current page.
     *
     * @param tab The tab to wait for the page loading to be started.
     * @param expectedUrl The expected url of the started page load.  Pass in null if starting
     *                    any load is sufficient.
     * @param loadTrigger The trigger action that will result in a page load started event
     *                    to be fired (not run on the UI thread by default).
     * @param secondsToWait The number of seconds to wait for the page to be load to be started.
     */
    public static void waitForTabPageLoadStart(final Tab tab, @Nullable final String expectedUrl,
            Runnable loadTrigger, long secondsToWait) {
        final CallbackHelper startedCallback = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.addObserver(new EmptyTabObserver() {
                @Override
                public void onPageLoadStarted(Tab tab, String url) {
                    if (expectedUrl == null || TextUtils.equals(url, expectedUrl)) {
                        startedCallback.notifyCalled();
                        tab.removeObserver(this);
                    }
                }
            });
        });
        loadTrigger.run();
        try {
            startedCallback.waitForCallback(0, 1, secondsToWait, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            Assert.fail("Page did not start loading.  Tab information at time of failure --"
                    + " url: " + tab.getUrlString() + ", load progress: " + tab.getProgress()
                    + ", is loading: " + Boolean.toString(tab.isLoading()));
        }
    }

    /**
     * An observer that waits for a Tab to become interactable.
     *
     * Notifies the provided callback when:
     *  - the page has become interactable
     *  - the tab has been hidden and will not become interactable.
     * Stops observing with a failure if the tab has crashed.
     *
     * We treat the hidden case as success to handle loads in which a page immediately closes itself
     * or opens a new foreground tab (popup), and may not become interactable.
     */
    private static class TabPageInteractableObserver extends EmptyTabObserver {
        private Tab mTab;
        private CallbackHelper mCallback;

        public TabPageInteractableObserver(Tab tab, CallbackHelper interactableCallback) {
            mTab = tab;
            mCallback = interactableCallback;
        }

        @Override
        public void onCrash(Tab tab) {
            mCallback.notifyFailed("Tab crashed :(");
            mTab.removeObserver(this);
        }

        @Override
        public void onHidden(Tab tab, @TabHidingType int type) {
            mCallback.notifyCalled();
            mTab.removeObserver(this);
        }

        @Override
        public void onInteractabilityChanged(Tab tab, boolean interactable) {
            if (interactable) {
                mCallback.notifyCalled();
                mTab.removeObserver(this);
            }
        }
    }

    /**
     * Waits for the tab to become interactable. This occurs after load, once all view
     * animations have completed.
     *
     * @param tab The tab to wait for interactability on.
     */
    public static void waitForInteractable(final Tab tab) {
        Assert.assertFalse(ThreadUtils.runningOnUiThread());

        final CallbackHelper interactableCallback = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // If a tab is hidden, don't wait for interactivity. See note in
            // TabPageInteractableObserver.
            if (tab.isUserInteractable() || tab.isHidden()) {
                interactableCallback.notifyCalled();
                return;
            }
            tab.addObserver(new TabPageInteractableObserver(tab, interactableCallback));
        });

        try {
            interactableCallback.waitForCallback(0, 1, 10L, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            Assert.fail("Page never became interactable.");
        }
    }

    /**
     * Switch to the given TabIndex in the current tabModel.
     * @param tabIndex
     */
    public static void switchTabInCurrentTabModel(final ChromeActivity activity,
            final int tabIndex) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { TabModelUtils.setIndex(activity.getCurrentTabModel(), tabIndex); });
    }

    /**
     * Simulates a click to the normal (not incognito) new tab button.
     * <p>
     * Does not wait for the tab to be loaded.
     */
    public static void clickNewTabButton(
            Instrumentation instrumentation, ChromeTabbedActivity activity) {
        final TabModel normalTabModel = activity.getTabModelSelector().getModel(false);
        final CallbackHelper createdCallback = new CallbackHelper();
        normalTabModel.addObserver(new TabModelObserver() {
            @Override
            public void didAddTab(
                    Tab tab, @TabLaunchType int type, @TabCreationState int creationState) {
                createdCallback.notifyCalled();
                normalTabModel.removeObserver(this);
            }
        });
        // Tablet and phone have different new tab buttons; click the right one.
        if (activity.isTablet()) {
            StripLayoutHelper strip =
                    TabStripUtils.getStripLayoutHelper(activity, false /* incognito */);
            CompositorButton newTabButton = strip.getNewTabButton();
            TabStripUtils.clickCompositorButton(newTabButton, instrumentation, activity);
            instrumentation.waitForIdleSync();
        } else {
            TouchCommon.singleClickView(activity.findViewById(R.id.new_tab_button));
        }

        try {
            createdCallback.waitForCallback(null, 0, 1, 10, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            Assert.fail("Never received tab creation event");
        }
    }

    /**
     * Creates a new tab by invoking the 'New Tab' menu item.
     * <p>
     * Returns when the tab has been created and has finished navigating.
     */
    public static void newTabFromMenu(
            Instrumentation instrumentation, final ChromeActivity activity) {
        newTabFromMenu(instrumentation, activity, false, true);
    }

    /**
     * Creates a new tab by invoking the 'New Tab' or 'New Incognito Tab' menu item.
     * <p>
     * Returns when the tab has been created and has finished navigating.
     */
    public static void newTabFromMenu(Instrumentation instrumentation,
            final ChromeActivity activity, boolean incognito, boolean waitForNtpLoad) {
        final CallbackHelper createdCallback = new CallbackHelper();
        final CallbackHelper selectedCallback = new CallbackHelper();

        TabModel tabModel = activity.getTabModelSelector().getModel(incognito);
        TabModelObserver observer = new TabModelObserver() {
            @Override
            public void didAddTab(
                    Tab tab, @TabLaunchType int type, @TabCreationState int creationState) {
                createdCallback.notifyCalled();
            }

            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                selectedCallback.notifyCalled();
            }
        };
        tabModel.addObserver(observer);

        MenuUtils.invokeCustomMenuActionSync(instrumentation, activity,
                incognito ? R.id.new_incognito_tab_menu_id : R.id.new_tab_menu_id);

        try {
            createdCallback.waitForCallback(0);
        } catch (TimeoutException ex) {
            Assert.fail("Never received tab created event");
        }
        try {
            selectedCallback.waitForCallback(0);
        } catch (TimeoutException ex) {
            Assert.fail("Never received tab selected event");
        }
        tabModel.removeObserver(observer);

        Tab tab = activity.getActivityTab();
        waitForTabPageLoaded(tab, (String) null);
        if (waitForNtpLoad) NewTabPageTestUtils.waitForNtpLoaded(tab);
        instrumentation.waitForIdleSync();
        Log.d(TAG, "newTabFromMenu <<");
    }

    /**
     * New multiple tabs by invoking the 'new' menu item n times.
     * @param n The number of tabs you want to create.
     */
    public static void newTabsFromMenu(
            Instrumentation instrumentation, ChromeTabbedActivity activity, int n) {
        while (n > 0) {
            newTabFromMenu(instrumentation, activity);
            --n;
        }
    }

    /**
     * Creates a new tab in the specified model then waits for it to load.
     * <p>
     * Returns when the tab has been created and finishes loading.
     */
    public static void fullyLoadUrlInNewTab(Instrumentation instrumentation,
            final ChromeTabbedActivity activity, final String url, final boolean incognito) {
        newTabFromMenu(instrumentation, activity, incognito, false);

        final Tab tab = activity.getActivityTab();
        waitForTabPageLoaded(tab, url, new Runnable() {
            @Override
            public void run() {
                loadUrlOnUiThread(tab, url);
            }
        });
        instrumentation.waitForIdleSync();
    }

    public static void loadUrlOnUiThread(final Tab tab, final String url) {
        TestThreadUtils.runOnUiThreadBlocking(() -> { tab.loadUrl(new LoadUrlParams(url)); });
    }

    /**
     * Ensure that at least some given number of tabs are open.
     */
    public static void ensureNumOpenTabs(
            Instrumentation instrumentation, ChromeTabbedActivity activity, int newCount) {
        int curCount = getNumOpenTabs(activity);
        if (curCount < newCount) {
            newTabsFromMenu(instrumentation, activity, newCount - curCount);
        }
    }

    /**
     * Fetch the number of tabs open in the current model.
     */
    public static int getNumOpenTabs(final ChromeActivity activity) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
            @Override
            public Integer call() {
                return activity.getCurrentTabModel().getCount();
            }
        });
    }

    /**
     * Closes the current tab through TabModelSelector.
     * <p>
     * Returns after the tab has been closed.
     */
    public static void closeCurrentTab(
            final Instrumentation instrumentation, final ChromeActivity activity) {
        closeTabWithAction(instrumentation, activity, new Runnable() {
            @Override
            public void run() {
                instrumentation.runOnMainSync(new Runnable() {
                    @Override
                    public void run() {
                        TabModelUtils.closeCurrentTab(activity.getCurrentTabModel());
                    }
                });
            }
        });
    }

    /**
     * Closes a tab with the given action and waits for a tab closure to be observed.
     */
    public static void closeTabWithAction(
            Instrumentation instrumentation, final ChromeActivity activity, Runnable action) {
        final CallbackHelper closeCallback = new CallbackHelper();
        final TabModelObserver observer = new TabModelObserver() {
            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                closeCallback.notifyCalled();
            }
        };
        instrumentation.runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelSelector selector = activity.getTabModelSelector();
                for (TabModel tabModel : selector.getModels()) {
                    tabModel.addObserver(observer);
                }
            }
        });

        action.run();

        try {
            closeCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Tab closed event was never received");
        }
        instrumentation.runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelSelector selector = activity.getTabModelSelector();
                for (TabModel tabModel : selector.getModels()) {
                    tabModel.removeObserver(observer);
                }
            }
        });
        instrumentation.waitForIdleSync();
        Log.d(TAG, "closeTabWithAction <<");
    }

    /**
     * Close all tabs and waits for all tabs pending closure to be observed.
     */
    public static void closeAllTabs(
            Instrumentation instrumentation, final ChromeTabbedActivity activity) {
        final CallbackHelper closeCallback = new CallbackHelper();
        final TabModelObserver observer = new TabModelObserver() {
            @Override
            public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
                closeCallback.notifyCalled();
            }
        };
        instrumentation.runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelSelector selector = activity.getTabModelSelector();
                for (TabModel tabModel : selector.getModels()) {
                    tabModel.addObserver(observer);
                }
            }
        });

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getTabModelSelector().closeAllTabs(); });

        try {
            closeCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("All tabs pending closure event was never received");
        }
        instrumentation.runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelSelector selector = activity.getTabModelSelector();
                for (TabModel tabModel : selector.getModels()) {
                    tabModel.removeObserver(observer);
                }
            }
        });
        instrumentation.waitForIdleSync();
    }

    /**
     * Selects a tab with the given action and waits for the selection event to be observed.
     */
    public static void selectTabWithAction(
            Instrumentation instrumentation, final ChromeTabbedActivity activity, Runnable action) {
        final CallbackHelper selectCallback = new CallbackHelper();
        final TabModelObserver observer = new TabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                selectCallback.notifyCalled();
            }
        };
        instrumentation.runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelSelector selector = activity.getTabModelSelector();
                for (TabModel tabModel : selector.getModels()) {
                    tabModel.addObserver(observer);
                }
            }
        });

        action.run();

        try {
            selectCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Tab selected event was never received");
        }
        instrumentation.runOnMainSync(new Runnable() {
            @Override
            public void run() {
                TabModelSelector selector = activity.getTabModelSelector();
                for (TabModel tabModel : selector.getModels()) {
                    tabModel.removeObserver(observer);
                }
            }
        });
    }

    /**
     * Long presses the view, selects an item from the context menu, and
     * asserts that a new tab is opened and is incognito if expectIncognito is true.
     * For use in testing long-press context menu options that open new tabs.
     *
     * @param testRule The {@link ChromeTabbedActivityTestRule} used to retrieve the currently
     *                 running activity.
     * @param view The {@link View} to long press.
     * @param contextMenuItemId The context menu item to select on the view.
     * @param expectIncognito Whether the opened tab is expected to be incognito.
     * @param expectedUrl The expected url for the new tab.
     */
    public static void invokeContextMenuAndOpenInANewTab(ChromeTabbedActivityTestRule testRule,
            View view, int contextMenuItemId, boolean expectIncognito, final String expectedUrl)
            throws ExecutionException {
        final CallbackHelper createdCallback = new CallbackHelper();
        final TabModel tabModel =
                testRule.getActivity().getTabModelSelector().getModel(expectIncognito);
        tabModel.addObserver(new TabModelObserver() {
            @Override
            public void didAddTab(
                    Tab tab, @TabLaunchType int type, @TabCreationState int creationState) {
                if (TextUtils.equals(expectedUrl, tab.getUrlString())) {
                    createdCallback.notifyCalled();
                    tabModel.removeObserver(this);
                }
            }
        });

        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), view);
        Assert.assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                testRule.getActivity(), contextMenuItemId, 0));

        try {
            createdCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Never received tab creation event");
        }

        if (expectIncognito) {
            Assert.assertTrue(testRule.getActivity().getTabModelSelector().isIncognitoSelected());
        } else {
            Assert.assertFalse(testRule.getActivity().getTabModelSelector().isIncognitoSelected());
        }
    }

    /**
     * Long presses the view, selects an item from the context menu, and
     * asserts that a new tab is opened and is incognito if expectIncognito is true.
     * For use in testing long-press context menu options that open new tabs in a different
     * ChromeTabbedActivity instance.
     *
     * @param foregroundActivity The {@link ChromeTabbedActivity} currently in the foreground.
     * @param backgroundActivity The {@link ChromeTabbedActivity} currently in the background. The
     *                           new tab is expected to open in this activity.
     * @param view The {@link View} in the {@code foregroundActivity} to long press.
     * @param contextMenuItemId The context menu item to select on the view.
     * @param expectIncognito Whether the opened tab is expected to be incognito.
     * @param expectedUrl The expected url for the new tab.
     */
    public static void invokeContextMenuAndOpenInOtherWindow(
            ChromeTabbedActivity foregroundActivity, ChromeTabbedActivity backgroundActivity,
            View view, int contextMenuItemId, boolean expectIncognito, final String expectedUrl)
            throws ExecutionException {
        final CallbackHelper createdCallback = new CallbackHelper();
        final TabModel tabModel =
                backgroundActivity.getTabModelSelector().getModel(expectIncognito);
        tabModel.addObserver(new TabModelObserver() {
            @Override
            public void didAddTab(
                    Tab tab, @TabLaunchType int type, @TabCreationState int creationState) {
                if (TextUtils.equals(expectedUrl, tab.getUrlString())) {
                    createdCallback.notifyCalled();
                    tabModel.removeObserver(this);
                }
            }
        });

        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), view);
        Assert.assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                foregroundActivity, contextMenuItemId, 0));

        try {
            createdCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            Assert.fail("Never received tab creation event");
        }

        if (expectIncognito) {
            Assert.assertTrue(backgroundActivity.getTabModelSelector().isIncognitoSelected());
        } else {
            Assert.assertFalse(backgroundActivity.getTabModelSelector().isIncognitoSelected());
        }
    }

    /**
     * Issues a fake notification about the renderer being killed.
     *
     * @param tab {@link Tab} instance where the target renderer resides.
     * @param wasOomProtected True if the renderer was protected from the OS out-of-memory killer
     *                        (e.g. renderer for the currently selected tab)
     */
    public static void simulateRendererKilledForTesting(Tab tab, boolean wasOomProtected) {
        TabWebContentsObserver observer = TabWebContentsObserver.get(tab);
        if (observer != null) {
            observer.simulateRendererKilledForTesting(wasOomProtected);
        }
    }

    public static void waitForTitle(Tab tab, String newTitle) {
        TabTitleObserver titleObserver = new TabTitleObserver(tab, newTitle);
        try {
            titleObserver.waitForTitleUpdate(TITLE_UPDATE_TIMEOUT_SECONDS);
        } catch (TimeoutException e) {
            Assert.fail(String.format(Locale.ENGLISH,
                    "Tab title didn't update to %s in time.", newTitle));
        }
    }

    /**
     * @return Whether or not the loading and rendering of the page is done.
     */
    public static boolean isLoadingAndRenderingDone(Tab tab) {
        return isRendererReady(tab) && tab.getProgress() >= CONSIDERED_READY_LOAD_PERCENTAGE;
    }

    /**
     * @return Whether or not the tab has something valid to render.
     */
    public static boolean isRendererReady(Tab tab) {
        if (tab.getNativePage() != null) return true;
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return false;

        RenderWidgetHostView rwhv = webContents.getRenderWidgetHostView();
        return rwhv != null && rwhv.isReady();
    }
}
