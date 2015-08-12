Polymer({

    is: 'paper-tab',

    behaviors: [
      Polymer.IronControlState
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
      down: '_onDown'
    },

    get _parentNoink () {
      var parent = Polymer.dom(this).parentNode;
      return !!parent && !!parent.noink;
    },

    _onDown: function(e) {
      this.noink = !!this.noink || !!this._parentNoink;
    }
  });