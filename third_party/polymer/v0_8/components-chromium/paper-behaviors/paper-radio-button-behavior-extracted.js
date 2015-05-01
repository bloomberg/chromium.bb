

  Polymer.PaperRadioButtonInk = {

    observers: [
      `_focusedChanged(focused)`
    ],

    _focusedChanged: function(focused) {
      if (!this.$.ink)
        return;

      if (focused) {
        var rect = this.$.ink.getBoundingClientRect();
        this.$.ink.mousedownAction();
      } else {
        this.$.ink.mouseupAction();
      }
    }

  };

  Polymer.PaperRadioButtonBehavior = [
    Polymer.IronControlState,
    Polymer.IronButtonState,
    Polymer.PaperRadioButtonInk
  ];

