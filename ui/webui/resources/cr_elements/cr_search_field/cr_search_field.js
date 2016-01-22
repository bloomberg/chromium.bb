// Copyright 2015 The Chromium Authors. All rights reserved.
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

var SearchField = Polymer({
  is: 'cr-search-field',

  properties: {
    label: {
      type: String,
      value: '',
    },

    clearLabel: {
      type: String,
      value: '',
    },

    showingSearch_: {
      type: Boolean,
      value: false,
      observer: 'showingSearchChanged_',
    },
  },

  /**
   * Returns the value of the search field.
   * @return {string}
   */
  getValue: function() {
    var searchInput = this.getSearchInput_();
    return searchInput ? searchInput.value : '';
  },

  /** @param {SearchFieldDelegate} delegate */
  setDelegate: function(delegate) {
    this.delegate_ = delegate;
  },

  showAndFocus: function() {
    this.showingSearch_ = true;
    this.focus_();
  },

  /** @private */
  focus_: function() {
    this.async(function() {
      if (!this.showingSearch_)
        return;

      var searchInput = this.getSearchInput_();
      if (searchInput)
        searchInput.focus();
    });
  },

  /**
   * @return {?Element}
   * @private
   */
  getSearchInput_: function() {
    return this.$$('#search-input');
  },

  /** @private */
  onSearchTermSearch_: function() {
    if (this.delegate_)
      this.delegate_.onSearchTermSearch(this.getValue());
  },

  /** @private */
  onSearchTermKeydown_: function(e) {
    if (e.keyIdentifier == 'U+001B')  // Escape.
      this.showingSearch_ = false;
  },

  /** @private */
  showingSearchChanged_: function() {
    if (this.showingSearch_) {
      this.focus_();
      return;
    }

    var searchInput = this.getSearchInput_();
    if (!searchInput)
      return;

    searchInput.value = '';
    this.onSearchTermSearch_();
  },

  /** @private */
  toggleShowingSearch_: function() {
    this.showingSearch_ = !this.showingSearch_;
  },
});
