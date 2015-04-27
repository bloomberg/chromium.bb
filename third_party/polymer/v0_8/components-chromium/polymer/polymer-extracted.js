

  Polymer.Base._addFeature({

    _registerFeatures: function() {
      // identity
      this._prepIs();
      // inheritance
      this._prepExtends();
      // factory
      this._prepConstructor();
      // template
      this._prepTemplate();
      // template markup
      this._prepAnnotations();
      // accessors
      this._prepEffects();
      // shared behaviors
      this._prepBehaviors();
      // accessors part 2
      this._prepBindings();
      // dom encapsulation
      this._prepShady();
    },

    _prepBehavior: function(b) {
      this._addPropertyEffects(b.properties || b.accessors);
      this._addComplexObserverEffects(b.observers);
    },

    _initFeatures: function() {
      // manage local dom
      this._poolContent();
      // manage configuration
      this._setupConfigure();
      // host stack
      this._pushHost();
      // instantiate template
      this._stampTemplate();
      // host stack
      this._popHost();
      // concretize template references
      this._marshalAnnotationReferences();
      // setup debouncers
      this._setupDebouncers();
      // concretize effects on instance
      this._marshalInstanceEffects();
      // acquire instance behaviors
      this._marshalBehaviors();
      // acquire initial instance attribute values
      this._marshalAttributes();
      // top-down initial distribution, configuration, & ready callback
      this._tryReady();
    },

    _marshalBehavior: function(b) {
      // publish attributes to instance
      this._installHostAttributes(b.hostAttributes);
      // establish listeners on instance
      this._listenListeners(b.listeners);
    }

  });

