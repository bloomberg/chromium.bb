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

  attached: function() {
    // Listen events of parent element.
    this.listen(assert(this.parentElement), 'down', 'onDown_');
    this.listen(assert(this.parentElement), 'up', 'onUp_');
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
    var animationPlayer = this.$.ripple.animate([
      {
        width: '2%',
        height: '2%',
        opacity: 0,
        offset: 0,
        easing: 'liner'
      },
      {
        width: '50%',
        height: '50%',
        opacity: 0.2,
        offset: 1
      }
    ], 150);

    this._setPressed(true);

    this.pressAnimationPromise_ = new Promise(
        animationPlayer.addEventListener.bind(animationPlayer, 'finish'));
  },

  /**
   * Performs burst animation.
   */
  performBurstAnimation: function() {
    var pressAnimationPromise = this.pressAnimationPromise_ !== null ?
        this.pressAnimationPromise_ : Promise.resolve();
    this.pressAnimationPromise_ = null;

    // Wait if press animation is performing.
    pressAnimationPromise.then(function() {
      this._setPressed(false);

      this.$.ripple.animate([
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
    }.bind(this));
  }
});
