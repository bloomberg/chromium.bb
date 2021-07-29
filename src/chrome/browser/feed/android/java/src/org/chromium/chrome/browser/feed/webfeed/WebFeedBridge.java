// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Class providing access to functionality provided by the Web Feed native component.
 */
@JNINamespace("feed")
public class WebFeedBridge {
    // Access to JNI test hooks for other libraries. This can go away once more Feed code is
    // migrated to chrome/browser/feed.
    public static org.chromium.base.JniStaticTestMocker<WebFeedBridge.Natives>
    getTestHooksForTesting() {
        return WebFeedBridgeJni.TEST_HOOKS;
    }

    private WebFeedBridge() {}

    /** Container for past visit counts. */
    public static class VisitCounts {
        /** The total number of visits. */
        public final int visits;
        /** The number of per day boolean visits (days when at least one visit happened) */
        public final int dailyVisits;

        VisitCounts(int visits, int dailyVisits) {
            this.visits = visits;
            this.dailyVisits = dailyVisits;
        }
    }

    /**
     * Obtains visit information for a website within a limited number of days in the past.
     * @param url The URL for which the host will be queried for past visits.
     * @param callback The callback to receive the past visits query results.
     *            Upon failure, VisitCounts is populated with 0 visits.
     */
    public static void getVisitCountsToHost(GURL url, Callback<VisitCounts> callback) {
        WebFeedBridgeJni.get().getRecentVisitCountsToHost(
                url, (result) -> callback.onResult(new VisitCounts(result[0], result[1])));
    }

    /** Container for a Web Feed metadata. */
    public static class WebFeedMetadata {
        /** Unique identifier of this web feed. */
        public final byte[] id;
        /** The title of the Web Feed. */
        public final String title;
        /** The URL that best represents this Web Feed. */
        public final GURL visitUrl;
        /** Subscription status */
        public final @WebFeedSubscriptionStatus int subscriptionStatus;
        /** Whether the web feed has content available. */
        public final boolean isActive;
        /** Whether the web feed is recommended. */
        public final boolean isRecommended;

        @CalledByNative("WebFeedMetadata")
        public WebFeedMetadata(byte[] id, String title, GURL visitUrl,
                @WebFeedSubscriptionStatus int subscriptionStatus, boolean isActive,
                boolean isRecommended) {
            this.id = id;
            this.title = title;
            this.visitUrl = visitUrl;
            this.subscriptionStatus = subscriptionStatus;
            this.isActive = isActive;
            this.isRecommended = isRecommended;
        }
    }

    /**
     * Returns the Web Feed metadata for the web feed associated with this page. May return a
     * subscribed, recently subscribed, or recommended Web Feed.
     * @param tab The tab showing the page.
     * @param url The URL for which the status is being requested.
     * @param callback The callback to receive the Web Feed metadata, or null if it is not found.
     */
    public static void getWebFeedMetadataForPage(
            Tab tab, GURL url, Callback<WebFeedMetadata> callback) {
        WebFeedBridgeJni.get().findWebFeedInfoForPage(
                new WebFeedPageInformation(url, tab), callback);
    }

    /**
     * Returns Web Feed metadata respective to the provided identifier. The callback will receive
     * `null` if no matching recommended or followed Web Feed is found.
     * @param webFeedId The idenfitier of the Web Feed.
     * @param callback The callback to receive the Web Feed metadata, or null if it is not found.
     */
    public void getWebFeedMetadata(byte[] webFeedId, Callback<WebFeedMetadata> callback) {
        WebFeedBridgeJni.get().findWebFeedInfoForWebFeedId(webFeedId, callback);
    }

    /**
     * Fetches the list of followed Web Feeds.
     * @param callback The callback to receive the list of followed Web Feeds.
     */
    public static void getAllFollowedWebFeeds(Callback<List<WebFeedMetadata>> callback) {
        WebFeedBridgeJni.get().getAllSubscriptions((Object[] webFeeds) -> {
            ArrayList<WebFeedMetadata> list = new ArrayList<>();
            for (Object o : webFeeds) {
                list.add((WebFeedMetadata) o);
            }
            callback.onResult(list);
        });
    }

