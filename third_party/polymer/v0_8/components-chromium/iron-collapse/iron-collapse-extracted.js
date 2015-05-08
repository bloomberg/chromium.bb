

  Polymer({

    is: 'iron-collapse',

    properties: {

      /**
       * If true, the orientation is horizontal; otherwise is vertical.
       *
       * @attribute horizontal
       * @type Boolean
       * @default false
       */
      horizontal: {
        type: Boolean,
        value: false,
        observer: 'horizontalChanged'
      },

      /**
       * Set opened to true to show the collapse element and to false to hide it.
       *
       * @attribute opened
       * @type Boolean
       * @default false
       */
      opened: {
        type: Boolean,
        value: false,
        notify: true,
        observer: 'openedChanged'
      }

    },

    listeners: {
      transitionend: 'transitionEnd'
    },

    ready: function() {
      // Avoid transition at the beginning e.g. page loads and enable
      // transitions only after the element is rendered and ready.
      this._enableTransition = true;
    },

    horizontalChanged: function() {
      this.dimension = this.horizontal ? 'width' : 'height';
      this.style.transitionProperty = this.dimension;
    },

    openedChanged: function() {
      this[this.opened ? 'show' : 'hide']();
    },

    /**
     * Toggle the opened state.
     *
     * @method toggle
     */
    toggle: function() {
      this.opened = !this.opened;
    },

    show: function() {
      this.toggleClass('iron-collapse-closed', false);
      this.updateSize('auto', false);
      var s = this.calcSize();
      this.updateSize('0px', false);
      // force layout to ensure transition will go
      this.offsetHeight;
      this.updateSize(s, true);
    },

    hide: function() {
      this.toggleClass('iron-collapse-opened', false);
      this.updateSize(this.calcSize(), false);
      // force layout to ensure transition will go
      this.offsetHeight;
      this.updateSize('0px', true);
    },

    updateSize: function(size, animated) {
      this.enableTransition(animated);
      var s = this.style;
      var nochange = s[this.dimension] === size;
      s[this.dimension] = size;
      if (animated && nochange) {
        this.transitionEnd();
      }
    },

    calcSize: function() {
      return this.getBoundingClientRect()[this.dimension] + 'px';
    },

    enableTransition: function(enabled) {
      this.style.transitionDuration = (enabled && this._enableTransition) ? '' : '0s';
    },

    transitionEnd: function() {
      if (this.opened) {
        this.updateSize('auto', false);
      }
      this.toggleClass('iron-collapse-closed', !this.opened);
      this.toggleClass('iron-collapse-opened', this.opened);
      this.enableTransition(false);
    }

  });

