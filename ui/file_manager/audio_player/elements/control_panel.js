// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  /**
   * Moves |target| element above |anchor| element, in order to match the
   * bottom lines.
   * @param {HTMLElement} target Target element.
   * @param {HTMLElement} anchor Anchor element.
   */
  function matchBottomLine(target, anchor) {
    var targetRect = target.getBoundingClientRect();
    var anchorRect = anchor.getBoundingClientRect();

    var pos = {
      left: anchorRect.left + anchorRect.width / 2 - targetRect.width / 2,
      bottom: window.innerHeight - anchorRect.bottom,
    };

    target.style.position = 'fixed';
    target.style.left = pos.left + 'px';
    target.style.bottom = pos.bottom + 'px';
  }

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
        observer: 'volumeSliderShownChanged',
        notify: true
      }
    },

    /**
     * Initializes an element. This method is called automatically when the
     * element is ready.
     */
    ready: function() {
      var onFocusoutBound = this.onVolumeControllerFocusout_.bind(this);

      this.$.volumeSlider.addEventListener('focusout', onFocusoutBound);
      this.$.volumeButton.addEventListener('focusout', onFocusoutBound);

      // Prevent the time slider from being moved by arrow keys.
      this.$.timeInput.addEventListener('keydown', function(event) {
        switch (event.keyCode) {
          case 37:  // Left arrow
          case 38:  // Up arrow
          case 39:  // Right arrow
          case 40:  // Down arrow
            event.preventDefault();
            break;
        };
      });
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
     * Invoked when the property 'volumeSliderShown' changes.
     * @param {boolean} shown
     */
    volumeSliderShownChanged: function(shown) {
      this.showVolumeController_(shown);
    },

    /**
     * Invoked when the focus goes out of the volume elements.
     * @param {!UIEvent} event The focusout event.
     * @private
     */
    onVolumeControllerFocusout_: function(event) {
      if (this.volumeSliderShown) {
        // If the focus goes out of the volume, hide the volume control.
        if (!event.relatedTarget ||
            (!this.$.volumeButton.contains(event.relatedTarget) &&
             !this.$.volumeSlider.contains(event.relatedTarget))) {
          this.volumeSliderShown = false;
        }
      }
    },

    /**
     * Shows/hides the volume controller.
     * @param {boolean} show True to show the controller, false to hide.
     * @private
     */
    showVolumeController_: function(show) {
      if (show) {
        matchBottomLine(this.$.volumeContainer, this.$.volumeButton);
        this.$.volumeContainer.style.visibility = 'visible';
      } else {
        this.$.volumeContainer.style.visibility = 'hidden';
      }
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
