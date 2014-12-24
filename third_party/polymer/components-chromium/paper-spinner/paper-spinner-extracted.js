
    Polymer('paper-spinner',{
      eventDelegates: {
        'animationend': 'reset',
        'webkitAnimationEnd': 'reset'
      },
      publish: {
        /**
         * Displays the spinner.
         *
         * @attribute active
         * @type boolean
         * @default false
         */
        active: {value: false, reflect: true},

        /**
         * Alternative text content for accessibility support.
         * If alt is present, it will add an aria-label whose content matches alt when active.
         * If alt is not present, it will default to 'loading' as the alt value.
         * @attribute alt
         * @type string
         * @default 'loading'
         */
        alt: {value: 'loading', reflect: true}
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

      activeChanged: function() {
        if (this.active) {
          this.$.spinnerContainer.classList.remove('cooldown');
          this.$.spinnerContainer.classList.add('active');
          this.removeAttribute('aria-hidden');
        } else {
          this.$.spinnerContainer.classList.add('cooldown');
          this.setAttribute('aria-hidden', 'true');
        }
      },

      altChanged: function() {
        if (this.alt === '') {
          this.setAttribute('aria-hidden', 'true');
        } else {
          this.removeAttribute('aria-hidden');
        }
        this.setAttribute('aria-label', this.alt);
      },

      reset: function() {
        this.$.spinnerContainer.classList.remove('active', 'cooldown');
      }
    });
  