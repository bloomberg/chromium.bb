// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A link row is a UI element similar to a button, though usually wider than a
 * button (taking up the whole 'row'). The name link comes from the intended use
 * of this element to take the user to another page in the app or to an external
 * page (somewhat like an HTML link).
 * Note: the ripple handling was taken from Polymer v1 paper-icon-button-light.
 */
Polymer({
  is: 'cr-link-row',

  properties: {
    startIcon: {
      type: String,
      value: '',
    },

    label: {
      type: String,
      value: '',
    },

    subLabel: {
      type: String,
      /* Value used for noSubLabel attribute. */
      value: '',
      observer: 'onSubLabelChange_',
    },

    disabled: {
      type: Boolean,
      reflectToAttribute: true,
    },

    external: {
      type: Boolean,
      value: false,
    },

    /** @private {string|undefined} */
    ariaDescribedBy_: String,
  },

  listeners: {
    down: 'onDown_',
    up: 'onUp_',
  },

  /** @type {boolean} */
  get noink() {
    return this.$.icon.noink;
  },

  /** @type {boolean} */
  set noink(value) {
    this.$.icon.noink = value;
  },

  focus: function() {
    this.$.icon.focus();
  },

  /** @private */
  onDown_: function() {
    // If the icon has focus, we want to preemptively blur it. Otherwise it will
    // be blurred immediately after gaining focus. The ripple will either
    // disappear when the mouse button is held down or the ripple will not
    // be rendered. This is an issue only when the cr-link-row is clicked, not
    // the icon itself.
    if (this.shadowRoot.activeElement == this.$.icon) {
      this.$.icon.blur();
    }
    // When cr-link-row is click, <body> will gain focus after the down event is
    // handled. We need to wait until the next task to focus on the icon.
    setTimeout(() => {
      this.focus();
    });
  },

  /** @private */
  onUp_: function() {
    this.$.icon.hideRipple();
  },

  /** @private */
  getIconClass_: function() {
    return this.external ? 'icon-external' : 'subpage-arrow';
  },

  /** @private */
  onSubLabelChange_: function() {
    this.ariaDescribedBy_ = this.subLabel ? 'subLabel' : undefined;
  },
});
