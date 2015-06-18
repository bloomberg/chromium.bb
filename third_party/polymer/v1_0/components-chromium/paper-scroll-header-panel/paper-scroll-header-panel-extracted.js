
(function() {

  'use strict';

  Polymer({

    /**
     * Fired when the content has been scrolled.
     *
     * @event content-scroll
     */

    /**
     * Fired when the header is transformed.
     *
     * @event paper-header-transform
     */

    is: 'paper-scroll-header-panel',

    behaviors: [
      Polymer.IronResizableBehavior
    ],

    properties: {

      /**
       * If true, the header's height will condense to `_condensedHeaderHeight`
       * as the user scrolls down from the top of the content area.
       */
      condenses: {
        type: Boolean,
        value: false,
        observer: '_condensesChanged'
      },

      /**
       * If true, no cross-fade transition from one background to another.
       */
      noDissolve: {
        type: Boolean,
        value: false
      },

      /**
       * If true, the header doesn't slide back in when scrolling back up.
       */
      noReveal: {
        type: Boolean,
        value: false
      },

      /**
       * If true, the header is fixed to the top and never moves away.
       */
      fixed: {
        type: Boolean,
        value: false
      },

      /**
       * If true, the condensed header is always shown and does not move away.
       */
      keepCondensedHeader: {
        type: Boolean,
        value: false
      },

      /**
       * The height of the header when it is at its full size.
       *
       * By default, the height will be measured when it is ready.  If the height
       * changes later the user needs to either set this value to reflect the
       * new height or invoke `measureHeaderHeight()`.
       */
      headerHeight: {
        type: Number,
        value: 0
      },

      /**
       * The height of the header when it is condensed.
       *
       * By default, `_condensedHeaderHeight` is 1/3 of `headerHeight` unless
       * this is specified.
       */
      condensedHeaderHeight: {
        type: Number,
        value: 0
      },

      /**
       * By default, the top part of the header stays when the header is being
       * condensed.  Set this to true if you want the top part of the header
       * to be scrolled away.
       */
      scrollAwayTopbar: {
        type: Boolean,
        value: false
      },

      _headerMargin: {
        type: Number
      },

      _prevScrollTop: {
        type: Number
      },

      _y: {
        type: Number
      }

    },

    observers: [
      '_setup(_headerMargin, headerHeight, fixed)',
      '_headerHeightChanged(headerHeight, condensedHeaderHeight)',
      '_condensedHeaderHeightChanged(headerHeight, condensedHeaderHeight)'
    ],

    listeners: {
      'iron-resize': 'measureHeaderHeight'
    },

    ready: function() {
      this.async(this.measureHeaderHeight, 5);
      this._scrollHandler = this._scroll.bind(this);
      this.scroller.addEventListener('scroll', this._scrollHandler);
    },

    detached: function() {
      this.scroller.removeEventListener('scroll', this._scrollHandler);
    },

    /**
     * Returns the header element.
     *
     * @property header
     * @type Object
     */
    get header() {
      return Polymer.dom(this.$.headerContent).getDistributedNodes()[0];
    },

    /**
     * Returns the content element.
     *
     * @property content
     * @type Object
     */
    get content() {
      return Polymer.dom(this.$.mainContent).getDistributedNodes()[0];
    },

    /**
     * Returns the scrollable element.
     *
     * @property scroller
     * @type Object
     */
    get scroller() {
      return this.$.mainContainer;
    },

    /**
     * Invoke this to tell `paper-scroll-header-panel` to re-measure the header's
     * height.
     *
     * @method measureHeaderHeight
     */
    measureHeaderHeight: function() {
      var header = this.header;
      if (header && header.offsetHeight) {
        this.headerHeight = header.offsetHeight;
      }
    },

    _headerHeightChanged: function() {
      if (!this.condensedHeaderHeight) {
        // assume condensedHeaderHeight is 1/3 of the headerHeight
        this.condensedHeaderHeight = this.headerHeight * 1 / 3;
      }
    },

    _condensedHeaderHeightChanged: function() {
      if (this.headerHeight) {
        this._headerMargin = this.headerHeight - this.condensedHeaderHeight;
      }
    },

    _condensesChanged: function() {
      if (this.condenses) {
        this._scroll();
      } else {
        // reset transform/opacity set on the header
        this._condenseHeader(null);
      }
    },

    _setup: function() {
      var s = this.scroller.style;
      s.paddingTop = this.fixed ? '' : this.headerHeight + 'px';

      s.top = this.fixed ? this.headerHeight + 'px' : '';

      if (this.fixed) {
        this._transformHeader(null);
      } else {
        this._scroll();
      }
    },

    _transformHeader: function(y) {
      var s = this.$.headerContainer.style;
      this._translateY(s, -y);

      if (this.condenses) {
        this._condenseHeader(y);
      }

      this.fire('paper-header-transform', {y: y, height: this.headerHeight,
          condensedHeight: this.condensedHeaderHeight});
    },

    _condenseHeader: function(y) {
      var reset = (y === null);

      // adjust top bar in paper-header so the top bar stays at the top
      if (!this.scrollAwayTopbar && this.header.$ && this.header.$.topBar) {
        this._translateY(this.header.$.topBar.style,
            reset ? null : Math.min(y, this._headerMargin));
      }
      // transition header bg
      var hbg = this.$.headerBg.style;
      if (!this.noDissolve) {
        hbg.opacity = reset ? '' : (this._headerMargin - y) / this._headerMargin;
      }
      // adjust header bg so it stays at the center
      this._translateY(hbg, reset ? null : y / 2);
      // transition condensed header bg
      if (!this.noDissolve) {
        var chbg = this.$.condensedHeaderBg.style;
        chbg = this.$.condensedHeaderBg.style;
        chbg.opacity = reset ? '' : y / this._headerMargin;

        // adjust condensed header bg so it stays at the center
        this._translateY(chbg, reset ? null : y / 2);
      }
    },

    _translateY: function(s, y) {
      var t = (y === null) ? '' : 'translate3d(0, ' + y + 'px, 0)';
      setTransform(s, t);
    },

    /** @param {Event=} event */
    _scroll: function(event) {
      if (!this.header) {
        return;
      }

      var sTop = this.scroller.scrollTop;

      this._y = this._y || 0;
      this._prevScrollTop = this._prevScrollTop || 0;

      var y = Math.min(this.keepCondensedHeader ?
          this._headerMargin : this.headerHeight, Math.max(0,
          (this.noReveal ? sTop : this._y + sTop - this._prevScrollTop)));

      if (this.condenses && this._prevScrollTop >= sTop && sTop > this._headerMargin) {
        y = Math.max(y, this._headerMargin);
      }

      if (!event || !this.fixed && y !== this._y) {
        this._transformHeader(y);
      }

      this._prevScrollTop = Math.max(sTop, 0);
      this._y = y;

      if (event) {
        this.fire('content-scroll', {target: this.scroller}, {cancelable: false});
      }
    }

  });

  //determine proper transform mechanizm
  if (document.documentElement.style.transform !== undefined) {
    var setTransform = function(style, string) {
      style.transform = string;
    }
  } else {
    var setTransform = function(style, string) {
      style.webkitTransform = string;
    }
  }

})();

