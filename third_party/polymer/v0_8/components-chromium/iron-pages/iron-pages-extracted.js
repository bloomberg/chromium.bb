

  Polymer({

    is: 'iron-pages',

    behaviors: [
      Polymer.IronResizableBehavior,
      Polymer.IronSelectableBehavior
    ],

    observers: [
      '_selectedPageChanged(selected)'
    ],

    _selectedPageChanged: function(selected, old) {
      this.async(this.notifyResize);
    }
  });

