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
  extends: 'button',

  behaviors: [Polymer.PaperRippleBehavior],

  properties: {
    iconClass: String,

    label: String,

    subLabel: {
      type: String,
      /* Value used for noSubLabel attribute. */
      value: '',
    },
  },

  listeners: {
    'down': '_rippleDown',
    'up': '_rippleUp',
    'focus': '_rippleDown',
    'blur': '_rippleUp',
  },

  _rippleDown: function() {
    this.getRipple().uiDownAction();
  },

  _rippleUp: function() {
    this.getRipple().uiUpAction();
  },

  _createRipple: function() {
    this._rippleContainer = this.$.icon;
    return Polymer.PaperInkyFocusBehaviorImpl._createRipple.call(this);
  },

  /**
   * @param {...*} var_args
   */
  ensureRipple: function(var_args) {
    var lastRipple = this._ripple;
    Polymer.PaperRippleBehavior.ensureRipple.apply(this, arguments);
    if (this._ripple && this._ripple !== lastRipple) {
      this._ripple.center = true;
      this._ripple.classList.add('circle');
    }
  }
});
