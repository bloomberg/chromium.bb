// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for dictionaries and interfaces used by
 * language settings.
 */

/**
 * Settings and state for a particular enabled language.
 */
export type LanguageState = {
  language: chrome.languageSettingsPrivate.Language,
  removable: boolean,
  spellCheckEnabled: boolean,
  translateEnabled: boolean,
  isManaged: boolean,
  isForced: boolean,
  downloadDictionaryFailureCount: number,
  downloadDictionaryStatus:
      (chrome.languageSettingsPrivate.SpellcheckDictionaryStatus|null),
};

/**
 * Settings and state for spellcheck languages.
 */
export type SpellCheckLanguageState = {
  language: chrome.languageSettingsPrivate.Language,
  spellCheckEnabled: boolean,
  isManaged: boolean,
  downloadDictionaryFailureCount: number,
  downloadDictionaryStatus:
      (chrome.languageSettingsPrivate.SpellcheckDictionaryStatus|null),
};

/**
 * Languages data to expose to consumers.
 * supported: an array of languages, ordered alphabetically, set once
 *     at initialization.
 * enabled: an array of enabled language states, ordered by preference.
 * translateTarget: the default language to translate into.
 * prospectiveUILanguage: the "prospective" UI language, i.e., the one to be
 *     used on next restart. Matches the current UI language preference unless
 *     the user has chosen a different language without restarting. May differ
 *     from the actually used language (navigator.language). Chrome OS and
 *     Windows only.
 * spellCheckOnLanguages: an array of spell check languages that are currently
 *     in use, including the languages force-enabled by policy.
 * spellCheckOffLanguages: an array of spell check languages that are currently
 *     not in use, including the languages force-disabled by policy.
 */
export type LanguagesModel = {
  supported: Array<chrome.languageSettingsPrivate.Language>,
  enabled: Array<LanguageState>,
  translateTarget: string,
  alwaysTranslate: Array<chrome.languageSettingsPrivate.Language>,
  neverTranslate: Array<chrome.languageSettingsPrivate.Language>,
  spellCheckOnLanguages: Array<SpellCheckLanguageState>,
  spellCheckOffLanguages: Array<SpellCheckLanguageState>,
  // TODO(dpapad): Wrap prospectiveUILanguage with if expr "is_win" block.
  prospectiveUILanguage?: string,
};

/**
 * Helper methods for reading and writing language settings.
 * @interface
 */
export interface LanguageHelper {
  whenReady(): Promise<void>;

  // <if expr="is_win">
  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   */
  setProspectiveUILanguage(languageCode: string): void;

  /**
   * True if the prospective UI language has been changed.
   */
  requiresRestart(): boolean;

  // </if>

  /**
   * @return The language code for ARC IMEs.
   */
  getArcImeLanguageCode(): string;

  isLanguageCodeForArcIme(languageCode: string): boolean;

  isLanguageTranslatable(language: chrome.languageSettingsPrivate.Language):
      boolean;
  isLanguageEnabled(languageCode: string): boolean;

  /**
   * Enables the language, making it available for spell check and input.
   */
  enableLanguage(languageCode: string): void;

  disableLanguage(languageCode: string): void;

  /**
   * Returns true iff provided languageState is the only blocked language.
   */
  isOnlyTranslateBlockedLanguage(languageState: LanguageState): boolean;

  /**
   * Returns true iff provided languageState can be disabled.
   */
  canDisableLanguage(languageState: LanguageState): boolean;

  canEnableLanguage(language: chrome.languageSettingsPrivate.Language): boolean;

  /**
   * Moves the language in the list of enabled languages by the given offset.
   * @param upDirection True if we need to move toward the front, false if we
   *     need to move toward the back.
   */
  moveLanguage(languageCode: string, upDirection: boolean): void;

  /**
   * Moves the language directly to the front of the list of enabled languages.
   */
  moveLanguageToFront(languageCode: string): void;

  /**
   * Enables translate for the given language by removing the translate
   * language from the blocked languages preference.
   */
  enableTranslateLanguage(languageCode: string): void;

  /**
   * Disables translate for the given language by adding the translate
   * language to the blocked languages preference.
   */
  disableTranslateLanguage(languageCode: string): void;

  /**
   * Sets whether a given language should always be automatically translated.
   */
  setLanguageAlwaysTranslateState(
      languageCode: string, alwaysTranslate: boolean): void;

  /**
   * Enables or disables spell check for the given language.
   */
  toggleSpellCheck(languageCode: string, enable: boolean): void;

  /**
   * Converts the language code for translate. There are some differences
   * between the language set the Translate server uses and that for
   * Accept-Language.
   */
  convertLanguageCodeForTranslate(languageCode: string): string;

  /**
   * Given a language code, returns just the base language. E.g., converts
   * 'en-GB' to 'en'.
   */
  getLanguageCodeWithoutRegion(languageCode: string): string;

  getLanguage(languageCode: string): chrome.languageSettingsPrivate.Language
      |undefined;

  retryDownloadDictionary(languageCode: string): void;
}
