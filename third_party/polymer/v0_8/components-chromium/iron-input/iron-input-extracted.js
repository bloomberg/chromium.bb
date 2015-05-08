

  Polymer({

    is: 'iron-input',

    extends: 'input',

    properties: {

      /**
       * Use this property instead of `value` for two-way data binding.
       */
      bindValue: {
        observer: '_bindValueChanged',
        type: String
      },

      /**
       * Set to true to prevent the user from entering invalid input or setting
       * invalid `bindValue`.
       */
      preventInvalidInput: {
        type: Boolean
      },

      _previousValidInput: {
        type: String,
        value: ''
      }

    },

    listeners: {
      'input': '_onInput'
    },

    ready: function() {
      this._validateValue();
      this.bindValue = this.value;
    },

    _bindValueChanged: function() {
      // If this was called as a result of user input, then |_validateValue|
      // has already been called in |_onInput|, and it doesn't need to be
      // called again.
      if (this.value != this.bindValue) {
        this.value = this.bindValue;
        this._validateValue();
      }

      // manually notify because we don't want to notify until after setting value
      this.fire('bind-value-changed', {value: this.bindValue});
    },

    _onInput: function() {
      this._validateValue();
    },

    _validateValue: function() {
      var value;
      if (this.preventInvalidInput && !this.validity.valid) {
        value = this._previousValidInput;
      } else {
        value = this._previousValidInput = this.value;
      }
      this.bindValue = this.value = value;
    }

  })
