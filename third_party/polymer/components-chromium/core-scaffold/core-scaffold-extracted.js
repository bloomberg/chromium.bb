

  Polymer('core-scaffold', {
    
    /**
     * Fired when the main content has been scrolled.  `event.detail.target` returns
     * the scrollable element which you can use to access scroll info such as
     * `scrollTop`.
     *
     *     <core-scaffold on-scroll="{{scrollHandler}}">
     *       ...
     *     </core-scaffold>
     *
     *
     *     scrollHandler: function(event) {
     *       var scroller = event.detail.target;
     *       console.log(scroller.scrollTop);
     *     }
     *
     * @event scroll
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
       * When the browser window size is smaller than the `responsiveWidth`, 
       * `core-drawer-panel` changes to a narrow layout. In narrow layout, 
       * the drawer will be stacked on top of the main panel.
       *
       * @attribute responsiveWidth
       * @type string
       * @default '600px'
       */    
      responsiveWidth: '600px',
      
      /**
       * If true, position the drawer to the right. Also place menu icon to
       * the right of the content instead of left.
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
      disableSwipe: false,
  
      /**
       * Used to control the header and scrolling behaviour of `core-header-panel`
       *
       * @attribute mode
       * @type string
       * @default 'seamed'
       */     
      mode: {value: 'seamed', reflect: true}
    },
    
    ready: function() {
      this._scrollHandler = this.scroll.bind(this);
      this.$.headerPanel.addEventListener('scroll', this._scrollHandler);
    },
    
    detached: function() {
      this.$.headerPanel.removeEventListener('scroll', this._scrollHandler);
    },

    /**
     * Toggle the drawer panel
     * @method togglePanel
     */    
    togglePanel: function() {
      this.$.drawerPanel.togglePanel();
    },

    /**
     * Open the drawer panel
     * @method openDrawer
     */      
    openDrawer: function() {
      this.$.drawerPanel.openDrawer();
    },

    /**
     * Close the drawer panel
     * @method closeDrawer
     */     
    closeDrawer: function() {
      this.$.drawerPanel.closeDrawer();
    },
    
    /**
     * Returns the scrollable element on the main area.
     *
     * @property scroller
     * @type Object
     */
    get scroller() {
      return this.$.headerPanel.scroller;
    },
    
    scroll: function(e) {
      this.fire('scroll', {target: e.detail.target}, this, false);
    }
    
  });

