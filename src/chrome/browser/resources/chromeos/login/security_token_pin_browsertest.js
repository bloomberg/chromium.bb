// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the <security-token-pin> Polymer element.
 */

GEN_INCLUDE([
  '//chrome/test/data/webui/polymer_browser_test_base.js',
]);

GEN('#include "content/public/test/browser_test.h"');

var PolymerSecurityTokenPinTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://oobe/login';
  }
};

TEST_F('PolymerSecurityTokenPinTest', 'All', function() {
  const DEFAULT_PARAMETERS = {
    codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
    enableUserInput: true,
    errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.NONE,
    attemptsLeft: -1
  };

  let securityTokenPin;
  let pinKeyboardContainer;
  let pinKeyboard;
  let progressElement;
  let pinInput;
  let inputField;
  let errorContainer;
  let errorElement;
  let submitElement;
  let backElement;

  setup(() => {
    securityTokenPin = document.createElement('security-token-pin');
    document.body.appendChild(securityTokenPin);
    securityTokenPin.onBeforeShow();
    securityTokenPin.parameters = DEFAULT_PARAMETERS;

    pinKeyboardContainer = securityTokenPin.$$('#pinKeyboardContainer');
    assert(pinKeyboardContainer);
    pinKeyboard = securityTokenPin.$$('#pinKeyboard');
    assert(pinKeyboard);
    progressElement = securityTokenPin.$$('#progress');
    assert(progressElement);
    pinInput = pinKeyboard.$$('#pinInput');
    assert(pinInput);
    inputField = pinInput.$$('input');
    assert(inputField);
    errorContainer = securityTokenPin.$$('#errorContainer');
    assert(errorContainer);
    errorElement = securityTokenPin.$$('#error');
    assert(errorElement);
    submitElement = securityTokenPin.$$('#submit');
    assert(submitElement);
    backElement = securityTokenPin.$$('#back');
    assert(backElement);
  });

  teardown(() => {
    securityTokenPin.remove();
  });

  // Test that the 'completed' event is fired when the user submits the input.
  test('completion events in basic flow', () => {
    const FIRST_PIN = '0123';
    const SECOND_PIN = '987';

    let completedEventDetail = null;
    securityTokenPin.addEventListener('completed', (event) => {
      expectNotEquals(event.detail, null);
      expectEquals(completedEventDetail, null);
      completedEventDetail = event.detail;
    });
    securityTokenPin.addEventListener('cancel', () => {
      expectNotReached();
    });

    // The user enters some value. No 'completed' event is triggered so far.
    pinInput.value = FIRST_PIN;
    expectEquals(completedEventDetail, null);

    // The user submits the PIN. The 'completed' event has been triggered.
    submitElement.click();
    expectEquals(completedEventDetail, FIRST_PIN);
    completedEventDetail = null;

    // The response arrives, requesting to prompt for the PIN again.
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PIN,
      attemptsLeft: -1
    };

    // The user enters some value. No new 'completed' event is triggered so far.
    pinInput.value = SECOND_PIN;
    expectEquals(completedEventDetail, null);

    // The user submits the new PIN. The 'completed' event has been triggered.
    submitElement.click();
    expectEquals(completedEventDetail, SECOND_PIN);
  });

  // Test that the 'cancel' event is fired when the user aborts the dialog.
  test('completion events in cancellation flow', () => {
    let cancelEventCount = 0;
    securityTokenPin.addEventListener('cancel', () => {
      ++cancelEventCount;
    });
    securityTokenPin.addEventListener('completed', () => {
      expectNotReached();
    });

    // The user clicks the 'back' button. The cancel event is triggered.
    backElement.click();
    expectEquals(cancelEventCount, 1);
  });

  // Test that the submit button is only enabled when the input is non-empty.
  test('submit button availability', () => {
    // Initially, the submit button is disabled.
    expectTrue(submitElement.disabled);

    // The user enters a single digit. The submit button is enabled.
    pinInput.value = '1';
    expectFalse(submitElement.disabled);

    // The user clears the input. The submit button is disabled.
    pinInput.value = '';
    expectTrue(submitElement.disabled);
  });

  // Test that the input field is disabled when the final error is displayed and
  // no further user input is expected.
  test('input availability', () => {
    // Initially, the input is enabled.
    expectFalse(inputField.disabled);

    // The user enters and submits a PIN. The response arrives, requesting the
    // PIN again. The input is still enabled.
    pinInput.value = '123';
    submitElement.click();
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PIN,
      attemptsLeft: -1
    };
    expectFalse(inputField.disabled);

    // The user enters and submits a PIN again. The response arrives, with a
    // final error. The input is disabled.
    pinInput.value = '456';
    submitElement.click();
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: false,
      errorLabel:
          OobeTypes.SecurityTokenPinDialogErrorType.MAX_ATTEMPTS_EXCEEDED,
      attemptsLeft: 0
    };
    expectTrue(inputField.disabled);
  });

  // Test that the input field gets cleared when the user is prompted again.
  test('input cleared on new request', () => {
    const PIN = '123';
    pinInput.value = PIN;
    expectEquals(inputField.value, PIN);

    // The user submits the PIN. The response arrives, requesting the PIN again.
    // The input gets cleared.
    submitElement.click();
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PIN,
      attemptsLeft: -1
    };
    expectEquals(pinInput.value, '');
    expectEquals(inputField.value, '');
  });

  // Test that the input field gets cleared when the request fails with the
  // final error.
  test('input cleared on final error', () => {
    // The user enters and submits a PIN. The response arrives, requesting the
    // PIN again. The input is cleared.
    const PIN = '123';
    pinInput.value = PIN;
    expectEquals(inputField.value, PIN);

    // The user submits the PIN. The response arrives, reporting a final error
    // and that the user input isn't requested anymore. The input gets cleared.
    submitElement.click();
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: false,
      errorLabel:
          OobeTypes.SecurityTokenPinDialogErrorType.MAX_ATTEMPTS_EXCEEDED,
      attemptsLeft: 0
    };
    expectEquals(pinInput.value, '');
    expectEquals(inputField.value, '');
  });

  // Test that the PIN can be entered via the on-screen PIN keypad.
  test('PIN input via keypad', () => {
    const PIN = '13097';

    let completedEventDetail = null;
    securityTokenPin.addEventListener('completed', (event) => {
      completedEventDetail = event.detail;
    });

    // The user clicks the buttons of the on-screen keypad. The input field is
    // updated accordingly.
    for (const character of PIN)
      pinKeyboard.$$('#digitButton' + character).click();
    expectEquals(pinInput.value, PIN);
    expectEquals(inputField.value, PIN);

    // The user submits the PIN. The completed event is fired, containing the
    // PIN.
    submitElement.click();
    expectEquals(completedEventDetail, PIN);
  });

  // Test that the error is displayed only when it's set in the request.
  test('error visibility', () => {
    function getErrorContainerVisibility() {
      return getComputedStyle(errorContainer).getPropertyValue('visibility');
    }

    // Initially, no error is shown.
    expectEquals(getErrorContainerVisibility(), 'hidden');
    expectFalse(pinInput.hasAttribute('invalid'));

    // The user submits some PIN, and the error response arrives. The error gets
    // displayed.
    pinInput.value = '123';
    submitElement.click();
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PIN,
      attemptsLeft: -1
    };
    expectEquals(getErrorContainerVisibility(), 'visible');
    expectTrue(pinInput.hasAttribute('invalid'));

    // The user modifies the input field. No error is shown.
    pinInput.value = '4';
    expectEquals(getErrorContainerVisibility(), 'hidden');
    expectFalse(pinInput.hasAttribute('invalid'));
  });

  // Test the text of the label for the |INVALID_PIN| error.
  test('label text: invalid PIN', () => {
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PIN,
      attemptsLeft: -1
    };
    expectEquals(
        errorElement.textContent,
        loadTimeData.getString('securityTokenPinDialogUnknownInvalidPin'));
  });

  // Test the text of the label for the |INVALID_PUK| error.
  test('label text: invalid PUK', () => {
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PUK,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PUK,
      attemptsLeft: -1
    };
    expectEquals(
        errorElement.textContent,
        loadTimeData.getString('securityTokenPinDialogUnknownInvalidPuk'));
  });

  // Test the text of the label for the |MAX_ATTEMPTS_EXCEEDED| error.
  test('label text: max attempts exceeded', () => {
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: false,
      errorLabel:
          OobeTypes.SecurityTokenPinDialogErrorType.MAX_ATTEMPTS_EXCEEDED,
      attemptsLeft: 0
    };
    expectEquals(
        errorElement.textContent,
        loadTimeData.getString(
            'securityTokenPinDialogUnknownMaxAttemptsExceeded'));
  });

  // Test the text of the label for the |UNKNOWN| error.
  test('label text: unknown error', () => {
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.UNKNOWN,
      attemptsLeft: -1
    };
    expectEquals(
        errorElement.textContent,
        loadTimeData.getString('securityTokenPinDialogUnknownError'));
  });

  // Test the text of the label when the number of attempts left is given.
  test('label text: attempts number', () => {
    const ATTEMPTS_LEFT = 3;
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.NONE,
      attemptsLeft: ATTEMPTS_LEFT
    };
    expectEquals(
        errorElement.textContent,
        loadTimeData.getStringF(
            'securityTokenPinDialogAttemptsLeft', ATTEMPTS_LEFT));
  });

  // Test that the label is empty when the number of attempts is, heuristically,
  // too big to be displayed for the user.
  test('label text: hidden attempts number', () => {
    const BIG_ATTEMPTS_LEFT = 4;
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.NONE,
      attemptsLeft: BIG_ATTEMPTS_LEFT
    };
    expectEquals(errorElement.textContent, '');
  });

  // Test the text of the label for the |INVALID_PIN| error when the number of
  // attempts left is given.
  test('label text: invalid PIN with attempts number', () => {
    const ATTEMPTS_LEFT = 3;
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PIN,
      attemptsLeft: ATTEMPTS_LEFT
    };
    expectEquals(
        errorElement.textContent,
        loadTimeData.getStringF(
            'securityTokenPinDialogErrorAttempts',
            loadTimeData.getString('securityTokenPinDialogUnknownInvalidPin'),
            ATTEMPTS_LEFT));
  });

  // Test the text of the label for the |INVALID_PIN| error when the number of
  // attempts left is, heuristically, too big to be displayed for the user.
  test('label text: invalid PIN with hidden attempts number', () => {
    const BIG_ATTEMPTS_LEFT = 4;
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PIN,
      attemptsLeft: BIG_ATTEMPTS_LEFT
    };
    expectEquals(
        errorElement.textContent,
        loadTimeData.getString('securityTokenPinDialogUnknownInvalidPin'));
  });

  // Test that no scrolling is necessary in order to see all dots after entering
  // a PIN of a typical length.
  test('8-digit PIN fits into input', () => {
    const PIN_LENGTH = 8;
    inputField.value = '0'.repeat(PIN_LENGTH);
    expectGT(inputField.scrollWidth, 0);
    expectLE(inputField.scrollWidth, inputField.clientWidth);
  });

  // Test that the distance between characters (dots) is set in a correct way
  // and doesn't fall back to the default value.
  test('PIN input letter-spacing is correctly set up', () => {
    expectNotEquals(
        getComputedStyle(inputField).getPropertyValue('letter-spacing'),
        'normal');
  });

  // Test that the focus on the input field isn't lost when the PIN is requested
  // again after the failed verification.
  test('focus restores after progress animation', () => {
    // The PIN keyboard is displayed initially.
    expectFalse(pinKeyboardContainer.hidden);
    expectTrue(progressElement.hidden);

    // The PIN keyboard gets focused.
    securityTokenPin.focus();
    expectEquals(securityTokenPin.shadowRoot.activeElement, pinKeyboard);
    expectEquals(inputField.getRootNode().activeElement, inputField);

    // The user submits some value while keeping the focus on the input field.
    pinInput.value = '123';
    const enterEvent = new Event('keydown');
    enterEvent.keyCode = 13;
    pinInput.dispatchEvent(enterEvent);
    // The PIN keyboard is replaced by the animation UI.
    expectTrue(pinKeyboardContainer.hidden);
    expectFalse(progressElement.hidden);

    // The response arrives, requesting to prompt for the PIN again.
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PIN,
      attemptsLeft: -1
    };
    // The PIN keyboard is shown again, replacing the animation UI.
    expectFalse(pinKeyboardContainer.hidden);
    expectTrue(progressElement.hidden);
    // The focus is on the input field.
    expectEquals(securityTokenPin.shadowRoot.activeElement, pinKeyboard);
    expectEquals(inputField.getRootNode().activeElement, inputField);
  });

  // Test that the input field gets focused when the PIN is requested again
  // after the failed verification.
  test('focus set after progress animation', () => {
    // The PIN keyboard is displayed initially.
    expectFalse(pinKeyboardContainer.hidden);
    expectTrue(progressElement.hidden);

    // The user submits some value using the "Submit" UI button.
    pinInput.value = '123';
    submitElement.click();
    // The PIN keyboard is replaced by the animation UI.
    expectTrue(pinKeyboardContainer.hidden);
    expectFalse(progressElement.hidden);

    // The response arrives, requesting to prompt for the PIN again.
    securityTokenPin.parameters = {
      codeType: OobeTypes.SecurityTokenPinDialogType.PIN,
      enableUserInput: true,
      errorLabel: OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PIN,
      attemptsLeft: -1
    };
    // The PIN keyboard is shown again, replacing the animation UI.
    expectFalse(pinKeyboardContainer.hidden);
    expectTrue(progressElement.hidden);
    // The focus is on the input field.
    expectEquals(securityTokenPin.shadowRoot.activeElement, pinKeyboard);
    expectEquals(inputField.getRootNode().activeElement, inputField);
  });

  mocha.run();
});
