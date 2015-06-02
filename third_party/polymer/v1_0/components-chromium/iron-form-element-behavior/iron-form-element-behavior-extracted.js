

  /** 
  
  @demo demo/index.html
  @polymerBehavior 
  
  */
  Polymer.IronFormElementBehavior = {

    properties: {

      /**
       * The name of this element.
       */
      name: {
        type: String
      },

      /**
       * The value for this element.
       */
      value: {
        notify: true,
        type: String
      },
    },

    attached: function() {
      this.fire('iron-form-element-register');
    }

  };

