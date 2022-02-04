// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {afterNextRender, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {$} from 'chrome://resources/js/util.m.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// clang-format on

/**
 * @fileoverview Common testing utils methods used for OOBE tast tests.
 */

class TestElementApi {
  /**
   * Returns HTMLElement with $$ support.
   * @return {HTMLElement}
   */
  element() {
    throw 'element() should be defined!';
  }

  /**
   * Returns whether the element is visible.
   * @return {boolean}
   */
  isVisible() {
    return !this.element().hidden;
  }

  /**
   * Returns whether the element is enabled.
   * @return {boolean}
   */
  isEnabled() {
    return !this.element().disabled;
  }
}

class ScreenElementApi extends TestElementApi {
  constructor(id) {
    super();
    this.id = id;
    this.nextButton = undefined;
  }

  /** @override */
  element() {
    return $(this.id);
  }

  /**
   * Click on the primary action button ("Next" usually).
   */
  clickNext() {
    assert(this.nextButton);
    this.nextButton.click();
  }

  /**
   * Returns whether the screen should be skipped.
   * @return {boolean}
   */
  shouldSkip() {
    return false;
  }
}

class PolymerElementApi extends TestElementApi {
  constructor(parent, query) {
    super();
    this.parent = parent;
    this.query = query;
  }

  /** @override */
  element() {
    assert(this.parent.element());
    return this.parent.element().shadowRoot.querySelector(this.query);
  }

  /**
   * Assert element is visible/enabled and click on the element.
   */
  click() {
    assert(this.isVisible());
    assert(this.isEnabled());
    this.element().click();
  }
}

class TextFieldApi extends PolymerElementApi {
  constructor(parent, query) {
    super(parent, query);
  }

  /**
   * Assert element is visible/enabled and fill in the element with a value.
   * @param {string} value
   */
  typeInto(value) {
    assert(this.isVisible());
    assert(this.isEnabled());
    this.element().value = value;
    this.element().dispatchEvent(new Event('input'));
    this.element().dispatchEvent(new Event('change'));
  }
}

class HIDDetectionScreenTester extends ScreenElementApi {
  constructor() {
    super('hid-detection');
    this.nextButton = new PolymerElementApi(this, '#hid-continue-button');
  }

  // Must be called to enable the next button
  emulateDevicesConnected() {
    chrome.send('HIDDetectionScreen.emulateDevicesConnectedForTesting');
  }

  touchscreenDetected() {
    // Touchscreen entire row is only visible when touchscreen is detected.
    let touchscreenRow = new PolymerElementApi(this, '#hid-touchscreen-entry');
    return touchscreenRow.isVisible();
  }

  mouseDetected() {
    let mouseTickIcon = new PolymerElementApi(this, '#mouse-tick');
    return mouseTickIcon.isVisible();
  }

  keyboardDetected() {
    let keyboardTickIcon = new PolymerElementApi(this, '#keyboard-tick');
    return keyboardTickIcon.isVisible();
  }

  canClickNext() {
    return this.nextButton.isEnabled();
  }
}

class WelcomeScreenTester extends ScreenElementApi {
  constructor() {
    super('connect');
  }

  /** @override */
  clickNext() {
    if (!this.nextButton) {
      let mainStep = new PolymerElementApi(this, '#welcomeScreen');
      this.nextButton = new PolymerElementApi(mainStep, '#getStarted');
    }

    assert(this.nextButton);
    this.nextButton.click();
  }
}

class NetworkScreenTester extends ScreenElementApi {
  constructor() {
    super('network-selection');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }

  /** @override */
  shouldSkip() {
    return loadTimeData.getBoolean('testapi_shouldSkipNetworkFirstShow');
  }
}

class EulaScreenTester extends ScreenElementApi {
  constructor() {
    super('oobe-eula-md');
    this.eulaStep = new PolymerElementApi(this, '#eulaDialog');
    this.nextButton = new PolymerElementApi(this, '#acceptButton');
  }

  /** @override */
  shouldSkip() {
    // Eula screen should skipped on non-branded build and on CfM devices.
    return loadTimeData.getBoolean('testapi_shouldSkipEula');
  }

  /**
   * Returns if the EULA Screen is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible() && this.eulaStep.isVisible() &&
        this.nextButton.isVisible();
  }

  getNextButtonName() {
    return loadTimeData.getString('oobeEulaAcceptAndContinueButtonText');
  }
}

class UpdateScreenTester extends ScreenElementApi {
  constructor() {
    super('oobe-update');
  }
}

class EnrollmentScreenTester extends ScreenElementApi {
  constructor() {
    super('enterprise-enrollment');
  }
}

class UserCreationScreenTester extends ScreenElementApi {
  constructor() {
    super('user-creation');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }
}

class GaiaScreenTester extends ScreenElementApi {
  constructor() {
    super('gaia-signin');
    this.signinFrame = new PolymerElementApi(this, '#signin-frame-dialog');
    this.gaiaDialog = new PolymerElementApi(this.signinFrame, '#gaiaDialog');
    this.gaiaLoading = new PolymerElementApi(this, '#gaia-loading');
  }

  /**
   * Returns if the Gaia Screen is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible() && !this.gaiaLoading.isVisible() &&
        this.signinFrame.isVisible() && this.gaiaDialog.isVisible();
  }
}

class ConfirmSamlPasswordScreenTester extends ScreenElementApi {
  constructor() {
    super('saml-confirm-password');
    this.passwordInput = new TextFieldApi(this, '#passwordInput');
    this.confirmPasswordInput = new TextFieldApi(this, '#confirmPasswordInput');
    this.nextButton = new PolymerElementApi(this, '#next');
  }

  /**
   * Enter password input fields with password value and submit the form.
   * @param {string} password
   */
  enterManualPasswords(password) {
    this.passwordInput.typeInto(password);
    Polymer.RenderStatus.afterNextRender(assert(this.element()), () => {
      this.confirmPasswordInput.typeInto(password);
      Polymer.RenderStatus.afterNextRender(assert(this.element()), () => {
        this.clickNext();
      });
    });
  }
}

