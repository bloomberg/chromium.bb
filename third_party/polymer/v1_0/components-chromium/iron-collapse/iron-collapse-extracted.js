Polymer({

    is: 'iron-collapse',

    properties: {

      /**
       * If true, the orientation is horizontal; otherwise is vertical.
       *
       * @attribute horizontal
       */
      horizontal: {
        type: Boolean,
        value: false,
        observer: '_horizontalChanged'
      },

      /**
       * Set opened to true to show the collapse element and to false to hide it.
       *
       * @attribute opened
       */
      opened: {
        type: Boolean,
        value: false,
        notify: true,
        observer: '_openedChanged'
      }

    },

    hostAttributes: {
      role: 'group',
      'aria-expanded': 'false'
    },

    listeners: {
      transitionend: '_transitionEnd'
    },

    ready: function() {
      // Avoid transition at the beginning e.g. page loads and enable
      // transitions only after the element is rendered and ready.
      this._enableTransition = true;
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
      this.opened = true;    
    },

    hide: function() {
      this.opened = false;    
    },

    updateSize: function(size, animated) {
      this.enableTransition(animated);
      var s = this.style;
      var nochange = s[this.dimension] === size;
      s[this.dimension] = size;
      if (animated && nochange) {
        this._transitionEnd();
      }
    },

    enableTransition: function(enabled) {
      this.style.transitionDuration = (enabled && this._enableTransition) ? '' : '0s';
    },

    _horizontalChanged: function() {
      this.dimension = this.horizontal ? 'width' : 'height';
      this.style.transitionProperty = this.dimension;
    },

    _openedChanged: function() {
      if (this.opened) {
        this.setAttribute('aria-expanded', 'true');
        this.setAttribute('aria-hidden', 'false');

        this.toggleClass('iron-collapse-closed', false);
        this.updateSize('auto', false);
        var s = this._calcSize();
        this.updateSize('0px', false);
        // force layout to ensure transition will go
        /** @suppress {suspiciousCode} */ this.offsetHeight;
        this.updateSize(s, true);
        // focus the current collapse
        this.focus();
      } else {
        this.setAttribute('aria-expanded', 'false');
        this.setAttribute('aria-hidden', 'true');

        this.toggleClass('iron-collapse-opened', false);
        this.updateSize(this._calcSize(), false);
        // force layout to ensure transition will go
        /** @suppress {suspiciousCode} */ this.offsetHeight;
        this.updateSize('0px', true);
      }
    },

    _transitionEnd: function() {
      if (this.opened) {
        this.updateSize('auto', false);
      }
      this.toggleClass('iron-collapse-closed', !this.opened);
      this.toggleClass('iron-collapse-opened', this.opened);
      this.enableTransition(false);
    },

    _calcSize: function() {
      return this.getBoundingClientRect()[this.dimension] + 'px';
    },


  });