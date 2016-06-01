// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(tsergeant): Add tests for cr-toolbar-search-field.
Polymer({
  is: 'cr-toolbar-search-field',

  behaviors: [CrSearchFieldBehavior],

  properties: {
    narrow: {
      type: Boolean,
      reflectToAttribute: true,
    },

    // Prompt text to display in the search field.
    label: String,

    // Tooltip to display on the clear search button.
    clearLabel: String,
  },

  listeners: {
    'tap': 'showSearch_',
  },

  /**
   * @param {boolean} narrow
   * @return {number}
   * @private
   */
  computeIconTabIndex_: function(narrow) {
    return narrow ? 0 : -1;
  },

  /** @private */
  onInputBlur_: function() {
    if (!this.hasSearchText)
      this.showingSearch = false;
  },

  /**
   * Expand the search field when a key is pressed with it focused. This ensures
   * it can be used correctly by tab-focusing. 'keypress' is used instead of
   * 'keydown' to avoid expanding on non-text keys (shift, escape, etc).
   * @private
   */
  onSearchTermKeypress_: function() {
    this.showingSearch = true;
  },

  /**
   * @param {Event} e
   * @private
   */
  showSearch_: function(e) {
    if (e.target != this.$.clearSearch)
      this.showingSearch = true;
  },

  /**
   * @param {Event} e
   * @private
   */
  hideSearch_: function(e) {
    this.showingSearch = false;
    e.stopPropagation();
  }
});
