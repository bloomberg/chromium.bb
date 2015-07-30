// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  Polymer({
    is: 'volume-controller',

    properties: {
      /**
       * Width of the element in pixels. Must be specified before ready() is
       * called. Dynamic change is not supported.
       * @type {number}
       */
      width: {
        type: Number,
        value: 32
      },

      /**
       * Height of the element in pixels. Must be specified before ready() is
       * called. Dynamic change is not supported.
       * @type {number}
       */
      height: {
        type: Number,
        value: 100
      },

      /**
       * Volume. 0 is silent, and 100 is maximum.
       * @type {number}
       */
      value: {
        type: Number,
        value: 50,
        observer: 'valueChanged',
        notify: true
      },

      /**
       * Volume. 100 is silent, and 0 is maximum.
       * @type {number}
       */
      rawValue: {
        type: Number,
        value: 0,
        observer: 'rawValueChanged',
        notify: true
      }
    },

    /**
     * Initializes an element. This method is called automatically when the
     * element is ready.
     */
    ready: function() {
      this.style.width = this.width + 'px';
      this.style.height = this.height + 'px';

      this.rawValueInput = this.$.rawValueInput;
      this.bar = this.$.bar;

      this.rawValueInput.style.width = this.height + 'px';
      this.rawValueInput.style.height = this.width + 'px';
      this.rawValueInput.style.webkitTransformOrigin =
          (this.width / 2) + 'px ' +
          (this.width / 2 - 2) + 'px';

      var barLeft = (this.width / 2 - 1);
      this.bar.style.left = barLeft + 'px';
      this.bar.style.right = barLeft + 'px';

      this.addEventListener('keydown', this.onKeyDown_.bind(this));
    },

    /**
     * Invoked when the 'volume' value is changed.
     * @param {number} newValue New value.
     * @param {number} oldValue Old value.
     */
    valueChanged: function(newValue, oldValue) {
      if (oldValue != newValue)
        this.rawValue = 100 - newValue;
    },

    /**
     * Invoked when the 'rawValue' property is changed.
     * @param {number} newValue New value.
     * @param {number} oldValue Old value.
     */
    rawValueChanged: function(newValue, oldValue) {
      if (oldValue !== newValue)
        this.value = 100 - newValue;
    },

    /**
     * Invoked when the 'keydown' event is fired.
     * @param {Event} event The event object.
     */
    onKeyDown_: function(event) {
      switch (event.keyIdentifier) {
        // Prevents the default behavior. These key should be handled in
        // <audio-player> element.
        case 'Up':
        case 'Down':
        case 'PageUp':
        case 'PageDown':
          event.preventDefault();
          break;
      }
    },

    /**
     * Computes style for '.filled' element based on raw value.
     * @return {string}
     */
    computeFilledStyle_: function(rawValue) {
      return 'height: ' + rawValue + '%;';
    }
  });
})();  // Anonymous closure
