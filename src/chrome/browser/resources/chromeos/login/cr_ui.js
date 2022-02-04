// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common OOBE controller methods for use in OOBE and login.
 * This file is shared between OOBE and login. Add only methods that need to be
 * shared between all *two* screens here.
 */

// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {i18nTemplate} from 'chrome://resources/js/i18n_template_no_process.m.js';
// #import {$} from 'chrome://resources/js/util.m.js';
// #import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

// #import {DisplayManager} from './display_manager.m.js';
// #import {DISPLAY_TYPE} from './components/display_manager_types.m.js';
// #import {DemoModeTestHelper} from './demo_mode_test_helper.m.js';
// #import {loadTimeData} from './i18n_setup.js';
// #import {OobeTypes} from './components/oobe_types.m.js';

cr.define('cr.ui', function() {
  /* #ignore */ var DisplayManager = cr.ui.login.DisplayManager;

  /**
   * Out of box controller. It manages initialization of screens,
   * transitions, error messages display.
   */
  /* #export */ class Oobe extends DisplayManager {
    /**
     * OOBE initialization coordination. Used by tests to wait for OOBE
     * to fully load when using the HTLImports polyfill.
     * TODO(crbug.com/1111387) - Remove once migrated to JS modules.
     * Remove spammy logging when closer to M89 branch point.
     */
    static waitForOobeToLoad() {
      return new Promise((resolve, reject) => {
        if (this.initializationComplete) {
          // TODO(crbug.com/1111387) - Remove excessive logging.
          console.warn('OOBE is already initialized. Continuing...');
          resolve();
        } else {
          // TODO(crbug.com/1111387) - Remove excessive logging.
          console.warn('OOBE not loaded yet. Waiting...');
          this.initCallbacks.push(resolve);
        }
      });
    }

    /**
     * Called when focus is returned from ash::SystemTray.
     * @param {boolean} reverse Is focus returned in reverse order?
     */
    static focusReturned(reverse) {
      const screen = Oobe.getInstance().currentScreen;
      if (screen && screen.onFocusReturned) {
        screen.onFocusReturned(reverse);
      }
    }

    /**
     * Handle accelerators. These are passed from native code instead of a JS
     * event handler in order to make sure that embedded iframes cannot swallow
     * them.
     * @param {string} name Accelerator name.
     */
    static handleAccelerator(name) {
      Oobe.getInstance().handleAccelerator(name);
    }

    /**
     * Shows the given screen.
     * @param {Object} screen Screen params dict, e.g. {id: screenId,
     *   data: data}
     */
    static showScreen(screen) {
      Oobe.getInstance().showScreen(screen);
    }

    /**
     * Updates missing API keys message visibility.
     * @param {boolean} show True if the message should be visible.
     */
    static showAPIKeysNotice(show) {
      $('api-keys-notice-container').hidden = !show;
    }

    /**
     * Updates version label visibility.
     * @param {boolean} show True if version label should be visible.
     */
    static showVersion(show) {
      Oobe.getInstance().showVersion(show);
    }

    /**
     * Update body class to switch between OOBE UI and Login UI.
     * @param {boolean} showOobe True if UI is in an OOBE mode (as opposed to
     * login).
     */
    static showOobeUI(showOobe) {
      if (showOobe) {
        document.body.classList.add('oobe-display');
      } else {
        document.body.classList.remove('oobe-display');
        Oobe.getInstance().prepareForLoginDisplay_();
      }
    }

    /**
     * Enables keyboard driven flow.
     * @param {boolean} value True if keyboard navigation flow is forced.
     */
    static enableKeyboardFlow(value) {
      Oobe.getInstance().forceKeyboardFlow = value;
    }

    /**
     * Changes some UI which depends on the virtual keyboard being shown/hidden.
     */
    static setVirtualKeyboardShown(shown) {
      Oobe.getInstance().virtualKeyboardShown = shown;
    }

    /**
     * Shows signin UI.
     * @param {string} opt_email An optional email for signin UI.
     */
    static showSigninUI(opt_email) {
      Oobe.getInstance().showSigninUI(opt_email);
    }

    /**
     * Resets sign-in input fields.
     * @param {boolean} forceOnline Whether online sign-in should be forced.
     * If |forceOnline| is false previously used sign-in type will be used.
     */
    static resetSigninUI(forceOnline) {
      Oobe.getInstance().resetSigninUI(forceOnline);
    }

    /**
     * Show user-pods.
     */
    static showUserPods() {
      if (Oobe.getInstance().showingViewsLogin) {
        chrome.send('hideOobeDialog');
        return;
      }
      this.showSigninUI("");
      this.resetSigninUI(true);
    }

    /**
     * Sets the current size of the client area (display size).
     * @param {number} width client area width
     * @param {number} height client area height
     */
    static setClientAreaSize(width, height) {
      Oobe.getInstance().setClientAreaSize(width, height);
    }

    /**
     * Sets the current height of the shelf area.
     * @param {number} height current shelf height
     */
    static setShelfHeight(height) {
      Oobe.getInstance().setShelfHeight(height);
    }

    static setOrientation(isHorizontal) {
      Oobe.getInstance().setOrientation(isHorizontal);
    }

    /**
     * Sets the required size of the oobe dialog.
     * @param {number} width oobe dialog width
     * @param {number} height oobe dialog height
     */
    static setDialogSize(width, height) {
      Oobe.getInstance().setDialogSize(width, height);
    }

    /**
     * Sets the hint for calculating OOBE dialog margins.
     * @param {OobeTypes.DialogPaddingMode} mode
     */
    static setDialogPaddingMode(mode) {
      Oobe.getInstance().setDialogPaddingMode(mode);
    }

    /**
     * Sets the number of users on the views login screen.
     * @param {number} userCount The number of users.
     */
    static setLoginUserCount(userCount) {
      Oobe.getInstance().setLoginUserCount(userCount);
    }

    /**
     * Skip to login screen for telemetry.
     */
    static skipToLoginForTesting() {
      chrome.send('skipToLoginForTesting');
    }

    /**
     * Login for telemetry.
     * @param {string} username Login username.
     * @param {string} password Login password.
     * @param {string} gaia_id GAIA ID.
     * @param {boolean} enterpriseEnroll Login as an enterprise enrollment?
     */
    static loginForTesting(
        username, password, gaia_id, enterpriseEnroll = false) {
      // Helper method that runs |fn| after |screenName| is visible.
      function waitForOobeScreen(screenName, fn) {
        if (Oobe.getInstance().currentScreen &&
            Oobe.getInstance().currentScreen.id === screenName) {
          fn();
        } else {
          $('oobe').addEventListener('screenchanged', function handler(e) {
            if (e.detail == screenName) {
              $('oobe').removeEventListener('screenchanged', handler);
              fn();
            }
          });
        }
      }

      chrome.send('skipToLoginForTesting');

      if (!enterpriseEnroll) {
        chrome.send('completeLogin', [gaia_id, username, password, false]);
      } else {
        waitForOobeScreen('gaia-signin', function() {
          // TODO(crbug.com/1100910): migrate logic to dedicated test api.
          chrome.send('toggleEnrollmentScreen');
          chrome.send('toggleFakeEnrollment');
        });

        waitForOobeScreen('enterprise-enrollment', function() {
          chrome.send('oauthEnrollCompleteLogin', [username]);
        });
      }
    }  // loginForTesting

    /**
     * Guest login for telemetry.
     */
    static guestLoginForTesting() {
      this.skipToLoginForTesting();
      chrome.send('launchIncognito');
    }

    /**
     * Gaia login screen for telemetry.
     */
    static addUserForTesting() {
      this.skipToLoginForTesting();
      chrome.send('addUser');
    }

    /**
     * Shows the add user dialog. Used in browser tests.
     */
    static showAddUserForTesting() {
      chrome.send('showAddUser');
    }

    /**
     * Hotrod requisition for telemetry.
     */
    static remoraRequisitionForTesting() {
      chrome.send('WelcomeScreen.setDeviceRequisition', ['remora']);
    }

    /**
     * Begin enterprise enrollment for telemetry.
     */
    static switchToEnterpriseEnrollmentForTesting() {
      // TODO(crbug.com/1100910): migrate logic to dedicated test api.
      chrome.send('toggleEnrollmentScreen');
    }

    /**
     * Finish enterprise enrollment for telemetry.
     */
    static enterpriseEnrollmentDone() {
      // TODO(crbug.com/1100910): migrate logic to dedicated test api.
      chrome.send('oauthEnrollClose', ['done']);
    }

    /**
     * Returns true if enrollment was successful. Dismisses the enrollment
     * attribute screen if it's present.
     *
     * @suppress {missingProperties}
     * $('enterprise-enrollment').uiStep
     * TODO(crbug.com/1229130) - Remove this suppression.
     */
    static isEnrollmentSuccessfulForTest() {
      const step = $('enterprise-enrollment').uiStep;
      // TODO(crbug.com/1229130) - Improve this check.
      if (step === OobeTypes.EnrollmentStep.ATTRIBUTE_PROMPT) {
        chrome.send('oauthEnrollAttributes', ['', '']);
        return true;
      }

      return step === OobeTypes.EnrollmentStep.SUCCESS;
    }

    /**
     * Starts online demo mode setup for telemetry. Is used in autotests.
     */
    static setUpOnlineDemoModeForTesting() {
      DemoModeTestHelper.setUp('online');
    }

    /**
     * Get the primary display's name.
     *
     * Same as the displayInfo.name parameter returned by
     * chrome.system.display.getInfo(), but unlike chrome.system it's available
     * during OOBE.
     */
    static getPrimaryDisplayNameForTesting() {
      return cr.sendWithPromise('getPrimaryDisplayNameForTesting');
    }

    /**
     * Click on the primary action button ("Next" usually) for Gaia. On the
     * Login or Enterprise Enrollment screen.
     *
     * @suppress {missingProperties}
     * $('...').clickPrimaryButtonForTesting()
     * TODO(crbug.com/1229130) - Remove this suppression.
     */
    static clickGaiaPrimaryButtonForTesting() {
      if (!$('gaia-signin').hidden) {
        $('gaia-signin').clickPrimaryButtonForTesting();
      } else {
        assert(!$('enterprise-enrollment').hidden);
        $('enterprise-enrollment').clickPrimaryButtonForTesting();
      }
    }
    /**
     * Initializes the OOBE flow.  This will cause all C++ handlers to
     * be invoked to do final setup.
     */
    static initialize() {
      Oobe.getInstance().initialize();
      chrome.send('screenStateInitialize');
    }

    /**
     * Reloads content of the page (localized strings, options of the select
     * controls).
     * @param {!Object} data New dictionary with i18n values.
     */
    static reloadContent(data) {
      // Reload global local strings, process DOM tree again.
      loadTimeData.overrideValues(data);
      i18nTemplate.process(document, loadTimeData);

      // Update localized content of the screens.
      Oobe.getInstance().updateLocalizedContent_();
    }

    /**
     * Updates "device in tablet mode" state when tablet mode is changed.
     * @param {Boolean} isInTabletMode True when in tablet mode.
     */
    static setTabletModeState(isInTabletMode) {
      Oobe.getInstance().setTabletModeState_(isInTabletMode);
    }

    /**
     * Updates OOBE configuration when it is loaded.
     * @param {!OobeTypes.OobeConfiguration} configuration OOBE configuration.
     */
    static updateOobeConfiguration(configuration) {
      Oobe.getInstance().updateOobeConfiguration_(configuration);
    }

  }  // class Oobe

  Oobe.initializationComplete = false;
  Oobe.initCallbacks = [];
  /**
   * Some ForTesting APIs directly access to DOM. Because this script is loaded
   * in header, DOM tree may not be available at beginning.
   * In DOMContentLoaded, after Oobe.initialize() is done, this is marked to
   * true, indicating ForTesting methods can be called.
   * External script using ForTesting APIs should wait for this condition.
   * @type {boolean}
   */
  Oobe.readyForTesting = false;

  cr.addSingletonGetter(Oobe);

  // #cr_define_end
  // Export
  return {Oobe: Oobe};
});
