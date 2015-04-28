// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-expand-button' is a chrome-specific wrapper around paper-button that
 * toggles between an opened (expanded) and closed state.
 *
 * Example:
 *
 *    <cr-expand-button expanded="{{sectionIsExpanded}}"></cr-expand-button>
 *
 * @group Chrome Elements
 * @element cr-expand-button
 */
Polymer('cr-expand-button', {
  publish: {
    /**
     * If true, the button is in the expanded state and will show the
     * 'expand-less' icon. If false, the button shows the 'expand-more' icon.
     *
     * @attribute expanded
     * @type {boolean}
     * @default false
     */
    expanded: false,

    /**
     * If true, the button will be disabled and greyed out.
     *
     * @attribute disabled
     * @type {boolean}
     * @default false
     */
    disabled: {value: false, reflect: true},
  },
});
