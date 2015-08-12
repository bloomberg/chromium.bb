Polymer({

    is: 'paper-button',

    behaviors: [
      Polymer.PaperButtonBehavior
    ],

    properties: {

      /**
       * If true, the button should be styled with a shadow.
       */
      raised: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
        observer: '_calculateElevation'
      }
    },

    _calculateElevation: function() {
      if (!this.raised) {
        this._elevation = 0;
      } else {
        Polymer.PaperButtonBehaviorImpl._calculateElevation.apply(this);
      }
    },

    _computeContentClass: function(receivedFocusFromKeyboard) {
      var className = 'content ';
      if (receivedFocusFromKeyboard) {
        className += ' keyboard-focus';
      }
      return className;
    }
  });