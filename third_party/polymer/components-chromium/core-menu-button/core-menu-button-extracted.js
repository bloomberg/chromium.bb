

  Polymer('core-menu-button',{

    overlayListeners: {
      'core-overlay-open': 'openAction',
      'core-activate': 'activateAction'
    },

    activateAction: function() {
      this.opened = false;
    }

  });

