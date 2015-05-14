
  Polymer({
    is: 'paper-checkbox',

    // The custom properties shim is currently an opt-in feature.
    enableCustomStyleProperties: true,

    behaviors: [
      Polymer.PaperRadioButtonBehavior
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
       *
       * @attribute checked
       * @type boolean
       * @default false
       */
      checked: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        notify: true,
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

    ready: function() {
      this.toggles = true;

      if (this.$.checkboxLabel.textContent == '') {
        this.$.checkboxLabel.hidden = true;
      } else {
        this.setAttribute('aria-label', this.$.checkboxLabel.textContent);
      }
    },

    // button-behavior hook
    _buttonStateChanged: function() {
      this.checked = this.active;
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
    },

    _computeCheckmarkClass: function(checked) {
      if (!checked) {
        return 'hidden';
      }
    }
  })
