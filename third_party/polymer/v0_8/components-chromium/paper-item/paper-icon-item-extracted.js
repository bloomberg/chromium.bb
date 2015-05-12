

(function() {

  Polymer({

    is: 'paper-icon-item',

    enableCustomStyleProperties: true,

    hostAttributes: {
      'role': 'listitem'
    },

    properties: {

      /**
       * The width of the icon area.
       *
       * @attribute iconWidth
       * @type String
       * @default '56px'
       */
      iconWidth: {
        type: String,
        value: '56px'
      }

    },

    ready: function() {
      this.$.contentIcon.style.width = this.iconWidth;
    }

  });

})();

