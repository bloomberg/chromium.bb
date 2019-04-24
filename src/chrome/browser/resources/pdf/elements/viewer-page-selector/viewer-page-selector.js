// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'viewer-page-selector',

  properties: {
    /**
     * The number of pages the document contains.
     */
    docLength: {type: Number, value: 1, observer: 'docLengthChanged_'},

    /**
     * The current page being viewed (1-based). A change to pageNo is mirrored
     * immediately to the input field. A change to the input field is not
     * mirrored back until pageNoCommitted() is called and change-page is fired.
     */
    pageNo: {
      type: Number,
      value: 1,
    },

    strings: Object,
  },

  /** @return {!CrInputElement} */
  get pageSelector() {
    return this.$.pageselector;
  },

  pageNoCommitted: function() {
    const page = parseInt(this.pageSelector.value, 10);

    if (!isNaN(page) && page <= this.docLength && page > 0) {
      this.fire('change-page', {page: page - 1, origin: 'pageselector'});
    } else {
      this.pageSelector.value = this.pageNo.toString();
    }
    this.pageSelector.blur();
  },

  /** @private */
  docLengthChanged_: function() {
    const numDigits = this.docLength.toString().length;
    // Set both sides of the slash to the same width, so that the layout is
    // exactly centered. We add 1px because the unit `ch` does not provide
    // exact whole number pixels, and therefore seems to have 1px-off bugginess.
    const width = `calc(${numDigits}ch + 1px)`;
    this.pageSelector.style.width = width;
    this.$['pagelength-spacer'].style.width = width;
  },

  select: function() {
    this.pageSelector.select();
  },

  /**
   * @return {boolean} True if the selector input field is currently focused.
   */
  isActive: function() {
    return this.shadowRoot.activeElement == this.pageSelector;
  },

  /**
   * Immediately remove any non-digit characters.
   * @private
   */
  onInputValueChange_: function() {
    this.pageSelector.value = this.pageSelector.value.replace(/[^\d]/, '');
  },
});
