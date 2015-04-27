

  Polymer.DomModule = document.createElement('dom-module');

  Polymer.Base._addFeature({

    _registerFeatures: function() {
      // identity
      this._prepIs();
      // shared behaviors
      this._prepBehaviors();
      // inheritance
      this._prepExtends();
     // factory
      this._prepConstructor();
      // template
      this._prepTemplate();
      // dom encapsulation
      this._prepShady();
    },

    _prepBehavior: function() {},

    _initFeatures: function() {
      // manage local dom
      this._poolContent();
      // host stack
      this._pushHost();
      // instantiate template
      this._stampTemplate();
      // host stack
      this._popHost();
      // setup debouncers
      this._setupDebouncers();
      // instance shared behaviors
      this._marshalBehaviors();
      // top-down initial distribution, configuration, & ready callback
      this._tryReady();
    },

    _marshalBehavior: function(b) {
      // publish attributes to instance
      this._installHostAttributes(b.hostAttributes);
    }

  });

