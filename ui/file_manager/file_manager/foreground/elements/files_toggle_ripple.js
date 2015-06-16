// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Toggle Ripple.
 *
 * You can change ripple color by the following CSS selector.
 *
 * files-toggle-ripple#my-button::shadow .ripple {
 *   background-color: black;
 * }
 *
 * Ripple size of the activated state is same with the size of this element.
 */
var FilesToggleRipple = Polymer({
  is: 'files-toggle-ripple',

  properties: {
    'activated': {
      type: Boolean,
      value: false,
      observer: 'activatedChanged_'
    },
  },

  /**
   * Called when value of activated property is changed.
   * @param {boolean} newValue New value.
   * @param {boolean} oldValue Old value.
   * @private
   */
  activatedChanged_: function(newValue, oldValue) {
    if (newValue === oldValue)
      return;

    if (newValue)
      this.performActivateAnimation_();
    else
      this.performDeactivateAnimation_();
  },

  /**
   * Perform activate animation.
   * @private
   */
  performActivateAnimation_: function() {
    this.$.ripple.animate([
      {opacity: 0, offset: 0, easing: 'linear'},
      {opacity: 0.2, offset: 1}
    ], 50);
    this.$.ripple.animate([
      {
        width: '31.9%',
        height: '31.9%',
        offset: 0,
        easing: 'cubic-bezier(0, 0, 0.330066603741, 0.931189591041)'
      },
      {
        width: '80.9%',
        height: '80.9%',
        offset: 0.5,
        easing: 'cubic-bezier(0.435623148352, 0.141946042876, 0.2, 1.0)'
      },
      {
        width: '50%',
        height: '50%',
        offset: 1
      }
    ], 500);
    this.$.ripple.animate([
      {
        'border-radius': '50%',
        offset: 0,
        easing: 'linear'
      },
      {
        'border-radius': '50%',
        offset: 0.333,
        easing: 'cubic-bezier(0.109613342381, 0.32112094549, 0.2, 1.0)'
      },
      {
        'border-radius': '2px',
        offset: 1
      }
    ], 750);

    this.$.ripple.classList.add('activated');
  },

  /**
   * Perform deactivate animation.
   * @private
   */
  performDeactivateAnimation_: function() {
    this.$.ripple.animate([
      {opacity: 0.2, offset: 0, easing: 'linear'},
      {opacity: 0, offset: 1}
    ], 150);
    this.$.ripple.animate([
      {
        width: '50%',
        height: '50%',
        offset: 0,
        easing: 'cubic-bezier(0, 0, 0.6, 1)'
      },
      {
        width: '83.0%',
        height: '83.0%',
        offset: 1
      }
    ], 150);
    this.$.ripple.animate([
      {
        'border-radius': '2px',
        offset: 0,
        easing: 'cubic-bezier(0, 0, 0.2, 1)'
      },
      {
        'border-radius': '50%',
        offset: 1
      }
    ], 150);

    this.$.ripple.classList.remove('activated');
  }
});
