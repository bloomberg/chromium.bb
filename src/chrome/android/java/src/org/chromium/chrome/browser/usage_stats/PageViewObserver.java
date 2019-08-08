// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.usage.UsageStatsManager;
import android.content.Context;
import android.net.Uri;
import android.webkit.URLUtil;

import org.chromium.base.Log;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.content_public.browser.NavigationHandle;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Class that observes url and tab changes in order to track when browsing stops and starts for each
 * visited fully-qualified domain name (FQDN).
 */
@SuppressLint("NewApi")
public class PageViewObserver {
    private static final String TAG = "PageViewObserver";
    private static final String AMP_QUERY_PARAM = "amp_js_v";

    private final Activity mActivity;
    private final TabModelSelectorTabModelObserver mTabModelObserver;
    private final TabModelSelector mTabModelSelector;
    private final TabObserver mTabObserver;
    private final EventTracker mEventTracker;
    private final TokenTracker mTokenTracker;
    private final SuspensionTracker mSuspensionTracker;

    private Tab mCurrentTab;
    private String mLastFqdn;

    public PageViewObserver(Activity activity, TabModelSelector tabModelSelector,
            EventTracker eventTracker, TokenTracker tokenTracker,
            SuspensionTracker suspensionTracker) {
        mActivity = activity;
        mTabModelSelector = tabModelSelector;
        mEventTracker = eventTracker;
        mTokenTracker = tokenTracker;
        mSuspensionTracker = suspensionTracker;
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onShown(Tab tab, @TabSelectionType int type) {
                if (!tab.isLoading() && !tab.isBeingRestored()) {
                    updateUsingCanonicalUrl(tab);
                }
            }

            @Override
            public void onHidden(Tab tab, @TabHidingType int type) {
                updateUrl(null);
            }

            @Override
            public void onUpdateUrl(Tab tab, String url) {
                assert tab == mCurrentTab;
                String newFqdn = getValidFqdnOrEmptyString(url);
                // We don't call updateUrl() here to avoid reporting start events for domains
                // that never paint, e.g. link shorteners. We still need to check the SuspendedTab
                // state because a tab that's suspended can't paint, and the user could be
                // navigating away from a suspended domain.
                checkSuspendedTabState(mSuspensionTracker.isWebsiteSuspended(newFqdn), newFqdn);
            }

            @Override
            public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                assert tab == mCurrentTab;

                updateUsingCanonicalUrl(tab);
            }

