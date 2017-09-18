// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview A lightweight toast.
 */
Polymer({
  is: 'cr-toast',

  properties: {
    duration: {
      type: Number,
      value: 0,
    },

    open: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  /** @private {number|null} */
  hideTimeoutId_: null,

  show: function() {
    this.open = true;

    if (!this.duration)
      return;

    if (this.hideTimeoutId_ != null)
      window.clearTimeout(this.hideTimeoutId_);

    this.hideTimeoutId_ = window.setTimeout(() => {
      this.hide();
      this.hideTimeoutId_ = null;
    }, this.duration);
  },

  hide: function() {
    this.open = false;
  },
});
