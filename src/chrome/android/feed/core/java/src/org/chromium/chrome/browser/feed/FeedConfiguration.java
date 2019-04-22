// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.text.TextUtils;

import com.google.android.libraries.feed.host.config.Configuration;
import com.google.android.libraries.feed.host.config.Configuration.ConfigKey;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeFeatureList;

/**
 * Collection of configurable parameters and default values given to the Feed. Every getter passes
 * checks to see if it has been overridden by a field trail param.
 */
public final class FeedConfiguration {
    /** Do not allow construction */
    private FeedConfiguration() {}

    private static final String CARD_MENU_TOOLTIP_ELIGIBLE = "card_menu_tooltip_eligible";
    /** Default value for if card menus should have tooltips enabled. */
    public static final boolean CARD_MENU_TOOLTIP_ELIGIBLE_DEFAULT = false;

    private static final String FEED_SERVER_ENDPOINT = "feed_server_endpoint";
    /** Default value for server endpoint. */
    public static final String FEED_SERVER_ENDPOINT_DEFAULT =
            "https://www.google.com/httpservice/noretry/DiscoverClankService/FeedQuery";

    private static final String FEED_SERVER_METHOD = "feed_server_method";
    /** Default value for feed server method. */
    public static final String FEED_SERVER_METHOD_DEFAULT = "GET";

    private static final String FEED_SERVER_RESPONSE_LENGTH_PREFIXED =
            "feed_server_response_length_prefixed";
    /** Default value for feed server response length prefixed. */
    public static final boolean FEED_SERVER_RESPONSE_LENGTH_PREFIXED_DEFAULT = true;

    private static final String FEED_UI_ENABLED = "feed_ui_enabled";
    /** Default value for the type of UI to request from the server. */
    public static final boolean FEED_UI_ENABLED_DEFAULT = false;

    private static final String INITIAL_NON_CACHED_PAGE_SIZE = "initial_non_cached_page_size";
    /** Default value for initial non cached page size. */
    public static final int INITIAL_NON_CACHED_PAGE_SIZE_DEFAULT = 10;

    private static final String LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS =
            "logging_immediate_content_threshold_ms";
    /** Default value for logging immediate content threshold. */
    public static final int LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS_DEFAULT = 1000;

    private static final String MANAGE_INTERESTS_ENABLED = "manage_interests_enabled";
    /** Default value for whether to use menu options to launch interest management page. */
    public static final boolean MANAGE_INTERESTS_ENABLED_DEFAULT = false;

    private static final String NON_CACHED_MIN_PAGE_SIZE = "non_cached_min_page_size";
    /** Default value for non cached minimum page size. */
    public static final int NON_CACHED_MIN_PAGE_SIZE_DEFAULT = 5;

    private static final String NON_CACHED_PAGE_SIZE = "non_cached_page_size";
    /** Default value for non cached page size. */
    public static final int NON_CACHED_PAGE_SIZE_DEFAULT = 25;

    private static final String SESSION_LIFETIME_MS = "session_lifetime_ms";
    /** Default value for session lifetime. */
    public static final int SESSION_LIFETIME_MS_DEFAULT = 3600000;

    private static final String SNIPPETS_ENABLED = "snippets_enabled";
    /** Default value for whether to show article snippets. */
    public static final boolean SNIPPETS_ENABLED_DEFAULT = false;

    private static final String TRIGGER_IMMEDIATE_PAGINATION = "trigger_immediate_pagination";
    /** Default value for triggering immediate pagination. */
    public static final boolean TRIGGER_IMMEDIATE_PAGINATION_DEFAULT = false;

    private static final String UNDOABLE_ACTIONS_ENABLED = "undoable_actions_enabled";
    /** Default value for if undoable actions should be presented to the user. */
    public static final boolean UNDOABLE_ACTIONS_ENABLED_DEFAULT = false;

    private static final String USE_SECONDARY_PAGE_REQUEST = "use_secondary_page_request";
    /** Default value for pagination behavior. */
    public static final boolean USE_SECONDARY_PAGE_REQUEST_DEFAULT = false;

