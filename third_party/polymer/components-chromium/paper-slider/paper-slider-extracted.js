

  Polymer('paper-slider', {
    
    /**
     * Fired when the slider's value changes.
     *
     * @event change
     */

    /**
     * Fired when the slider's value changes due to manual interaction.
     *
     * Changes to the slider's value due to changes in an underlying
     * bound variable will not trigger this event.
     *
     * @event manual-change
     */
     
    /**
     * If true, the slider thumb snaps to tick marks evenly spaced based
     * on the `step` property value.
     *
     * @attribute snaps
     * @type boolean
     * @default false
     */
    snaps: false,
    
    /**
     * If true, a pin with numeric value label is shown when the slider thumb 
     * is pressed.  Use for settings for which users need to know the exact 
     * value of the setting.
     *
     * @attribute pin
     * @type boolean
     * @default false
     */
    pin: false,
    
    /**
     * If true, this slider is disabled.  A disabled slider cannot be tapped
     * or dragged to change the slider value.
     *
     * @attribute disabled
     * @type boolean
     * @default false
     */
    disabled: false,
    
    /**
     * The number that represents the current secondary progress.
     *
     * @attribute secondaryProgress
     * @type number
     * @default 0
     */
    secondaryProgress: 0,
    
    /**
     * If true, an input is shown and user can use it to set the slider value.
     *
     * @attribute editable
     * @type boolean
     * @default false
     */
    editable: false,
    
    /**
     * The immediate value of the slider.  This value is updated while the user
     * is dragging the slider.
     *
     * @attribute immediateValue
     * @type number
     * @default 0
     */
    
    observe: {
      'min max step snaps': 'update'
    },
    
    ready: function() {
      this.update();
    },
    
    update: function() {
      this.positionKnob(this.calcRatio(this.value));
      this.updateMarkers();
    },
    
    valueChanged: function() {
      this.update();
      this.fire('change');
    },
    
    expandKnob: function() {
      this.$.sliderKnob.classList.add('expand');
    },
    
    resetKnob: function() {
      this.expandJob && this.expandJob.stop();
      this.$.sliderKnob.classList.remove('expand');
    },
    
    positionKnob: function(ratio) {
      this._ratio = ratio;
      this.immediateValue = this.calcStep(this.calcKnobPosition()) || 0;
      if (this.snaps) {
        this._ratio = this.calcRatio(this.immediateValue);
      }
      this.$.sliderKnob.style.left = this._ratio * 100 + '%';
    },
    
    immediateValueChanged: function() {
      this.$.sliderKnob.classList.toggle('ring', this.immediateValue <= this.min);
    },
    
    inputChange: function() {
      this.value = this.$.input.value;
      this.fire('manual-change');
    },
    
    calcKnobPosition: function() {
      return (this.max - this.min) * this._ratio + this.min;
    },
    
    measureWidth: function() {
      this._w = this.$.sliderBar.offsetWidth;
    },
    
    trackStart: function(e) {
      this.measureWidth();
      this._x = this._ratio * this._w;
      this._startx = this._x || 0;
      this._minx = - this._startx;
      this._maxx = this._w - this._startx;
      this.$.sliderKnob.classList.add('dragging');
      e.preventTap();
    },

    trackx: function(e) {
      var x = Math.min(this._maxx, Math.max(this._minx, e.dx));
      this._x = this._startx + x;
      this._ratio = this._x / this._w;
      this.immediateValue = this.calcStep(this.calcKnobPosition()) || 0;
      var s =  this.$.sliderKnob.style;
      s.transform = s.webkitTransform = 'translate3d(' + (this.snaps ? 
          (this.calcRatio(this.immediateValue) * this._w) - this._startx : x) + 'px, 0, 0)';
    },
    
    trackEnd: function() {
      var s =  this.$.sliderKnob.style;
      s.transform = s.webkitTransform = '';
      this.$.sliderKnob.classList.remove('dragging');
      this.resetKnob();
      this.value = this.immediateValue;
      this.fire('manual-change');
    },
    
    bardown: function(e) {
      this.measureWidth();
      this.$.sliderKnob.classList.add('transiting');
      var rect = this.$.sliderBar.getBoundingClientRect();
      this.positionKnob((e.x - rect.left) / this._w);
      this.value = this.calcStep(this.calcKnobPosition());
      this.expandJob = this.job(this.expandJob, this.expandKnob, 60);
      this.fire('manual-change');
    },
    
    knobTransitionEnd: function() {
      this.$.sliderKnob.classList.remove('transiting');
    },
    
    updateMarkers: function() {
      this.markers = [], l = (this.max - this.min) / this.step;
      for (var i = 0; i < l; i++) {
        this.markers.push('');
      }
    },
    
    increment: function() {
      this.value = this.clampValue(this.value + this.step);
    },
    
    decrement: function() {
      this.value = this.clampValue(this.value - this.step);
    },
    
    keydown: function(e) {
      if (this.disabled) {
        return;
      }
      var c = e.keyCode;
      if (c === 37) {
        this.decrement();
        this.fire('manual-change');
      } else if (c === 39) {
        this.increment();
        this.fire('manual-change');
      }
    }

  });

