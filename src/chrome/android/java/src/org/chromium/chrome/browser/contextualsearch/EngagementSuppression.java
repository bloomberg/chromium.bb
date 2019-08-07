// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSwitch;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;

/**
 * Provides a ContextualSearchHeuristic for Engagement with the feature.
 */
public class EngagementSuppression extends ContextualSearchHeuristic {
    private final ChromePreferenceManager mPreferenceManager;
    private final boolean mIsConditionSatisfied;
    private final boolean mIsEnabled;

    /**
     * Constructs an object that tracks panel opens and other measures of engagement.
     */
    EngagementSuppression() {
        mPreferenceManager = ChromePreferenceManager.getInstance();
        mIsEnabled = ContextualSearchFieldTrial.getSwitch(
                ContextualSearchSwitch.IS_ENGAGEMENT_SUPPRESSION_ENABLED);
        // Used for manual testing; suppress when we've had an entity impression but no open,
        // OR had a Quick Action presented but none taken and at least one ignored.
        boolean hadEntityImpression =
                mPreferenceManager.readInt(
                        ChromePreferenceManager.CONTEXTUAL_SEARCH_ENTITY_IMPRESSIONS_COUNT)
                > 0;
        boolean hadEntityOpen =
                mPreferenceManager.readInt(
                        ChromePreferenceManager.CONTEXTUAL_SEARCH_ENTITY_OPENS_COUNT)
                > 0;
        boolean hadQuickActionImpression =
                mPreferenceManager.readInt(
                        ChromePreferenceManager.CONTEXTUAL_SEARCH_QUICK_ACTION_IMPRESSIONS_COUNT)
                > 0;
        boolean hadQuickActionTaken =
                mPreferenceManager.readInt(
                        ChromePreferenceManager.CONTEXTUAL_SEARCH_QUICK_ACTIONS_TAKEN_COUNT)
                > 0;
        boolean hadQuickActionIgnored =
                mPreferenceManager.readInt(
                        ChromePreferenceManager.CONTEXTUAL_SEARCH_QUICK_ACTIONS_IGNORED_COUNT)
                > 0;
        mIsConditionSatisfied = (hadEntityImpression && !hadEntityOpen)
                || (hadQuickActionImpression && !hadQuickActionTaken && hadQuickActionIgnored);
    }

    /**
     * Registers that the user saw a Quick Action how they engaged with that feature.
     * @param wasPanelOpened Whether the panel was opened.
     * @param wasActionClicked Whether the user actually did the quick action, e.g. dialed the
     *        phone number.
     */
    public static void registerQuickActionImpression(
            boolean wasPanelOpened, boolean wasActionClicked) {
        ChromePreferenceManager prefs = ChromePreferenceManager.getInstance();
        prefs.incrementInt(
                ChromePreferenceManager.CONTEXTUAL_SEARCH_QUICK_ACTION_IMPRESSIONS_COUNT);
        if (wasActionClicked) {
            prefs.incrementInt(ChromePreferenceManager.CONTEXTUAL_SEARCH_QUICK_ACTIONS_TAKEN_COUNT);
        } else if (wasPanelOpened) {
            prefs.incrementInt(
                    ChromePreferenceManager.CONTEXTUAL_SEARCH_QUICK_ACTIONS_IGNORED_COUNT);
        }
    }

    /**
     * Registers that the user saw Contextual Cards data, and whether they engaged with the feature
     * by opening the panel.
     * @param wasPanelOpened Whether the panel was opened.
     */
    public static void registerContextualCardsImpression(boolean wasPanelOpened) {
        ChromePreferenceManager prefs = ChromePreferenceManager.getInstance();
        prefs.incrementInt(ChromePreferenceManager.CONTEXTUAL_SEARCH_ENTITY_IMPRESSIONS_COUNT);
        if (wasPanelOpened) {
            prefs.incrementInt(ChromePreferenceManager.CONTEXTUAL_SEARCH_ENTITY_OPENS_COUNT);
        }
    }

    // ============================================================================================
    // ContextualSearchHeuristic overrides.
    // ============================================================================================

    @Override
    protected boolean isConditionSatisfiedAndEnabled() {
        return mIsConditionSatisfied && mIsEnabled;
    }

    @Override
    protected void logRankerTapSuppression(ContextualSearchInteractionRecorder logger) {
        // These counters are updated in ContextualSearchPolcy when taps and opens are registered.
        logger.logFeature(ContextualSearchInteractionRecorder.Feature.TAP_COUNT,
                mPreferenceManager.readInt(
                        ChromePreferenceManager.CONTEXTUAL_SEARCH_ALL_TIME_TAP_COUNT));
        logger.logFeature(ContextualSearchInteractionRecorder.Feature.OPEN_COUNT,
                mPreferenceManager.readInt(
                        ChromePreferenceManager.CONTEXTUAL_SEARCH_ALL_TIME_OPEN_COUNT));
        logger.logFeature(ContextualSearchInteractionRecorder.Feature.QUICK_ANSWER_COUNT,
                mPreferenceManager.readInt(
                        ChromePreferenceManager.CONTEXTUAL_SEARCH_ALL_TIME_TAP_QUICK_ANSWER_COUNT));
        // These counters are updated in the #registerX static methods of this class.
        logger.logFeature(ContextualSearchInteractionRecorder.Feature.ENTITY_IMPRESSIONS_COUNT,
                mPreferenceManager.readInt(
                        ChromePreferenceManager.CONTEXTUAL_SEARCH_ENTITY_IMPRESSIONS_COUNT));
        logger.logFeature(ContextualSearchInteractionRecorder.Feature.ENTITY_OPENS_COUNT,
                mPreferenceManager.readInt(
                        ChromePreferenceManager.CONTEXTUAL_SEARCH_ENTITY_OPENS_COUNT));
        logger.logFeature(
                ContextualSearchInteractionRecorder.Feature.QUICK_ACTION_IMPRESSIONS_COUNT,
                mPreferenceManager.readInt(
                        ChromePreferenceManager.CONTEXTUAL_SEARCH_QUICK_ACTION_IMPRESSIONS_COUNT));
        logger.logFeature(ContextualSearchInteractionRecorder.Feature.QUICK_ACTIONS_TAKEN_COUNT,
                mPreferenceManager.readInt(
                        ChromePreferenceManager.CONTEXTUAL_SEARCH_QUICK_ACTIONS_TAKEN_COUNT));
        logger.logFeature(ContextualSearchInteractionRecorder.Feature.QUICK_ACTIONS_IGNORED_COUNT,
                mPreferenceManager.readInt(
                        ChromePreferenceManager.CONTEXTUAL_SEARCH_QUICK_ACTIONS_IGNORED_COUNT));
    }
}
