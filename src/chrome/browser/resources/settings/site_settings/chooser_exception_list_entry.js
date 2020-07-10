// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'chooser-exception-list-entry' shows a single chooser exception for a given
 * chooser type.
 */
Polymer({
  is: 'chooser-exception-list-entry',

  behaviors: [SiteSettingsBehavior],

  properties: {
    /**
     * Chooser exception object to display in the widget.
     * @type {!ChooserException}
     */
    exception: Object,

    /** @private */
    lastFocused_: Object,
  },
});
