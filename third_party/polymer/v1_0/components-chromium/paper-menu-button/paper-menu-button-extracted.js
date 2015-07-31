
  (function() {
    'use strict';

    var PaperMenuButton = Polymer({

      is: 'paper-menu-button',

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
         * Set to true to disable animations when opening and closing the
         * dropdown.
         */
        noAnimations: {
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
        this.fire('paper-open');
        this.$.dropdown.open();
      },

      /**
       * Hide the dropdown content.
       */
      close: function() {
        this.fire('paper-close');
        this.$.dropdown.close();
      },

      _onIronActivate: function(event) {
        this.close();
      }
    });

    PaperMenuButton.ANIMATION_CUBIC_BEZIER = 'cubic-bezier(.3,.95,.5,1)';
    PaperMenuButton.MAX_ANIMATION_TIME_MS = 400;

    Polymer.PaperMenuButton = PaperMenuButton;
  })();
