// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.content.Context;
import android.net.Uri;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.text.format.DateUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.blink_public.input.SelectionGranularity;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanelInterface;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSetting;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSwitch;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchInternalStateController.InternalState;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchSelectionController.SelectionType;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchUma.ContextualSearchPreference;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsBridge;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.version.ChromeVersionInfo;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.url.GURL;

import java.util.regex.Pattern;

/**
 * Handles business decision policy for the {@code ContextualSearchManager}.
 */
class ContextualSearchPolicy {
    private static final String TAG = "ContextualSearch";
    private static final Pattern CONTAINS_WHITESPACE_PATTERN = Pattern.compile("\\s");
    private static final String DOMAIN_GOOGLE = "google";
    private static final String PATH_AMP = "/amp/";
    private static final int REMAINING_NOT_APPLICABLE = -1;
    private static final int TAP_TRIGGERED_PROMO_LIMIT = 50;

    // Constants related to the Contextual Search preference.
    private static final String CONTEXTUAL_SEARCH_DISABLED = "false";
    private static final String CONTEXTUAL_SEARCH_ENABLED = "true";

    private final SharedPreferencesManager mPreferencesManager;
    private final ContextualSearchSelectionController mSelectionController;
    private final RelatedSearchesStamp mRelatedSearchesStamp;
    private ContextualSearchNetworkCommunicator mNetworkCommunicator;
    private ContextualSearchPanelInterface mSearchPanel;

    // Members used only for testing purposes.
    private boolean mDidOverrideFullyEnabledForTesting;
    private boolean mFullyEnabledForTesting;
    private Integer mTapTriggeredPromoLimitForTesting;
    private boolean mDidOverrideAllowSendingPageUrlForTesting;
    private boolean mAllowSendingPageUrlForTesting;

    /**
     * ContextualSearchPolicy constructor.
     */
    public ContextualSearchPolicy(ContextualSearchSelectionController selectionController,
            ContextualSearchNetworkCommunicator networkCommunicator) {
        mPreferencesManager = SharedPreferencesManager.getInstance();

        mSelectionController = selectionController;
        mNetworkCommunicator = networkCommunicator;
        if (selectionController != null) selectionController.setPolicy(this);
        mRelatedSearchesStamp = new RelatedSearchesStamp(this);
    }

    /**
     * Sets the handle to the ContextualSearchPanel.
     * @param panel The ContextualSearchPanel.
     */
    public void setContextualSearchPanel(ContextualSearchPanelInterface panel) {
        mSearchPanel = panel;
    }

    /**
     * @return The number of additional times to show the promo on tap, 0 if it should not be shown,
     *         or a negative value if the counter has been disabled or the user has accepted
     *         the promo.
     */
    int getPromoTapsRemaining() {
        if (!isUserUndecided()) return REMAINING_NOT_APPLICABLE;

        // Return a non-negative value if opt-out promo counter is enabled, and there's a limit.
        DisableablePromoTapCounter counter = getPromoTapCounter();
        if (counter.isEnabled()) {
            int limit = getPromoTapTriggeredLimit();
            if (limit >= 0) return Math.max(0, limit - counter.getCount());
        }

        return REMAINING_NOT_APPLICABLE;
    }

    private int getPromoTapTriggeredLimit() {
        return mTapTriggeredPromoLimitForTesting != null
                ? mTapTriggeredPromoLimitForTesting.intValue()
                : TAP_TRIGGERED_PROMO_LIMIT;
    }

    /**
     * @return the {@link DisableablePromoTapCounter}.
     */
    DisableablePromoTapCounter getPromoTapCounter() {
        return DisableablePromoTapCounter.getInstance(mPreferencesManager);
    }

    /**
     * @return Whether a Tap gesture is currently supported as a trigger for the feature.
     */
    boolean isTapSupported() {
        return (isContextualSearchFullyEnabled()
                       || ContextualSearchFieldTrial.getSwitch(
                               ContextualSearchSwitch
                                       .IS_CONTEXTUAL_SEARCH_TAP_DISABLE_OVERRIDE_ENABLED))
                ? true
                : (getPromoTapsRemaining() != 0);
    }

