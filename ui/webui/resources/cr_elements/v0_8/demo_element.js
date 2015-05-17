// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for UI wrapping a list of cr- elements.
 */
Polymer({
  is: 'cr-demo-element',

  properties: {
    checkboxChecked: {
      type: Boolean,
      value: true,
      observer: 'checkboxCheckedChanged_'
    },

    collapseOpened: {
      type: Boolean,
      value: true,
      observer: 'collapseOpenedChanged_'
    },

    inputValue: {
      type: String,
      value: '',
      observer: 'inputValueChanged_'
    },
  },

  checkboxCheckedChanged_: function() {
    console.log('checkboxCheckedChanged=' + this.checkboxChecked);
  },

  collapseOpenedChanged_: function() {
    console.log('collapseOpened=' + this.collapseOpened);
  },

  inputValueChanged_: function() {
    console.log('inputValue=' + this.inputValue);
  }
});
