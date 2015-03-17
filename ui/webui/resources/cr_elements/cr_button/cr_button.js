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
 *      <core-icon icon="favorite"></core-icon> Custom button
 *    </cr-button>
 *
 * @group Chrome Elements
 * @element cr-button
 */
Polymer('cr-button', {
  publish: {
    /**
     * If true, the button will be styled with a shadow.
     *
     * @attribute raised
     * @type boolean
     * @default false
     */
    raised: {value: false, reflect: true},

    /**
     * If true, the button will be disabled and greyed out.
     *
     * @attribute disabled
     * @type boolean
     * @default false
     */
    disabled: {value: false, reflect: true},
  },
});