class PinSetupScreenTester extends ScreenElementApi {
  constructor() {
    super('pin-setup');
    this.skipButton = new PolymerElementApi(this, '#setupSkipButton');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
    this.doneButton = new PolymerElementApi(this, '#doneButton');
    this.backButton = new PolymerElementApi(this, '#backButton');
    let pinSetupKeyboard = new PolymerElementApi(this, '#pinKeyboard');
    let pinKeyboard = new PolymerElementApi(pinSetupKeyboard, '#pinKeyboard');
    this.pinField = new TextFieldApi(pinKeyboard, '#pinInput');
    this.pinButtons = {};
    for (let i = 0; i <= 9; i++) {
      this.pinButtons[i.toString()] =
          new PolymerElementApi(pinKeyboard, '#digitButton' + i.toString());
    }
  }

  /**
   * Enter PIN into PINKeyboard input field, without submitting.
   * @param {string} pin
   */
  enterPin(pin) {
    this.pinField.typeInto(pin);
  }

  /**
   * Presses a single digit button in the PIN keyboard.
   * @param {string} digit String with single digit to be clicked on.
   */
  pressPinDigit(digit) {
    this.pinButtons[digit].click();
  }
}

class EnrollmentSignInStep extends PolymerElementApi {
  constructor(parent) {
    super(parent, '#step-signin');
    this.signInFrame = new PolymerElementApi(this, '#signin-frame');
    this.nextButton = new PolymerElementApi(this, '#primary-action-button');
  }

  /**
   * Returns if the Enrollment Signing step is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible() && this.signInFrame.isVisible() &&
        this.nextButton.isVisible();
  }
}

class EnrollmentSuccessStep extends PolymerElementApi {
  constructor(parent) {
    super(parent, '#step-success');
    this.nextButton = new PolymerElementApi(parent, '#successDoneButton');
  }

  /**
   * Returns if the Enrollment Success step is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible() && this.nextButton.isVisible();
  }

  clickNext() {
    this.nextButton.click();
  }
}

class EnrollmentErrorStep extends PolymerElementApi {
  constructor(parent) {
    super(parent, '#step-error');
    this.retryButton = new PolymerElementApi(parent, '#errorRetryButton');
    this.errorMsgContainer = new PolymerElementApi(parent, '#errorMsg');
  }

  /**
   * Returns if the Enrollment Error step is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible() && this.errorMsgContainer.isVisible();
  }

  /**
   * Returns if enterprise enrollment can be retried.
   * @return {boolean}
   */
  canRetryEnrollment() {
    return this.retryButton.isVisible() && this.retryButton.isEnabled();
  }

  /**
   * Click the Retry button on the enrollment error screen.
   */
  clickRetryButton() {
    assert(this.canRetryEnrollment());
    this.retryButton.click();
  }

  /**
   * Returns the error text shown on the enrollment error screen.
   * @return {string}
   */
  getErrorMsg() {
    assert(this.isReadyForTesting());
    return this.errorMsgContainer.element().innerText;
  }
}

class EnterpriseEnrollmentScreenTester extends ScreenElementApi {
  constructor() {
    super('enterprise-enrollment');
    this.signInStep = new EnrollmentSignInStep(this);
    this.successStep = new EnrollmentSuccessStep(this);
    this.errorStep = new EnrollmentErrorStep(this);
    this.enrollmentInProgressDlg = new PolymerElementApi(this, '#step-working');
  }

  /**
   * Returns if enrollment is in progress.
   * @return {boolean}
   */
  isEnrollmentInProgress() {
    return this.enrollmentInProgressDlg.isVisible();
  }
}

class OobeApiProvider {
  constructor() {
    this.screens = {
      HIDDetectionScreen: new HIDDetectionScreenTester(),
      WelcomeScreen: new WelcomeScreenTester(),
      NetworkScreen: new NetworkScreenTester(),
      EulaScreen: new EulaScreenTester(),
      UpdateScreen: new UpdateScreenTester(),
      EnrollmentScreen: new EnrollmentScreenTester(),
      UserCreationScreen: new UserCreationScreenTester(),
      GaiaScreen: new GaiaScreenTester(),
      ConfirmSamlPasswordScreen: new ConfirmSamlPasswordScreenTester(),
      PinSetupScreen: new PinSetupScreenTester(),
      EnterpriseEnrollmentScreen: new EnterpriseEnrollmentScreenTester(),
    };

    this.loginWithPin = function(username, pin) {
      chrome.send('OobeTestApi.loginWithPin', [username, pin]);
    };

    this.advanceToScreen = function(screen) {
      chrome.send('OobeTestApi.advanceToScreen', [screen]);
    };

    this.skipPostLoginScreens = function() {
      chrome.send('OobeTestApi.skipPostLoginScreens');
    };
  }
}

window.OobeAPI = new OobeApiProvider();
