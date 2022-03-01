// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
  This file is for recording Chrome Browser's language settings metrics on the
  frontend.
*/

/**
 * These values are persisted to LanguageSettingsActionType
 * in tools/metrics/histograms/enums.xml. Entries should not be renumbered and
 * should never be reused.
 */
export enum LanguageSettingsActionType {
  // CLICK_ON_ADD_LANGUAGE = 1, // Deprecated, use ADD_LANGUAGE in
  // LanguageSettingsPageImpression
  LANGUAGE_ADDED = 2,
  LANGUAGE_REMOVED = 3,
  DISABLE_TRANSLATE_GLOBALLY = 4,
  ENABLE_TRANSLATE_GLOBALLY = 5,
  DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE = 6,
  ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE = 7,
  LANGUAGE_LIST_REORDERED = 8,
}

/**
 * These values are persisted to LanguageSettingsPageImpression
 * in tools/metrics/histograms/enums.xml. Entries should not be renumbered and
 * should never be reused.
 */
export enum LanguageSettingsPageImpressionType {
  MAIN = 0,
  ADD_LANGUAGE = 1,
  // LANGUAGE_DETAILS = 2, // iOS only
  CHROME_LANGUAGE = 3,
  ADVANCED_LANGUAGE_SETTINGS = 4,
  TARGET_LANGUAGE = 5,
  LANGUAGE_OVERFLOW_MENU_OPENED = 6,
}

export interface LanguageSettingsMetricsProxy {
  /**
   * Records the interaction to enumerated histogram.
   */
  recordSettingsMetric(interaction: LanguageSettingsActionType): void;

  /**
   * Records the interaction to enumerated histogram.
   */
  recordPageImpressionMetric(interaction: LanguageSettingsPageImpressionType):
      void;
}

export class LanguageSettingsMetricsProxyImpl implements
    LanguageSettingsMetricsProxy {
  recordSettingsMetric(interaction: LanguageSettingsActionType) {
    chrome.metricsPrivate.recordEnumerationValue(
        'LanguageSettings.Actions', interaction,
        Object.keys(LanguageSettingsActionType).length);
  }

  recordPageImpressionMetric(interaction: LanguageSettingsPageImpressionType) {
    chrome.metricsPrivate.recordEnumerationValue(
        'LanguageSettings.PageImpression', interaction,
        Object.keys(LanguageSettingsPageImpressionType).length);
  }

  static getInstance(): LanguageSettingsMetricsProxy {
    return instance || (instance = new LanguageSettingsMetricsProxyImpl());
  }

  static setInstance(obj: LanguageSettingsMetricsProxy) {
    instance = obj;
  }
}

let instance: LanguageSettingsMetricsProxy|null = null;
