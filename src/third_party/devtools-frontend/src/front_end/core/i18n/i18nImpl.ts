// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as I18n from '../../third_party/i18n/i18n.js';
import * as Platform from '../platform/platform.js';
import * as Root from '../root/root.js';

import {DevToolsLocale} from './DevToolsLocale.js';

import type * as i18nTypes from './i18nTypes.js';

const i18nInstance = new I18n.I18n.I18n();

// All the locales that are part of the DevTools bundle and should not be fetched
// remotely. Keep this list in sync with "copied_devtools_locale_files" in
// "all_devtools_files.gni" (except the pseudo locales).
const BUNDLED_LOCALES = new Set<string>(['en-US', 'en-XL', 'zh']);

/**
 * Look up the best available locale for the requested language through these fall backs:
 * - exact match
 * - progressively shorter prefixes (`de-CH-1996` -> `de-CH` -> `de`)
 * - the default locale ('en-US') if no match is found
 *
 * If `locale` isn't provided, the default is used.
 */
export function lookupClosestSupportedDevToolsLocale(locale: string): string {
  return i18nInstance.lookupClosestSupportedLocale(locale);
}

/**
 * Returns a list of all supported DevTools locales, including pseudo locales.
 */
export function getAllSupportedDevToolsLocales(): string[] {
  return [...i18nInstance.supportedLocales];
}

/**
 * Returns the Url from which a locale can be fetched. This depends on the
 * specific locale, as some are bundled with DevTools while others
 * have to be fetched remotely.
 */
function getLocaleFetchUrl(locale: Intl.UnicodeBCP47LocaleIdentifier): string {
  const remoteBase = Root.Runtime.getRemoteBase();
  if (remoteBase && remoteBase.base && !BUNDLED_LOCALES.has(locale)) {
    return `${remoteBase.base}core/i18n/locales/${locale}.json`;
  }
  return new URL(`../../core/i18n/locales/${locale}.json`, import.meta.url).toString();
}

/**
 * Fetches the locale data of the specified locale.
 * Callers have to ensure that `locale` is an officilly supported locale.
 * Depending whether a locale is present in `bundledLocales`, the data will be
 * fetched locally or remotely.
 */
export async function fetchAndRegisterLocaleData(locale: Intl.UnicodeBCP47LocaleIdentifier): Promise<void> {
  const localeDataTextPromise = Root.Runtime.loadResourcePromise(getLocaleFetchUrl(locale));
  const timeoutPromise =
      new Promise((resolve, reject) => setTimeout(() => reject(new Error('timed out fetching locale')), 5000));
  const localeDataText = await Promise.race([timeoutPromise, localeDataTextPromise]);
  const localeData = JSON.parse(localeDataText as string);
  i18nInstance.registerLocaleData(locale, localeData);
}

/**
 * Returns an anonymous function that wraps a call to retrieve a localized string.
 * This is introduced so that localized strings can be declared in environments where
 * the i18n system has not been configured and so, cannot be directly invoked. Instead,
 * strings are lazily localized when they are used. This is used for instance in the
 * meta files used to register module extensions.
 */
export function getLazilyComputedLocalizedString(
    registeredStrings: I18n.LocalizedStringSet.RegisteredFileStrings, id: string, values: i18nTypes.Values = {}): () =>
    Platform.UIString.LocalizedString {
  return (): Platform.UIString.LocalizedString => getLocalizedString(registeredStrings, id, values);
}

/**
 * Retrieve the localized string.
 */
export function getLocalizedString(
    registeredStrings: I18n.LocalizedStringSet.RegisteredFileStrings, id: string,
    values: i18nTypes.Values = {}): Platform.UIString.LocalizedString {
  return registeredStrings.getLocalizedStringSetFor(DevToolsLocale.instance().locale).getLocalizedString(id, values) as
      Platform.UIString.LocalizedString;
}

/**
 * Register a file's UIStrings with i18n, return function to generate the string ids.
 */
export function registerUIStrings(
    path: string, stringStructure: {[key: string]: string}): I18n.LocalizedStringSet.RegisteredFileStrings {
  return i18nInstance.registerFileStrings(path, stringStructure);
}

/**
 * Returns a span element that may contains other DOM element as placeholders
 */
