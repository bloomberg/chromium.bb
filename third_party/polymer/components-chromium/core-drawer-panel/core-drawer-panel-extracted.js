

  Polymer('core-drawer-panel', {
    /**
     * Fired when the narrow layout changes.
     * 
     * @event core-responsive-change
     * @param {Object} detail
     * @param {boolean} detail.narrow true if the panel is in narrow layout.
     */

    publish: {
      
      /**
       * Width of the drawer panel.
       *
       * @attribute drawerWidth
       * @type string
       * @default '256px'
       */
      drawerWidth: '256px',
      
      /**
       * Max-width when the panel changes to narrow layout.
       *
       * @attribute responsiveWidth
       * @type string
       * @default '640px'
       */
      responsiveWidth: '640px',
      
      /**
       * The panel that is being selected. `drawer` for the drawer panel and
       * `main` for the main panel.
       *
       * @attribute selected
       * @type string
       * @default null
       */
      selected: {value: null, reflect: true},
      
      /**
       * The panel to be selected when `core-drawer-panel` changes to narrow 
       * layout.
       *
       * @attribute defaultSelected
       * @type string
       * @default 'main'
       */
      defaultSelected: 'main',
    
      /**
       * Returns true if the panel is in narrow layout.  This is useful if you
       * need to show/hide elements based on the layout.
       *
       * @attribute narrow
       * @type boolean
       * @default false
       */
      narrow: {value: false, reflect: true},
      
      /**
       * If true, position the drawer to the right.
       *
       * @attribute rightDrawer
       * @type boolean
       * @default false
       */
      rightDrawer: false,
      
      /**
       * If true, swipe to open/close the drawer is disabled.
       *
       * @attribute disableSwipe
       * @type boolean
       * @default false
       */
      disableSwipe: false
    },
    
    eventDelegates: {
      trackstart: 'trackStart',
      trackx: 'trackx',
      trackend: 'trackEnd'
    },
    
    transition: false,

    edgeSwipeSensitivity : 15,
    
    dragging : false,
    
    domReady: function() {
      // to avoid transition at the beginning e.g. page loads
      // NOTE: domReady is already raf delayed and delaying another frame
      // ensures a layout has occurred.
      this.async(function() {
        this.transition = true;
      });
    },

    /**
     * Toggles the panel open and closed.
     * 
     * @method togglePanel
     */
    togglePanel: function() {
      this.selected = this.selected === 'main' ? 'drawer' : 'main';
    },
    
    /**
     * Opens the drawer.
     * 
     * @method openDrawer
     */
    openDrawer: function() {
      this.selected = 'drawer';
    },
    
    /**
     * Closes the drawer.
     * 
     * @method closeDrawer
     */
    closeDrawer: function() {
      this.selected = 'main';
    },

    queryMatchesChanged: function() {
      if (this.queryMatches) {
        this.selected = this.defaultSelected;
      }
      this.narrow = this.queryMatches;
      this.setAttribute('touch-action', 
          this.narrow && !this.disableSwipe ? 'pan-y' : '');
      this.fire('core-responsive-change', {narrow: this.narrow});
    },
    
    // swipe support for the drawer, inspired by
    // https://github.com/Polymer/core-drawer-panel/pull/6
    trackStart : function(e) {
      if (this.narrow && !this.disableSwipe) {
        this.dragging = true;

        if (this.selected === 'main') {
          this.dragging = this.rightDrawer ?
              e.pageX >= this.offsetWidth - this.edgeSwipeSensitivity :
              e.pageX <= this.edgeSwipeSensitivity;
        }

        if (this.dragging) {
          this.width = this.$.drawer.offsetWidth;
          this.transition = false;
          e.preventTap();
        }
      }
    },

    trackx : function(e) {
      if (this.dragging) {
        var x;
        if (this.rightDrawer) {
          x = Math.max(0, (this.selected === 'main') ? this.width + e.dx : e.dx);
        } else {
          x = Math.min(0, (this.selected === 'main') ? e.dx - this.width : e.dx);
        }
        this.moveDrawer(x);
      }
    },

    trackEnd : function(e) {
      if (this.dragging) {
        this.dragging = false;
        this.transition = true;
        this.moveDrawer(null);

        if (this.rightDrawer) {
          this.selected = e.xDirection > 0 ? 'main' : 'drawer';
        } else {
          this.selected = e.xDirection > 0 ? 'drawer' : 'main';
        }
      }
    },
    
    moveDrawer: function(translateX) {
      var s = this.$.drawer.style;
      s.webkitTransform = s.transform = 
          translateX === null ? '' : 'translate3d(' + translateX + 'px, 0, 0)';
    }

  });

