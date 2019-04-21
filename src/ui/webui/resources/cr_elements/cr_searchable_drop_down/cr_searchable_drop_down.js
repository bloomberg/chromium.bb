// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-searchable-drop-down' implements a search box with a
 * suggestions drop down.
 *
 * If the update-value-on-input flag is set, value will be set to whatever is
 * in the input box. Otherwise, value will only be set when an element in items
 * is clicked.
 */
Polymer({
  is: 'cr-searchable-drop-down',

  properties: {
    autofocus: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * Whether space should be left below the text field to display an error
     * message. Must be true for |errorMessage| to be displayed.
     */
    errorMessageAllowed: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * When |errorMessage| is set, the text field is highlighted red and
     * |errorMessage| is displayed beneath it.
     */
    errorMessage: String,

    placeholder: String,

    /** @type {!Array<string>} */
    items: Array,

    /** @type {string} */
    value: {
      type: String,
      notify: true,
    },

    /** @private {string} */
    searchTerm_: String,

    /** @type {string} */
    label: {
      type: String,
      value: '',
    },

    /** @type {boolean} */
    updateValueOnInput: Boolean,
  },

  /** @private */
  onClick_: function() {
    this.$$('iron-dropdown').open();
  },

  /** @private */
  onInput_: function() {
    this.searchTerm_ = this.$.search.value;

    if (this.updateValueOnInput) {
      this.value = this.$.search.value;
    }
  },

  /**
   * @param {{model:Object}} event
   * @private
   */
  onSelect_: function(event) {
    this.$$('iron-dropdown').close();

    this.value = event.model.item;
    this.searchTerm_ = '';
  },

  /** @private */
  filterItems_: function(searchTerm) {
    if (!searchTerm) {
      return null;
    }
    return function(item) {
      return item.toLowerCase().includes(searchTerm.toLowerCase());
    };
  },

  /**
   * @param {string} errorMessage
   * @param {boolean} errorMessageAllowed
   * @return {boolean}
   * @private
   */
  shouldShowErrorMessage_: function(errorMessage, errorMessageAllowed) {
    return !!this.getErrorMessage_(errorMessage, errorMessageAllowed);
  },

  /**
   * @param {string} errorMessage
   * @param {boolean} errorMessageAllowed
   * @return {string}
   * @private
   */
  getErrorMessage_: function(errorMessage, errorMessageAllowed) {
    if (!errorMessageAllowed) {
      return '';
    }
    return errorMessage;
  },

});