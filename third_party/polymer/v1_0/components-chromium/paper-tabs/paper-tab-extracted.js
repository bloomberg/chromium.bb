

  Polymer({

    is: 'paper-tab',

    behaviors: [
      Polymer.IronControlState,
      Polymer.IronButtonState
    ],

    properties: {

      /**
       * If true, ink ripple effect is disabled.
       *
       * @attribute noink
       */
      noink: {
        type: Boolean,
        value: false
      }

    },

    hostAttributes: {
      role: 'tab'
    },

    listeners: {
      down: '_updateNoink'
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

