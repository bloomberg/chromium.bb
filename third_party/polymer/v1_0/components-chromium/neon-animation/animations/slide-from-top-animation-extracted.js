Polymer({

    is: 'slide-from-top-animation',

    behaviors: [
      Polymer.NeonAnimationBehavior
    ],

    configure: function(config) {
      var node = config.node;

      if (config.transformOrigin) {
        this.setPrefixedProperty(node, 'transformOrigin', config.transformOrigin);
      } else {
        this.setPrefixedProperty(node, 'transformOrigin', '50% 0');
      }

      this._effect = new KeyframeEffect(node, [
        {'transform': 'translateY(-100%)'},
        {'transform': 'translateY(0%)'}
      ], this.timingFromConfig(config));

      return this._effect;
    }

  });