

(function() {

  Polymer({

    is: 'paper-input-error',

    enableCustomStyleProperties: true,

    hostAttributes: {
      'add-on': '',
      'role': 'alert'
    },

    properties: {

      /**
       * Set to true to show the error.
       */
      invalid: {
        reflectToAttribute: true,
        type: Boolean
      }

    },

    attached: function() {
      this.fire('addon-attached');
    }

  })

})();

