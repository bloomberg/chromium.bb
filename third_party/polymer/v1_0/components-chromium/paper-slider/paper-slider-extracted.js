Polymer({
    is: 'paper-slider',

    behaviors: [
      Polymer.IronFormElementBehavior,
      Polymer.PaperInkyFocusBehavior,
      Polymer.IronRangeBehavior
    ],

    properties: {

      /**
       * If true, the slider thumb snaps to tick marks evenly spaced based
       * on the `step` property value.
       */
      snaps: {
        type: Boolean,
        value: false,
        notify: true
      },

      /**
       * If true, a pin with numeric value label is shown when the slider thumb
       * is pressed. Use for settings for which users need to know the exact
       * value of the setting.
       */
      pin: {
        type: Boolean,
        value: false,
        notify: true
      },

      /**
       * The number that represents the current secondary progress.
       */
      secondaryProgress: {
        type: Number,
        value: 0,
        notify: true,
        observer: '_secondaryProgressChanged'
      },

      /**
       * If true, an input is shown and user can use it to set the slider value.
       */
      editable: {
        type: Boolean,
        value: false
      },

      /**
       * The immediate value of the slider.  This value is updated while the user
       * is dragging the slider.
       */
      immediateValue: {
        type: Number,
        value: 0,
        readOnly: true
      },

      /**
       * The maximum number of markers
       */
      maxMarkers: {
        type: Number,
        value: 0,
        notify: true,
        observer: '_maxMarkersChanged'
      },

      /**
       * If true, the knob is expanded
       */
      expand: {
        type: Boolean,
        value: false,
        readOnly: true
      },

      /**
       * True when the user is dragging the slider.
       */
      dragging: {
        type: Boolean,
        value: false,
        readOnly: true
      },

      transiting: {
        type: Boolean,
        value: false,
        readOnly: true
      },

      markers: {
        type: Array,
        readOnly: true,
        value: []
      },
    },

    observers: [
      '_updateKnob(value, min, max, snaps, step)',
      '_minChanged(min)',
      '_maxChanged(max)',
      '_valueChanged(value)',
      '_immediateValueChanged(immediateValue)'
    ],

    hostAttributes: {
      role: 'slider',
      tabindex: 0
    },

    keyBindings: {
      'left down pagedown home': '_decrementKey',
      'right up pageup end': '_incrementKey'
    },

    ready: function() {
      // issue polymer/polymer#1305

      this.async(function() {
        this._updateKnob(this.value);
      }, 1);
    },

    /**
     * Increases value by `step` but not above `max`.
     * @method increment
     */
    increment: function() {
      this.value = this._clampValue(this.value + this.step);
    },

    /**
     * Decreases value by `step` but not below `min`.
     * @method decrement
     */
    decrement: function() {
      this.value = this._clampValue(this.value - this.step);
    },

    _updateKnob: function(value) {
      this._positionKnob(this._calcRatio(value));
    },

    _minChanged: function() {
      this.setAttribute('aria-valuemin', this.min);
    },

    _maxChanged: function() {
      this.setAttribute('aria-valuemax', this.max);
    },

    _valueChanged: function() {
      this.setAttribute('aria-valuenow', this.value);
      this.fire('value-change');
    },

    _immediateValueChanged: function() {
      if (this.dragging) {
        this.fire('immediate-value-change');
      } else {
        this.value = this.immediateValue;
      }
    },

    _secondaryProgressChanged: function() {
      this.secondaryProgress = this._clampValue(this.secondaryProgress);
    },

    _fixForInput: function(immediateValue) {
      // paper-input/issues/114
      return this.immediateValue.toString();
    },

    _expandKnob: function() {
      this._setExpand(true);
    },

    _resetKnob: function() {
      this.cancelDebouncer('expandKnob');
      this._setExpand(false);
    },

    _positionKnob: function(ratio) {
      this._setImmediateValue(this._calcStep(this._calcKnobPosition(ratio)));
      this._setRatio(this._calcRatio(this.immediateValue));

      this.$.sliderKnob.style.left = (this.ratio * 100) + '%';
    },

    _inputChange: function() {
      this.value = this.$$('#input').value;
      this.fire('change');
    },

    _calcKnobPosition: function(ratio) {
      return (this.max - this.min) * ratio + this.min;
    },

    _onTrack: function(event) {
      event.stopPropagation();
      switch (event.detail.state) {
        case 'start':
          this._trackStart(event);
          break;
        case 'track':
          this._trackX(event);
          break;
        case 'end':
          this._trackEnd();
          break;
      }
    },

    _trackStart: function(event) {
      this._w = this.$.sliderBar.offsetWidth;
      this._x = this.ratio * this._w;
      this._startx = this._x || 0;
      this._minx = - this._startx;
      this._maxx = this._w - this._startx;
      this.$.sliderKnob.classList.add('dragging');

      this._setDragging(true);
    },

    _trackX: function(e) {
      if (!this.dragging) {
        this._trackStart(e);
      }

      var dx = Math.min(this._maxx, Math.max(this._minx, e.detail.dx));
      this._x = this._startx + dx;

      var immediateValue = this._calcStep(this._calcKnobPosition(this._x / this._w));
      this._setImmediateValue(immediateValue);

      // update knob's position
      var translateX = ((this._calcRatio(immediateValue) * this._w) - this._startx);
      this.translate3d(translateX + 'px', 0, 0, this.$.sliderKnob);
    },

    _trackEnd: function() {
      var s = this.$.sliderKnob.style;

      this.$.sliderKnob.classList.remove('dragging');
      this._setDragging(false);
      this._resetKnob();
      this.value = this.immediateValue;

      s.transform = s.webkitTransform = '';

      this.fire('change');
    },

    _knobdown: function(event) {
      this._expandKnob();

      // cancel selection
      event.preventDefault();

      // set the focus manually because we will called prevent default
      this.focus();
    },

    _bardown: function(event) {
      this._w = this.$.sliderBar.offsetWidth;
      var rect = this.$.sliderBar.getBoundingClientRect();
      var ratio = (event.detail.x - rect.left) / this._w;
      var prevRatio = this.ratio;

      this._setTransiting(true);

      this._positionKnob(ratio);

      this.debounce('expandKnob', this._expandKnob, 60);

      // if the ratio doesn't change, sliderKnob's animation won't start
      // and `_knobTransitionEnd` won't be called
      // Therefore, we need to manually update the `transiting` state

      if (prevRatio === this.ratio) {
        this._setTransiting(false);
      }

      this.async(function() {
        this.fire('change');
      });

      // cancel selection
      event.preventDefault();
    },

    _knobTransitionEnd: function(event) {
      if (event.target === this.$.sliderKnob) {
        this._setTransiting(false);
      }
    },

    _maxMarkersChanged: function(maxMarkers) {
      var l = (this.max - this.min) / this.step;
      if (!this.snaps && l > maxMarkers) {
        this._setMarkers([]);
      } else {
        this._setMarkers(new Array(l));
      }
    },

    _mergeClasses: function(classes) {
      return Object.keys(classes).filter(
        function(className) {
          return classes[className];
        }).join(' ');
    },

    _getClassNames: function() {
      var classes = {};

      classes.disabled = this.disabled;
      classes.pin = this.pin;
      classes.snaps = this.snaps;
      classes.ring = this.immediateValue <= this.min;
      classes.expand = this.expand;
      classes.dragging = this.dragging;
      classes.transiting = this.transiting;
      classes.editable = this.editable;

      return this._mergeClasses(classes);
    },

    _getProgressClass: function() {
      return this._mergeClasses({
        transiting: this.transiting
      });
    },

    _incrementKey: function(event) {
      if (event.detail.key === 'end') {
        this.value = this.max;
      } else {
        this.increment();
      }
      this.fire('change');
    },

    _decrementKey: function(event) {
      if (event.detail.key === 'home') {
        this.value = this.min;
      } else {
        this.decrement();
      }
      this.fire('change');
    }
  });

  /**
   * Fired when the slider's value changes.
   *
   * @event value-change
   */

  /**
   * Fired when the slider's immediateValue changes.
   *
   * @event immediate-value-change
   */

  /**
   * Fired when the slider's value changes due to user interaction.
   *
   * Changes to the slider's value due to changes in an underlying
   * bound variable will not trigger this event.
   *
   * @event change
   */