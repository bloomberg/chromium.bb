// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-slider' is a slider component used to select a number from
 * a continuous or discrete range of numbers.
 */

cr.exportPath('cr_slider');

/**
 * The |value| is the corresponding value that the current slider tick is
 * associated with. The string |label| is shown in the UI as the label for the
 * current slider value. The |ariaValue| number is used for aria-valuemin,
 * aria-valuemax, and aria-valuenow, and is optional. If missing, |value| will
 * be used instead.
 * @typedef {{
 *   value: number,
 *   label: string,
 *   ariaValue: (number|undefined),
 * }}
 */
cr_slider.SliderTick;

(() => {
  /**
   * @param {number} min
   * @param {number} max
   * @param {number} value
   * @return {number}
   */
  function clamp(min, max, value) {
    return Math.min(max, Math.max(min, value));
  }

  /**
   * @param {!(cr_slider.SliderTick|number)} tick
   * @return {number}
   */
  function getAriaValue(tick) {
    if (Number.isFinite(/** @type {number} */ (tick))) {
      return /** @type {number} */ (tick);
    } else if (tick.ariaValue != undefined) {
      return /** @type {number} */ (tick.ariaValue);
    } else {
      return tick.value;
    }
  }

  /**
   * The following are the events emitted from cr-slider.
   *
   * cr-slider-value-changed: fired when updating slider via the UI.
   * dragging-changed: fired on pointer down and on pointer up.
   */
  Polymer({
    is: 'cr-slider',

    behaviors: [
      Polymer.PaperRippleBehavior,
    ],

    properties: {
      disabled: {
        type: Boolean,
        value: false,
      },

      /**
       * Internal representation of disabled depending on |disabled| and
       * |ticks|.
       * @private
       */
      disabled_: {
        type: Boolean,
        computed: 'computeDisabled_(disabled, ticks.*)',
        reflectToAttribute: true,
        observer: 'onDisabledChanged_',
      },

      dragging: {
        type: Boolean,
        value: false,
        notify: true,
        reflectToAttribute: true,
      },

      updatingFromKey: {
        type: Boolean,
        value: false,
        notify: true,
      },

      markerCount: {
        type: Number,
        value: 0,
      },

      max: {
        type: Number,
        value: 100,
      },

      min: {
        type: Number,
        value: 0,
      },

      /**
       * When set to false, the keybindings are not handled by this component,
       * for example when the owner of the component wants to set up its own
       * keybindings.
       */
      noKeybindings: {
        type: Boolean,
        value: false,
      },

      snaps: {
        type: Boolean,
        value: false,
      },

      /**
       * The data associated with each tick on the slider. Each element in the
       * array contains a value and the label corresponding to that value.
       * @type {!Array<cr_slider.SliderTick>|!Array<number>}
       */
      ticks: {
        type: Array,
        value: () => [],
      },

      value: {
        type: Number,
        value: 0,
      },

      /** @private */
      holdDown_: {
        type: Boolean,
        value: false,
        observer: 'onHoldDownChanged_',
        reflectToAttribute: true,
      },

      /** @private */
      label_: {
        type: String,
        value: '',
      },

      /** @private */
      isRtl_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * |transiting_| is set to true when bar is touched or clicked. This
       * triggers a single position transition effect to take place for the
       * knob, bar and label. When the transition is complete, |transiting_| is
       * set to false resulting in no transition effect during dragging, manual
       * value updates and keyboard events.
       * @private
       */
      transiting_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    },

    hostAttributes: {
      role: 'slider',
    },

    observers: [
      'onTicksChanged_(ticks.*)',
      'updateUi_(ticks.*, value, min, max)',
      'updateValue_(value, min, max)',
    ],

    listeners: {
      focus: 'onFocus_',
      blur: 'onBlur_',
      keydown: 'onKeyDown_',
      keyup: 'onKeyUp_',
      pointerdown: 'onPointerDown_',
    },

    /** @private {Map<string, number>} */
    deltaKeyMap_: null,


    /** @private {EventTracker} */
    draggingEventTracker_: null,

    /** @override */
    attached: function() {
      this.isRtl_ = window.getComputedStyle(this)['direction'] === 'rtl';
      this.deltaKeyMap_ = new Map([
        ['ArrowDown', -1],
        ['ArrowUp', 1],
        ['PageDown', -1],
        ['PageUp', 1],
        ['ArrowLeft', this.isRtl_ ? 1 : -1],
        ['ArrowRight', this.isRtl_ ? -1 : 1],
      ]);
      this.draggingEventTracker_ = new EventTracker();
    },

    /** @private */
    computeDisabled_: function() {
      return this.disabled || this.ticks.length == 1;
    },

    /**
     * When markers are displayed on the slider, they are evenly spaced across
     * the entire slider bar container and are rendered on top of the bar and
     * bar container. The location of the marks correspond to the discrete
     * values that the slider can have.
     * @return {!Array} The array items have no type since this is used to
     *     create |markerCount| number of markers.
     * @private
     */
    getMarkers_: function() {
      return new Array(Math.max(0, this.markerCount - 1));
    },

    /**
     * @param {number} index
     * @return {string}
     * @private
     */
    getMarkerClass_: function(index) {
      const currentStep = (this.markerCount - 1) * this.getRatio();
      return index < currentStep ? 'active-marker' : 'inactive-marker';
    },

    /**
     * The ratio is a value from 0 to 1.0 corresponding to a location along the
     * slider bar where 0 is the minimum value and 1.0 is the maximum value.
     * This is a helper function used to calculate the bar width, knob location
     * and label location.
     * @return {number}
     */
    getRatio: function() {
      return (this.value - this.min) / (this.max - this.min);
    },

    /**
     * Removes all event listeners related to dragging, and cancels ripple.
     * @param {number} pointerId
     * @private
     */
    stopDragging_: function(pointerId) {
      this.draggingEventTracker_.removeAll();
      this.releasePointerCapture(pointerId);
      this.dragging = false;
      // If there is a ripple animation in progress, setTimeout will hold off
      // on updating |holdDown_|.
      setTimeout(() => {
        this.holdDown_ = false;
      });
    },

    /** @private */
    onBlur_: function() {
      this.holdDown_ = false;
    },

    /** @private */
    onDisabledChanged_: function() {
      this.setAttribute('tabindex', this.disabled_ ? -1 : 0);
      this.blur();
    },

    /** @private */
    onFocus_: function() {
      this.holdDown_ = true;

      if (this.shadowRoot.activeElement == this.$.knob) {
        return;
      }
    },

    /** @private */
    onHoldDownChanged_: function() {
      this.getRipple().holdDown = this.holdDown_;
    },

    /**
     * @param {!Event} event
     * @private
     */
    onKeyDown_: function(event) {
      if (this.disabled_ || this.noKeybindings) {
        return;
      }

      if (event.metaKey || event.shiftKey || event.altKey || event.ctrlKey) {
        return;
      }

      /** @type {number|undefined} */
      let newValue;
      if (event.key == 'Home') {
        newValue = this.min;
      } else if (event.key == 'End') {
        newValue = this.max;
      } else if (this.deltaKeyMap_.has(event.key)) {
        newValue = this.value + this.deltaKeyMap_.get(event.key);
      }

      if (newValue == undefined) {
        return;
      }

      this.updatingFromKey = true;
      if (this.updateValue_(newValue)) {
        this.fire('cr-slider-value-changed');
      }
      event.preventDefault();
      event.stopPropagation();
      setTimeout(() => {
        this.holdDown_ = true;
      });
    },

    /**
     * @param {!Event} event
     * @private
     */
    onKeyUp_: function(event) {
      if (event.key == 'Home' || event.key == 'End' ||
          this.deltaKeyMap_.has(event.key)) {
        setTimeout(() => {
          this.updatingFromKey = false;
        });
      }
    },

    /**
     * When the left-mouse button is pressed, the knob location is updated and
     * dragging starts.
     * @param {!PointerEvent} event
     * @private
     */
    onPointerDown_: function(event) {
      if (this.disabled_ ||
          event.buttons != 1 && event.pointerType == 'mouse') {
        return;
      }

      this.dragging = true;
      this.transiting_ = true;
      this.updateValueFromClientX_(event.clientX);
      // If there is a ripple animation in progress, setTimeout will hold off on
      // updating |holdDown_|.
      setTimeout(() => {
        this.$.knob.focus();
        this.holdDown_ = true;
      });

      this.setPointerCapture(event.pointerId);
      const stopDragging = this.stopDragging_.bind(this, event.pointerId);

      this.draggingEventTracker_.add(this, 'pointermove', e => {
        // If the left-button on the mouse is pressed by itself, then update.
        // Otherwise stop capturing the mouse events because the drag operation
        // is complete.
        if (e.buttons != 1 && e.pointerType == 'mouse') {
          stopDragging();
          return;
        }
        this.updateValueFromClientX_(e.clientX);
      });
      this.draggingEventTracker_.add(this, 'pointercancel', stopDragging);
      this.draggingEventTracker_.add(this, 'pointerdown', stopDragging);
      this.draggingEventTracker_.add(this, 'pointerup', stopDragging);
      this.draggingEventTracker_.add(this, 'keydown', e => {
        if (e.key == 'Escape' || e.key == 'Tab' || e.key == 'Home' ||
            e.key == 'End' || this.deltaKeyMap_.has(e.key)) {
          stopDragging();
        }
      });
    },

    /** @private */
    onTicksChanged_: function() {
      if (this.ticks.length > 1) {
        this.snaps = true;
        this.max = this.ticks.length - 1;
        this.min = 0;
      }
      this.updateValue_(this.value);
    },

    /** @private */
    onTransitionEnd_: function() {
      this.transiting_ = false;
    },

    /** @private */
    updateUi_: function() {
      const percent = `${this.getRatio() * 100}%`;
      this.$.bar.style.width = percent;
      this.$.knobAndLabel.style.marginInlineStart = percent;

      const ticks = this.ticks;
      const value = this.value;
      if (ticks && ticks.length > 0 && value >= 0 && value < ticks.length &&
          Number.isInteger(value)) {
        const tick = ticks[this.value];
        this.label_ = Number.isFinite(tick) ? '' : tick.label;
        const ariaValueNow = getAriaValue(tick);
        this.setAttribute('aria-valuetext', this.label_ || ariaValueNow);
        this.setAttribute('aria-valuenow', ariaValueNow);
        this.setAttribute('aria-valuemin', getAriaValue(ticks[0]));
        this.setAttribute('aria-valuemax', getAriaValue(ticks.slice(-1)[0]));
      } else {
        this.setAttribute('aria-valuetext', value);
        this.setAttribute('aria-valuenow', value);
        this.setAttribute('aria-valuemin', this.min);
        this.setAttribute('aria-valuemax', this.max);
      }
    },

    /**
     * @param {number} value
     * @return {boolean}
     * @private
     */
    updateValue_: function(value) {
      if (this.snaps) {
        // Skip update if |value| has not passed the next value .8 units away.
        // The value will update as the drag approaches the next value.
        if (Math.abs(this.value - value) < .8) {
          return false;
        }
        value = Math.round(value);
      }
      value = clamp(this.min, this.max, value);
      if (this.value == value) {
        return false;
      }
      this.value = value;
      return true;
    },

    /**
     * @param {number} clientX
     * @private
     */
    updateValueFromClientX_: function(clientX) {
      const rect = this.$.container.getBoundingClientRect();
      let ratio = (clientX - rect.left) / rect.width;
      if (this.isRtl_) {
        ratio = 1 - ratio;
      }
      if (this.updateValue_(ratio * (this.max - this.min) + this.min)) {
        this.fire('cr-slider-value-changed');
      }
    },

    _createRipple: function() {
      this._rippleContainer = this.$.knob;
      const ripple = Polymer.PaperRippleBehavior._createRipple();
      ripple.id = 'ink';
      ripple.setAttribute('recenters', '');
      ripple.classList.add('circle', 'toggle-ink');
      return ripple;
    },
  });
})();
