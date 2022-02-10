// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles metrics for the settings pages. */

/**
 * Contains all possible recorded interactions across privacy settings pages.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SettingsPrivacyElementInteractions enum in
 * histograms/enums.xml
 */
export enum PrivacyElementInteractions {
  SYNC_AND_GOOGLE_SERVICES = 0,
  CHROME_SIGN_IN = 1,
  DO_NOT_TRACK = 2,
  PAYMENT_METHOD = 3,
  NETWORK_PREDICTION = 4,
  MANAGE_CERTIFICATES = 5,
  SAFE_BROWSING = 6,
  PASSWORD_CHECK = 7,
  IMPROVE_SECURITY = 8,
  COOKIES_ALL = 9,
  COOKIES_INCOGNITO = 10,
  COOKIES_THIRD = 11,
  COOKIES_BLOCK = 12,
  COOKIES_SESSION = 13,
  SITE_DATA_REMOVE_ALL = 14,
  SITE_DATA_REMOVE_FILTERED = 15,
  SITE_DATA_REMOVE_SITE = 16,
  COOKIE_DETAILS_REMOVE_ALL = 17,
  COOKIE_DETAILS_REMOVE_ITEM = 18,
  SITE_DETAILS_CLEAR_DATA = 19,
  // Leave this at the end.
  COUNT = 20,
}

/**
 * Contains all safety check interactions.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SafetyCheckInteractions enum in
 * histograms/enums.xml
 */
export enum SafetyCheckInteractions {
  RUN_SAFETY_CHECK = 0,
  UPDATES_RELAUNCH = 1,
  PASSWORDS_MANAGE_COMPROMISED_PASSWORDS = 2,
  SAFE_BROWSING_MANAGE = 3,
  EXTENSIONS_REVIEW = 4,
  CHROME_CLEANER_REBOOT = 5,
  CHROME_CLEANER_REVIEW_INFECTED_STATE = 6,
  PASSWORDS_CARET_NAVIGATION = 7,
  SAFE_BROWSING_CARET_NAVIGATION = 8,
  EXTENSIONS_CARET_NAVIGATION = 9,
  CHROME_CLEANER_CARET_NAVIGATION = 10,
  PASSWORDS_MANAGE_WEAK_PASSWORDS = 11,
  // Leave this at the end.
  COUNT = 12,
}

/**
 * Contains all safe browsing interactions.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the UserAction in safe_browsing_settings_metrics.h.
 */
export enum SafeBrowsingInteractions {
  SAFE_BROWSING_SHOWED = 0,
  SAFE_BROWSING_ENHANCED_PROTECTION_CLICKED = 1,
  SAFE_BROWSING_STANDARD_PROTECTION_CLICKED = 2,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_CLICKED = 3,
  SAFE_BROWSING_ENHANCED_PROTECTION_EXPAND_ARROW_CLICKED = 4,
  SAFE_BROWSING_STANDARD_PROTECTION_EXPAND_ARROW_CLICKED = 5,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_CONFIRMED = 6,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_DENIED = 7,
  // Leave this at the end.
  COUNT = 8,
}

/**
 * All Privacy guide interactions with metrics.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with SettingsPrivacyGuideInteractions in emus.xml.
 */
export enum PrivacyGuideInteractions {
  WELCOME_NEXT_BUTTON = 0,
  MSBB_NEXT_BUTTON = 1,
  HISTORY_SYNC_NEXT_BUTTON = 2,
  SAFE_BROWSING_NEXT_BUTTON = 3,
  COOKIES_NEXT_BUTTON = 4,
  COMPLETION_NEXT_BUTTON = 5,
  SETTINGS_LINK_ROW_ENTRY = 6,
  PROMO_ENTRY = 7,
  SWAA_COMPLETION_LINK = 8,
  PRIVACY_SANDBOX_COMPLETION_LINK = 9,
  // Leave this at the end.
  COUNT = 10,
}

export interface MetricsBrowserProxy {
  /**
   * Helper function that calls recordAction with one action from
   * tools/metrics/actions/actions.xml.
   */
  recordAction(action: string): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.SafetyCheck.Interactions histogram
   */
  recordSafetyCheckInteractionHistogram(interaction: SafetyCheckInteractions):
      void;

  /**
   * Helper function that calls recordHistogram for the
   * SettingsPage.PrivacyElementInteractions histogram
   */
  recordSettingsPageHistogram(interaction: PrivacyElementInteractions): void;

  /**
   * Helper function that calls recordHistogram for the
   * SafeBrowsing.Settings.UserAction histogram
   */
  recordSafeBrowsingInteractionHistogram(interaction: SafeBrowsingInteractions):
      void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.PrivacyGuide.NextNavigation histogram
   */
  recordPrivacyGuideNextNavigationHistogram(interaction:
                                                PrivacyGuideInteractions): void;

  /**
   * Helper function that calls recordHistogram for the
   * Settings.PrivacyGuide.EntryExit histogram
   */
  recordPrivacyGuideEntryExitHistogram(interaction: PrivacyGuideInteractions):
      void;
}

export class MetricsBrowserProxyImpl implements MetricsBrowserProxy {
  recordAction(action: string) {
    chrome.send('metricsHandler:recordAction', [action]);
  }

  recordSafetyCheckInteractionHistogram(interaction: SafetyCheckInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.SafetyCheck.Interactions', interaction,
      SafetyCheckInteractions.COUNT
    ]);
  }

  recordSettingsPageHistogram(interaction: PrivacyElementInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyElementInteractions', interaction,
      PrivacyElementInteractions.COUNT
    ]);
  }

  recordSafeBrowsingInteractionHistogram(interaction:
                                             SafeBrowsingInteractions) {
    // TODO(crbug.com/1124491): Set the correct suffix for
    // SafeBrowsing.Settings.UserAction. Use the .Default suffix for now.
    chrome.send('metricsHandler:recordInHistogram', [
      'SafeBrowsing.Settings.UserAction.Default', interaction,
      SafeBrowsingInteractions.COUNT
    ]);
  }

  recordPrivacyGuideNextNavigationHistogram(interaction:
                                                PrivacyGuideInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyGuide.NextNavigation', interaction,
      PrivacyGuideInteractions.COUNT
    ]);
  }

  recordPrivacyGuideEntryExitHistogram(interaction: PrivacyGuideInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyGuide.EntryExit', interaction,
      PrivacyGuideInteractions.COUNT
    ]);
  }

  static getInstance(): MetricsBrowserProxy {
    return instance || (instance = new MetricsBrowserProxyImpl());
  }

  static setInstance(obj: MetricsBrowserProxy) {
    instance = obj;
  }
}

let instance: MetricsBrowserProxy|null = null;
