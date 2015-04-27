

  /**
   * Automatically extend using objects referenced in `mixins` array. 
   * 
   *     Polymer({
   *     
   *       mixins: [
   *         someMixinObject
   *       ]
   *     
   *       ...
   *     
   *     });
   * 
   * @class base feature: mixins
   */

  Polymer.Base._addFeature({

    _prepMixins: function() {
      if (this.mixins) {
        this.mixins.forEach(function(m) {
          Polymer.Base.extend(this, m);
        }, this);
      }
    }

  });

