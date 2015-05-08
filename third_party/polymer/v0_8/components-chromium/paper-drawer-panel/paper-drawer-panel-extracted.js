

  (function() {

    'use strict';

    function classNames(obj) {
      var classNames = [];
      for (var key in obj) {
        if (obj.hasOwnProperty(key) && obj[key]) {
          classNames.push(key);
        }
      }

      return classNames.join(' ');
    }

    Polymer({

      is: 'paper-drawer-panel',

      /**
       * Fired when the narrow layout changes.
       *
       * @event paper-responsive-change {{narrow: boolean}} detail -
       *     narrow: true if the panel is in narrow layout.
       */

      /**
       * Fired when the selected panel changes.
       *
       * Listening for this event is an alternative to observing changes in the `selected` attribute.
       * This event is fired both when a panel is selected and deselected.
       * The `isSelected` detail property contains the selection state.
       *
       * @event paper-select {{isSelected: boolean, item: Object}} detail -
       *     isSelected: True for selection and false for deselection.
       *     item: The panel that the event refers to.
       */

      properties: {

        /**
         * The panel to be selected when `paper-drawer-panel` changes to narrow
         * layout.
         *
         * @attribute defaultSelected
         * @type string
         * @default 'main'
         */
        defaultSelected: {
          type: String,
          value: 'main'
        },

        /**
         * If true, swipe from the edge is disable.
         *
         * @attribute disableEdgeSwipe
         * @type boolean
         * @default false
         */
        disableEdgeSwipe: Boolean,

        /**
         * If true, swipe to open/close the drawer is disabled.
         *
         * @attribute disableSwipe
         * @type boolean
         * @default false
         */
        disableSwipe: Boolean,

        // Whether the user is dragging the drawer interactively.
        dragging: {
          type: Boolean,
          value: false
        },

        /**
         * Width of the drawer panel.
         *
         * @attribute drawerWidth
         * @type string
         * @default '256px'
         */
        drawerWidth: {
          type: String,
          value: '256px'
        },

        // How many pixels on the side of the screen are sensitive to edge
        // swipes and peek.
        edgeSwipeSensitivity: {
          type: Number,
          value: 30
        },

        /**
         * If true, ignore `responsiveWidth` setting and force the narrow layout.
         *
         * @attribute forceNarrow
         * @type boolean
         * @default false
         */
        forceNarrow: {
          observer: 'forceNarrowChanged',
          type: Boolean,
          value: false
        },

        // Whether the browser has support for the transform CSS property.
        hasTransform: {
          type: Boolean,
          value: function() {
            return 'transform' in this.style;
          }
        },

        // Whether the browser has support for the will-change CSS property.
        hasWillChange: {
          type: Boolean,
          value: function() {
            return 'willChange' in this.style;
          }
        },

        /**
         * Returns true if the panel is in narrow layout.  This is useful if you
         * need to show/hide elements based on the layout.
         *
         * @attribute narrow
         * @type boolean
         * @default false
         */
        narrow: {
          reflectToAttribute: true,
          type: Boolean,
          value: false
        },

        // Whether the drawer is peeking out from the edge.
        peeking: {
          type: Boolean,
          value: false
        },

        /**
         * Max-width when the panel changes to narrow layout.
         *
         * @attribute responsiveWidth
         * @type string
         * @default '640px'
         */
        responsiveWidth: {
          type: String,
          value: '640px'
        },

        /**
         * If true, position the drawer to the right.
         *
         * @attribute rightDrawer
         * @type boolean
         * @default false
         */
        rightDrawer: {
          type: Boolean,
          value: false
        },

        /**
         * The panel that is being selected. `drawer` for the drawer panel and
         * `main` for the main panel.
         *
         * @attribute selected
         * @type string
         * @default null
         */
        selected: {
          reflectToAttribute: true,
          type: String,
          value: null
        },

        /**
         * The attribute on elements that should toggle the drawer on tap, also elements will
         * automatically be hidden in wide layout.
         */
        drawerToggleAttribute: {
          type: String,
          value: 'paper-drawer-toggle'
        },

        /**
         * Whether the transition is enabled.
         */
        transition: {
          type: Boolean,
          value: false
        },

        /**
         * Starting X coordinate of a tracking gesture. It is non-null only between trackStart and
         * trackEnd events.
         * @type {?number}
         */
        _startX: {
          type: Number,
          value: null
        }

      },

      listeners: {
        click: 'onClick',
        track: 'onTrack'

        // TODO: Implement tap handlers when taps are supported.
        //
        // down: 'downHandler',
        // up: 'upHandler'
      },

      _computeIronSelectorClass: function(narrow, transition, dragging, rightDrawer) {
        return classNames({
          dragging: dragging,
          'narrow-layout': narrow,
          'right-drawer': rightDrawer,
          transition: transition
        });
      },

      _computeDrawerStyle: function(drawerWidth) {
        return 'width:' + drawerWidth + ';';
      },

      _computeMainStyle: function(narrow, rightDrawer, drawerWidth) {
        var style = '';

        style += 'left:' + ((narrow || rightDrawer) ? '0' : drawerWidth) + ';'

        if (rightDrawer) {
          style += 'right:' + (narrow ? '' : drawerWidth) + ';';
        } else {
          style += 'right:;'
        }

        return style;
      },

      _computeMediaQuery: function(forceNarrow, responsiveWidth) {
        return forceNarrow ? '' : '(max-width: ' + responsiveWidth + ')';
      },

      _computeSwipeOverlayHidden: function(narrow, disableEdgeSwipe) {
        return !narrow || disableEdgeSwipe;
      },

      onTrack: function(event) {
        switch (event.detail.state) {
          case 'end':
            this.trackEnd(event);
            break;
          case 'move':
            this.trackX(event);
            break;
          case 'start':
            this.trackStart(event);
            break;
        }
      },

      ready: function() {
        // Avoid transition at the beginning e.g. page loads and enable
        // transitions only after the element is rendered and ready.
        this.transition = true;
      },

      /**
       * Toggles the panel open and closed.
       *
       * @method togglePanel
       */
      togglePanel: function() {
        if (this.isMainSelected()) {
          this.openDrawer();
        } else {
          this.closeDrawer();
        }
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

      _responsiveChange: function(narrow) {
        this.narrow = narrow;

        if (this.narrow) {
          this.selected = this.defaultSelected;
        }

        this.setAttribute('touch-action', this.swipeAllowed() ? 'pan-y' : '');
        this.fire('paper-responsive-change', {narrow: this.narrow});
      },

      onQueryMatchesChanged: function(e) {
        this._responsiveChange(e.detail.value);
      },

      forceNarrowChanged: function() {
        this._responsiveChange(this.forceNarrow);
      },

      swipeAllowed: function() {
        return this.narrow && !this.disableSwipe;
      },

      isMainSelected: function() {
        return this.selected === 'main';
      },

      startEdgePeek: function() {
        this.width = this.$.drawer.offsetWidth;
        this.moveDrawer(this.translateXForDeltaX(this.rightDrawer ?
            -this.edgeSwipeSensitivity : this.edgeSwipeSensitivity));
        this.peeking = true;
      },

      stopEdgePeek: function() {
        if (this.peeking) {
          this.peeking = false;
          this.moveDrawer(null);
        }
      },

      // TODO: Implement tap handlers when taps are supported.
      //
      // downHandler: function(e) {
      //   if (!this.dragging && this.isMainSelected() && this.isEdgeTouch(e)) {
      //     this.startEdgePeek();
      //   }
      // },
      //
      // upHandler: function(e) {
      //   this.stopEdgePeek();
      // },

      onClick: function(e) {
        var isTargetToggleElement = e.target &&
          this.drawerToggleAttribute &&
          e.target.hasAttribute(this.drawerToggleAttribute);

        if (isTargetToggleElement) {
          this.togglePanel();
        }
      },

      isEdgeTouch: function(event) {
        var x = event.detail.x;

        return !this.disableEdgeSwipe && this.swipeAllowed() &&
          (this.rightDrawer ?
            x >= this.offsetWidth - this.edgeSwipeSensitivity :
            x <= this.edgeSwipeSensitivity);
      },

      trackStart: function(event) {
        if (this.swipeAllowed()) {
          this.dragging = true;
          this._startX = event.detail.x;

          if (this.isMainSelected()) {
            this.dragging = this.peeking || this.isEdgeTouch(event);
          }

          if (this.dragging) {
            this.width = this.$.drawer.offsetWidth;
            this.transition = false;

            // TODO: Re-enable when tap gestures are implemented.
            //
            // e.preventTap();
          }
        }
      },

      translateXForDeltaX: function(deltaX) {
        var isMain = this.isMainSelected();

        if (this.rightDrawer) {
          return Math.max(0, isMain ? this.width + deltaX : deltaX);
        } else {
          return Math.min(0, isMain ? deltaX - this.width : deltaX);
        }
      },

      trackX: function(event) {
        var dx = event.detail.x - this._startX;

        if (this.dragging) {
          if (this.peeking) {
            if (Math.abs(dx) <= this.edgeSwipeSensitivity) {
              // Ignore trackx until we move past the edge peek.
              return;
            }

            this.peeking = false;
          }

          this.moveDrawer(this.translateXForDeltaX(dx));
        }
      },

      trackEnd: function(event) {
        if (this.dragging) {
          var xDirection = (event.detail.x - this._startX) > 0;

          this.dragging = false;
          this._startX = null;
          this.transition = true;
          this.moveDrawer(null);

          if (this.rightDrawer) {
            this[(xDirection > 0) ? 'closeDrawer' : 'openDrawer']();
          } else {
            this[(xDirection > 0) ? 'openDrawer' : 'closeDrawer']();
          }
        }
      },

      transformForTranslateX: function(translateX) {
        if (translateX === null) {
          return '';
        }

        return this.hasWillChange ? 'translateX(' + translateX + 'px)' :
            'translate3d(' + translateX + 'px, 0, 0)';
      },

      moveDrawer: function(translateX) {
        var s = this.$.drawer.style;

        if (this.hasTransform) {
          s.transform = this.transformForTranslateX(translateX);
        } else {
          s.webkitTransform = this.transformForTranslateX(translateX);
        }
      },

      onSelect: function(e) {
        e.preventDefault();
        this.selected = e.detail.selected;
      }

    });

  }());

