Polymer({
      is: 'paper-icon-item',

      hostAttributes: {
        'role': 'option',
        'tabindex': '0'
      },

      behaviors: [
        Polymer.IronControlState,
        Polymer.IronButtonState
      ]
    });