// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Carrier Provisioning subpage in Cellular Setup flow. This element contains a
 * webview element that loads the carrier's provisioning portal. It also has an
 * error state that displays a message for errors that may happen during this
 * step.
 */
Polymer({
  is: 'provisioning-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Whether error state should be shown.
     * @type {boolean}
     */
    showError: {
      type: Boolean,
      value: false,
    },
  },
});
