Polymer('paper-menu-button-overlay-container');;

    Polymer('paper-menu-button', {

      publish: {

        /**
         * If true, this menu is currently visible.
         *
         * @attribute opened
         * @type boolean
         * @default false
         */
        opened: { value: false, reflect: true },

        /**
         * The horizontal alignment of the pulldown menu relative to the button.
         *
         * @attribute halign
         * @type 'left' | 'right'
         * @default 'left'
         */
        halign: { value: 'left', reflect: true },

        /**
         * The vertical alignment of the pulldown menu relative to the button.
         *
         * @attribute valign
         * @type 'bottom' | 'top'
         * @default 'top'
         */
        valign: {value: 'top', reflect: true}
      },

      /**
       * The URL of an image for the icon.  Should not use `icon` property
       * if you are using this property.
       *
       * @attribute src
       * @type string
       * @default ''
       */
      src: '',

      /**
       * Specifies the icon name or index in the set of icons available in
       * the icon set.  Should not use `src` property if you are using this
       * property.
       *
       * @attribute icon
       * @type string
       * @default ''
       */
      icon: '',

      slow: false,

      tapAction: function() {
        if (this.disabled) {
          return;
        }

        this.super();
        this.toggle();
      },

      /**
       * Toggle the opened state of the menu.
       *
       * @method toggle
       */
      toggle: function() {
        this.opened = !this.opened;
      }

    });
  