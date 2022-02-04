// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-change-device-language-dialog' is a dialog for
 * changing device language.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_search_field/cr_search_field.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import '//resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import './shared_style.js';
import '//resources/cr_components/chromeos/localized_link/localized_link.js';
import './languages.js';
import '../../settings_shared_css.js';

import {CrScrollableBehavior} from '//resources/cr_elements/cr_scrollable_behavior.m.js';
import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LifetimeBrowserProxyImpl} from '../../lifetime_browser_proxy.js';
import {recordClick, recordNavigation, recordPageBlur, recordPageFocus, recordSearch, recordSettingChange, setUserActionRecorderForTesting} from '../metrics_recorder.m.js';

import {InputsShortcutReminderState, LanguagesMetricsProxy, LanguagesMetricsProxyImpl, LanguagesPageInteraction} from './languages_metrics_proxy.js';
import {LanguageHelper, LanguagesModel} from './languages_types.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-change-device-language-dialog',

  behaviors: [
    CrScrollableBehavior,
    I18nBehavior,
  ],

  properties: {
    /** @type {!LanguagesModel|undefined} */
    languages: Object,

    /** @private {!Array<!chrome.languageSettingsPrivate.Language>} */
    displayedLanguages_: {
      type: Array,
      computed: `getPossibleDeviceLanguages_(languages.supported,
          languages.enabled.*, lowercaseQueryString_)`,
    },

    /** @private {boolean} */
    displayedLanguagesEmpty_: {
      type: Boolean,
      computed: 'isZero_(displayedLanguages_.length)',
    },

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /** @private {?chrome.languageSettingsPrivate.Language} */
    selectedLanguage_: {
      type: Object,
      value: null,
    },

    /** @private */
    disableActionButton_: {
      type: Boolean,
      computed: 'shouldDisableActionButton_(selectedLanguage_)',
    },

    /** @private */
    lowercaseQueryString_: {
      type: String,
      value: '',
    },
  },

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onSearchChanged_(e) {
    this.lowercaseQueryString_ = e.detail.toLowerCase();
  },

  /**
   * @return {!Array<!chrome.languageSettingsPrivate.Language>} A list of
   *     possible device language.
   * @private
   */
  getPossibleDeviceLanguages_() {
    return this.languages.supported
        .filter(language => {
          if (!language.supportsUI || language.isProhibitedLanguage ||
              language.code === this.languages.prospectiveUILanguage) {
            return false;
          }

          return !this.lowercaseQueryString_ ||
              language.displayName.toLowerCase().includes(
                  this.lowercaseQueryString_) ||
              language.nativeDisplayName.toLowerCase().includes(
                  this.lowercaseQueryString_);
        })
        .sort((a, b) => {
          // Sort by native display name so the order of languages is
          // deterministic in case the user selects the wrong language.
          // We need to manually specify a locale in localeCompare for
          // determinism (as changing language may change sort order if a locale
          // is not manually specified).
          return a.nativeDisplayName.localeCompare(b.nativeDisplayName, 'en');
        });
  },

  /**
   * @param {boolean} selected
   * @private
   */
  getItemClass_(selected) {
    return selected ? 'selected' : '';
  },

  /**
   * @param {!chrome.languageSettingsPrivate.Language} item
   * @param {boolean} selected
   * @return {!string}
   * @private
   */
  getAriaLabelForItem_(item, selected) {
    const instruction = selected ? 'selectedDeviceLanguageInstruction' :
                                   'notSelectedDeviceLanguageInstruction';
    return this.i18n(instruction, this.getDisplayText_(item));
  },

  /**
   * @param {!chrome.languageSettingsPrivate.Language} language
   * @return {string} The text to be displayed.
   * @private
   */
  getDisplayText_(language) {
    let displayText = language.nativeDisplayName;
    // If the local name is different, add it.
    if (language.displayName !== language.nativeDisplayName) {
      displayText += ' - ' + language.displayName;
    }
    return displayText;
  },

  /** @private */
  shouldDisableActionButton_() {
    return this.selectedLanguage_ === null;
  },

  /** @private */
  onCancelButtonTap_() {
    this.$.dialog.close();
  },

  /**
   * Sets device language and restarts device.
   * @private
   */
  onActionButtonTap_() {
    assert(this.selectedLanguage_);
    const languageCode = this.selectedLanguage_.code;
    this.languageHelper.setProspectiveUILanguage(languageCode);
    // If the language isn't enabled yet, it should be added and moved to top.
    // If it's already present, we don't do anything.
    if (!this.languageHelper.isLanguageEnabled(languageCode)) {
      this.languageHelper.enableLanguage(languageCode);
      this.languageHelper.moveLanguageToFront(languageCode);
    }
    recordSettingChange();
    LanguagesMetricsProxyImpl.getInstance().recordInteraction(
        LanguagesPageInteraction.RESTART);
    LifetimeBrowserProxyImpl.getInstance().signOutAndRestart();
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeydown_(e) {
    // Close dialog if 'esc' is pressed and the search box is already empty.
    if (e.key === 'Escape' && !this.$.search.getValue().trim()) {
      this.$.dialog.close();
    } else if (e.key !== 'PageDown' && e.key !== 'PageUp') {
      this.$.search.scrollIntoViewIfNeeded();
    }
  },

  /**
   * @param {number} num
   * @return {boolean}
   * @private
   */
  isZero_(num) {
    return num === 0;
  },
});
