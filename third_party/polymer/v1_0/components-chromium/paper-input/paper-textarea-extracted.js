

(function() {

  Polymer({

    is: 'paper-textarea',

    behaviors: [
      Polymer.PaperInputBehavior
    ],

    properties: {

      _ariaLabelledBy: {
        observer: '_ariaLabelledByChanged',
        type: String
      },

      _ariaDescribedBy: {
        observer: '_ariaDescribedByChanged',
        type: String
      }

    },

    _ariaLabelledByChanged: function(ariaLabelledBy) {
      this.$.input.textarea.setAttribute('aria-labelledby', ariaLabelledBy);
    },

    _ariaDescribedByChanged: function(ariaDescribedBy) {
      this.$.input.textarea.setAttribute('aria-describedby', ariaDescribedBy);
    }

  });

})();