            @Override
            public void onCrash(Tab tab) {
                updateUrl(null);
            }

            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                // We only check isLikelySubframeAmpNavigation here because this is the only
                // observer method that fires for subframe navigations. The reason we have the
                // isLikelySubframeAmpNavigation at all is that onDidFinishNavigation triggers very
                // often, and we only want to bother getting the canonical URL if we think there's a
                // good chance that it will actually be present.
                if (isLikelySubframeAmpNavigation(navigation)) {
                    updateUsingCanonicalUrl(tab);
                }
            }
        };

        mTabModelObserver = new TabModelSelectorTabModelObserver(tabModelSelector) {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                activeTabChanged(tab);
            }

            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                activeTabChanged(tab);
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                assert tab != null;
                if (tab != mCurrentTab) return;

                updateUrl(null);
                switchObserverToTab(null);
            }

            @Override
            public void tabRemoved(Tab tab) {
                assert tab != null;
                if (tab != mCurrentTab) return;

                updateUrl(null);
                switchObserverToTab(null);
            }
        };

        switchObserverToTab(tabModelSelector.getCurrentTab());
    }

    /** Notify PageViewObserver that {@code fqdn} was just suspended or un-suspended. */
    public void notifySiteSuspensionChanged(String fqdn, boolean isSuspended) {
        SuspendedTab suspendedTab = SuspendedTab.from(mCurrentTab);
        if (fqdn.equals(mLastFqdn) || fqdn.equals(suspendedTab.getFqdn())) {
            if (checkSuspendedTabState(isSuspended, fqdn)) {
                reportStop();
            }
        }
    }

    private void updateUsingCanonicalUrl(Tab tab) {
        if (tab.getWebContents() == null || tab.getWebContents().getMainFrame() == null) {
            updateUrl(tab.getUrl());
            return;
        }

        tab.getWebContents().getMainFrame().getCanonicalUrlForSharing((canonicalUrl) -> {
            if (tab != mCurrentTab) return;
            String urlToUse = tab.getUrl();
            if (canonicalUrl != null && canonicalUrl.length() > 0) {
                urlToUse = canonicalUrl;
            }
            updateUrl(urlToUse);
        });
    }

    /**
     * Updates our state from the previous url to {@code newUrl}. This can result in any/all of the
     * following:
     * 1. Suspension or un-suspension of mCurrentTab.
     * 2. Reporting a stop event for mLastFqdn.
     * 3. Reporting a start event for the fqdn of {@code newUrl}.
     */
    private void updateUrl(String newUrl) {
        String newFqdn = getValidFqdnOrEmptyString(newUrl);
        boolean isSameDomain = newFqdn.equals(mLastFqdn);
        boolean isValidProtocol = URLUtil.isHttpUrl(newUrl) || URLUtil.isHttpsUrl(newUrl);

        boolean didSuspend =
                checkSuspendedTabState(mSuspensionTracker.isWebsiteSuspended(newFqdn), newFqdn);

        if (mLastFqdn != null && (didSuspend || !isSameDomain)) {
            reportStop();
        }

        if (isValidProtocol && !didSuspend && !isSameDomain) {
            mLastFqdn = newFqdn;
            mEventTracker.addWebsiteEvent(new WebsiteEvent(
                    System.currentTimeMillis(), mLastFqdn, WebsiteEvent.EventType.START));
            reportToPlatformIfDomainIsTracked("reportUsageStart", mLastFqdn);
        }
    }

    /**
     * Hides or shows the SuspendedTab for mCurrentTab, based on:
     * 1. If it is currently shown or hidden
     * 2. Its current fqdn, if any.
     * 3. If fqdn is newly suspended or not.
     * There are really only two important cases; either the SuspendedTab is showing and should be
     * hidden, or it's hidden and should be shown.
     */
    private boolean checkSuspendedTabState(boolean isNewlySuspended, String fqdn) {
        SuspendedTab suspendedTab = SuspendedTab.from(mCurrentTab);
        // We don't need to do anything in situations where the current state matches the desired;
        // i.e. either the suspended tab is already showing with the correct fqdn, or the suspended
        // tab is hidden and should be hidden.
        if (isNewlySuspended && fqdn.equals(suspendedTab.getFqdn())) return false;
        if (!isNewlySuspended && !suspendedTab.isShowing()) return false;

        if (isNewlySuspended) {
            suspendedTab.show(fqdn);
            return true;
        } else {
            suspendedTab.removeIfPresent();
            if (!mCurrentTab.isLoading() && !SadTab.isShowing(mCurrentTab)) {
                mCurrentTab.reload();
            }
        }
        return false;
    }

    private void reportStop() {
        mEventTracker.addWebsiteEvent(new WebsiteEvent(
                System.currentTimeMillis(), mLastFqdn, WebsiteEvent.EventType.STOP));
        reportToPlatformIfDomainIsTracked("reportUsageStop", mLastFqdn);
        mLastFqdn = null;
    }

    private void activeTabChanged(Tab tab) {
        assert tab != null;
        if (tab == mCurrentTab || mTabModelSelector.getCurrentTab() != tab) return;

        switchObserverToTab(tab);
        // If the newly active tab is hidden, we don't want to check its URL yet; we'll wait until
        // the onShown event fires.
        if (mCurrentTab != null && !tab.isHidden()) {
            updateUsingCanonicalUrl(tab);
        }
    }

    private void switchObserverToTab(Tab tab) {
        if (mCurrentTab != tab && mCurrentTab != null) {
            mCurrentTab.removeObserver(mTabObserver);
        }

        if (tab != null && tab.isIncognito()) {
            mCurrentTab = null;
            return;
        }

        mCurrentTab = tab;
        if (mCurrentTab != null) {
            mCurrentTab.addObserver(mTabObserver);
        }
    }

    private void reportToPlatformIfDomainIsTracked(String reportMethodName, String fqdn) {
        mTokenTracker.getTokenForFqdn(fqdn).then((token) -> {
            if (token == null) return;

            try {
                UsageStatsManager instance =
                        (UsageStatsManager) mActivity.getSystemService(Context.USAGE_STATS_SERVICE);
                Method reportMethod = UsageStatsManager.class.getDeclaredMethod(
                        reportMethodName, Activity.class, String.class);

                reportMethod.invoke(instance, mActivity, token);
            } catch (InvocationTargetException | NoSuchMethodException | IllegalAccessException e) {
                Log.e(TAG, "Failed to report to platform API", e);
            }
        });
    }

    private static String getValidFqdnOrEmptyString(String url) {
        if (url == null) return "";
        String host = Uri.parse(url).getHost();
        return host == null ? "" : host;
    }

    private boolean isLikelySubframeAmpNavigation(NavigationHandle navigation) {
        if (navigation.isInMainFrame()) return false;
        String subframeUrl = navigation.getUrl();
        if (subframeUrl == null || subframeUrl.length() == 0) return false;

        // Our heuristic for AMP pages, based on AMPPageLoadMetricsObserver, is to look for a
        // subframe navigation that contains "amp_js_v" in the query. This might produce a false
        // positive, but we're OK with that; we'll just call updateUrl an extra time. This is a
        // no-op if the URL hasn't actually changed.
        String query = Uri.parse(subframeUrl).getQuery();
        if (query == null) return false;
        return query.contains(AMP_QUERY_PARAM);
    }
}
