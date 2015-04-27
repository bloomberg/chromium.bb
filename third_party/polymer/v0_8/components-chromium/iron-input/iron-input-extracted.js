

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
      }

    },

    listeners: {
      'input': '_onInput'
    },

    attached: function() {
      this.bindValue = this.value;
    },

    _bindValueChanged: function() {
      this.value = this.bindValue;
      // manually notify because we don't want to notify until after setting value
      this.fire('bind-value-changed', {value: this.bindValue});
    },

    _onInput: function(event) {
      this.bindValue = event.target.value;
    }

  })
