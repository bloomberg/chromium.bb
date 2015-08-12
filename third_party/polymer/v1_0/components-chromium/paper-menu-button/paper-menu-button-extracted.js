(function() {
    'use strict';

    var PaperMenuButton = Polymer({
      is: 'paper-menu-button',

      /**
       * Fired when the dropdown opens.
       *
       * @event paper-dropdown-open
       */

      /**
       * Fired when the dropdown closes.
       *
       * @event paper-dropdown-close
       */

      behaviors: [
        Polymer.IronA11yKeysBehavior,
        Polymer.IronControlState
      ],

      properties: {

        /**
         * True if the content is currently displayed.
         */
        opened: {
          type: Boolean,
          value: false,
          notify: true
        },

        /**
         * The orientation against which to align the menu dropdown
         * horizontally relative to the dropdown trigger.
         */
        horizontalAlign: {
          type: String,
          value: 'left',
          reflectToAttribute: true
        },

        /**
         * The orientation against which to align the menu dropdown
         * vertically relative to the dropdown trigger.
         */
        verticalAlign: {
          type: String,
          value: 'top',
          reflectToAttribute: true
        },

        /**
         * A pixel value that will be added to the position calculated for the
         * given `horizontalAlign`. Use a negative value to offset to the
         * left, or a positive value to offset to the right.
         */
        horizontalOffset: {
          type: Number,
          value: 0,
          notify: true
        },

        /**
         * A pixel value that will be added to the position calculated for the
         * given `verticalAlign`. Use a negative value to offset towards the
         * top, or a positive value to offset towards the bottom.
         */
        verticalOffset: {
          type: Number,
          value: 0,
          notify: true
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
         * Set to true to disable automatically closing the dropdown after
         * a selection has been made.
         */
        ignoreActivate: {
          type: Boolean,
          value: false
        },

        /**
         * An animation config. If provided, this will be used to animate the
         * opening of the dropdown.
         */
        openAnimationConfig: {
          type: Object,
          value: function() {
            return [{
              name: 'fade-in-animation',
              timing: {
                delay: 100,
                duration: 200
              }
            }, {
              name: 'paper-menu-grow-width-animation',
              timing: {
                delay: 100,
                duration: 150,
                easing: PaperMenuButton.ANIMATION_CUBIC_BEZIER
              }
            }, {
              name: 'paper-menu-grow-height-animation',
              timing: {
                delay: 100,
                duration: 275,
                easing: PaperMenuButton.ANIMATION_CUBIC_BEZIER
              }
            }];
          }
        },

        /**
         * An animation config. If provided, this will be used to animate the
         * closing of the dropdown.
         */
        closeAnimationConfig: {
          type: Object,
          value: function() {
            return [{
              name: 'fade-out-animation',
              timing: {
                duration: 150
              }
            }, {
              name: 'paper-menu-shrink-width-animation',
              timing: {
                delay: 100,
                duration: 50,
                easing: PaperMenuButton.ANIMATION_CUBIC_BEZIER
              }
            }, {
              name: 'paper-menu-shrink-height-animation',
              timing: {
                duration: 200,
                easing: 'ease-in'
              }
            }];
          }
        }
      },

      hostAttributes: {
        role: 'group',
        'aria-haspopup': 'true'
      },

      listeners: {
        'iron-activate': '_onIronActivate'
      },

      /**
       * Make the dropdown content appear as an overlay positioned relative
       * to the dropdown trigger.
       */
      open: function() {
        if (this.disabled) {
          return;
        }

        this.$.dropdown.open();
      },

      /**
       * Hide the dropdown content.
       */
      close: function() {
        this.$.dropdown.close();
      },

      /**
       * When an `iron-activate` event is received, the dropdown should
       * automatically close on the assumption that a value has been chosen.
       *
       * @param {CustomEvent} event A CustomEvent instance with type
       * set to `"iron-activate"`.
       */
      _onIronActivate: function(event) {
        if (!this.ignoreActivate) {
          this.close();
        }
      },

      /**
       * When the dropdown opens, the `paper-menu-button` fires `paper-open`.
       * When the dropdown closes, the `paper-menu-button` fires `paper-close`.
       *
       * @param {boolean} opened True if the dropdown is opened, otherwise false.
       * @param {boolean} oldOpened The previous value of `opened`.
       */
      _openedChanged: function(opened, oldOpened) {
        if (opened) {
          this.fire('paper-dropdown-open');
        } else if (oldOpened != null) {
          this.fire('paper-dropdown-close');
        }
      },

      /**
       * If the dropdown is open when disabled becomes true, close the
       * dropdown.
       *
       * @param {boolean} disabled True if disabled, otherwise false.
       */
      _disabledChanged: function(disabled) {
        Polymer.IronControlState._disabledChanged.apply(this, arguments);
        if (disabled && this.opened) {
          this.close();
        }
      }
    });

    PaperMenuButton.ANIMATION_CUBIC_BEZIER = 'cubic-bezier(.3,.95,.5,1)';
    PaperMenuButton.MAX_ANIMATION_TIME_MS = 400;

    Polymer.PaperMenuButton = PaperMenuButton;
  })();