

  /**
   * Support `extends` property (for type-extension only).
   *
   * If the mixin is String-valued, the corresponding Polymer module
   * is mixed in.
   *
   *     Polymer({
   *       is: 'pro-input',
   *       extends: 'input',
   *       ...
   *     });
   * 
   * Type-extension objects are created using `is` notation in HTML, or via
   * the secondary argument to `document.createElement` (the type-extension
   * rules are part of the Custom Elements specification, not something 
   * created by Polymer). 
   * 
   * Example:
   * 
   *     <!-- right: creates a pro-input element -->
   *     <input is="pro-input">
   *   
   *     <!-- wrong: creates an unknown element -->
   *     <pro-input>  
   * 
   *     <script>
   *        // right: creates a pro-input element
   *        var elt = document.createElement('input', 'pro-input');
   * 
   *        // wrong: creates an unknown element
   *        var elt = document.createElement('pro-input');
   *     <\script>
   *
   *   @class base feature: extends
   */

  Polymer.Base._addFeature({

    _prepExtends: function() {
      if (this.extends) {
        this.__proto__ = this.getExtendedPrototype(this.extends);
      }
    },

    getExtendedPrototype: function(tag) {
      return this.getExtendedNativePrototype(tag);
    },

    nativePrototypes: {}, // static

    getExtendedNativePrototype: function(tag) {
      var p = this.nativePrototypes[tag];
      if (!p) {
        var np = this.getNativePrototype(tag);
        p = this.extend(Object.create(np), Polymer.Base);
        this.nativePrototypes[tag] = p;
      }
      return p;
    },

    getNativePrototype: function(tag) {
      // TODO(sjmiles): sad necessity
      return Object.getPrototypeOf(document.createElement(tag));
    }

  });

