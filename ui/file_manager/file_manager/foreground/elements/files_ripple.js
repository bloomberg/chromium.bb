// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Files ripple.
 *
 * Circle ripple effect with burst animation.
 */
var FilesRipple = Polymer({
  is: 'files-ripple',

  properties: {
    pressed: {
      type: Boolean,
      readOnly: true,
      value: false,
      reflectToAttribute: true
    }
  },

  /**
   * Promise to be resolved when press animation is completed. Resolved promise
   * can be set if press animation is already completed.
   * @private {Promise}
   */
  pressAnimationPromise_: null,

  ready: function() {
    /** @type {HTMLElement} */
    this.ripple_ = this.$.ripple;
  },

  attached: function() {
    const node = assert(this.parentElement || this.parentNode.host);
    // Listen events of parent element.
    this.listen(node, 'down', 'onDown_');
    this.listen(node, 'up', 'onUp_');
  },

  /**
   * @private
   */
  onDown_: function() {
    this.performPressAnimation();
  },

  /**
   * @private
   */
  onUp_: function() {
    this.performBurstAnimation();
  },

  /**
   * Performs press animation.
   */
  performPressAnimation: function() {
    /** @type {EventTarget} */
    const animationPlayer = this.ripple_.animate([
      {
        width: '2%',
        height: '2%',
        opacity: 0,
        offset: 0,
        easing: 'linear'
      },
      {
        width: '50%',
        height: '50%',
        opacity: 0.2,
        offset: 1
      }
    ], 150);

    this._setPressed(true);

    this.pressAnimationPromise_ = new Promise((resolve, reject) => {
      animationPlayer.addEventListener('finish', resolve, false);
      animationPlayer.addEventListener('cancel', reject, false);
    });
  },

  /**
   * Performs burst animation.
   */
  performBurstAnimation: function() {
    const pressAnimationPromise = this.pressAnimationPromise_ !== null ?
        this.pressAnimationPromise_ : Promise.resolve();
    this.pressAnimationPromise_ = null;

    // Wait if press animation is performing.
    pressAnimationPromise.then(() => {
      this._setPressed(false);

      this.ripple_.animate([
        {
          opacity: 0.2,
          offset: 0,
          easing: 'linear'
        },
        {
          opacity: 0,
          offset: 1
        }
      ], 150);
      this.ripple_.animate([
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
    });
  }
});
