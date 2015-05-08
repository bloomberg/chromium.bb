

  Polymer({

    is: 'paper-button',

    behaviors: [
      Polymer.PaperButtonBehavior
    ],

    properties: {

      /**
       * If true, the button should be styled with a shadow.
       *
       * @attribute raised
       * @type boolean
       * @default false
       */
      raised: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
        observer: '_buttonStateChanged'
      }

    },

    ready: function() {
      if (!this.hasAttribute('role')) {
        this.setAttribute('role', 'button');
      }
    },

    _buttonStateChanged: function() {
      this._calculateElevation();
    }

  });

