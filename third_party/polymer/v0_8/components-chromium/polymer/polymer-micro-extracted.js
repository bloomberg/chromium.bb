

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
    },

    _prepBehavior: function() {},

    _initFeatures: function() {
      // setup debouncers
      this._setupDebouncers();
      // acquire behaviors
      this._marshalBehaviors();
    },

    _marshalBehavior: function(b) {
      // publish attributes to instance
      this._installHostAttributes(b.hostAttributes);
    }

  });