    /**
     * @return whether or not the Contextual Search Result should be preloaded before the user
     *         explicitly interacts with the feature.
     */
    boolean shouldPrefetchSearchResult() {
        if (isMandatoryPromoAvailable()
                || PreloadPagesSettingsBridge.getState() == PreloadPagesState.NO_PRELOADING) {
            return false;
        }

        // We never preload unless we have sent page context (done through a Resolve request).
        // Only some gestures can resolve, and only when resolve privacy rules are met.
        return isResolvingGesture() && shouldPreviousGestureResolve();
    }

    /**
     * Determines whether the current gesture can trigger a resolve request to use page context.
     * This only checks the gesture, not privacy status -- {@see #shouldPreviousGestureResolve}.
     */
    boolean isResolvingGesture() {
        return (mSelectionController.getSelectionType() == SelectionType.TAP
                       && !isLiteralSearchTapEnabled())
                || mSelectionController.getSelectionType() == SelectionType.RESOLVING_LONG_PRESS;
    }

    /**
     * Determines whether the gesture being processed is allowed to resolve.
     * TODO(donnd): rename to be more descriptive. Maybe isGestureAllowedToResolve?
     * @return Whether the previous gesture should resolve.
     */
    boolean shouldPreviousGestureResolve() {
        if (isMandatoryPromoAvailable()
                || ContextualSearchFieldTrial.getSwitch(
                        ContextualSearchSwitch.IS_SEARCH_TERM_RESOLUTION_DISABLED)) {
            return false;
        }

        // The user must have decided on privacy to resolve page content on HTTPS.
        return isContextualSearchFullyEnabled();
    }

