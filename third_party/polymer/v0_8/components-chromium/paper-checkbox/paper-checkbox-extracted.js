
  Polymer({
    is: 'paper-checkbox',

    // The custom properties shim is currently an opt-in feature.
    enableCustomStyleProperties: true,

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
       *
       * @attribute checked
       * @type boolean
       * @default false
       */
      checked: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        observer: '_checkedChanged'
      },

      /**
       * If true, the user cannot interact with this element.
       *
       * @attribute disabled
       * @type boolean
       * @default false
       */
      disabled: {
        type: Boolean
      }
    },

    listeners: {
      keydown: '_onKeyDown',
      mousedown: '_onMouseDown'
    },

    ready: function() {
      if (this.$.checkboxLabel.textContent == '') {
        this.$.checkboxLabel.hidden = true;
      } else {
        this.setAttribute('aria-label', this.$.checkboxLabel.textContent);
      }
    },

    _computeCheckboxClass: function(checked) {
      if (checked) {
        return 'checked';
      }
    },

    _computeCheckmarkClass: function(checked) {
      if (!checked) {
        return 'hidden';
      }
    },

    _onKeyDown: function(e) {
      // Enter key.
      if (e.keyCode === 13) {
        this._onMouseDown();
        e.preventDefault();
      }
    },

    _onMouseDown: function() {
      if (this.disabled) {
        return;
      }

      var old = this.checked;
      this.checked = !this.checked;

      if (this.checked !== old) {
        this.fire('iron-change');
      }
    },

    _checkedChanged: function() {
      this.setAttribute('aria-checked', this.checked ? 'true' : 'false');
      this.fire('iron-change');
    }
  })
