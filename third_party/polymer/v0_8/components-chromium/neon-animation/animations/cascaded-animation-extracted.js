

  Polymer({

    is: 'cascaded-animation',

    behaviors: [
      Polymer.NeonAnimationBehavior
    ],

    properties: {

      _animationMeta: {
        type: Object,
        value: function() {
          return new Polymer.IronMeta({type: 'animation'});
        }
      }

    },

    configure: function(config) {
      var animationConstructor = this._animationMeta.byKey(config.animation);
      if (!animationConstructor) {
        console.warn(this.is + ':', 'constructor for', config.animation, 'not found!');
        return;
      }

      var nodes = config.nodes;
      var effects = [];
      var nodeDelay = config.nodeDelay || 50;

      config.timing = config.timing || {};
      config.timing.delay = config.timing.delay || 0;

      var oldDelay = config.timing.delay;
      for (var node, index = 0; node = nodes[index]; index++) {
        config.timing.delay += nodeDelay;
        config.node = node;

        var animation = new animationConstructor();
        var effect = animation.configure(config);

        effects.push(effect);
      }
      config.timing.delay = oldDelay;

      this._effect = new GroupEffect(effects);
      return this._effect;
    }

  });