    /** @return Whether a long-press gesture can resolve. */
    boolean canResolveLongpress() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_LONGPRESS_RESOLVE)
                || ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_TRANSLATIONS);
    }

    /**
     * Returns whether surrounding context can be accessed by other systems or not.
     * @return Whether surroundings are available.
     */
    boolean canSendSurroundings() {
        // The user must have decided on privacy to send page content on HTTPS.
        return isContextualSearchFullyEnabled();
    }

    /**
     * @return Whether the Mandatory Promo is enabled.
     */
    boolean isMandatoryPromoAvailable() {
        if (!isUserUndecided()
                || !ContextualSearchFieldTrial.getSwitch(
                        ContextualSearchSwitch.IS_MANDATORY_PROMO_ENABLED)) {
            return false;
        }

        return getPromoOpenCount() >= ContextualSearchFieldTrial.getValue(
                       ContextualSearchSetting.MANDATORY_PROMO_LIMIT);
    }

    /**
     * @return Whether the Opt-out promo is available to be shown in any panel.
     */
    boolean isPromoAvailable() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_NEW_SETTINGS)) {
            // Only show promo card a limited number of times.
            return isUserUndecided()
                    && getContextualSearchPromoCardShownCount()
                    < ContextualSearchFieldTrial.getDefaultPromoCardShownTimes();
        }
        return isUserUndecided();
    }

    /**
     * Returns whether conditions are right for an IPH for Longpress to be shown.
     * We only show this for users that have already opted-in because it's all about using page
     * context with the right gesture.
     */
    boolean isLongpressInPanelHelpCondition() {
        // We no longer support an IPH in the panel for promoting a Longpress instead of a Tap.
        return false;
    }

    /**
     * Registers that a tap has taken place by incrementing tap-tracking counters.
     */
    void registerTap() {
        if (isPromoAvailable()) {
            DisableablePromoTapCounter promoTapCounter = getPromoTapCounter();
            // Bump the counter only when it is still enabled.
            if (promoTapCounter.isEnabled()) promoTapCounter.increment();
        }
        int tapsSinceOpen = mPreferencesManager.incrementInt(
                ChromePreferenceKeys.CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_COUNT);
        if (isUserUndecided()) {
            ContextualSearchUma.logTapsSinceOpenForUndecided(tapsSinceOpen);
        } else {
            ContextualSearchUma.logTapsSinceOpenForDecided(tapsSinceOpen);
        }
        mPreferencesManager.incrementInt(ChromePreferenceKeys.CONTEXTUAL_SEARCH_ALL_TIME_TAP_COUNT);
    }

    /**
     * Updates all the counters to account for an open-action on the panel.
     */
    void updateCountersForOpen() {
        // Always completely reset the tap counters that accumulate only since the last open.
        mPreferencesManager.writeInt(
                ChromePreferenceKeys.CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_COUNT, 0);
        mPreferencesManager.writeInt(
                ChromePreferenceKeys.CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_QUICK_ANSWER_COUNT, 0);

        // Disable the "promo tap" counter, but only if we're using the Opt-out onboarding.
        // For Opt-in, we never disable the promo tap counter.
        if (isPromoAvailable()) {
            getPromoTapCounter().disable();

            // Bump the total-promo-opens counter.
            int count = mPreferencesManager.incrementInt(
                    ChromePreferenceKeys.CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT);
            ContextualSearchUma.logPromoOpenCount(count);
        }
        mPreferencesManager.incrementInt(
                ChromePreferenceKeys.CONTEXTUAL_SEARCH_ALL_TIME_OPEN_COUNT);
    }

    /**
     * Updates Tap counters to account for a quick-answer caption shown on the panel.
     * @param wasActivatedByTap Whether the triggering gesture was a Tap or not.
     * @param doesAnswer Whether the caption is considered an answer rather than just
     *                          informative.
     */
    void updateCountersForQuickAnswer(boolean wasActivatedByTap, boolean doesAnswer) {
        if (wasActivatedByTap && doesAnswer) {
            mPreferencesManager.incrementInt(
                    ChromePreferenceKeys.CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_QUICK_ANSWER_COUNT);
            mPreferencesManager.incrementInt(
                    ChromePreferenceKeys.CONTEXTUAL_SEARCH_ALL_TIME_TAP_QUICK_ANSWER_COUNT);
        }
    }

    /**
     * @return Whether a verbatim request should be made for the given base page, assuming there
     *         is no existing request.
     */
    boolean shouldCreateVerbatimRequest() {
        @SelectionType
        int selectionType = mSelectionController.getSelectionType();
        return (mSelectionController.getSelectedText() != null
                && (selectionType == SelectionType.LONG_PRESS || !shouldPreviousGestureResolve()));
    }

    /**
     * @return whether the experiment that causes a tap gesture to trigger a literal search for the
     *         selection (rather than sending context to resolve a search term) is enabled.
     */
    boolean isLiteralSearchTapEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_LITERAL_SEARCH_TAP);
    }

    /**
     * Determines whether an error from a search term resolution request should
     * be shown to the user, or not.
     */
    boolean shouldShowErrorCodeInBar() {
        // Builds with lots of real users should not see raw error codes.
        return !(ChromeVersionInfo.isStableBuild() || ChromeVersionInfo.isBetaBuild());
    }

    /**
     * Logs the current user's state, including preference, tap and open counters, etc.
     */
    void logCurrentState() {
        ContextualSearchUma.logPreferenceState();
        RelatedSearchesUma.logRelatedSearchesPermissionsForAllUsers(
                hasSendUrlPermissions(), canSendSurroundings());

        // Log the number of promo taps remaining.
        int promoTapsRemaining = getPromoTapsRemaining();
        if (promoTapsRemaining >= 0) ContextualSearchUma.logPromoTapsRemaining(promoTapsRemaining);

        // Also log the total number of taps before opening the promo, even for those
        // that are no longer tap limited. That way we'll know the distribution of the
        // number of taps needed before opening the promo.
        DisableablePromoTapCounter promoTapCounter = getPromoTapCounter();
        boolean wasOpened = !promoTapCounter.isEnabled();
        int count = promoTapCounter.getCount();
        if (wasOpened) {
            ContextualSearchUma.logPromoTapsBeforeFirstOpen(count);
        } else {
            ContextualSearchUma.logPromoTapsForNeverOpened(count);
        }
    }

    /**
     * Logs details about the Search Term Resolution.
     * Should only be called when a search term has been resolved.
     * @param searchTerm The Resolved Search Term.
     */
    void logSearchTermResolutionDetails(String searchTerm) {
        // Only log for decided users so the data reflect fully-enabled behavior.
        // Otherwise we'll get skewed data; more HTTP pages than HTTPS (since those don't resolve),
        // and it's also possible that public pages, e.g. news, have more searches for multi-word
        // entities like people.
        if (isContextualSearchFullyEnabled()) {
            GURL url = mNetworkCommunicator.getBasePageUrl();
            ContextualSearchUma.logBasePageProtocol(isBasePageHTTP(url));
            boolean isSingleWord = !CONTAINS_WHITESPACE_PATTERN.matcher(searchTerm.trim()).find();
            ContextualSearchUma.logSearchTermResolvedWords(isSingleWord);
        }
    }

    /**
     * Logs whether the current user is qualified to do Related Searches requests. This does not
     * check if Related Searches is actually enabled for the current user, only whether they are
     * qualified. We use this to gauge whether each group has a balanced number of qualified users.
     * Can be logged multiple times since we'll just look at the user-count of this histogram.
     * @param basePageLanguage The language of the page, to check if supported by the server.
     */
    void logRelatedSearchesQualifiedUsers(String basePageLanguage) {
        if (mRelatedSearchesStamp.isQualifiedForRelatedSearches(basePageLanguage)) {
            RelatedSearchesUma.logRelatedSearchesQualifiedUsers();
        }
    }

    /**
     * Whether this request should include sending the URL of the base page to the server.
     * Several conditions are checked to make sure it's OK to send the URL, but primarily this is
     * based on whether the user has checked the setting for "Make searches and browsing better".
     * @return {@code true} if the URL should be sent.
     */
    boolean doSendBasePageUrl() {
        if (!isContextualSearchFullyEnabled()) return false;

        // Check whether there is a Field Trial setting preventing us from sending the page URL.
        if (ContextualSearchFieldTrial.getSwitch(
                    ContextualSearchSwitch.IS_SEND_BASE_PAGE_URL_DISABLED)) {
            return false;
        }

        // Ensure that the default search provider is Google.
        if (!TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle()) return false;

        // Only allow HTTP or HTTPS URLs.
        GURL url = mNetworkCommunicator.getBasePageUrl();

        if (url == null || !UrlUtilities.isHttpOrHttps(url)) {
            return false;
        }

        return hasSendUrlPermissions();
    }

    /**
     * Determines whether the user has given permission to send URLs through the "Make searches and
     * browsing better" user setting.
     * @return Whether we can send a URL.
     */
    boolean hasSendUrlPermissions() {
        if (mDidOverrideAllowSendingPageUrlForTesting) return mAllowSendingPageUrlForTesting;

        // Check whether the user has enabled anonymous URL-keyed data collection.
        // This is surfaced on the relatively new "Make searches and browsing better" user setting.
        // In case an experiment is active for the legacy UI call through the unified consent
        // service.
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                Profile.getLastUsedRegularProfile());
    }

    /**
     * The search provider icon is animated every time on long press if the user has never opened
     * the panel before and once a day on tap.
     *
     * @return Whether the search provider icon should be animated.
     */
    boolean shouldAnimateSearchProviderIcon() {
        if (mSearchPanel.isShowing()) return false;

        @SelectionType
        int selectionType = mSelectionController.getSelectionType();
        if (selectionType == SelectionType.TAP) {
            long currentTimeMillis = System.currentTimeMillis();
            long lastAnimatedTimeMillis = mPreferencesManager.readLong(
                    ChromePreferenceKeys.CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME);
            if (Math.abs(currentTimeMillis - lastAnimatedTimeMillis) > DateUtils.DAY_IN_MILLIS) {
                mPreferencesManager.writeLong(
                        ChromePreferenceKeys.CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME,
                        currentTimeMillis);
                return true;
            } else {
                return false;
            }
        } else if (selectionType == SelectionType.LONG_PRESS) {
            // If the panel has never been opened before, getPromoOpenCount() will be 0.
            // Once the panel has been opened, regardless of whether or not the user has opted-in or
            // opted-out, the promo open count will be greater than zero.
            return isUserUndecided() && getPromoOpenCount() == 0;
        }

        return false;
    }

    /**
     * Returns whether a transition that is both from and to the given state should be done.
     * This allows prevention of the short-circuiting that ignores a state transition to the current
     * state in cases where rerunning the current state might safeguard against problematic
     * behavior.
     * @param state The current state, which is also the state being transitioned into.
     * @return {@code true} to go ahead with the logic for that state transition even though we're
     *     already in that state. {@code false} indicates that ignoring this redundant state
     *     transition is fine.
     */
    boolean shouldRetryCurrentState(@InternalState int state) {
        // Make sure we don't get stuck in the IDLE state if the panel is still showing.
        // See https://crbug.com/1251774
        return state == InternalState.IDLE && mSearchPanel != null
                && (mSearchPanel.isShowing() || mSearchPanel.isActive());
    }

    /**
     * @return Whether the given URL is used for Accelerated Mobile Pages by Google.
     */
    boolean isAmpUrl(String url) {
        Uri uri = Uri.parse(url);
        if (uri == null || uri.getHost() == null || uri.getPath() == null) return false;

        return uri.getHost().contains(DOMAIN_GOOGLE) && uri.getPath().startsWith(PATH_AMP);
    }

    /**
     * @return Whether the Contextual Search feature was disabled by the user explicitly.
     */
    static boolean isContextualSearchDisabled() {
        return getPrefService()
                .getString(Pref.CONTEXTUAL_SEARCH_ENABLED)
                .equals(CONTEXTUAL_SEARCH_DISABLED);
    }

    /**
     * @return Whether the Contextual Search feature was enabled by the user explicitly.
     */
    static boolean isContextualSearchEnabled() {
        return getPrefService()
                .getString(Pref.CONTEXTUAL_SEARCH_ENABLED)
                .equals(CONTEXTUAL_SEARCH_ENABLED);
    }

    /**
     * @return Whether the Contextual Search feature is uninitialized (preference unset by the
     *         user).
     */
    static boolean isContextualSearchUninitialized() {
        return getPrefService().getString(Pref.CONTEXTUAL_SEARCH_ENABLED).isEmpty();
    }

    /**
     * @return Whether the Contextual Search fully privacy opt-in was disabled by the user
     *         explicitly.
     */
    static boolean isContextualSearchOptInDisabled() {
        return !getPrefService().getBoolean(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED);
    }

    /**
     * @return Whether the Contextual Search fully privacy opt-in was enabled by the user
     *         explicitly.
     */
    static boolean isContextualSearchOptInEnabled() {
        return getPrefService().getBoolean(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED);
    }

    /**
     * @return Whether the Contextual Search fully privacy opt-in is uninitialized (preference unset
     *         by the user).
     */
    static boolean isContextualSearchOptInUninitialized() {
        return !getPrefService().hasPrefPath(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED);
    }

    /**
     * @return Count of times the promo card has been shown.
     */
    static int getContextualSearchPromoCardShownCount() {
        return getPrefService().getInteger(Pref.CONTEXTUAL_SEARCH_PROMO_CARD_SHOWN_COUNT);
    }

    /**
     * Sets Count of times the promo card has been shown.
     */
    private static void setContextualSearchPromoCardShownCount(int count) {
        getPrefService().setInteger(Pref.CONTEXTUAL_SEARCH_PROMO_CARD_SHOWN_COUNT, count);
    }

    /**
     * @return Whether the Contextual Search feature is disabled when the prefs service considers it
     *         managed.
     */
    static boolean isContextualSearchDisabledByPolicy() {
        return getPrefService().isManagedPreference(Pref.CONTEXTUAL_SEARCH_ENABLED)
                && isContextualSearchDisabled();
    }

    /**
     * Sets the contextual search states based on the user's selection in the promo card.
     * If the the feature CONTEXTUAL_SEARCH_NEW_SETTINGS enabled,
     * @See {@link #setContextualSearchFullyOptedIn(boolean)}.
     * If the the feature CONTEXTUAL_SEARCH_NEW_SETTINGS is not enabled,
     * @See {@link #setContextualSearchState(boolean)}.
     * @param enabled Whether The user to choose fully Contextual Search privacy opt-in.
     */
    static void setContextualSearchPromoCardSelection(boolean enabled) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_NEW_SETTINGS)) {
            setContextualSearchFullyOptedIn(enabled);
        } else {
            setContextualSearchState(enabled);
        }
    }

    /**
     * Explicitly sets whether Contextual Search states.
     * 'enabled' is true - fully opt in.
     * 'enabled' is false - the feature is disabled.
     * @param enabled Whether Contextual Search should be enabled.
     */
    static void setContextualSearchState(boolean enabled) {
        @ContextualSearchPreference
        int onState = ContextualSearchPreference.ENABLED;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_NEW_SETTINGS)) {
            onState = isContextualSearchOptInEnabled() ? ContextualSearchPreference.ENABLED
                                                       : ContextualSearchPreference.UNINITIALIZED;
        }
        setContextualSearchStateInternal(enabled ? onState : ContextualSearchPreference.DISABLED);
    }

    /**
     * @return Whether the Contextual Search feature was fully opted in based on the preference
     *         itself.
     */
    static boolean isContextualSearchPrefFullyOptedIn() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_NEW_SETTINGS)
                && !isContextualSearchOptInUninitialized()) {
            return isContextualSearchOptInEnabled();
        }
        return isContextualSearchEnabled();
    }

    /**
     * Sets whether the user is fully opted in for Contextual Search Privacy.
     * 'enabled' is true - fully opt in.
     * 'enabled' is false - remain undecided.
     * @param enabled Whether Contextual Search privacy is opted in.
     */
    static void setContextualSearchFullyOptedIn(boolean enabled) {
        getPrefService().setBoolean(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED, enabled);
        setContextualSearchStateInternal(enabled ? ContextualSearchPreference.ENABLED
                                                 : ContextualSearchPreference.UNINITIALIZED);
    }

    /** Notifies that a promo card has been shown. */
    static void onPromoShown() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_NEW_SETTINGS)) {
            int count = getContextualSearchPromoCardShownCount();
            count++;
            setContextualSearchPromoCardShownCount(count);
            ContextualSearchUma.logRevisedPromoOpenCount(count);
        }
    }

    /**
     * @return Whether the Contextual Search Multilevel settings UI should be shown.
     */
    static boolean shouldShowMultilevelSettingsUI() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_NEW_SETTINGS);
    }

    /**
     * @param state The state for the Contextual Search.
     */
    private static void setContextualSearchStateInternal(@ContextualSearchPreference int state) {
        switch (state) {
            case ContextualSearchPreference.UNINITIALIZED:
                getPrefService().clearPref(Pref.CONTEXTUAL_SEARCH_ENABLED);
                break;
            case ContextualSearchPreference.ENABLED:
                getPrefService().setString(
                        Pref.CONTEXTUAL_SEARCH_ENABLED, CONTEXTUAL_SEARCH_ENABLED);
                break;
            case ContextualSearchPreference.DISABLED:
                getPrefService().setString(
                        Pref.CONTEXTUAL_SEARCH_ENABLED, CONTEXTUAL_SEARCH_DISABLED);
                break;
            default:
                Log.e(TAG, "Unexpected state for ContextualSearchPreference state=" + state);
                break;
        }
    }

    /**
     * @return The PrefService associated with last used Profile.
     */
    private static PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    // --------------------------------------------------------------------------------------------
    // Testing support.
    // --------------------------------------------------------------------------------------------

    /**
     * Overrides the decided/undecided state for the user preference.
     * @param decidedState Whether the user has decided or not.
     */
    @VisibleForTesting
    void overrideDecidedStateForTesting(boolean decidedState) {
        mDidOverrideFullyEnabledForTesting = true;
        mFullyEnabledForTesting = decidedState;
    }

    /**
     * Overrides the user preference for sending the page URL to Google.
     * @param doAllowSendingPageUrl Whether to allow sending the page URL or not, for tests.
     */
    @VisibleForTesting
    void overrideAllowSendingPageUrlForTesting(boolean doAllowSendingPageUrl) {
        mDidOverrideAllowSendingPageUrlForTesting = true;
        mAllowSendingPageUrlForTesting = doAllowSendingPageUrl;
    }

    /**
     * @return count of times the panel with the promo has been opened.
     */
    @VisibleForTesting
    int getPromoOpenCount() {
        return mPreferencesManager.readInt(ChromePreferenceKeys.CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT);
    }

    /**
     * @return The number of times the user has tapped since the last panel open.
     */
    @VisibleForTesting
    int getTapCount() {
        return mPreferencesManager.readInt(
                ChromePreferenceKeys.CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_COUNT);
    }

    // --------------------------------------------------------------------------------------------
    // Additional considerations.
    // --------------------------------------------------------------------------------------------

    /**
     * @return The ISO country code for the user's home country, or an empty string if not
     *         available or privacy-enabled.
     */
    @NonNull
    String getHomeCountry(Context context) {
        if (ContextualSearchFieldTrial.getSwitch(
                    ContextualSearchSwitch.IS_SEND_HOME_COUNTRY_DISABLED)) {
            return "";
        }

        TelephonyManager telephonyManager =
                (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        if (telephonyManager == null) return "";

        String simCountryIso = telephonyManager.getSimCountryIso();
        return TextUtils.isEmpty(simCountryIso) ? "" : simCountryIso;
    }

    /**
     * @return Whether a promo is needed because the user is still undecided
     *         on enabling or disabling the feature.
     */
    boolean isUserUndecided() {
        if (mDidOverrideFullyEnabledForTesting) return !mFullyEnabledForTesting;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_NEW_SETTINGS)) {
            return isContextualSearchUninitialized() && isContextualSearchOptInUninitialized();
        }

        return isContextualSearchUninitialized();
    }

    /**
     * @return Whether a user explicitly enabled the Contextual Search feature.
     */
    boolean isContextualSearchFullyEnabled() {
        if (mDidOverrideFullyEnabledForTesting) return mFullyEnabledForTesting;

        return isContextualSearchEnabled();
    }

    /**
     * @param url The URL of the base page.
     * @return Whether the given content view is for an HTTP page.
     */
    boolean isBasePageHTTP(@Nullable GURL url) {
        return url != null && UrlConstants.HTTP_SCHEME.equals(url.getScheme());
    }

    // --------------------------------------------------------------------------------------------
    // Related Searches Support.
    // --------------------------------------------------------------------------------------------

    /**
     * Gets the runtime processing stamp for Related Searches. This typically gets the value from
     * a param from a Field Trial Feature.
     * @param basePageLanguage The language of the page, to check for server support.
     * @return A {@code String} whose value describes the schema version and current processing
     *         of Related Searches, or an empty string if the user is not qualified to request
     *         Related Searches or the feature is not enabled.
     */
    String getRelatedSearchesStamp(String basePageLanguage) {
        return mRelatedSearchesStamp.getRelatedSearchesStamp(basePageLanguage);
    }

    /**
     * @return whether the given parameter is currently enabled in the Related Searches Variation
     *         configuration.
     */
    boolean isRelatedSearchesParamEnabled(String paramName) {
        return ContextualSearchFieldTrial.isRelatedSearchesParamEnabled(paramName);
    }

    /** @return whether we're missing the Related Searches configuration stamp. */
    boolean isMissingRelatedSearchesConfiguration() {
        return TextUtils.isEmpty(
                ContextualSearchFieldTrial.getRelatedSearchesExperimentConfigurationStamp());
    }

    // --------------------------------------------------------------------------------------------
    // Contextual Triggers Support.
    // --------------------------------------------------------------------------------------------

    /**
     * Returns the size of the selection that should be shown in response to a Tap gesture.
     * The typical return value is word granularity, but sentence selection and others may be
     * supported too.
     */
    @SelectionGranularity
    int getSelectionSize() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_TRIGGERS_SELECTION_SIZE)
                ? SelectionGranularity.SENTENCE
                : SelectionGranularity.WORD;
    }

    /** Returns whether the selection handles should be shown in response to a Tap gesture. */
    boolean getSelectionShouldShowHandles() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_TRIGGERS_SELECTION_HANDLES);
    }

    /** Returns whether the selection context menu should be shown in response to a Tap gesture. */
    boolean getSelectionShouldShowMenu() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_TRIGGERS_SELECTION_MENU);
    }

    // --------------------------------------------------------------------------------------------
    // Testing helpers.
    // --------------------------------------------------------------------------------------------

    /**
     * Sets the {@link ContextualSearchNetworkCommunicator} to use for server requests.
     * @param networkCommunicator The communicator for all future requests.
     */
    @VisibleForTesting
    public void setNetworkCommunicator(ContextualSearchNetworkCommunicator networkCommunicator) {
        mNetworkCommunicator = networkCommunicator;
    }
}
