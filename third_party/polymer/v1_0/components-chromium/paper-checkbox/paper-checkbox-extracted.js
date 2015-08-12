Polymer({
      is: 'paper-checkbox',

      behaviors: [
        Polymer.PaperInkyFocusBehavior
      ],

      hostAttributes: {
        role: 'checkbox',
        'aria-checked': false,
        tabindex: 0
      },

      properties: {
        /**
         * Fired when the checked state changes due to user interaction.
         *
         * @event change
         */

        /**
         * Fired when the checked state changes.
         *
         * @event iron-change
         */

        /**
         * Gets or sets the state, `true` is checked and `false` is unchecked.
         */
        checked: {
          type: Boolean,
          value: false,
          reflectToAttribute: true,
          notify: true,
          observer: '_checkedChanged'
        },

        /**
         * If true, the button toggles the active state with each tap or press
         * of the spacebar.
         */
        toggles: {
          type: Boolean,
          value: true,
          reflectToAttribute: true
        }
      },

      attached: function() {
        var trimmedText = Polymer.dom(this).textContent.trim();
        if (trimmedText === '') {
          this.$.checkboxLabel.hidden = true;
        }
        // Don't stomp over a user-set aria-label.
        if (trimmedText !== '' && !this.getAttribute('aria-label')) {
          this.setAttribute('aria-label', trimmedText);
        }
        this._isReady = true;
      },

      // button-behavior hook
      _buttonStateChanged: function() {
        if (this.disabled) {
          return;
        }
        if (this._isReady) {
          this.checked = this.active;
        }
      },

      _checkedChanged: function(checked) {
        this.setAttribute('aria-checked', this.checked ? 'true' : 'false');
        this.active = this.checked;
        this.fire('iron-change');
      },

      _computeCheckboxClass: function(checked) {
        if (checked) {
          return 'checked';
        }
        return '';
      },

      _computeCheckmarkClass: function(checked) {
        if (!checked) {
          return 'hidden';
        }
        return '';
      }
    });
