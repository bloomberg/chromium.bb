// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * cr-slider wraps a paper-slider. It maps the slider's values from a linear UI
 * range to a range of real values.  When |value| does not map exactly to a
 * tick mark, it interpolates to the nearest tick.
 *
 * Unlike paper-slider, there is no distinction between value and
 * immediateValue; when either changes, the |value| property is updated.
 */
Polymer({
  is: 'cr-slider',

  properties: {
    /** The value the slider represents and controls. */
    value: {
      type: Number,
      notify: true,
    },

    /** @type {!Array<number>} Values corresponding to each tick. */
    tickValues: {type: Array, value: []},

    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    min: Number,

    max: Number,

    labelMin: String,

    labelMax: String,
  },

  observers: [
    'valueChanged_(value, tickValues.*)',
  ],

  /**
   * Sets the |value| property to the value corresponding to the knob position
   * after a user action.
   * @private
   */
  onSliderChanged_: function() {
    if (this.tickValues && this.tickValues.length > 0)
      this.value = this.tickValues[this.$.slider.immediateValue];
    else
      this.value = this.$.slider.immediateValue;
  },

  /**
   * Updates the knob position when |value| changes. If the knob is still being
   * dragged, this instead forces |value| back to the current position.
   * @private
   */
  valueChanged_: function() {
    // If |tickValues| is empty, simply set current value to the slider.
    if (this.tickValues.length == 0) {
      this.$.slider.value = this.value;
      return;
    }

    // First update the slider settings if |tickValues| was set.
    var numTicks = Math.max(1, this.tickValues.length);
    this.$.slider.max = numTicks - 1;
    // Limit the number of ticks to 10 to keep the slider from looking too busy.
    /** @const */ var MAX_TICKS = 10;
    this.$.slider.snaps = numTicks < MAX_TICKS;
    this.$.slider.maxMarkers = numTicks < MAX_TICKS ? numTicks : 0;

    if (this.$.slider.dragging && this.tickValues.length > 0 &&
        this.value != this.tickValues[this.$.slider.immediateValue]) {
      // The value changed outside cr-slider but we're still holding the knob,
      // so set the value back to where the knob was.
      // Async so we don't confuse Polymer's data binding.
      this.async(function() {
        this.value = this.tickValues[this.$.slider.immediateValue];
      });
      return;
    }

    // Convert from the public |value| to the slider index (where the knob
    // should be positioned on the slider).
    var sliderIndex =
        this.tickValues.length > 0 ? this.tickValues.indexOf(this.value) : 0;
    if (sliderIndex == -1) {
      // No exact match.
      sliderIndex = this.findNearestIndex_(this.tickValues, this.value);
    }
    this.$.slider.value = sliderIndex;
  },

  /**
   * Returns the index of the item in |arr| closest to |value|.
   * @param {!Array<number>} arr
   * @param {number} value
   * @return {number}
   * @private
   */
  findNearestIndex_: function(arr, value) {
    var closestIndex;
    var minDifference = Number.MAX_VALUE;
    for (var i = 0; i < arr.length; i++) {
      var difference = Math.abs(arr[i] - value);
      if (difference < minDifference) {
        closestIndex = i;
        minDifference = difference;
      }
    }

    assert(typeof closestIndex != 'undefined');
    return closestIndex;
  },
});
