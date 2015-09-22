
    Polymer({
      is: 'paper-checkbox',

      behaviors: [
        Polymer.PaperInkyFocusBehavior,
        Polymer.IronCheckedElementBehavior
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
        ariaActiveAttribute: {
          type: String,
          value: 'aria-checked'
        }
      },

      attached: function() {
        this._isReady = true;

        // Don't stomp over a user-set aria-label.
        if (!this.getAttribute('aria-label')) {
          this.updateAriaLabel();
        }
      },

      /**
       * Update the checkbox aria-label. This is a temporary workaround not
       * being able to observe changes in <content>
       * (see: https://github.com/Polymer/polymer/issues/1773)
       *
       * Call this if you manually change the contents of the checkbox
       * and want the aria-label to match the new contents.
       */
      updateAriaLabel: function() {
        this.setAttribute('aria-label', Polymer.dom(this).textContent.trim());
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

      _computeCheckboxClass: function(checked, invalid) {
        var className = '';
        if (checked) {
          className += 'checked ';
        }
        if (invalid) {
          className += 'invalid';
        }
        return className;
      },

      _computeCheckmarkClass: function(checked) {
        return checked ? '' : 'hidden';
      }
    });
  