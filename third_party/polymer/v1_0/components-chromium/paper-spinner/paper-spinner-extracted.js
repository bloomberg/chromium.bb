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
          type: Boolean,
          value: false,
          reflectToAttribute: true,
          observer: '_activeChanged'
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
          type: String,
          value: 'loading',
          observer: '_altChanged'
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

      _computeSpinnerContainerClassName: function(active, coolingDown) {
        return [
          active || coolingDown ? 'active' : '',
          coolingDown ? 'cooldown' : ''
        ].join(' ');
      },

      _activeChanged: function(active, old) {
        this._setAriaHidden(!active);
        this._coolingDown = !active && old;
      },

      _altChanged: function(alt) {
        // user-provided `aria-label` takes precedence over prototype default
        if (alt === this.getPropertyInfo('alt').value) {
          this.alt = this.getAttribute('aria-label') || alt;
        } else {
          this._setAriaHidden(alt==='');
          this.setAttribute('aria-label', alt);
        }
      },

      _setAriaHidden: function(hidden) {
        var attr = 'aria-hidden';
        if (hidden) {
          this.setAttribute(attr, 'true');
        } else {
          this.removeAttribute(attr);
        }
      },

      reset: function() {
        this.active = false;
        this._coolingDown = false;
      }
    });