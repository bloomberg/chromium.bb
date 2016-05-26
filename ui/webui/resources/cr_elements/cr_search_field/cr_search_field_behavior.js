// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
var SearchFieldDelegate = function() {};

SearchFieldDelegate.prototype = {
  /**
   * @param {string} value
   */
  onSearchTermSearch: assertNotReached,
};

/**
 * Implements an incremental search field which can be shown and hidden.
 * Canonical implementation is <cr-search-field>.
 * @polymerBehavior
 */
var CrSearchFieldBehavior = {
  properties: {
    label: {
      type: String,
      value: '',
    },

    clearLabel: {
      type: String,
      value: '',
    },

    showingSearch: {
      type: Boolean,
      value: false,
      notify: true,
      observer: 'showingSearchChanged_',
      reflectToAttribute: true
    },

    hasSearchText: Boolean,
  },

  /**
   * @return {string} The value of the search field.
   */
  getValue: function() {
    return this.$.searchInput.value;
  },

  /**
   * Sets the value of the search field, if it exists.
   * @param {string} value
   */
  setValue: function(value) {
    // Use bindValue when setting the input value so that changes propagate
    // correctly.
    this.$.searchInput.bindValue = value;
    this.hasSearchText = value != '';
  },

  /** @param {SearchFieldDelegate} delegate */
  setDelegate: function(delegate) {
    this.delegate_ = delegate;
  },

  /** @return {!Promise<boolean>} */
  showAndFocus: function() {
    this.showingSearch = true;
    return this.focus_();
  },

  /**
   * @return {!Promise<boolean>}
   * @private
   */
  focus_: function() {
    return new Promise(function(resolve) {
      this.async(function() {
        if (this.showingSearch) {
          this.$.searchInput.focus();
        }
        resolve(this.showingSearch);
      });
    }.bind(this));
  },

  /** @private */
  onSearchTermSearch_: function() {
    this.hasSearchText = this.getValue() != '';
    if (this.delegate_)
      this.delegate_.onSearchTermSearch(this.getValue());
  },

  /** @private */
  onSearchTermKeydown_: function(e) {
    if (e.key == 'Escape')
      this.showingSearch = false;
  },

  /** @private */
  showingSearchChanged_: function() {
    if (this.showingSearch) {
      this.focus_();
      return;
    }

    this.setValue('');
    this.$.searchInput.blur();
    this.onSearchTermSearch_();
  },

  /** @private */
  toggleShowingSearch_: function() {
    this.showingSearch = !this.showingSearch;
  },
};
