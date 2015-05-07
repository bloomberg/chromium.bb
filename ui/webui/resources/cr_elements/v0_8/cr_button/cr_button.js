// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-button' is a chrome-specific wrapper around paper-button.
 *
 * Example:
 *
 *    <cr-button>Press Here</cr-button>
 *    <cr-button raised>Raised Button</cr-button>
 *    <cr-button>
 *      <iron-icon icon="favorite"></iron-icon> Custom button
 *    </cr-button>
 *
 * @group Chrome Elements
 * @element cr-button
 */
Polymer({
  is: 'cr-button',

  properties: {
    /**
     * If true, the button will be styled with a shadow.
     */
    raised: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    },

    /**
     * If true, the button will be disabled and greyed out.
     */
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    },
  },
});
