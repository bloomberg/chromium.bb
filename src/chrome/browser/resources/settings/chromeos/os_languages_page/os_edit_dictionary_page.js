// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-edit-dictionary-page' is a sub-page for editing
 * the "dictionary" of custom words used for spell check.
 */

// Max valid word size, keep in sync with kMaxCustomDictionaryWordBytes in
// //components/spellcheck/common/spellcheck_common.h
const MAX_CUSTOM_DICTIONARY_WORD_BYTES = 99;

/** @enum {number} */
const NewWordState = {
  NO_WORD: 0,
  VALID_WORD: 1,
  WORD_ALREADY_ADDED: 2,
  WORD_TOO_LONG: 3,
};

import {afterNextRender, Polymer, html, flush, Templatizer, TemplateInstanceBase} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';
import '//resources/cr_elements/icons.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import '//resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import {recordSettingChange, recordSearch, setUserActionRecorderForTesting, recordPageFocus, recordPageBlur, recordClick, recordNavigation} from '../metrics_recorder.m.js';
import {routes} from '../os_route.m.js';
import {GlobalScrollTargetBehavior, setGlobalScrollTarget} from '../global_scroll_target_behavior.js';
import '../../settings_shared_css.js';
import {LanguagesBrowserProxy, LanguagesBrowserProxyImpl} from './languages_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-edit-dictionary-page',

  behaviors: [
    I18nBehavior,
    GlobalScrollTargetBehavior,
  ],

  properties: {
    /** @private */
    newWordValue_: {
      type: String,
      value: '',
    },

    /**
     * Needed for GlobalScrollTargetBehavior.
     * @override
     */
    subpageRoute: {
      type: Object,
      value: routes.OS_LANGUAGES_EDIT_DICTIONARY,
    },

    /** @private {!Array<string>} */
    words_: {
      type: Array,
      value: [],
    },

    /** @private */
    hasWords_: {
      type: Boolean,
      value: false,
      computed: 'computeHasWords_(words_.length)',
    },

    /** @private */
    disableAddButton_: {
      type: Boolean,
      value: true,
      computed: 'shouldDisableAddButton_(newWordState_)',
    },

    /** @private */
    newWordState_: {
      type: Number,
      value: NewWordState.NO_WORD,
      computed: 'updateNewWordState_(newWordValue_, words_.*)',
    }
  },

  /** @private {?LanguageSettingsPrivate} */
  languageSettingsPrivate_: null,

  /** @override */
  created() {
    this.languageSettingsPrivate_ =
        LanguagesBrowserProxyImpl.getInstance().getLanguageSettingsPrivate();
  },

  /** @override */
  ready() {
    this.languageSettingsPrivate_.getSpellcheckWords(words => {
      this.words_ = words;
    });

    this.languageSettingsPrivate_.onCustomDictionaryChanged.addListener(
        this.onCustomDictionaryChanged_.bind(this));

    // Add a key handler for the new-word input.
    this.$.keys.target = this.$.newWord;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasWords_() {
    return this.words_.length > 0;
  },

  /**
   * Adds the word in the new-word input to the dictionary.
   * @private
   */
  addWordFromInput_() {
    // Spaces are allowed, but removing leading and trailing whitespace.
    const word = this.getTrimmedNewWord_();
    this.newWordValue_ = '';
    if (word) {
      this.languageSettingsPrivate_.addSpellcheckWord(word);
      recordSettingChange();
    }
  },

  /**
   * @return {string}
   * @private
   */
  getTrimmedNewWord_() {
    return this.newWordValue_.trim();
  },

  /**
   * @return {NewWordState}
   * @private
   */
  updateNewWordState_() {
    const trimmedNewWord = this.getTrimmedNewWord_();
    if (!trimmedNewWord.length) {
      return NewWordState.NO_WORD;
    }
    if (this.words_.includes(trimmedNewWord)) {
      return NewWordState.WORD_ALREADY_ADDED;
    }
    if (new Blob([trimmedNewWord]).size > MAX_CUSTOM_DICTIONARY_WORD_BYTES) {
      return NewWordState.WORD_TOO_LONG;
    }
    return NewWordState.VALID_WORD;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldDisableAddButton_() {
    return this.newWordState_ !== NewWordState.VALID_WORD;
  },

  /**
   * @return {string}
   * @private
   */
  getErrorMessage_() {
    switch (this.newWordState_) {
      case NewWordState.WORD_TOO_LONG:
        return this.i18n('addDictionaryWordLengthError');
      case NewWordState.WORD_ALREADY_ADDED:
        return this.i18n('addDictionaryWordDuplicateError');
      default:
        return '';
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  isNewWordInvalid_() {
    return this.newWordState_ === NewWordState.WORD_TOO_LONG ||
        this.newWordState_ === NewWordState.WORD_ALREADY_ADDED;
  },

  /**
   * Handles tapping on the Add Word button.
   * @private
   */
  onAddWordTap_() {
    this.addWordFromInput_();
    this.$.newWord.focus();
  },

  /**
   * Handles updates to the word list. Additions are unshifted to the top
   * of the list so that users can see them easily.
   * @param {!Array<string>} added
   * @param {!Array<string>} removed
   * @private
   */
  onCustomDictionaryChanged_(added, removed) {
    for (const word of removed) {
      this.arrayDelete('words_', word);
    }

    for (const word of added) {
      if (!this.words_.includes(word)) {
        this.unshift('words_', word);
      }
    }
  },

  /**
   * Handles Enter and Escape key presses for the new-word input.
   * @param {!CustomEvent<!{key: string}>} e
   * @private
   */
  onKeysPress_(e) {
    if (e.detail.key === 'enter' && !this.disableAddButton_) {
      this.addWordFromInput_();
    } else if (e.detail.key === 'esc') {
      e.detail.keyboardEvent.target.value = '';
    }
  },

  /**
   * Handles tapping on a "Remove word" icon button.
   * @param {!{model: !{item: string}}} e
   * @private
   */
  onRemoveWordTap_(e) {
    this.languageSettingsPrivate_.removeSpellcheckWord(e.model.item);
    recordSettingChange();
  }
});