    /**
     * Refreshes the list of followed web feeds from the server. See
     * `WebFeedSubscriptions.RefreshSubscriptions`.
     */
    public static void refreshFollowedWebFeeds(Callback<Boolean> callback) {
        WebFeedBridgeJni.get().refreshSubscriptions(callback);
    }

    /** Container for results from a follow request. */
    public static class FollowResults {
        /** Status of follow request. */
        public final @WebFeedSubscriptionRequestStatus int requestStatus;
        /** The metadata from the followed Web Feed. `null` if the operation was not successful. */
        public final @Nullable WebFeedMetadata metadata;

        @CalledByNative("FollowResults")
        FollowResults(
                @WebFeedSubscriptionRequestStatus int requestStatus, WebFeedMetadata metadata) {
            this.requestStatus = requestStatus;
            this.metadata = metadata;
        }
    }

    /** Container for results from an Unfollow request. */
    public static class UnfollowResults {
        @CalledByNative("UnfollowResults")
        UnfollowResults(@WebFeedSubscriptionRequestStatus int requestStatus) {
            this.requestStatus = requestStatus;
        }
        // Result of the operation.
        public final @WebFeedSubscriptionRequestStatus int requestStatus;
    }

    /**
     * Requests to follow of the most relevant Web Feed represented by the provided URL.
     * @param tab The tab with the loaded page that should be followed.
     * @param url The URL that indicates the Web Feed to be followed.
     * @param callback The callback to receive the follow results.
     */
    public static void followFromUrl(Tab tab, GURL url, Callback<FollowResults> callback) {
        WebFeedBridgeJni.get().followWebFeed(new WebFeedPageInformation(url, tab), callback);
    }

    /**
     * Requests to follow of the Web Feed represented by the provided identifier.
     * @param webFeedId The identifier of the Web Feed to be followed.
     * @param callback The callback to receive the follow results.
     */
    public static void followFromId(byte[] webFeedId, Callback<FollowResults> callback) {
        WebFeedBridgeJni.get().followWebFeedById(webFeedId, callback);
    }

    /**
     * Requests the unfollowing of the Web Feed subscription from the provided identifier.
     * @param webFeedId The Web Feed identifier.
     * @param callback The callback to receive the unfollow result.
     */
    public static void unfollow(byte[] webFeedId, Callback<UnfollowResults> callback) {
        WebFeedBridgeJni.get().unfollowWebFeed(webFeedId, callback);
    }

    /** This is deprecated, do not use. */
    @Deprecated
    public static class FollowedIds {
        /** The follow subscription identifier. */
        public final String followId;
        /** The identifier of the followed Web Feed. */
        public final String webFeedId;

        @VisibleForTesting
        public FollowedIds(String followId, String webFeedId) {
            this.followId = followId;
            this.webFeedId = webFeedId;
        }
    }

    /** Information about a web page, which may be used to identify an associated Web Feed. */
    public static class WebFeedPageInformation {
        /** The URL of the page. */
        public final GURL mUrl;
        /** The tab hosting the page. */
        public final Tab mTab;
        WebFeedPageInformation(GURL url, Tab tab) {
            mUrl = url;
            mTab = tab;
        }

        @CalledByNative("WebFeedPageInformation")
        GURL getUrl() {
            return mUrl;
        }

        @CalledByNative("WebFeedPageInformation")
        Tab getTab() {
            return mTab;
        }
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        void followWebFeed(WebFeedPageInformation pageInfo, Callback<FollowResults> callback);
        void followWebFeedById(byte[] webFeedId, Callback<FollowResults> callback);
        void unfollowWebFeed(byte[] webFeedId, Callback<UnfollowResults> callback);
        void findWebFeedInfoForPage(
                WebFeedPageInformation pageInfo, Callback<WebFeedMetadata> callback);
        void findWebFeedInfoForWebFeedId(byte[] webFeedId, Callback<WebFeedMetadata> callback);
        void getAllSubscriptions(Callback<Object[]> callback);
        void refreshSubscriptions(Callback<Boolean> callback);
        void getRecentVisitCountsToHost(GURL url, Callback<int[]> callback);
    }
}
