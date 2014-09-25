
    Polymer('paper-menu-button-overlay', {

      publish: {

        /**
         * The `relatedTarget` is an element used to position the overlay, for example a
         * button the user taps to show a menu.
         *
         * @attribute relatedTarget
         * @type Element
         */
        relatedTarget: null,

        /**
         * The horizontal alignment of the overlay relative to the `relatedTarget`.
         *
         * @attribute halign
         * @type 'left'|'right'|'center'
         * @default 'left'
         */
        halign: 'left'

      },

      updateTargetDimensions: function() {
        this.super();

        var t = this.target;
        this.target.cachedSize = t.getBoundingClientRect();
      },

      positionTarget: function() {
        if (this.relatedTarget) {

          var rect = this.relatedTarget.getBoundingClientRect();

          if (this.halign === 'left') {
            this.target.style.left = rect.left + 'px';
          } else if (this.halign === 'right') {
            this.target.style.right = (window.innerWidth - rect.right) + 'px';
          } else {
            this.target.style.left = (rect.left - (rect.width - this.target.cachedSize.width) / 2) + 'px';
          }

          if (this.valign === 'top') {
            this.target.style.top = rect.top + 'px';
          } else if (this.valign === 'bottom') {
            this.target.style.top = rect.bottom + 'px';
          } else {
            this.target.style.top = rect.top + 'px';
          }

          // this.target.style.top = rect.top + 'px';

        } else {
          this.super();
        }
      }

    });
  