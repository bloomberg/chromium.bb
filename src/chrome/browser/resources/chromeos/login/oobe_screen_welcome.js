// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview OOBE welcome screen implementation.
 */

login.createScreen('WelcomeScreen', 'connect', function() {
  var CONTEXT_KEY_LOCALE = 'locale';
  var CONTEXT_KEY_INPUT_METHOD = 'input-method';
  var CONTEXT_KEY_TIMEZONE = 'timezone';

  return {
    EXTERNAL_API: [],

    /** @override */
    decorate: function() {
      var welcomeScreen = $('oobe-welcome-md');
      welcomeScreen.screen = this;

      this.context.addObserver(
          CONTEXT_KEY_INPUT_METHOD, function(inputMethodId) {
            $('oobe-welcome-md').setSelectedKeyboard(inputMethodId);
          });

      this.updateLocalizedContent();
    },

    onLanguageSelected_: function(languageId) {
      this.context.set(CONTEXT_KEY_LOCALE, languageId);
      this.commitContextChanges();
    },

    onKeyboardSelected_: function(inputMethodId) {
      this.context.set(CONTEXT_KEY_INPUT_METHOD, inputMethodId);
      this.commitContextChanges();
    },

    onTimezoneSelected_: function(timezoneId) {
      this.context.set(CONTEXT_KEY_TIMEZONE, timezoneId);
      this.commitContextChanges();
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

    /** Called when OOBE configuration is loaded.
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
