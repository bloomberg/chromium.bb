
    Polymer({
      is: 'paper-item',

      hostAttributes: {
        role: 'listitem',
        tabindex: '0'
      },

      behaviors: [
        Polymer.IronControlState,
        Polymer.IronButtonState
      ]
    });
  