// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview OOBE welcome screen implementation.
 */

login.createScreen('WelcomeScreen', 'connect', function() {
  return {
    EXTERNAL_API:
        ['onInputMethodIdSetFromBackend', 'onTimezoneIdSetFromBackend'],

    /** @override */
    decorate: function() {
      var welcomeScreen = $('oobe-welcome-md');
      welcomeScreen.screen = this;

      this.updateLocalizedContent();
    },

    onInputMethodIdSetFromBackend: function(inputMethodId) {
      $('oobe-welcome-md').setSelectedKeyboard(inputMethodId);
    },

    onTimezoneIdSetFromBackend: function(timezoneId) {
      // Timezone change triggers a localized content update so we don't need to
      // do anything here.
    },

    onLanguageSelected_: function(languageId) {
      chrome.send('WelcomeScreen.setLocaleId', [languageId]);
    },

    onKeyboardSelected_: function(inputMethodId) {
      chrome.send('WelcomeScreen.setInputMethodId', [inputMethodId]);
    },

    onTimezoneSelected_: function(timezoneId) {
      chrome.send('WelcomeScreen.setTimezoneId', [timezoneId]);
    },

    onBeforeShow: function(data) {
      var debuggingLinkVisible =
          data && 'isDeveloperMode' in data && data['isDeveloperMode'];
      $('oobe-welcome-md').debuggingLinkVisible = debuggingLinkVisible;
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('welcomeScreenTitle');
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('oobe-welcome-md');
    },

    /**
     * This is called after resources are updated.
     */
    updateLocalizedContent: function() {
      $('oobe-welcome-md').updateLocalizedContent();
    },

    /**
     * Called when OOBE configuration is loaded.
     * @param {!OobeTypes.OobeConfiguration} configuration
     */
    updateOobeConfiguration: function(configuration) {
      $('oobe-welcome-md').updateOobeConfiguration(configuration);
    },

    /**
     * Updates "device in tablet mode" state when tablet mode is changed.
     * @param {Boolean} isInTabletMode True when in tablet mode.
     */
    setTabletModeState: function(isInTabletMode) {
      $('oobe-welcome-md').setTabletModeState(isInTabletMode);
    },

    /**
     * Window-resize event listener (delivered through the display_manager).
     */
    onWindowResize: function() {
      $('oobe-welcome-md').onWindowResize();
    },
  };
});
