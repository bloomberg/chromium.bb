(function() {
      'use strict';

      Polymer({
        is: 'iron-dropdown',

        behaviors: [
          Polymer.IronControlState,
          Polymer.IronA11yKeysBehavior,
          Polymer.IronOverlayBehavior,
          Polymer.NeonAnimationRunnerBehavior
        ],

        properties: {
          /**
           * The orientation against which to align the dropdown content
           * horizontally relative to the dropdown trigger.
           * Overridden from `Polymer.IronFitBehavior`.
           */
          horizontalAlign: {
            type: String,
            value: 'left',
            reflectToAttribute: true
          },

          /**
           * The orientation against which to align the dropdown content
           * vertically relative to the dropdown trigger.
           * Overridden from `Polymer.IronFitBehavior`.
           */
          verticalAlign: {
            type: String,
            value: 'top',
            reflectToAttribute: true
          },

          /**
           * An animation config. If provided, this will be used to animate the
           * opening of the dropdown.
           */
          openAnimationConfig: {
            type: Object
          },

          /**
           * An animation config. If provided, this will be used to animate the
           * closing of the dropdown.
           */
          closeAnimationConfig: {
            type: Object
          },

          /**
           * If provided, this will be the element that will be focused when
           * the dropdown opens.
           */
          focusTarget: {
            type: Object
          },

          /**
           * Set to true to disable animations when opening and closing the
           * dropdown.
           */
          noAnimations: {
            type: Boolean,
            value: false
          },

          /**
           * By default, the dropdown will constrain scrolling on the page
           * to itself when opened.
           * Set to true in order to prevent scroll from being constrained
           * to the dropdown when it opens.
           */
          allowOutsideScroll: {
            type: Boolean,
            value: false
          }
        },

        listeners: {
          'neon-animation-finish': '_onNeonAnimationFinish'
        },

        observers: [
          '_updateOverlayPosition(positionTarget, verticalAlign, horizontalAlign, verticalOffset, horizontalOffset)'
        ],

        /**
         * The element that is contained by the dropdown, if any.
         */
        get containedElement() {
          return Polymer.dom(this.$.content).getDistributedNodes()[0];
        },

        /**
         * The element that should be focused when the dropdown opens.
         * @deprecated
         */
        get _focusTarget() {
          return this.focusTarget || this.containedElement;
        },

        /**
         * Called when the value of `opened` changes.
         * Overridden from `IronOverlayBehavior`
         */
        _openedChanged: function() {
          if (this.opened && this.disabled) {
            this.cancel();
          } else {
            this.cancelAnimation();
            this.sizingTarget = this.containedElement || this.sizingTarget;
            this._updateAnimationConfig();
            if (this.opened && !this.allowOutsideScroll) {
              Polymer.IronDropdownScrollManager.pushScrollLock(this);
            } else {
              Polymer.IronDropdownScrollManager.removeScrollLock(this);
            }
            Polymer.IronOverlayBehaviorImpl._openedChanged.apply(this, arguments);
          }
        },

        /**
         * Overridden from `IronOverlayBehavior`.
         */
        _renderOpened: function() {
          if (!this.noAnimations && this.animationConfig && this.animationConfig.open) {
            if (this.withBackdrop) {
              this.backdropElement.open();
            }
            this.$.contentWrapper.classList.add('animating');
            this.playAnimation('open');
          } else {
            Polymer.IronOverlayBehaviorImpl._renderOpened.apply(this, arguments);
          }
        },

        /**
         * Overridden from `IronOverlayBehavior`.
         */
        _renderClosed: function() {
          if (!this.noAnimations && this.animationConfig && this.animationConfig.close) {
            if (this.withBackdrop) {
              this.backdropElement.close();
            }
            this.$.contentWrapper.classList.add('animating');
            this.playAnimation('close');
          } else {
            Polymer.IronOverlayBehaviorImpl._renderClosed.apply(this, arguments);
          }
        },

        /**
         * Called when animation finishes on the dropdown (when opening or
         * closing). Responsible for "completing" the process of opening or
         * closing the dropdown by positioning it or setting its display to
         * none.
         */
        _onNeonAnimationFinish: function() {
          this.$.contentWrapper.classList.remove('animating');
          if (this.opened) {
            Polymer.IronOverlayBehaviorImpl._finishRenderOpened.apply(this);
          } else {
            Polymer.IronOverlayBehaviorImpl._finishRenderClosed.apply(this);
          }
        },

        /**
         * Constructs the final animation config from different properties used
         * to configure specific parts of the opening and closing animations.
         */
        _updateAnimationConfig: function() {
          var animationConfig = {};
          var animations = [];

          if (this.openAnimationConfig) {
            // NOTE(cdata): When making `display:none` elements visible in Safari,
            // the element will paint once in a fully visible state, causing the
            // dropdown to flash before it fades in. We prepend an
            // `opaque-animation` to fix this problem:
            animationConfig.open = [{
              name: 'opaque-animation',
            }].concat(this.openAnimationConfig);
            animations = animations.concat(animationConfig.open);
          }

          if (this.closeAnimationConfig) {
            animationConfig.close = this.closeAnimationConfig;
            animations = animations.concat(animationConfig.close);
          }

          animations.forEach(function(animation) {
            animation.node = this.containedElement;
          }, this);

          this.animationConfig = animationConfig;
        },

        /**
         * Updates the overlay position based on configured horizontal
         * and vertical alignment.
         */
        _updateOverlayPosition: function() {
          if (this.isAttached) {
            // This triggers iron-resize, and iron-overlay-behavior will call refit if needed.
            this.notifyResize();
          }
        },

        /**
         * Useful to call this after the element, the window, or the `fitInfo`
         * element has been resized. Will maintain the scroll position.
         */
        refit: function () {
          if (!this.opened) {
            return
          }
          var containedElement = this.containedElement;
          var scrollTop;
          var scrollLeft;

          if (containedElement) {
            scrollTop = containedElement.scrollTop;
            scrollLeft = containedElement.scrollLeft;
          }
          Polymer.IronFitBehavior.refit.apply(this, arguments);

          if (containedElement) {
            containedElement.scrollTop = scrollTop;
            containedElement.scrollLeft = scrollLeft;
          }
        },

        /**
         * Apply focus to focusTarget or containedElement
         */
        _applyFocus: function () {
          var focusTarget = this.focusTarget || this.containedElement;
          if (focusTarget && this.opened && !this.noAutoFocus) {
            focusTarget.focus();
          } else {
            Polymer.IronOverlayBehaviorImpl._applyFocus.apply(this, arguments);
          }
        }
      });
    })();