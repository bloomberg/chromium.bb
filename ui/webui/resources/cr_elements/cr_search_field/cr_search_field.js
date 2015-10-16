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
    },
  },

  /** @param {SearchFieldDelegate} delegate */
  setDelegate: function(delegate) {
    this.delegate_ = delegate;
  },

  /**
   * Returns the value of the search field.
   * @return {string}
   */
  getValue: function() {
    var searchInput = this.$$('#search-input');
    return searchInput ? searchInput.value : '';
  },

  /** @private */
  onSearchTermSearch_: function() {
    if (this.delegate_)
      this.delegate_.onSearchTermSearch(this.getValue());
  },

  /** @private */
  onSearchTermKeydown_: function(e) {
    assert(this.showingSearch_);
    if (e.keyIdentifier == 'U+001B')  // Escape.
      this.toggleShowingSearch_();
  },

  /** @private */
  toggleShowingSearch_: function() {
    this.showingSearch_ = !this.showingSearch_;
    this.async(function() {
      var searchInput = this.$$('#search-input');
      if (this.showingSearch_) {
        searchInput.focus();
      } else {
        searchInput.value = '';
        this.onSearchTermSearch_();
      }
    });
  },
});
