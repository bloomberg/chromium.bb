// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsServiceConfig;
import org.chromium.chrome.browser.tabmodel.TabModel;

/** Utility class for price tracking. */
public class PriceTrackingUtilities {
    public static final String TRACK_PRICES_ON_TABS =
            ChromePreferenceKeys.PRICE_TRACKING_TRACK_PRICES_ON_TABS;
    @VisibleForTesting
    public static final String PRICE_WELCOME_MESSAGE_CARD =
            ChromePreferenceKeys.PRICE_TRACKING_PRICE_WELCOME_MESSAGE_CARD;
    @VisibleForTesting
    public static final String PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT =
            ChromePreferenceKeys.PRICE_TRACKING_PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT;
    @VisibleForTesting
    public static final String PRICE_ALERTS_MESSAGE_CARD =
            ChromePreferenceKeys.PRICE_TRACKING_PRICE_ALERTS_MESSAGE_CARD;
    @VisibleForTesting
    public static final String PRICE_ALERTS_MESSAGE_CARD_SHOW_COUNT =
            ChromePreferenceKeys.PRICE_TRACKING_PRICE_ALERTS_MESSAGE_CARD_SHOW_COUNT;

    @VisibleForTesting
    public static final SharedPreferencesManager SHARED_PREFERENCES_MANAGER =
            SharedPreferencesManager.getInstance();

    // TODO(crbug.com/1307949): Clean up this api.
    @Deprecated
    public static void flipTrackPricesOnTabs() {
        final boolean enableTrackPricesOnTabs = SHARED_PREFERENCES_MANAGER.readBoolean(
                TRACK_PRICES_ON_TABS, PriceTrackingFeatures.isPriceTrackingEnabled());
        SHARED_PREFERENCES_MANAGER.writeBoolean(TRACK_PRICES_ON_TABS, !enableTrackPricesOnTabs);
    }

    /**
     * Update SharedPreferences when users turn on/off the feature tracking prices on tabs.
     */
    public static void setTrackPricesOnTabsEnabled(boolean enabled) {
        SHARED_PREFERENCES_MANAGER.writeBoolean(TRACK_PRICES_ON_TABS, enabled);
    }

    /**
     * @return Whether the track prices on tabs is turned on by users.
     */
    public static boolean isTrackPricesOnTabsEnabled() {
        return PriceTrackingFeatures.isPriceTrackingEligible()
                && SHARED_PREFERENCES_MANAGER.readBoolean(
                        TRACK_PRICES_ON_TABS, PriceTrackingFeatures.isPriceTrackingEnabled());
    }

    /**
     * Forbid showing the PriceWelcomeMessageCard any more.
     */
    public static void disablePriceWelcomeMessageCard() {
        SHARED_PREFERENCES_MANAGER.writeBoolean(PRICE_WELCOME_MESSAGE_CARD, false);
    }

    /**
     * @return Whether the PriceWelcomeMessageCard is enabled.
     */
    public static boolean isPriceWelcomeMessageCardEnabled() {
        return PriceTrackingFeatures.isPriceTrackingEligible()
                && SHARED_PREFERENCES_MANAGER.readBoolean(
                        PRICE_WELCOME_MESSAGE_CARD, PriceTrackingFeatures.isPriceTrackingEnabled());
    }

    /**
     * Increase the show count of PriceWelcomeMessageCard every time it shows in the tab switcher.
     */
    public static void increasePriceWelcomeMessageCardShowCount() {
        SHARED_PREFERENCES_MANAGER.writeInt(
                PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT, getPriceWelcomeMessageCardShowCount() + 1);
    }

    /**
     * @return The show count of PriceWelcomeMessageCard.
     */
    public static int getPriceWelcomeMessageCardShowCount() {
        return SHARED_PREFERENCES_MANAGER.readInt(PRICE_WELCOME_MESSAGE_CARD_SHOW_COUNT, 0);
    }

    /**
     * Forbid showing the PriceAlertsMessageCard any more.
     */
    public static void disablePriceAlertsMessageCard() {
        SHARED_PREFERENCES_MANAGER.writeBoolean(PRICE_ALERTS_MESSAGE_CARD, false);
    }

    // TODO(crbug.com/1326572): Needs to rethink these conditions before starting implicit tracking.
    /**
     * @return Whether the PriceAlertsMessageCard is enabled. We don't show this message card if
     *         user can already receive price drop notifications, see {@link
     *         PriceDropNotificationManager#canPostNotification()}.
     */
    public static boolean isPriceAlertsMessageCardEnabled() {
        return PriceTrackingFeatures.isPriceDropNotificationEligible()
                && CommerceSubscriptionsServiceConfig.isImplicitSubscriptionsEnabled()
                && SHARED_PREFERENCES_MANAGER.readBoolean(
                        PRICE_ALERTS_MESSAGE_CARD, PriceTrackingFeatures.isPriceTrackingEnabled());
    }

    /**
     * Increase the show count of PriceAlertsMessageCard every time it shows in the tab switcher.
     */
    public static void increasePriceAlertsMessageCardShowCount() {
        SHARED_PREFERENCES_MANAGER.writeInt(
                PRICE_ALERTS_MESSAGE_CARD_SHOW_COUNT, getPriceAlertsMessageCardShowCount() + 1);
    }

    /**
     * Decrease the show count of PriceAlertsMessageCard. Right now it is used to correct the show
     * count when PriceAlertsMessageCard is deprioritized by PriceWelcomeMessageCard.
     */
    public static void decreasePriceAlertsMessageCardShowCount() {
        SHARED_PREFERENCES_MANAGER.writeInt(
                PRICE_ALERTS_MESSAGE_CARD_SHOW_COUNT, getPriceAlertsMessageCardShowCount() - 1);
    }

    /**
     * @return The show count of PriceAlertsMessageCard.
     */
    public static int getPriceAlertsMessageCardShowCount() {
        return SHARED_PREFERENCES_MANAGER.readInt(PRICE_ALERTS_MESSAGE_CARD_SHOW_COUNT, 0);
    }

    /**
     * @return if the {@link TabModel} is eligible for price tracking. Not all tab models are - for
     *         example incognito tabs are not eligible for price tracking.
     */
    public static boolean isTabModelPriceTrackingEligible(TabModel tabModel) {
        // Incognito Tabs are not eligible for price tracking.
        return !tabModel.getProfile().isOffTheRecord();
    }

    // TODO(crbug.com/1307949): Clean up price tracking menu.
    /**
     * @return whether we should show the PriceTrackingSettings menu item in grid tab switcher.
     */
    public static boolean shouldShowPriceTrackingMenu() {
        return false;
    }
}