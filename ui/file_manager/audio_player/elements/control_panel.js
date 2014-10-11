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

  /**
   * Converts the time into human friendly string.
   * @param {number} time Time to be converted.
   * @return {string} String representation of the given time
   */
  function time2string(time) {
    return ~~(time / 60000) + ':' + ('0' + ~~(time / 1000 % 60)).slice(-2);
  }

  Polymer('control-panel', {
    /**
     * Initializes an element. This method is called automatically when the
     * element is ready.
     */
    ready: function() {
      var onFocusoutBound = this.onVolumeControllerFocusout_.bind(this);
      this.$.volumeSlider.addEventListener('focusout', onFocusoutBound);
      this.$.volumeButton.addEventListener('focusout', onFocusoutBound);
    },

    /**
     * Model object of the Audio Player.
     * @type {AudioPlayerModel}
     */
    model: null,

    /**
     * Invoked when the model changed.
     * @param {AudioPlayerModel} oldValue Old Value.
     * @param {AudioPlayerModel} newValue New Value.
     */
    modelChanged: function(oldValue, newValue) {
      this.$.volumeSlider.model = newValue;
    },

    /**
     * Current elapsed time in the current music in millisecond.
     * @type {number}
     */
    time: 0,

    /**
     * String representation of 'time'.
     * @type {number}
     * @private
     */
    get timeString_() {
      return time2string(this.time);
    },

    /**
     * Total length of the current music in millisecond.
     * @type {number}
     */
    duration: 0,

    /**
     * String representation of 'duration'.
     * @type {string}
     * @private
     */
    get durationString_() {
      return time2string(this.duration);
    },

    /**
     * Flag whether the volume slider is expanded or not.
     * @type {boolean}
     */
    volumeSliderShown: false,

    /**
     * Flag whether the audio is playing or paused. True if playing, or false
     * paused.
     * @type {boolean}
     */
    playing: false,

    /**
     * Invoked when the 'duration' property is changed.
     * @param {number} oldValue old value.
     * @param {number} newValue new value.
     */
    durationChanged: function(oldValue, newValue) {
      // Reset the current playback position.
      this.time = 0;
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
     * Invoked the volume button is clicked.
     * @type {Event} event The event.
     */
    volumeButtonClick: function(event) {
      this.showVolumeController_(this.volumeSliderShown);
      event.stopPropagation();
    },

    /**
     * Invoked when the focus goes out of the volume elements.
     * @param {FocusEvent} event The focusout event.
     * @private
     */
    onVolumeControllerFocusout_: function(event) {
      if (this.volumeSliderShown) {
        // If the focus goes out of the volume, hide the volume control.
        if (!event.relatedTarget ||
            (event.relatedTarget !== this.$.volumeButton &&
             event.relatedTarget !== this.$.volumeSlider)) {
          this.showVolumeController_(false);
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
  });
})();  // Anonymous closure
