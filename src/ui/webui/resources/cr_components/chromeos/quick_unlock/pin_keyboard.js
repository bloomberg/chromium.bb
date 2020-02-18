// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'pin-keyboard' is a keyboard that can be used to enter PINs or more generally
 * numeric values.
 *
 * Properties:
 *    value: The value of the PIN keyboard. Writing to this property will adjust
 *           the PIN keyboard's value.
 *
 * Events:
 *    pin-change: Fired when the PIN value has changed. The PIN is available at
 *                event.detail.pin.
 *    submit: Fired when the PIN is submitted. The PIN is available at
 *            event.detail.pin.
 *
 * Example:
 *    <pin-keyboard on-pin-change="onPinChange" on-submit="onPinSubmit">
 *    </pin-keyboard>
 */

(function() {

/**
 * Once auto backspace starts, the time between individual backspaces.
 * @type {number}
 * @const
 */
const REPEAT_BACKSPACE_DELAY_MS = 150;

/**
 * How long the backspace button must be held down before auto backspace
 * starts.
 * @type {number}
 * @const
 */
const INITIAL_BACKSPACE_DELAY_MS = 500;

/**
 * The key codes of the keys allowed to be used on the pin input, in addition to
 * number keys. Currently we allow backspace(8), tab(9), left(37) and right(39).
 * @type {Array<number>}
 * @const
 */
const PIN_INPUT_ALLOWED_NON_NUMBER_KEY_CODES = [8, 9, 37, 39];

/** @return {boolean} */
function receivedFocusFromKeyboard() {
  return !!document.querySelector('html.focus-outline-visible');
}

Polymer({
  is: 'pin-keyboard',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * Whether or not the keyboard's input element should be numerical
     * or password.
     */
    enablePassword: {
      type: Boolean,
      value: false,
    },

    hasError: Boolean,

    /**
     * The password element the pin keyboard is associated with. If this is not
     * set, then a default input element is shown and used.
     * @type {?HTMLElement}
     */
    passwordElement: Object,

    /**
     * The intervalID used for the backspace button set/clear interval.
     * @private
     */
    repeatBackspaceIntervalId_: {
      type: Number,
      value: 0,
    },

    /**
     * The timeoutID used for the auto backspace.
     * @private
     */
    startAutoBackspaceId_: {
      type: Number,
      value: 0,
    },

    /**
     * The value stored in the keyboard's input element.
     */
    value: {
      type: String,
      notify: true,
      value: '',
      observer: 'onPinValueChange_',
    },

    /**
     * @private
     */
    forceUnderline_: {
      type: Boolean,
      value: false,
    },

    /**
     * Enables pin placeholder.
     */
    enablePlaceholder: {
      type: Boolean,
      value: false,
    },

    /**
     * Enables letters to be displayed on the pin keyboard buttons.
     */
    enableLetters: {
      type: Boolean,
      value: false,
    },

    /**
     * Turns on "incognito mode". (FIXME after https://crbug.com/900351 is
     * fixed).
     */
    isIncognitoUi: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'blur': 'onBlur_',
    'focus': 'onFocus_',
  },

  /**
   * Gets the selection start of the input field.
   * @type {number}
   * @private
   */
  get selectionStart_() {
    return this.passwordElement_().selectionStart;
  },

  /**
   * Gets the selection end of the input field.
   * @type {number}
   * @private
   */
  get selectionEnd_() {
    return this.passwordElement_().selectionEnd;
  },

  /**
   * Sets the selection start of the input field.
   * @param {number} start The new selection start of the input element.
   * @private
   */
  set selectionStart_(start) {
    this.passwordElement_().selectionStart = start;
  },

  /**
   * Sets the selection end of the input field.
   * @param {number} end The new selection end of the input element.
   * @private
   */
  set selectionEnd_(end) {
    this.passwordElement_().selectionEnd = end;
  },

  /**
   * Transfers blur to the input element.
   */
  blur: function() {
    this.passwordElement_().blur();
  },

  /**
   * Transfers focus to the input element. This should not bring up the virtual
   * keyboard, if it is enabled. After focus, moves the caret to the correct
   * location if specified.
   * @param {number=} opt_selectionStart
   * @param {number=} opt_selectionEnd
   */
  focusInput: function(opt_selectionStart, opt_selectionEnd) {
    setTimeout(function() {
      this.passwordElement_().focus();
      this.selectionStart_ = opt_selectionStart || 0;
      this.selectionEnd_ = opt_selectionEnd || 0;
    }.bind(this), 0);
  },

  /**
   * Transfers focus to the input. Called when a non button element on the
   * PIN button area is clicked to prevent focus from leaving the input.
   */
  onRootTap_: function() {
    // Focus the input and place the selected region to its exact previous
    // location, as this function will not be called by something that will also
    // modify the input value.
    this.focusInput(this.selectionStart_, this.selectionEnd_);
  },

  /** @private */
  onFocus_: function() {
    this.forceUnderline_ = true;
  },

  /** @private */
  onBlur_: function() {
    this.forceUnderline_ = false;
  },

  /**
   * Called when a keypad number has been tapped.
   * @param {Event} event The event object.
   * @private
   */
  onNumberTap_: function(event) {
    const numberValue = event.target.getAttribute('value');

    // Add the number where the caret is, then update the selection range of the
    // input element.
    const selectionStart = this.selectionStart_;
    this.value = this.value.substring(0, this.selectionStart_) + numberValue +
        this.value.substring(this.selectionEnd_);

    // If a number button is clicked, we do not want to switch focus to the
    // button, therefore we transfer focus back to the input, but if a number
    // button is tabbed into, it should keep focus, so users can use tab and
    // spacebar/return to enter their PIN.
    if (!receivedFocusFromKeyboard()) {
      this.focusInput(selectionStart + 1, selectionStart + 1);
    }
    event.stopImmediatePropagation();
  },

  /** Fires a submit event with the current PIN value. */
  firePinSubmitEvent_: function() {
    this.fire('submit', {pin: this.value});
  },

  /**
   * Fires an update event with the current PIN value. The event will only be
   * fired if the PIN value has actually changed.
   * @param {string} value
   * @param {string} previous
   */
  onPinValueChange_: function(value, previous) {
    if (this.passwordElement) {
      this.passwordElement.value = value;
    }
    this.fire('pin-change', {pin: value});
  },

  /**
   * Called when the user wants to erase the last character of the entered
   * PIN value.
   * @private
   */
  onPinClear_: function() {
    // If the input is shown, clear the text based on the caret location or
    // selected region of the input element. If it is just a caret, remove the
    // character in front of the caret.
    let selectionStart = this.selectionStart_;
    const selectionEnd = this.selectionEnd_;
    if (selectionStart == selectionEnd && selectionStart) {
      selectionStart--;
    }

    this.value = this.value.substring(0, selectionStart) +
        this.value.substring(selectionEnd);

    // Move the caret or selected region to the correct new place.
    this.selectionStart_ = selectionStart;
    this.selectionEnd_ = selectionStart;
  },

  /**
   * Called when user taps the backspace the button. Only does something when
   * the tap comes from the keyboard. onBackspacePointerDown_ and
   * onBackspacePointerUp_ will handle the events if they come from mouse or
   * touch. Note: This does not support repeatedly backspacing by holding down
   * the space or enter key like touch or mouse does.
   * @param {Event} event The event object.
   * @private
   */
  onBackspaceTap_: function(event) {
    if (!receivedFocusFromKeyboard()) {
      return;
    }

    this.onPinClear_();
    this.clearAndReset_();
    event.stopImmediatePropagation();
  },

  /**
   * Called when the user presses or touches the backspace button. Starts a
   * timer which starts an interval to repeatedly backspace the pin value until
   * the interval is cleared.
   * @param {Event} event The event object.
   * @private
   */
  onBackspacePointerDown_: function(event) {
    this.startAutoBackspaceId_ = setTimeout(function() {
      this.repeatBackspaceIntervalId_ =
          setInterval(this.onPinClear_.bind(this), REPEAT_BACKSPACE_DELAY_MS);
    }.bind(this), INITIAL_BACKSPACE_DELAY_MS);

    if (!receivedFocusFromKeyboard()) {
      this.focusInput(this.selectionStart_, this.selectionEnd_);
    }
    event.stopImmediatePropagation();
  },

  /**
   * Helper function which clears the timer / interval ids and resets them.
   * @private
   */
  clearAndReset_: function() {
    clearInterval(this.repeatBackspaceIntervalId_);
    this.repeatBackspaceIntervalId_ = 0;
    clearTimeout(this.startAutoBackspaceId_);
    this.startAutoBackspaceId_ = 0;
  },

  /**
   * Called when the user unpresses or untouches the backspace button. Stops the
   * interval callback and fires a backspace event if there is no interval
   * running.
   * @param {Event} event The event object.
   * @private
   */
  onBackspacePointerUp_: function(event) {
    // If an interval has started, do not fire event on pointer up.
    if (!this.repeatBackspaceIntervalId_) {
      this.onPinClear_();
    }
    this.clearAndReset_();

    // Since on-down gives the input element focus, the input element will
    // already have focus when on-up is called. This will actually bring up the
    // virtual keyboard, even if focusInput() is wrapped in a setTimeout. Blur
    // the input element first to workaround this.
    this.blur();
    if (!receivedFocusFromKeyboard()) {
      this.focusInput(this.selectionStart_, this.selectionEnd_);
    }
    event.stopImmediatePropagation();
  },

  /**
   * Helper function to check whether a given |event| should be processed by
   * the numeric only input.
   * @param {Event} event The event object.
   * @private
   */
  isValidEventForInput_: function(event) {
    // Valid if the key is a number, and shift is not pressed.
    if ((event.keyCode >= 48 && event.keyCode <= 57) && !event.shiftKey) {
      return true;
    }

    // Valid if the key is one of the selected special keys defined in
    // |PIN_INPUT_ALLOWED_NON_NUMBER_KEY_CODES|.
    if (PIN_INPUT_ALLOWED_NON_NUMBER_KEY_CODES.indexOf(event.keyCode) > -1) {
      return true;
    }

    // Valid if the key is CTRL+A to allow users to quickly select the entire
    // PIN.
    if (event.keyCode == 65 && event.ctrlKey) {
      return true;
    }

    // The rest of the keys are invalid.
    return false;
  },

  /**
   * Called when a key event is pressed while the input element has focus.
   * @param {Event} event The event object.
   * @private
   */
  onInputKeyDown_: function(event) {
    // Up/down pressed, swallow the event to prevent the input value from
    // being incremented or decremented.
    if (event.keyCode == 38 || event.keyCode == 40) {
      event.preventDefault();
      return;
    }

    // Enter pressed.
    if (event.keyCode == 13) {
      this.firePinSubmitEvent_();
      event.preventDefault();
      return;
    }

    // Do not pass events that are not numbers or special keys we care about. We
    // use this instead of input type number because there are several issues
    // with input type number, such as no selectionStart/selectionEnd and
    // entered non numbers causes the caret to jump to the left.
    if (!this.isValidEventForInput_(event)) {
      event.preventDefault();
      return;
    }
  },

  /**
   * Disables the backspace button if nothing is entered.
   * @private
   */
  hasInput_: function() {
    return this.value.length > 0;
  },

  /**
   * Computes the value of the pin input placeholder.
   * @param {boolean} enablePassword
   * @param {boolean} enablePlaceholder
   * @private
   */
  getInputPlaceholder_: function(enablePassword, enablePlaceholder) {
    if (!enablePlaceholder) {
      return '';
    }

    return enablePassword ? this.i18n('pinKeyboardPlaceholderPinPassword') :
                            this.i18n('pinKeyboardPlaceholderPin');
  },

  /**
   * Computes the direction of the pin input.
   * @param {string} password
   * @private
   */
  isInputRtl_: function(password) {
    // +password will convert a string to a number or to NaN if that's not
    // possible. Number.isInteger will verify the value is not a NaN and that it
    // does not contain decimals.
    // This heuristic will fail for inputs like '1.0'.
    //
    // Since we still support users entering their passwords through the PIN
    // keyboard, we swap the input box to rtl when we think it is a password
    // (just numbers), if the document direction is rtl.
    return (document.dir == 'rtl') && !Number.isInteger(+password);
  },

  /**
   * Catch and stop propagation of context menu events since we the backspace
   * button can be held down on touch.
   * @param {!Event} e
   * @private
   */
  onContextMenu_: function(e) {
    e.preventDefault();
    e.stopPropagation();
  },

  /**
   * @return {!HTMLElement} Returns the native input element of |pinInput|.
   * @private
   */
  passwordElement_: function() {
    // |passwordElement| is null by default. It can be set to override the
    // input field that will be populated with the keypad.
    return this.passwordElement ||
        (/** @type {CrInputElement} */ (this.$.pinInput)).inputElement;
  },
});
})();
