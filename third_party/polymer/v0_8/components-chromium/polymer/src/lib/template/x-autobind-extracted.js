

  Polymer({

    is: 'x-autobind',

    extends: 'template',

    _registerFeatures: function() {
      this._prepExtends();
      this._prepConstructor();
    },

    _finishDistribute: function() {
      var parentDom = Polymer.dom(Polymer.dom(this).parentNode);
      parentDom.insertBefore(this.root, this);
    },

    _initFeatures: function() {
      this._template = this;
      this._prepAnnotations();
      this._prepEffects();
      this._prepBehaviors();
      this._prepBindings();
      Polymer.Base._initFeatures.call(this);
    }

  });