    private static final String USE_TIMEOUT_SCHEDULER = "use_timeout_scheduler";
    /** Default value for the type of scheduler handling. */
    public static final boolean USE_TIMEOUT_SCHEDULER_DEFAULT = true;

    private static final String VIEW_LOG_THRESHOLD = "view_log_threshold";
    /** Default value for logging view threshold. */
    public static final double VIEW_LOG_THRESHOLD_DEFAULT = 0.66d;

    /** @return Whether to show card tooltips */
    @VisibleForTesting
    static boolean getCardMenuTooltipEligible() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, CARD_MENU_TOOLTIP_ELIGIBLE,
                CARD_MENU_TOOLTIP_ELIGIBLE_DEFAULT);
    }

    /** @return Feed server endpoint to use to fetch content suggestions. */
    @VisibleForTesting
    static String getFeedServerEndpoint() {
        String paramValue = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, FEED_SERVER_ENDPOINT);
        return TextUtils.isEmpty(paramValue) ? FEED_SERVER_ENDPOINT_DEFAULT : paramValue;
    }

    /** @return Feed server method to use when fetching content suggestions. */
    @VisibleForTesting
    static String getFeedServerMethod() {
        String paramValue = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, FEED_SERVER_METHOD);
        return TextUtils.isEmpty(paramValue) ? FEED_SERVER_METHOD_DEFAULT : paramValue;
    }

    /** @return Whether server response should be length prefixed. */
    @VisibleForTesting
    static boolean getFeedServerResponseLengthPrefixed() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS,
                FEED_SERVER_RESPONSE_LENGTH_PREFIXED, FEED_SERVER_RESPONSE_LENGTH_PREFIXED_DEFAULT);
    }

    /** @return Whether to ask the server for "Feed" UI or just basic UI. */
    @VisibleForTesting
    static boolean getFeedUiEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, FEED_UI_ENABLED,
                FEED_UI_ENABLED_DEFAULT);
    }

    /** @return Used to decide where to place the more button initially. */
    @VisibleForTesting
    static int getInitialNonCachedPageSize() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, INITIAL_NON_CACHED_PAGE_SIZE,
                INITIAL_NON_CACHED_PAGE_SIZE_DEFAULT);
    }

    /**
     * @return How long before showing content after opening NTP is no longer considered immediate
     *         in UMA.
     */
    @VisibleForTesting
    static long getLoggingImmediateContentThresholdMs() {
        return (long) ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS,
                LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS,
                LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS_DEFAULT);
    }

    /** return Whether to show context menu option to launch to customization page. */
    @VisibleForTesting
    static boolean getManageInterestsEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, MANAGE_INTERESTS_ENABLED,
                MANAGE_INTERESTS_ENABLED_DEFAULT);
    }

    /** @return Used to decide where to place the more button. */
    @VisibleForTesting
    static int getNonCachedMinPageSize() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, NON_CACHED_MIN_PAGE_SIZE,
                NON_CACHED_MIN_PAGE_SIZE_DEFAULT);
    }

    /** @return Used to decide where to place the more button. */
    @VisibleForTesting
    static int getNonCachedPageSize() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, NON_CACHED_PAGE_SIZE,
                NON_CACHED_PAGE_SIZE_DEFAULT);
    }

    /** @return Time until feed stops restoring the UI. */
    @VisibleForTesting
    static long getSessionLifetimeMs() {
        return (long) ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, SESSION_LIFETIME_MS,
                SESSION_LIFETIME_MS_DEFAULT);
    }

    /**
     * @return Whether UI initially shows "More" button upon reaching the end of known content,
     *         when server could potentially have more content.
     */
    @VisibleForTesting
    static boolean getTriggerImmediatePagination() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, TRIGGER_IMMEDIATE_PAGINATION,
                TRIGGER_IMMEDIATE_PAGINATION_DEFAULT);
    }

    /** @return Whether to allow the present the user with the ability to undo actions. */
    @VisibleForTesting
    static boolean getUndoableActionsEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, UNDOABLE_ACTIONS_ENABLED,
                UNDOABLE_ACTIONS_ENABLED_DEFAULT);
    }

    /**
     * @return Whether the Feed's session handling should use logic to deal with timeouts and
     * placing new results below the fold.
     */
    @VisibleForTesting
    static boolean getUseTimeoutScheduler() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, USE_TIMEOUT_SCHEDULER,
                USE_TIMEOUT_SCHEDULER_DEFAULT);
    }

    /**
     * @return If secondary (a more intuitive) pagination approach should be used, or the original
     * Zine matching behavior should be used.
     */
    @VisibleForTesting
    static boolean getUseSecondaryPageRequest() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, USE_SECONDARY_PAGE_REQUEST,
                USE_SECONDARY_PAGE_REQUEST_DEFAULT);
    }

    /** @return How much of a card must be on screen to generate a UMA log view. */
    @VisibleForTesting
    static double getViewLogThreshold() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, VIEW_LOG_THRESHOLD,
                VIEW_LOG_THRESHOLD_DEFAULT);
    }

    /** @return Whether the article snippets feature is enabled. */
    @VisibleForTesting
    static boolean getSnippetsEnabled() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS, SNIPPETS_ENABLED,
                SNIPPETS_ENABLED_DEFAULT);
    }

    /**
     * @return A fully built {@link Configuration}, ready to be given to the Feed.
     */
    public static Configuration createConfiguration() {
        return new Configuration.Builder()
                .put(ConfigKey.CARD_MENU_TOOLTIP_ELIGIBLE,
                        FeedConfiguration.getCardMenuTooltipEligible())
                .put(ConfigKey.FEED_SERVER_ENDPOINT, FeedConfiguration.getFeedServerEndpoint())
                .put(ConfigKey.FEED_SERVER_METHOD, FeedConfiguration.getFeedServerMethod())
                .put(ConfigKey.FEED_SERVER_RESPONSE_LENGTH_PREFIXED,
                        FeedConfiguration.getFeedServerResponseLengthPrefixed())
                .put(ConfigKey.FEED_UI_ENABLED, FeedConfiguration.getFeedUiEnabled())
                .put(ConfigKey.INITIAL_NON_CACHED_PAGE_SIZE,
                        FeedConfiguration.getInitialNonCachedPageSize())
                .put(ConfigKey.LOGGING_IMMEDIATE_CONTENT_THRESHOLD_MS,
                        FeedConfiguration.getLoggingImmediateContentThresholdMs())
                .put(ConfigKey.MANAGE_INTERESTS_ENABLED,
                        FeedConfiguration.getManageInterestsEnabled())
                .put(ConfigKey.NON_CACHED_MIN_PAGE_SIZE,
                        FeedConfiguration.getNonCachedMinPageSize())
                .put(ConfigKey.NON_CACHED_PAGE_SIZE, FeedConfiguration.getNonCachedPageSize())
                .put(ConfigKey.SESSION_LIFETIME_MS, FeedConfiguration.getSessionLifetimeMs())
                .put(ConfigKey.SNIPPETS_ENABLED, FeedConfiguration.getSnippetsEnabled())
                .put(ConfigKey.TRIGGER_IMMEDIATE_PAGINATION,
                        FeedConfiguration.getTriggerImmediatePagination())
                .put(ConfigKey.UNDOABLE_ACTIONS_ENABLED,
                        FeedConfiguration.getUndoableActionsEnabled())
                .put(ConfigKey.USE_TIMEOUT_SCHEDULER, FeedConfiguration.getUseTimeoutScheduler())
                .put(ConfigKey.USE_SECONDARY_PAGE_REQUEST,
                        FeedConfiguration.getUseSecondaryPageRequest())
                .put(ConfigKey.VIEW_LOG_THRESHOLD, FeedConfiguration.getViewLogThreshold())
                .build();
    }
}
