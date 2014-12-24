

  Polymer('paper-tab', {
    
    /**
     * If true, ink ripple effect is disabled.
     *
     * @attribute noink
     * @type boolean
     * @default false
     */
    noink: false,
    
    eventDelegates: {
      down: 'downAction',
      up: 'upAction'
    },

    downAction: function(e) {
      if (this.noink || (this.parentElement && this.parentElement.noink)) {
        return;
      }
      this.$.ink.downAction(e);
    },

    upAction: function() {
      this.$.ink.upAction();
    },
    
    cancelRipple: function() {
      this.$.ink.upAction();
    }
    
  });
  
