

  (function() {

    var paperInput = CoreStyle.g.paperInput = CoreStyle.g.paperInput || {};

    paperInput.labelColor = '#757575';
    paperInput.focusedColor = '#4059a9';
    paperInput.invalidColor = '#d34336';

    Polymer('paper-input-decorator',{

      publish: {

        /**
         * The label for this input. It normally appears as grey text inside
         * the text input and disappears once the user enters text.
         *
         * @attribute label
         * @type string
         * @default ''
         */
        label: '',

        /**
         * If true, the label will "float" above the text input once the
         * user enters text instead of disappearing.
         *
         * @attribute floatingLabel
         * @type boolean
         * @default false
         */
        floatingLabel: false,

        /**
         * Set to true to style the element as disabled.
         *
         * @attribute disabled
         * @type boolean
         * @default false
         */
        disabled: {value: false, reflect: true},

        /**
         * Use this property to override the automatic label visibility.
         * If this property is set to `true` or `false`, the label visibility
         * will respect this value instead of be based on whether there is
         * a non-null value in the input.
         *
         * @attribute labelVisible
         * @type boolean
         * @default false
         */
        labelVisible: null,

        /**
         * Set this property to true to show the error message.
         *
         * @attribute isInvalid
         * @type boolean
         * @default false
         */
        isInvalid: false,

        /**
         * The message to display if the input value fails validation. If this
         * is unset or the empty string, a default message is displayed depending
         * on the type of validation error.
         *
         * @attribute error
         * @type string
         */
        error: '',

        focused: {value: false, reflect: true}

      },

      computed: {
        floatingLabelVisible: 'floatingLabel && !_labelVisible',
        _labelVisible: '(labelVisible === true || labelVisible === false) ? labelVisible : _autoLabelVisible'
      },

      ready: function() {
        // Delegate focus/blur events
        Polymer.addEventListener(this, 'focus', this.focusAction.bind(this), true);
        Polymer.addEventListener(this, 'blur', this.blurAction.bind(this), true);
      },

      attached: function() {
        this.input = this.querySelector('input,textarea');

        this.mo = new MutationObserver(function() {
          this.input = this.querySelector('input,textarea');
        }.bind(this));
        this.mo.observe(this, {childList: true});
      },

      detached: function() {
        this.mo.disconnect();
        this.mo = null;
      },

      prepareLabelTransform: function() {
        var toRect = this.$.floatedLabelText.getBoundingClientRect();
        var fromRect = this.$.labelText.getBoundingClientRect();
        if (toRect.width !== 0) {
          var sy = toRect.height / fromRect.height;
          this.$.labelText.cachedTransform =
            'scale3d(' + (toRect.width / fromRect.width) + ',' + sy + ',1) ' +
            'translate3d(0,' + (toRect.top - fromRect.top) / sy + 'px,0)';
        }
      },

      animateFloatingLabel: function() {
        if (!this.floatingLabel || this.labelAnimated) {
          return false;
        }

        if (!this.$.labelText.cachedTransform) {
          this.prepareLabelTransform();
        }

        // If there's still no cached transform, the input is invisible so don't
        // do the animation.
        if (!this.$.labelText.cachedTransform) {
          return false;
        }

        this.labelAnimated = true;
        // Handle interrupted animation
        this.async(function() {
          this.transitionEndAction();
        }, null, 250);

        if (this._labelVisible) {
          // Handle if the label started out floating
          if (!this.$.labelText.style.webkitTransform && !this.$.labelText.style.transform) {
            this.$.labelText.style.webkitTransform = this.$.labelText.cachedTransform;
            this.$.labelText.style.transform = this.$.labelText.cachedTransform;
            this.$.labelText.offsetTop;
          }
          this.$.labelText.style.webkitTransform = '';
          this.$.labelText.style.transform = '';
        } else {
          this.$.labelText.style.webkitTransform = this.$.labelText.cachedTransform;
          this.$.labelText.style.transform = this.$.labelText.cachedTransform;
          this.input.placeholder = '';
        }

        return true;
      },

      _labelVisibleChanged: function(old) {
        // do not do the animation on first render
        if (old !== undefined) {
          if (!this.animateFloatingLabel()) {
            this.updateInputLabel(this.input, this.label);
          }
        }
      },

      labelVisibleChanged: function() {
        if (this.labelVisible === 'true') {
          this.labelVisible = true;
        } else if (this.labelVisible === 'false') {
          this.labelVisible = false;
        }
      },

      labelChanged: function() {
        if (this.input) {
          this.updateInputLabel(this.input, this.label);
        }
      },

      isInvalidChanged: function() {
        this.classList.toggle('invalid', this.isInvalid);
      },

      focusedChanged: function() {
        this.updateLabelVisibility(this.input && this.input.value);
      },

      inputChanged: function(old) {
        if (this.input) {
          this.updateLabelVisibility(this.input.value);
          this.updateInputLabel(this.input, this.label);
        }
        if (old) {
          this.updateInputLabel(old, '');
        }
      },

      focusAction: function() {
        this.focused = true;
      },

      blurAction: function(e) {
        this.focused = false;
      },

      /**
       * Updates the label visibility based on a value. This is handled automatically
       * if the user is typing, but if you imperatively set the input value you need
       * to call this function.
       *
       * @method updateLabelVisibility
       * @param {string} value
       */
      updateLabelVisibility: function(value) {
        var v = (value !== null && value !== undefined) ? String(value) : value;
        this._autoLabelVisible = (!this.focused && !v) || (!this.floatingLabel && !v);
      },

      updateInputLabel: function(input, label) {
        if (this._labelVisible) {
          this.input.placeholder = this.label;
        } else {
          this.input.placeholder = '';
        }
        if (label) {
          input.setAttribute('aria-label', label);
        } else {
          input.removeAttribute('aria-label');
        }
      },

      inputAction: function(e) {
        this.updateLabelVisibility(e.target.value);
      },

      downAction: function(e) {
        if (this.disabled) {
          return;
        }

        if (this.focused) {
          return;
        }

        if (this.input) {
          this.input.focus();
          e.preventDefault();
        }

        // The underline spills from the tap location
        var rect = this.$.underline.getBoundingClientRect();
        var right = e.x - rect.left;
        this.$.focusedUnderline.style.mozTransformOrigin = right + 'px';
        this.$.focusedUnderline.style.webkitTransformOrigin = right + 'px ';
        this.$.focusedUnderline.style.transformOriginX = right + 'px';

        // Animations only run when the user interacts with the input
        this.underlineAnimated = true;

        // Handle interrupted animation
        this.async(function() {
          this.transitionEndAction();
        }, null, 250);
      },

      transitionEndAction: function() {
        this.underlineAnimated = false;
        this.labelAnimated = false;
        if (this._labelVisible) {
          this.input.placeholder = this.label;
        }
      }

    });

  }());

  