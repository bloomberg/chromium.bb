

  Polymer.PaperButtonElevation = {

    properties: {

      _elevation: {
        type: Number
      }

    },

    _calculateElevation: function() {
      var e = 1;
      if (this.disabled || !this.raised) {
        e = 0;
      } else if (this.active || this.pressed) {
        e = 2;
      } else if (this.focused) {
        e = 3;
      }
      this._elevation = e;
    }

  };

  Polymer.PaperButtonBehavior = [
    Polymer.IronControlState,
    Polymer.IronButtonState,
    Polymer.PaperButtonElevation
  ];

