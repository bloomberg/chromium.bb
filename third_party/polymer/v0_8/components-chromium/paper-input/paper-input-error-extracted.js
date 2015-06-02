

(function() {

  Polymer({

    is: 'paper-input-error',

    behaviors: [
      Polymer.PaperInputAddonBehavior
    ],

    hostAttributes: {
      'role': 'alert'
    },

    properties: {

      /**
       * True if the error is showing.
       */
      invalid: {
        readOnly: true,
        reflectToAttribute: true,
        type: Boolean
      }

    },

    update: function(state) {
      this._setInvalid(state.invalid);
    }

  })

})();

