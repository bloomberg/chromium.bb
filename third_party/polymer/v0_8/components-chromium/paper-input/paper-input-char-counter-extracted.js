

(function() {

  Polymer({

    is: 'paper-input-char-counter',

    enableCustomStyleProperties: true,

    hostAttributes: {
      'add-on': ''
    },

    properties: {

      /**
       * The associated input element.
       */
      inputElement: {
        type: Object
      },

      /**
       * The current value of the input element.
       */
      value: {
        type: String
      },

      /**
       * The character counter string.
       */
      charCounter: {
        computed: '_computeCharCounter(inputElement,value)',
        type: String
      }

    },

    attached: function() {
      this.fire('addon-attached');
    },

    _computeCharCounter: function(inputElement,value) {
      var str = value.length;
      if (inputElement.hasAttribute('maxlength')) {
        str += '/' + inputElement.maxLength;
      }
      return str;
    }

  });

})();

