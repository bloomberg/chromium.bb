

  /**
   * Automatically extend using objects referenced in `behaviors` array.
   *
   *     someBehaviorObject = {
   *       accessors: {
   *        value: {type: Number, observer: '_numberChanged'}
   *       },
   *       observers: [
   *         // ...
   *       ],
   *       ready: function() {
   *         // called before prototoype's ready
   *       },
   *       _numberChanged: function() {}
   *     };
   *
   *     Polymer({
   *
   *       behaviors: [
   *         someBehaviorObject
   *       ]
   *
   *       ...
   *
   *     });
   *
   * @class base feature: behaviors
   */

  Polymer.Base._addFeature({

    behaviors: [],

    _prepBehaviors: function() {
      this._flattenBehaviors();
      this._prepBehavior(this);
      this.behaviors.forEach(function(b) {
        this._mixinBehavior(b);
        this._prepBehavior(b);
      }, this);
    },

    _flattenBehaviors: function() {
      var flat = [];
      this.behaviors.forEach(function(b) {
        if (!b) {
          console.warn('Polymer: undefined behavior in [' + this.is + ']');
        } else if (b instanceof Array) {
          flat = flat.concat(b);
        } else {
          flat.push(b);
        }
      }, this);
      this.behaviors = flat;
    },

    _mixinBehavior: function(b) {
      Object.getOwnPropertyNames(b).forEach(function(n) {
        switch (n) {
          case 'registered':
          case 'properties':
          case 'observers':
          case 'listeners':
          case 'keyPresses':
          case 'hostAttributes':
          case 'created':
          case 'attached':
          case 'detached':
          case 'attributeChanged':
          case 'configure':
          case 'ready':
            break;
          default:
            this.copyOwnProperty(n, b, this);
            break;
        }
      }, this);
    },

    _doBehavior: function(name, args) {
      this.behaviors.forEach(function(b) {
        this._invokeBehavior(b, name, args);
      }, this);
      this._invokeBehavior(this, name, args);
    },

    _invokeBehavior: function(b, name, args) {
      var fn = b[name];
      if (fn) {
        fn.apply(this, args || Polymer.nar);
      }
    },

    _marshalBehaviors: function() {
      this.behaviors.forEach(function(b) {
        this._marshalBehavior(b);
      }, this);
      this._marshalBehavior(this);
    }

  });

