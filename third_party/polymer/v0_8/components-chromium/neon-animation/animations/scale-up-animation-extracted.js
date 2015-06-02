

  Polymer({

    is: 'scale-up-animation',

    behaviors: [
      Polymer.NeonAnimationBehavior
    ],

    configure: function(config) {
      var node = config.node;

      if (config.transformOrigin) {
        this.setPrefixedProperty(node, 'transformOrigin', config.transformOrigin);
      }

      this._effect = new KeyframeEffect(node, [
        {'transform': 'scale(0)'},
        {'transform': 'scale(1)'}
      ], this.timingFromConfig(config));

      return this._effect;
    }

  });

