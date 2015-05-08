
(function() {

  Polymer({

    is: 'paper-input-container',

    enableCustomStyleProperties: true,

    properties: {

      /**
       * Set to true to disable the floating label.
       */
      noLabelFloat: {
        type: Boolean,
        value: false
      },

      /**
       * The attribute to listen for value changes on.
       */
      attrForValue: {
        type: String,
        value: 'bind-value'
      },

      /**
       * Set to true to auto-validate the input value.
       */
      autoValidate: {
        type: Boolean,
        value: false
      },

      /**
       * True if the input has focus.
       */
      focused: {
        readOnly: true,
        type: Boolean,
        value: false
      },

      _addons: {
        type: Array,
        value: function() {
          return [];
        }
      },

      _inputHasContent: {
        type: Boolean,
        value: false
      },

      _inputIsInvalid: {
        type: Boolean,
        value: false
      },

      _inputSelector: {
        type: String,
        value: 'input,textarea,.paper-input-input'
      },

      _boundOnFocus: {
        type: Function,
        value: function() {
          return this._onFocus.bind(this);
        }
      },

      _boundOnBlur: {
        type: Function,
        value: function() {
          return this._onBlur.bind(this);
        }
      },

      _boundValueChanged: {
        type: Function,
        value: function() {
          return this._onValueChanged.bind(this);
        }
      }

    },

    listeners: {
      'addon-attached': '_onAddonAttached',
      'input': '_onInput'
    },

    get _valueChangedEvent() {
      return this.attrForValue + '-changed';
    },

    get _propertyForValue() {
      return Polymer.CaseMap.dashToCamelCase(this.attrForValue);
    },

    get _inputElement() {
      return Polymer.dom(this).querySelector(this._inputSelector);
    },

    ready: function() {
      this.addEventListener('focus', this._boundOnFocus, true);
      this.addEventListener('blur', this._boundOnBlur, true);
      this.addEventListener(this._valueChangedEvent, this._boundValueChanged, true);
    },

    attached: function() {
      this._handleInput(this._inputElement);
    },

    _onAddonAttached: function(event) {
      this._addons.push(event.target);
      this._handleInput(this._inputElement);
    },

    _onFocus: function() {
      this._setFocused(true);
    },

    _onBlur: function() {
      this._setFocused(false);
    },

    _onInput: function(event) {
      this._handleInput(event.target);
    },

    _onValueChanged: function(event) {
      this._handleInput(event.target);
    },

    _handleInput: function(inputElement) {
      var value = inputElement[this._propertyForValue] || inputElement.value;
      var valid = inputElement.checkValidity();

      // type="number" hack needed because this.value is empty until it's valid
      if (value || inputElement.type === 'number' && !valid) {
        this._inputHasContent = true;
      } else {
        this._inputHasContent = false;
      }

      if (this.autoValidate) {
        this._inputIsInvalid = !valid;
      }

      // notify add-ons
      for (var addon, i = 0; addon = this._addons[i]; i++) {
        // need to set all of these, or call method... thanks input type="number"!
        addon.inputElement = inputElement;
        addon.value = value;
        addon.invalid = !valid;
      }
    },

    _computeInputContentClass: function(noLabelFloat, focused, _inputHasContent, _inputIsInvalid) {
      var cls = 'input-content';
      if (!noLabelFloat) {
        if (_inputHasContent) {
          cls += ' label-is-floating';
          if (_inputIsInvalid) {
            cls += ' is-invalid';
          } else if (focused) {
            cls += " label-is-highlighted";
          }
        }
      } else {
        if (_inputHasContent) {
          cls += ' label-is-hidden';
        }
      }
      return cls;
    },

    _computeUnderlineClass: function(focused, _inputIsInvalid) {
      var cls = 'underline';
      if (_inputIsInvalid) {
        cls += ' is-invalid';
      } else if (focused) {
        cls += ' is-highlighted'
      }
      return cls;
    },

    _computeAddOnContentClass: function(focused, _inputIsInvalid) {
      var cls = 'add-on-content';
      if (_inputIsInvalid) {
        cls += ' is-invalid';
      } else if (focused) {
        cls += ' is-highlighted'
      }
      return cls;
    }

  });

})();
