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
      value: false,
      observer: 'checkboxCheckedChanged_'
    },

    collapseOpened: {
      type: Boolean,
      value: false,
      observer: 'collapseOpenedChanged_'
    },

    inputValues: {
      type: Array,
      value: function() { return ["", "input1", "input2", "", "input4"]; },
      observer: 'inputValuesChanged_'
    },

    checkboxLabel: {
      type: String,
      value: 'CheckboxLabel',
    },

    subLabel: {
      type: String,
      value: 'sub-label',
    },
  },

  checkboxCheckedChanged_: function() {
    console.log('checkboxCheckedChanged=' + this.checkboxChecked);
  },

  collapseOpenedChanged_: function() {
    console.log('collapseOpened=' + this.collapseOpened);
  },

  inputValuesChanged_: function() {
    console.log('inputValues=' + this.inputValues);
  }
});
