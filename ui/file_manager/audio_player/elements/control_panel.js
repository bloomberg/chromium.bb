// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  Polymer({
    is: 'control-panel',

    properties: {
      /**
       * Flag whether the audio is playing or paused. True if playing, or false
       * paused.
       */
      playing: {
        type: Boolean,
        value: false,
        notify: true
      },

      /**
       * Current elapsed time in the current music in millisecond.
       */
      time: {
        type: Number,
        value: 0,
        notify: true
      },

      /**
       * Total length of the current music in millisecond.
       */
      duration: {
        type: Number,
        value: 0
      },

      /**
       * Whether the shuffle button is ON.
       */
      shuffle: {
        type: Boolean,
        value: false,
        notify: true
      },

      /**
       * Whether the repeat button is ON.
       */
      repeat: {
        type: Boolean,
        value: false,
        notify: true
      },

      /**
       * The audio volume. 0 is silent, and 100 is maximum loud.
       */
      volume: {
        type: Number,
        notify: true
      },

      /**
       * Whether the expanded button is ON.
       */
      expanded: {
        type: Boolean,
        value: false,
        notify: true
      },

      /**
       * Whether the volume slider is expanded or not.
       */
      volumeSliderShown: {
        type: Boolean,
        value: false,
        notify: true,
        reflectToAttribute: true
      },

      /**
       * Whether the knob of time slider is being dragged.
       */
      dragging: {
        type: Boolean,
        value: false,
        notify: true
      }
    },

    /**
     * Initializes an element. This method is called automatically when the
     * element is ready.
     */
    ready: function() {
      this.$.timeSlider.addEventListener('value-change', function() {
        if (this.dragging)
          this.dragging = false;
      }.bind(this));
      this.$.timeSlider.addEventListener('immediate-value-change', function() {
        if (!this.dragging)
          this.dragging = true;
      }.bind(this));
    },

    /**
     * Invoked when the next button is clicked.
     */
    nextClick: function() {
      this.fire('next-clicked');
    },

    /**
     * Invoked when the play button is clicked.
     */
    playClick: function() {
      this.playing = !this.playing;
    },

    /**
     * Invoked when the previous button is clicked.
     */
    previousClick: function() {
      this.fire('previous-clicked');
    },

    /**
     * Converts the time into human friendly string.
     * @param {number} time Time to be converted.
     * @return {string} String representation of the given time
     */
    time2string_: function(time) {
      return ~~(time / 60000) + ':' + ('0' + ~~(time / 1000 % 60)).slice(-2);
    },

    /**
     * Converts the time and duration into human friendly string.
     * @param {number} time Time to be converted.
     * @param {number} duration Duration to be converted.
     * @return {string} String representation of the given time
     */
    computeTimeString_: function(time, duration) {
      return this.time2string_(time) + ' / ' + this.time2string_(duration);
    },

    /**
     * Computes state for play button based on 'playing' property.
     * @return {string}
     */
    computePlayState_: function(playing) {
      return playing ? "playing" : "ended";
    },

    /**
     * Computes style for '.filled' element of progress bar.
     * @return {string}
     */
    computeProgressBarStyle_: function(time, duration) {
      return 'width: ' + (time / duration * 100) + '%;';
    }
  });
})();  // Anonymous closure
