

  Polymer.PaperInputBehavior = {

    properties: {

      /**
       * The label for this input.
       */
      label: {
        type: String
      },

      /**
       * The value for this input.
       */
      value: {
        notify: true,
        type: String
      },

      /**
       * Set to true to disable this input.
       */
      disabled: {
        type: Boolean,
        value: false
      },

      /**
       * Set to true to prevent the user from entering invalid input.
       */
      preventInvalidInput: {
        type: Boolean
      },

      /**
       * The type of the input. The supported types are `text`, `number` and `password`.
       */
      type: {
        type: String
      },

      /**
       * A pattern to validate the `input` with.
       */
      pattern: {
        type: String
      },

      /**
       * Set to true to mark the input as required.
       */
      required: {
        type: Boolean,
        value: false
      },

      /**
       * The maximum length of the input value.
       */
      maxlength: {
        type: Number
      },

      /**
       * The error message to display when the input is invalid.
       */
      errorMessage: {
        type: String
      },

      /**
       * Set to true to show a character counter.
       */
      charCounter: {
        type: Boolean,
        value: false
      },

      /**
       * Set to true to disable the floating label.
       */
      noLabelFloat: {
        type: Boolean,
        value: false
      },

      /**
       * Set to true to auto-validate the input value.
       */
      autoValidate: {
        type: Boolean,
        value: false
      }

    },

    /**
     * Returns a reference to the input element.
     */
    get inputElement() {
      return this.$.input;
    }

  };

