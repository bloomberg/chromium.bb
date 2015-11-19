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
        notify: true,
        reflectToAttribute: true,
        observer: 'playingChanged_'
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
      },

      /**
       * Whether the knob of time slider is being dragged.
       */
      dragging: {
        type: Boolean,
        value: false,
        notify: true
      },

      /**
       * Dictionary which contains aria-labels for each controls.
       */
      ariaLabels: {
        type: Object,
        observer: 'ariaLabelsChanged_'
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
     * Converts the time and duration into human friendly string.
     * @param {number} time Time to be converted.
     * @param {number} duration Duration to be converted.
     * @return {string} String representation of the given time
     */
    computeTimeString_: function(time, duration) {
      return this.time2string_(time) + ' / ' + this.time2string_(duration);
    },

    /**
     * Invoked when the playing property is changed.
     * @param {boolean} playing
     * @private
     */
    playingChanged_: function(playing) {
      if (this.ariaLabels) {
        this.$.play.setAttribute('aria-label',
            playing ? this.ariaLabels.pause : this.ariaLabels.play);
      }
    },

    /**
     * Invoked when the ariaLabels property is changed.
     * @param {Object} ariaLabels
     * @private
     */
    ariaLabelsChanged_: function(ariaLabels) {
      assert(ariaLabels);
      // TODO(fukino): Use data bindings.
      this.$.volumeSlider.setAttribute('aria-label', ariaLabels.volumeSlider);
      this.$.shuffle.setAttribute('aria-label', ariaLabels.shuffle);
      this.$.repeat.setAttribute('aria-label', ariaLabels.repeat);
      this.$.previous.setAttribute('aria-label', ariaLabels.previous);
      this.$.play.setAttribute('aria-label',
          this.playing ? ariaLabels.pause : ariaLabels.play);
      this.$.next.setAttribute('aria-label', ariaLabels.next);
      this.$.volumeButton.setAttribute('aria-label', ariaLabels.volume);
      this.$.playList.setAttribute('aria-label', ariaLabels.playList);
      this.$.timeSlider.setAttribute('aria-label', ariaLabels.seekSlider);
    }
  });
})();  // Anonymous closure