export function getFormatLocalizedString(
    registeredStrings: I18n.LocalizedStringSet.RegisteredFileStrings, stringId: string,
    placeholders: Record<string, Object>): Element {
  const formatter =
      registeredStrings.getLocalizedStringSetFor(DevToolsLocale.instance().locale).getMessageFormatterFor(stringId);

  const icuElements = formatter.getAst();
  const args: Array<Object> = [];
  let formattedString = '';
  for (const element of icuElements) {
    if (element.type === /* argumentElement */ 1) {
      const placeholderValue = placeholders[element.value];
      if (placeholderValue) {
        args.push(placeholderValue);
        element.value = '%s';  // convert the {PH} back to %s to use Platform.UIString
      }
    }
    if ('value' in element) {
      formattedString += element.value;
    }
  }
  return formatLocalized(formattedString, args);
}

export function formatLocalized(formattedString: string, args: Array<Object>): Element {
  const substitution: Platform.StringUtilities.FormatterFunction<Object> = substitution => {
    return substitution;
  };

  function append(a: Element, b: undefined|string|Node): Element {
    if (b) {
      a.appendChild(typeof b === 'string' ? document.createTextNode(b) : b);
    }

    return a;
  }

  const formatters = {s: substitution};
  return Platform.StringUtilities.format(formattedString, args, formatters, document.createElement('span'), append)
      .formattedResult;
}

export function serializeUIString(string: string, values: Record<string, Object> = {}): string {
  const serializedMessage = {string, values};
  return JSON.stringify(serializedMessage);
}

export function deserializeUIString(serializedMessage: string): i18nTypes.SerializedMessage {
  if (!serializedMessage) {
    return {string: '', values: {}} as i18nTypes.SerializedMessage;
  }

  return JSON.parse(serializedMessage) as i18nTypes.SerializedMessage;
}

/**
 * Use this function in places where a `LocalizedString` is expected but the
 * term/phrase you want to use does not require translation.
 */
export function lockedString(str: string): Platform.UIString.LocalizedString {
  return str as Platform.UIString.LocalizedString;
}

/**
 * Same as `lockedString` but for places where `i18nLazyString` would be used otherwise.
 */
export function lockedLazyString(str: string): () => Platform.UIString.LocalizedString {
  return (): Platform.UIString.LocalizedString => str as Platform.UIString.LocalizedString;
}

/**
 * Returns a string of the form:
 *   "German (Austria) - Deutsch (Österreich)"
 * where the former locale representation is written in the currently enabled DevTools
 * locale and the latter locale representation is written in the locale of `localeString`.
 *
 * Should the two locales match (i.e. have the same language) then the latter locale
 * representation is written in English.
 */
export function getLocalizedLanguageRegion(
    localeString: Intl.UnicodeBCP47LocaleIdentifier,
    devtoolsLocale: DevToolsLocale): Platform.UIString.LocalizedString {
  // @ts-ignore TODO(crbug.com/1163928) Wait for Intl support.
  const locale = new Intl.Locale(localeString);
  // @ts-ignore TODO(crbug.com/1163928) Wait for Intl support.
  const devtoolsLoc = new Intl.Locale(devtoolsLocale.locale);
  const targetLanguage = locale.language === devtoolsLoc.language ? 'en' : locale.baseName;
  const languageInCurrentLocale =
      new Intl.DisplayNames([devtoolsLocale.locale], {type: 'language'}).of(locale.language);
  const languageInTargetLocale = new Intl.DisplayNames([targetLanguage], {type: 'language'}).of(locale.language);

  let wrappedRegionInCurrentLocale = '';
  let wrappedRegionInTargetLocale = '';

  if (locale.region) {
    const regionInCurrentLocale =
        new Intl.DisplayNames([devtoolsLocale.locale], {type: 'region', style: 'short'}).of(locale.region);
    const regionInTargetLocale =
        new Intl.DisplayNames([targetLanguage], {type: 'region', style: 'short'}).of(locale.region);
    wrappedRegionInCurrentLocale = ` (${regionInCurrentLocale})`;
    wrappedRegionInTargetLocale = ` (${regionInTargetLocale})`;
  }

  return `${languageInCurrentLocale}${wrappedRegionInCurrentLocale} - ${languageInTargetLocale}${
             wrappedRegionInTargetLocale}` as Platform.UIString.LocalizedString;
}
