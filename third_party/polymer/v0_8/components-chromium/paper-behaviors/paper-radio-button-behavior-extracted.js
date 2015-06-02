

  /** @polymerBehavior */
  Polymer.PaperRadioButtonInk = {

    observers: [
      '_focusedChanged(receivedFocusFromKeyboard)'
    ],

    _focusedChanged: function(receivedFocusFromKeyboard) {
      if (!this.$.ink) {
        return;
      }

      this.$.ink.holdDown = receivedFocusFromKeyboard;
    }

  };

  /** @polymerBehavior */
  Polymer.PaperRadioButtonBehavior = [
    Polymer.IronButtonState,
    Polymer.IronControlState,
    Polymer.PaperRadioButtonInk
  ];

