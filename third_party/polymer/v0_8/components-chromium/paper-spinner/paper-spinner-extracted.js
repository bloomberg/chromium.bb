

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

        is: 'paper-spinner',

        listeners: {
          'animationend': 'reset',
          'webkitAnimationEnd': 'reset'
        },

        properties: {

          /**
           * Displays the spinner.
           *
           * @attribute active
           * @type boolean
           * @default false
           */
          active: {
            observer: '_activeChanged',
            type: Boolean,
            value: false
          },

          /**
           * Alternative text content for accessibility support.
           * If alt is present, it will add an aria-label whose content matches alt when active.
           * If alt is not present, it will default to 'loading' as the alt value.
           *
           * @attribute alt
           * @type string
           * @default 'loading'
           */
          alt: {
            observer: '_altChanged',
            type: String,
            value: 'loading'
          },

          /**
           * True when the spinner is going from active to inactive. This is represented by a fade
           * to 0% opacity to the user.
           */
          _coolingDown: {
            type: Boolean,
            value: false
          },

          _spinnerContainerClassName: {
            type: String,
            computed: '_computeSpinnerContainerClassName(active, _coolingDown)'
          }

        },

        _computeSpinnerContainerClassName: function(active, _coolingDown) {
          return classNames({
            active: active || _coolingDown,
            cooldown: _coolingDown
          });
        },

        ready: function() {
          // Allow user-provided `aria-label` take preference to any other text alternative.
          if (this.hasAttribute('aria-label')) {
            this.alt = this.getAttribute('aria-label');
          } else {
            this.setAttribute('aria-label', this.alt);
          }

          if (!this.active) {
            this.setAttribute('aria-hidden', 'true');
          }
        },

        _activeChanged: function() {
          if (this.active) {
            this.removeAttribute('aria-hidden');
          } else {
            this._coolingDown = true;
            this.setAttribute('aria-hidden', 'true');
          }
        },

        _altChanged: function() {
          if (this.alt === '') {
            this.setAttribute('aria-hidden', 'true');
          } else {
            this.removeAttribute('aria-hidden');
          }

          this.setAttribute('aria-label', this.alt);
        },

        reset: function() {
          this.active = false;
          this._coolingDown = false;
        }

      });

    }());

  