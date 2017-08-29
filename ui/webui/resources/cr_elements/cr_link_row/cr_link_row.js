// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A link row is a UI element similar to a button, though usually wider than a
 * button (taking up the whole 'row'). The name link comes from the intended use
 * of this element to take the user to another page in the app or to an external
 * page (somewhat like an HTML link).
 */
Polymer({
  is: 'cr-link-row',
  extends: 'button',

  properties: {
    iconClass: String,

    label: String,

    subLabel: {
      type: String,
      /* Value used for noSubLabel attribute. */
      value: '',
    },
  },
});
