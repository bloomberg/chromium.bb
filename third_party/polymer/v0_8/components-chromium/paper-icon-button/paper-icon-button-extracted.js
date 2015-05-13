
  Polymer({
    is: 'paper-icon-button',

    behaviors: [
      Polymer.PaperButtonBehavior
    ],

    enableCustomStyleProperties: true,

    properties: {
      /**
       * The URL of an image for the icon. If the src property is specified,
       * the icon property should not be.
       */
      src: {
        type: String
      },

      /**
       * Specifies the icon name or index in the set of icons available in
       * the icon's icon set. If the icon property is specified,
       * the src property should not be.
       */
      icon: {
        type: String
      }
    }
  });
