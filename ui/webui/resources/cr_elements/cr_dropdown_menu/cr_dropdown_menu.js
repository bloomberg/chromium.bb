// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `cr-dropdown-menu` is a Chrome-specific wrapper around paper-dropdown-menu.
 * It includes a paper-dropdown so its content should just be a core-menu and
 * items.
 *
 * Example:
 *   <cr-dropdown-menu>
 *     <core-menu>
 *       <paper-item>Chrome</paper-item>
 *       <paper-item>Firefox</paper-item>
 *       <paper-item>IE</paper-item>
 *       <paper-item>Opera</paper-item>
 *     </core-menu>
 *   </cr-dropdown-menu>
 *
 * @group Chrome Elements
 * @element cr-dropdown-menu
 */

Polymer('cr-dropdown-menu', {
  publish: {
    /**
     * True if the menu is open.
     *
     * @attribute opened
     * @type boolean
     * @default false
     */
    opened: false,

    /**
     * A label for the control. The label is displayed if no item is selected.
     *
     * @attribute label
     * @type string
     * @default '<Dropdown Menu Label>'
     */
    label: '<Dropdown Menu Label>',

    /**
     * True if the menu is disabled.
     *
     * @attribute disabled
     * @type boolean
     * @default false
     */
    disabled: {value: false, reflected: true},
  },

  /** @override */
  domReady: function() {
    assert(
        this.querySelector('.menu'),
        'cr-dropdown-menu must have a menu child with class="menu".');
  },
});
