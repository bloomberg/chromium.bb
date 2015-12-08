Polymer({
      is: 'paper-tab',

      behaviors: [
        Polymer.IronControlState,
        Polymer.IronButtonState,
        Polymer.PaperRippleBehavior
      ],

      hostAttributes: {
        role: 'tab'
      },

      listeners: {
        down: '_updateNoink'
      },

      ready: function() {
        var ripple = this.getRipple();
        ripple.initialOpacity = 0.95;
        ripple.opacityDecayVelocity = 0.98;
      },

      attached: function() {
        this._updateNoink();
      },

      get _parentNoink () {
        var parent = Polymer.dom(this).parentNode;
        return !!parent && !!parent.noink;
      },

      _updateNoink: function() {
        this.noink = !!this.noink || !!this._parentNoink;
      }
    });